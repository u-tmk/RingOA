#include "zero_test.h"

#include <cstring>

#include "RingOA/sharing/sharing_2p_ring.h"
#include "RingOA/utils/bytes.h"
#include "RingOA/utils/logger.h"
#include "RingOA/utils/rng.h"
#include "RingOA/utils/to_string.h"
#include "RingOA/utils/utils.h"

namespace ringoa {
namespace proto {

void ZeroTestParameters::PrintParametersDebug(bool with_header, int key_width) const {
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    if (with_header) {
        Logger::DebugLog(LOC, Logger::MakeSeparatorLine());
        Logger::DebugLog(LOC, "[ZeroTest Params]");
        Logger::DebugLog(LOC, Logger::MakeSeparatorLine());
    }

    Logger::DebugLog(LOC, Logger::FormatKeyValue("InputDomainBits", ToString(input_domain_bits_), key_width));
    Logger::DebugLog(LOC, Logger::FormatKeyValue("DpfOutputBits", ToString(dpf_out_bits_), key_width));

    params_.PrintParametersDebug(false, key_width);

    if (with_header) {
        Logger::DebugLog(LOC, Logger::MakeSeparatorLine());
    }
#endif
}

ZeroTestKeys::ZeroTestKeys(
    uint64_t                  party_id,
    const ZeroTestParameters &params,
    size_t                    count)
    : dpf_key(),
      shr_in(count, 0) {
    dpf_key.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        dpf_key.emplace_back(party_id, params.GetParameters());
    }
}

size_t ZeroTestKeys::CalculateSerializedSize() const {
    size_t size = 0;

    size += sizeof(uint64_t);
    const size_t num = Size();
    for (size_t i = 0; i < num; ++i) {
        size += dpf_key[i].CalculateSerializedSize();
    }
    size += num * sizeof(uint64_t);
    return size;
}

void ZeroTestKeys::Serialize(std::vector<uint8_t> &buffer) const {
    const size_t start = buffer.size();

    // Number of keys
    const uint64_t num = Size();
    append_pod(buffer, num);

    // Serialize each DPF key
    for (const auto &key : dpf_key) {
        key.Serialize(buffer);
    }

    // Serialize shared random values
    append_array(buffer, shr_in.data(), static_cast<size_t>(num));

    // Check size
    const size_t written  = buffer.size() - start;
    const size_t expected = CalculateSerializedSize();
    if (written != expected) {
        throw std::runtime_error(LOC + " ZeroTestKeys::Serialize: serialized size mismatch: " +
                                 ToString(written) + " != " + ToString(expected));
    }
}

void ZeroTestKeys::Deserialize(const std::vector<uint8_t> &buffer,
                               const ZeroTestParameters   &params) {
    size_t offset = 0;
    Deserialize(buffer, params, offset);
}

void ZeroTestKeys::Deserialize(const std::vector<uint8_t> &buffer,
                               const ZeroTestParameters   &params,
                               size_t                     &offset) {
    const size_t start = offset;

    // Number of keys
    uint64_t num = 0;
    read_pod(buffer, offset, num);

    // Reset containers
    dpf_key.clear();
    shr_in.clear();

    dpf_key.reserve(num);
    shr_in.resize(num);

    // DPF keys: construct with params then deserialize in-place
    for (size_t i = 0; i < static_cast<size_t>(num); ++i) {
        dpf_key.emplace_back(/*party_id=*/0, params.GetParameters());
        dpf_key.back().Deserialize(buffer, offset);
    }

    // Shared random values (bulk)
    read_array(buffer, offset, shr_in.data(), static_cast<size_t>(num));

    // Check size
    const size_t read     = offset - start;
    const size_t expected = CalculateSerializedSize();
    if (read != expected) {
        throw std::runtime_error(LOC + " ZeroTestKeys::Deserialize: deserialized size mismatch: " +
                                 ToString(read) + " != " + ToString(expected));
    }
}

ZeroTest::ZeroTest(
    const ZeroTestParameters &params,
    ProtocolContext2P        &ctx)
    : params_(params),
      gen_(params.GetParameters()),
      eval_(params.GetParameters()),
      ctx_(ctx) {
}

std::pair<ZeroTestKeys, ZeroTestKeys> ZeroTest::GenerateKeys(const size_t count) const {
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "Generating ZeroTest keys, count = " + ToString(count));
#endif

    uint64_t     n = params_.GetInputDomainBits();
    ZeroTestKeys key_0(0, params_, count);
    ZeroTestKeys key_1(1, params_, count);

