#include "min3.h"

#include <cstring>

#include "RingOA/fss/prg.h"
#include "RingOA/sharing/beaver_triples_gen.h"
#include "RingOA/sharing/beaver_triples_io.h"
#include "RingOA/utils/bytes.h"
#include "RingOA/utils/logger.h"
#include "RingOA/utils/network.h"
#include "RingOA/utils/timer.h"
#include "RingOA/utils/to_string.h"
#include "RingOA/utils/utils.h"

namespace ringoa {
namespace proto {

void Min3Parameters::PrintParametersDebug(bool with_header, int key_width) const {
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    if (with_header) {
        Logger::DebugLog(LOC, Logger::MakeSeparatorLine());
        Logger::DebugLog(LOC, "[Min3 Params]");
        Logger::DebugLog(LOC, Logger::MakeSeparatorLine());
    }

    Logger::DebugLog(LOC, Logger::FormatKeyValue("InputDomainBits", ToString(input_domain_bits_), key_width));

    params_.PrintParametersDebug(false, key_width);

    if (with_header) {
        Logger::DebugLog(LOC, Logger::MakeSeparatorLine());
    }
#endif
}

Min3Keys::Min3Keys(
    uint64_t              party_id,
    const Min3Parameters &params,
    size_t                count)
    : ic_key_1(party_id, params.GetParameters(), count),
      ic_key_2(party_id, params.GetParameters(), count) {
}

size_t Min3Keys::CalculateSerializedSize() const {
    size_t size = 0;

    size += sizeof(uint64_t);
    size += ic_key_1.CalculateSerializedSize();
    size += ic_key_2.CalculateSerializedSize();
    return size;
}

void Min3Keys::Serialize(std::vector<uint8_t> &buffer) const {
    const size_t start = buffer.size();

    // Number of keys
    const uint64_t num = Size();
    append_pod(buffer, num);

    // Serialize the IntegerComparison keys
    ic_key_1.Serialize(buffer);
    ic_key_2.Serialize(buffer);

    // Check size
    const size_t written  = buffer.size() - start;
    const size_t expected = CalculateSerializedSize();
    if (written != expected) {
        throw std::runtime_error(LOC + " Min3Keys::Serialize: serialized size mismatch: " +
                                 ToString(written) + " != " + ToString(expected));
    }
}

void Min3Keys::Deserialize(const std::vector<uint8_t> &buffer, const Min3Parameters &params) {
    size_t offset = 0;
    Deserialize(buffer, params, offset);
}

void Min3Keys::Deserialize(const std::vector<uint8_t> &buffer,
                           const Min3Parameters       &params,
                           size_t                     &offset) {
    const size_t start = offset;

    // Number of keys
    uint64_t num = 0;
    read_pod(buffer, offset, num);

    ic_key_1.Deserialize(buffer, params.GetParameters(), offset);
    ic_key_2.Deserialize(buffer, params.GetParameters(), offset);

    // Check size
    const size_t read     = offset - start;
    const size_t expected = CalculateSerializedSize();
    if (read != expected) {
        throw std::runtime_error(LOC + " Min3Keys::Deserialize: deserialized size mismatch: " +
                                 ToString(read) + " != " + ToString(expected));
    }
}

Min3::Min3(
    const Min3Parameters &params,
    ProtocolContext2P    &ctx)
    : params_(params), comp_(params.GetParameters(), ctx), ctx_(ctx) {
}

void Min3::OfflineSetUp(const uint64_t count, const std::string &file_path) const {
    if (count == 0) {
        throw std::invalid_argument(LOC + " Min3::OfflineSetUp: count must be greater than 0");
    }

    const size_t num_triples = static_cast<size_t>(count) * 2;
    auto         shares      = ringoa::sharing::GenerateAndShareBeaverTriples(num_triples, ctx_.Config());
    ringoa::sharing::Save2PTriplesShares(
        file_path,
        shares.first,
        shares.second);
}

std::pair<Min3Keys, Min3Keys> Min3::GenerateKeys(const size_t count) const {
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "Generating Min3 key, count = " + ToString(count));
#endif

    Min3Keys key_0(0, params_, count);
    Min3Keys key_1(1, params_, count);

    auto ic_key_pair_1 = comp_.GenerateKeys(count);
    auto ic_key_pair_2 = comp_.GenerateKeys(count);