    for (size_t i = 0; i < count; ++i) {
        // Generate random inputs for the keys
        uint64_t r_in = Mod2N(GlobalRng::Rand<uint64_t>(), n);

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
        Logger::DebugLog(LOC, "Key " + ToString(i) + ": r_in = " + ToString(r_in));
#endif

        // Generate DPF keys
        auto dpf_keys = gen_.GenerateKeys(r_in, 1);

        key_0.dpf_key[i] = std::move(dpf_keys.first);
        key_1.dpf_key[i] = std::move(dpf_keys.second);

        // Generate shared random values
        std::pair<uint64_t, uint64_t> r_in_sh = ctx_.Arith().Share(r_in);

        key_0.shr_in[i] = r_in_sh.first;
        key_1.shr_in[i] = r_in_sh.second;
    }

    return std::make_pair(std::move(key_0), std::move(key_1));
}

uint64_t ZeroTest::EvaluateSharedInput(osuCrypto::Channel &chl, const ZeroTestKeyView &key, const uint64_t x) const {
    uint64_t party_id = key.dpf_key.party_id;
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "Evaluating ZeroTest protocol with shared inputs");
    std::string party_str = (party_id == 0) ? "[P0]" : "[P1]";
    Logger::DebugLog(LOC, party_str + " x_sh: " + ToString(x));
#endif

    if (party_id == 0) {
        // Reconstruct masked inputs
        uint64_t masked_x_0, masked_x_1, masked_x;
        ctx_.Arith().EvaluateAdd(x, key.shr_in, masked_x_0);
        ctx_.Arith().Reconst(party_id, chl, masked_x_0, masked_x_1, masked_x);

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
        Logger::DebugLog(LOC, party_str + " masked_x_0: " + ToString(masked_x_0));
        Logger::DebugLog(LOC, party_str + " masked_x: " + ToString(masked_x));
#endif
        return EvaluateMaskedInput(key, masked_x);
    } else {
        // Reconstruct masked inputs
        uint64_t masked_x_0, masked_x_1, masked_x;
        ctx_.Arith().EvaluateAdd(x, key.shr_in, masked_x_1);
        ctx_.Arith().Reconst(party_id, chl, masked_x_0, masked_x_1, masked_x);

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
        Logger::DebugLog(LOC, party_str + " masked_x_1: " + ToString(masked_x_1));
        Logger::DebugLog(LOC, party_str + " masked_x: " + ToString(masked_x));
#endif
        return EvaluateMaskedInput(key, masked_x);
    }
}

uint64_t ZeroTest::EvaluateMaskedInput(const ZeroTestKeyView &key, const uint64_t x) const {
    uint64_t party_id = key.dpf_key.party_id;
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "Evaluating ZeroTest protocol with masked inputs");
    std::string party_str = (party_id == 0) ? "[P0]" : "[P1]";
    Logger::DebugLog(LOC, party_str + " masked_x: " + ToString(x));
#endif

    uint64_t output = eval_.EvaluateAt(key.dpf_key, x);

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, party_str + " output: " + ToString(output));
#endif

    return output;
}

void ZeroTest::EvaluateSharedInputBatch(osuCrypto::Channel          &chl,
                                        const ZeroTestKeys          &keys,
                                        const std::vector<uint64_t> &x,
                                        std::vector<uint64_t>       &out) const {
    const size_t size = keys.Size();
    if (x.size() != size) {
        throw std::runtime_error("EvaluateSharedInputBatch: input size mismatch");
    }

    const uint64_t party_id = keys.dpf_key[0].party_id;

    std::vector<uint64_t> masked(size, 0);
    std::vector<uint64_t> masked_0(size, 0);
    std::vector<uint64_t> masked_1(size, 0);

    if (party_id == 0) {
        ctx_.Arith().EvaluateAdd(x, keys.shr_in, masked_0);
    } else {
        ctx_.Arith().EvaluateAdd(x, keys.shr_in, masked_1);
    }

    // Open masked inputs in one communication round
    ctx_.Arith().Reconst(party_id, chl, masked_0, masked_1, masked);
    EvaluateMaskedInputBatch(keys, masked, out);
}

void ZeroTest::EvaluateMaskedInputBatch(const ZeroTestKeys          &keys,
                                        const std::vector<uint64_t> &x,
                                        std::vector<uint64_t>       &out) const {
    const size_t size = keys.Size();
    if (x.size() != size) {
        throw std::runtime_error("EvaluateMaskedInputBatch: input size mismatch");
    }
    if (keys.dpf_key.size() != size) {
        throw std::runtime_error("EvaluateMaskedInputBatch: keys size mismatch");
    }

    eval_.EvaluateAt(keys.dpf_key, x, out);
}

}    // namespace proto
}    // namespace ringoa