    key_0.ic_key_1 = std::move(ic_key_pair_1.first);
    key_0.ic_key_2 = std::move(ic_key_pair_2.first);

    key_1.ic_key_1 = std::move(ic_key_pair_1.second);
    key_1.ic_key_2 = std::move(ic_key_pair_2.second);

    return std::make_pair(std::move(key_0), std::move(key_1));
}

void Min3::OnlineSetUp(const uint64_t party_id, const std::string &file_path) {
    ctx_.Arith().SetTriples(ringoa::sharing::Load2PTriplesShare(file_path, party_id));
}

uint64_t Min3::EvaluateSharedInput(osuCrypto::Channel &chl, const Min3KeyView &key, const std::array<uint64_t, 3> &inputs) const {
    uint64_t party_id = key.ic_key_1.ddcf_key.dcf_key.party_id;
    uint64_t x        = inputs[0];
    uint64_t y        = inputs[1];
    uint64_t z        = inputs[2];
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "Evaluating Min3 protocol with shared inputs");
    std::string party_str = (party_id == 0) ? "[P0]" : "[P1]";
    Logger::DebugLog(LOC, party_str + " (x_sh, y_sh, z_sh): (" +
                              ToString(inputs[0]) + ", " + ToString(inputs[1]) + ", " + ToString(inputs[2]) + ")");
#endif

    if (party_id == 0) {
        uint64_t less_xy_c = comp_.EvaluateSharedInput(chl, key.ic_key_1, x, y);
        uint64_t small_xy  = 0;
        ctx_.Arith().EvaluateSelect(party_id, chl, x, y, less_xy_c, small_xy);
        uint64_t less_xyz_c = comp_.EvaluateSharedInput(chl, key.ic_key_2, small_xy, z);
        uint64_t min_xyz    = 0;
        ctx_.Arith().EvaluateSelect(party_id, chl, small_xy, z, less_xyz_c, min_xyz);
        return min_xyz;
    } else {
        uint64_t less_xy_c = comp_.EvaluateSharedInput(chl, key.ic_key_1, x, y);
        uint64_t small_xy  = 0;
        ctx_.Arith().EvaluateSelect(party_id, chl, x, y, less_xy_c, small_xy);
        uint64_t less_xyz_c = comp_.EvaluateSharedInput(chl, key.ic_key_2, small_xy, z);
        uint64_t min_xyz    = 0;
        ctx_.Arith().EvaluateSelect(party_id, chl, small_xy, z, less_xyz_c, min_xyz);
        return min_xyz;
    }
}

void Min3::EvaluateSharedInputBatch(osuCrypto::Channel                         &chl,
                                    const Min3Keys                             &keys,
                                    const std::vector<std::array<uint64_t, 3>> &inputs,
                                    std::vector<uint64_t>                      &out) const {

    Logger::WarnLog(LOC, "This batch evaluation is not optimized. Communication is not parallelized, leading to more rounds than necessary.");

    const size_t count = inputs.size();
    if (keys.Size() != count) {
        throw std::invalid_argument(
            "Min3::EvaluateSharedInputBatch: keys.Size() != inputs.size()");
    }
    const uint64_t party_id = keys.ic_key_1.GetView(0).ddcf_key.dcf_key.party_id;

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "Evaluating Min3 batch with shared inputs");
    Logger::DebugLog(LOC, "Batch size: " + ToString(count));
#endif

    for (size_t i = 0; i < count; ++i) {
        const uint64_t x = inputs[i][0];
        const uint64_t y = inputs[i][1];
        const uint64_t z = inputs[i][2];

        const auto k1 = keys.ic_key_1.GetView(i);
        const auto k2 = keys.ic_key_2.GetView(i);

        const uint64_t less_xy_c = comp_.EvaluateSharedInput(chl, k1, x, y);
        uint64_t       small_xy  = 0;
        ctx_.Arith().EvaluateSelect(party_id, chl, x, y, less_xy_c, small_xy);

        const uint64_t less_xyz_c = comp_.EvaluateSharedInput(chl, k2, small_xy, z);
        uint64_t       min_xyz    = 0;
        ctx_.Arith().EvaluateSelect(party_id, chl, small_xy, z, less_xyz_c, min_xyz);

        out[i] = min_xyz;
    }
}

}    // namespace proto
}    // namespace ringoa
