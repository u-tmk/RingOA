#include "obliv_select.h"

#include <cstring>

#include "RingOA/fss/prg.h"
#include "RingOA/utils/bytes.h"
#include "RingOA/utils/logger.h"
#include "RingOA/utils/network.h"
#include "RingOA/utils/rng.h"
#include "RingOA/utils/timer.h"
#include "RingOA/utils/to_string.h"
#include "RingOA/utils/utils.h"

namespace ringoa {
namespace proto {

void OblivSelectParameters::PrintParametersDebug(bool with_header, int key_width) const {
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    if (with_header) {
        Logger::DebugLog(LOC, Logger::MakeSeparatorLine());
        Logger::DebugLog(LOC, "[OblivSelect Params]");
        Logger::DebugLog(LOC, Logger::MakeSeparatorLine());
    }

    Logger::DebugLog(LOC, Logger::FormatKeyValue("DatabaseIndexBits", ToString(db_index_bits_), key_width));
    Logger::DebugLog(LOC, Logger::FormatKeyValue("DpfOutputBits", ToString(dpf_out_bits_), key_width));
    Logger::DebugLog(LOC, Logger::FormatKeyValue("ShareBits", ToString(share_bitsize_), key_width));

    params_.PrintParametersDebug(false, key_width);

    if (with_header) {
        Logger::DebugLog(LOC, Logger::MakeSeparatorLine());
    }
#endif
}

OblivSelectKeys::OblivSelectKeys(
    uint64_t                     party_id,
    const OblivSelectParameters &params,
    size_t                       count)
    : party_id(party_id),
      key_from_prev(),
      key_from_next(),
      rsh_from_prev(count, 0),
      rsh_from_next(count, 0) {
    key_from_prev.reserve(count);
    key_from_next.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        key_from_prev.emplace_back(party_id, params.GetParameters());
        key_from_next.emplace_back(party_id, params.GetParameters());
    }
}

size_t OblivSelectKeys::CalculateSerializedSize() const {
    size_t size = 0;

    size += sizeof(uint64_t);    // number of keys
    size += sizeof(party_id);
    const size_t num = Size();
    for (size_t i = 0; i < num; ++i) {
        size += key_from_prev[i].CalculateSerializedSize();
        size += key_from_next[i].CalculateSerializedSize();
    }
    size += num * sizeof(uint64_t);    // rsh_from_prev
    size += num * sizeof(uint64_t);    // rsh_from_next
    return size;
}

void OblivSelectKeys::Serialize(std::vector<uint8_t> &buffer) const {
    const size_t start = buffer.size();

    // Number of keys
    const size_t num = Size();
    append_pod(buffer, num);

    // Serialize the party ID
    append_pod(buffer, party_id);

    // Serialize the DPF keys
    for (const auto &key : key_from_prev) {
        key.Serialize(buffer);
    }
    for (const auto &key : key_from_next) {
        key.Serialize(buffer);
    }

    // Serialize the random shares
    append_array(buffer, rsh_from_prev.data(), static_cast<size_t>(num));
    append_array(buffer, rsh_from_next.data(), static_cast<size_t>(num));

    // Check size
    const size_t written  = buffer.size() - start;
    const size_t expected = CalculateSerializedSize();
    if (written != expected) {
        throw std::runtime_error(LOC + " OblivSelectKeys::Serialize: serialized size mismatch: " +
                                 ToString(written) + " != " + ToString(expected));
    }
}

void OblivSelectKeys::Deserialize(const std::vector<uint8_t>  &buffer,
                                  const OblivSelectParameters &params) {
    size_t offset = 0;
    Deserialize(buffer, params, offset);
}

void OblivSelectKeys::Deserialize(const std::vector<uint8_t>  &buffer,
                                  const OblivSelectParameters &params,
                                  size_t                      &offset) {
    const size_t start = offset;

    // Number of keys
    uint64_t num = 0;
    read_pod(buffer, offset, num);

    // Deserialize the party ID
    read_pod(buffer, offset, party_id);

    // Reset containers
    key_from_prev.clear();
    key_from_next.clear();
    rsh_from_prev.clear();
    rsh_from_next.clear();

    key_from_prev.reserve(num);
    key_from_next.reserve(num);
    rsh_from_prev.resize(num);
    rsh_from_next.resize(num);

    // Deserialize the DPF keys
    for (size_t i = 0; i < static_cast<size_t>(num); ++i) {
        key_from_prev.emplace_back(0, params.GetParameters());
        key_from_prev.back().Deserialize(buffer, offset);
    }
    for (size_t i = 0; i < static_cast<size_t>(num); ++i) {
        key_from_next.emplace_back(0, params.GetParameters());
        key_from_next.back().Deserialize(buffer, offset);
    }

    // Deserialize the random shares
    read_array(buffer, offset, rsh_from_prev.data(), static_cast<size_t>(num));
    read_array(buffer, offset, rsh_from_next.data(), static_cast<size_t>(num));

    // Check size
    const size_t read     = offset - start;
    const size_t expected = CalculateSerializedSize();
    if (read != expected) {
        throw std::runtime_error(LOC + " OblivSelectKeys::Deserialize: deserialized size mismatch: " +
                                 ToString(read) + " != " + ToString(expected));
    }
}

size_t OblivSelectPreprocessMsg::CalculateSerializedSize() const {
    const size_t n = dpf_keys.size();

    size_t size = 0;

    size += sizeof(uint64_t);
    for (const auto &k : dpf_keys) {
        size += k.CalculateSerializedSize();
    }
    size += n * sizeof(uint64_t);    // r_share
    return size;
}

void OblivSelectPreprocessMsg::Serialize(std::vector<uint8_t> &buffer) const {
    const size_t start = buffer.size();

    const size_t n = dpf_keys.size();

    append_pod(buffer, static_cast<uint64_t>(n));
    for (const auto &k : dpf_keys) {
        k.Serialize(buffer);
    }
    append_array(buffer, r_share.data(), n);

    const size_t written  = buffer.size() - start;
    const size_t expected = CalculateSerializedSize();
    if (written != expected) {
        throw std::runtime_error(LOC + " OblivSelectPreprocessMsg::Serialize: serialized size mismatch: " +
                                 ToString(written) + " != " + ToString(expected));
    }
}

void OblivSelectPreprocessMsg::Deserialize(const std::vector<uint8_t>  &buffer,
                                           const OblivSelectParameters &params) {
    size_t offset = 0;
    Deserialize(buffer, params, offset);

    if (offset != buffer.size()) {
        throw std::runtime_error(LOC + " OblivSelectPreprocessMsg::Deserialize: trailing bytes: " +
                                 ToString(buffer.size() - offset));
    }
}

void OblivSelectPreprocessMsg::Deserialize(const std::vector<uint8_t>  &buffer,
                                           const OblivSelectParameters &params,
                                           size_t                      &offset) {
    const size_t start = offset;

    uint64_t n64 = 0;
    read_pod(buffer, offset, n64);

    const size_t n = static_cast<size_t>(n64);

    // Reset
    dpf_keys.clear();
    r_share.clear();

    dpf_keys.reserve(n);
    r_share.resize(n);

    // DPF keys
    for (size_t i = 0; i < n; ++i) {
        dpf_keys.emplace_back(/*party_id=*/0, params.GetParameters());
        dpf_keys.back().Deserialize(buffer, offset);
    }

    // shares
    read_array(buffer, offset, r_share.data(), n);

    const size_t read     = offset - start;
    const size_t expected = CalculateSerializedSize();
    if (read != expected) {
        throw std::runtime_error(LOC + " OblivSelectPreprocessMsg::Deserialize: deserialized size mismatch: " +
                                 ToString(read) + " != " + ToString(expected));
    }
}

OblivSelectPreprocessData OblivSelectPreprocessData::FromMessage(
    uint64_t                     party_id,
    const OblivSelectParameters &params,
    size_t                       count,
    OblivSelectPreprocessMsg   &&from_prev,
    OblivSelectPreprocessMsg   &&from_next) {

    OblivSelectPreprocessData out;

    out.keys = OblivSelectKeys(party_id, params, count);

    for (size_t i = 0; i < count; ++i) {
        out.keys.key_from_prev[i] = std::move(from_prev.dpf_keys[i]);
        out.keys.rsh_from_prev[i] = from_prev.r_share[i];

        out.keys.key_from_next[i] = std::move(from_next.dpf_keys[i]);
        out.keys.rsh_from_next[i] = from_next.r_share[i];
    }

    return out;
}

size_t OblivSelectPreprocessData::CalculateSerializedSize() const {
    size_t size = 0;

    size += keys.CalculateSerializedSize();
    return size;
}

void OblivSelectPreprocessData::Serialize(std::vector<uint8_t> &buffer) const {
    const size_t start = buffer.size();

    keys.Serialize(buffer);

    const size_t written  = buffer.size() - start;
    const size_t expected = CalculateSerializedSize();
    if (written != expected) {
        throw std::runtime_error(LOC + " OblivSelectPreprocessData::Serialize: serialized size mismatch: " +
                                 ToString(written) + " != " + ToString(expected));
    }
}

void OblivSelectPreprocessData::Deserialize(const std::vector<uint8_t>  &buffer,
                                            const OblivSelectParameters &params) {
    size_t offset = 0;
    Deserialize(buffer, params, offset);

    if (offset != buffer.size()) {
        throw std::runtime_error(LOC + " OblivSelectPreprocessData::Deserialize: trailing bytes: " +
                                 ToString(buffer.size() - offset));
    }
}

void OblivSelectPreprocessData::Deserialize(const std::vector<uint8_t>  &buffer,
                                            const OblivSelectParameters &params,
                                            size_t                      &offset) {
    const size_t start = offset;

    keys.Deserialize(buffer, params, offset);

    const size_t read     = offset - start;
    const size_t expected = CalculateSerializedSize();
    if (read != expected) {
        throw std::runtime_error(LOC + " OblivSelectPreprocessData::Deserialize: deserialized size mismatch: " +
                                 ToString(read) + " != " + ToString(expected));
    }
}

OblivSelect::OblivSelect(
    const OblivSelectParameters &params,
    ProtocolContext3PBinary     &ctx)
    : params_(params),
      gen_(params.GetParameters()),
      eval_(params.GetParameters()),
      ctx_(ctx),
      G_(fss::prg::PseudoRandomGenerator::GetInstance()) {
    timers_.full_domain = ctx_.Timers().GetOrCreateTimer("OblivSelect::FullDomainEval");
    timers_.dot_product = ctx_.Timers().GetOrCreateTimer("OblivSelect::DotProduct");
}

OblivSelectNeighborMsg OblivSelect::MakePreprocessMsg(size_t count) const {
    uint64_t d = params_.GetDbIndexBits();

    OblivSelectNeighborMsg out;

    // Generate OblivSelect keys
    for (size_t i = 0; i < count; ++i) {
        uint64_t r    = Mod2N(GlobalRng::Rand<uint64_t>(), d);
        auto     r_sh = ctx_.Bss().Share(r);

        auto key_pairs = gen_.GenerateKeys(r, 1);

        // To next: key_pairs.first, r_sh.first
        out.to_next.dpf_keys.emplace_back(std::move(key_pairs.first));
        out.to_next.r_share.emplace_back(r_sh.first);

        // To prev: key_pairs.second, r_sh.second
        out.to_prev.dpf_keys.emplace_back(std::move(key_pairs.second));
        out.to_prev.r_share.emplace_back(r_sh.second);

#if LOG_LEVEL >= LOG_LEVEL_TRACE
        Logger::TraceLog(LOC, "(i=" + ToString(i) + ") r: " + ToString(r));
#endif
    }

    return out;
}

OblivSelectNeighborMsgIn OblivSelect::ExchangePreprocessMsg(Channels &chls, OblivSelectNeighborMsg &&out) const {
    std::vector<uint8_t> to_prev, to_next;
    out.to_prev.Serialize(to_prev);
    out.to_next.Serialize(to_next);

    chls.prev.send(to_prev);
    chls.next.send(to_next);

    std::vector<uint8_t> in_prev, in_next;
    chls.next.recv(in_next);
    chls.prev.recv(in_prev);

    OblivSelectNeighborMsgIn in;
    in.from_prev.Deserialize(in_prev, params_);
    in.from_next.Deserialize(in_next, params_);
    return in;
}

OblivSelectPreprocessData OblivSelect::BuildPreprocessData(uint64_t                   party_id,
                                                           size_t                     count,
                                                           OblivSelectNeighborMsgIn &&in) const {
    return OblivSelectPreprocessData::FromMessage(
        party_id, params_, count,
        std::move(in.from_prev),
        std::move(in.from_next));
}

OblivSelectPreprocessData OblivSelect::Preprocess(Channels &chls, size_t count) const {
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "Starting OblivSelect preprocessing");
#endif

    OblivSelectNeighborMsg   out = MakePreprocessMsg(count);
    OblivSelectNeighborMsgIn in  = ExchangePreprocessMsg(chls, std::move(out));
    return BuildPreprocessData(chls.party_id, count, std::move(in));
}

void OblivSelect::ObliviousAccess(Channels                          &chls,
                                  const OblivSelectKeyView          &key,
                                  const sharing::Rep3ShareViewBlock &database,
                                  const sharing::Rep3Share64        &index,
                                  sharing::Rep3ShareBlock           &result) const {

    uint64_t party_id = chls.party_id;
    uint64_t d        = params_.GetDbIndexBits();

    if (database.Size() != (1UL << d)) {
        throw std::invalid_argument(LOC + " Database size does not match the number of nodes: " +
                                    ToString(database.Size()) + " != " + ToString(1UL << d));
    }

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "Evaluate OblivSelect key");
    std::string party_str = "[P" + ToString(party_id) + "] ";
    Logger::DebugLog(LOC, party_str + " idx: " + index.ToString());
    Logger::DebugLog(LOC, party_str + " db: " + database.ToString());
#endif

    // Reconstruct p ^ r_i
    auto [pr_prev, pr_next] = ReconstructMaskedValue(chls, key, index);
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, party_str + " pr_prev: " + ToString(pr_prev) + ", pr_next: " + ToString(pr_next));
#endif

    block dp_prev = ComputeDotProductBlockSIMD(key.key_from_next, database.share1, pr_prev);
    block dp_next = ComputeDotProductBlockSIMD(key.key_from_prev, database.share0, pr_next);

    block                   selected_sh = dp_prev ^ dp_next;
    sharing::Rep3ShareBlock r_sh;
    ctx_.Brss().Rand(r_sh);
    result[0] = selected_sh ^ r_sh[0] ^ r_sh[1];
    chls.next.send(result[0]);
    chls.prev.recv(result[1]);
}

void OblivSelect::ObliviousAccess(Channels                       &chls,
                                  const OblivSelectKeyView       &key,
                                  std::vector<block>             &uv_prev,
                                  std::vector<block>             &uv_next,
                                  const sharing::Rep3ShareView64 &database,
                                  const sharing::Rep3Share64     &index,
                                  sharing::Rep3Share64           &result) const {

    uint64_t party_id = chls.party_id;
    uint64_t d        = params_.GetDbIndexBits();
    uint64_t nu       = params_.GetParameters().GetTerminateBitsize();

    if (uv_prev.size() != (1UL << nu) || uv_next.size() != (1UL << nu)) {
        throw std::invalid_argument(LOC + " Output vector size does not match the number of nodes: " +
                                    ToString(uv_prev.size()) + " != " + ToString(1UL << nu) +
                                    " or " + ToString(uv_next.size()) + " != " + ToString(1UL << nu));
    }
    if (database.Size() != (1UL << d)) {
        throw std::invalid_argument(LOC + " Database size does not match the number of nodes: " +
                                    ToString(database.Size()) + " != " + ToString(1UL << d));
    }

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "Evaluate OblivSelect key");
    std::string party_str = "[P" + ToString(party_id) + "] ";
    Logger::DebugLog(LOC, party_str + " idx: " + index.ToString());
    Logger::DebugLog(LOC, party_str + " db: " + database.ToString());
#endif

    // Reconstruct p ^ r_i
    auto [pr_prev, pr_next] = ReconstructMaskedValue(chls, key, index);
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, party_str + " pr_prev: " + ToString(pr_prev) + ", pr_next: " + ToString(pr_next));
#endif

    // Evaluate DPF (uv_prev and uv_next are std::vector<block>, where block
    auto [dp_prev, dp_next] = EvaluateFullDomainThenDotProduct(
        party_id, key.key_from_prev, key.key_from_next, uv_prev, uv_next, database, pr_prev, pr_next);
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, party_str + "dp_prev: " + ToString(dp_prev) + ", dp_next: " + ToString(dp_next));
#endif

    uint64_t             selected_sh = dp_prev ^ dp_next;
    sharing::Rep3Share64 r_sh;
    ctx_.Brss().Rand(r_sh);
    result[0] = selected_sh ^ r_sh[0] ^ r_sh[1];
    chls.next.send(result[0]);
    chls.prev.recv(result[1]);
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, party_str + " result: " + ToString(result[0]) + ", " + ToString(result[1]));
#endif
}

void OblivSelect::ObliviousAccessPair(Channels                       &chls,
                                      const OblivSelectKeyView       &key1,
                                      const OblivSelectKeyView       &key2,
                                      std::vector<block>             &uv_prev,
                                      std::vector<block>             &uv_next,
                                      const sharing::Rep3ShareView64 &database,
                                      const sharing::Rep3ShareVec64  &index,
                                      sharing::Rep3ShareVec64        &result) const {

    uint64_t party_id = chls.party_id;
    uint64_t d        = params_.GetDbIndexBits();
    uint64_t nu       = params_.GetParameters().GetTerminateBitsize();

    if (uv_prev.size() != (1UL << nu) || uv_next.size() != (1UL << nu)) {
        throw std::invalid_argument(LOC + " Output vector size does not match the number of nodes: " +
                                    ToString(uv_prev.size()) + " != " + ToString(1UL << nu) +
                                    " or " + ToString(uv_next.size()) + " != " + ToString(1UL << nu));
    }
    if (database.Size() != (1UL << d)) {
        throw std::invalid_argument(LOC + " Database size does not match the number of nodes: " +
                                    ToString(database.Size()) + " != " + ToString(1UL << d));
    }

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "Evaluate OblivSelect key");
    std::string party_str = "[P" + ToString(party_id) + "] ";
    Logger::DebugLog(LOC, party_str + " idx: " + index.ToString());
    Logger::DebugLog(LOC, party_str + " db: " + database.ToString());
#endif

    // Reconstruct p ^ r_i
    // pr: [pr_prev1, pr_next1, pr_prev2, pr_next2]
    std::array<uint64_t, 4> pr = ReconstructMaskedValue(chls, key1, key2, index);
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, party_str + " pr_prev1: " + ToString(pr[0]) + ", pr_next1: " + ToString(pr[1]) +
                              ", pr_prev2: " + ToString(pr[2]) + ", pr_next2: " + ToString(pr[3]));
#endif

    // Evaluate DPF (uv_prev and uv_next are std::vector<block>, where block
    auto [dp_prev1, dp_next1] = EvaluateFullDomainThenDotProduct(
        party_id, key1.key_from_prev, key1.key_from_next, uv_prev, uv_next, database, pr[0], pr[1]);
    auto [dp_prev2, dp_next2] = EvaluateFullDomainThenDotProduct(
        party_id, key2.key_from_prev, key2.key_from_next, uv_prev, uv_next, database, pr[2], pr[3]);
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, party_str + "dp_prev1: " + ToString(dp_prev1) + ", dp_next1: " + ToString(dp_next1));
    Logger::DebugLog(LOC, party_str + "dp_prev2: " + ToString(dp_prev2) + ", dp_next2: " + ToString(dp_next2));
#endif

    uint64_t             selected1_sh = dp_prev1 ^ dp_next1;
    uint64_t             selected2_sh = dp_prev2 ^ dp_next2;
    sharing::Rep3Share64 r1_sh, r2_sh;
    ctx_.Brss().Rand(r1_sh);
    ctx_.Brss().Rand(r2_sh);
    result[0][0] = selected1_sh ^ r1_sh[0] ^ r1_sh[1];
    result[0][1] = selected2_sh ^ r2_sh[0] ^ r2_sh[1];
    chls.next.send(result[0]);
    chls.prev.recv(result[1]);
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, party_str + " result: " + ToString(result[0]) + ", " + ToString(result[1]));
#endif
}

block OblivSelect::ComputeDotProductBlockSIMD(const fss::dpf::DpfKey       &key,
                                              const std::span<const block> &database,
                                              const uint64_t                pr) const {
    uint64_t nu = params_.GetParameters().GetTerminateBitsize();

    // Breadth-first traversal for 8 nodes
    std::vector<block> start_seeds{key.init_seed}, next_seeds;
    std::vector<bool>  start_control_bits{key.party_id != 0}, next_control_bits;

    for (uint64_t i = 0; i < 3; ++i) {
        std::array<block, 2> expanded_seeds;
        std::array<bool, 2>  expanded_control_bits;
        next_seeds.resize(1U << (i + 1));
        next_control_bits.resize(1U << (i + 1));
        for (size_t j = 0; j < start_seeds.size(); j++) {
            EvaluateNextSeed(i, start_seeds[j], start_control_bits[j], expanded_seeds, expanded_control_bits, key);
            next_seeds[j * 2]            = expanded_seeds[fss::kLeft];
            next_seeds[j * 2 + 1]        = expanded_seeds[fss::kRight];
            next_control_bits[j * 2]     = expanded_control_bits[fss::kLeft];
            next_control_bits[j * 2 + 1] = expanded_control_bits[fss::kRight];
        }
        start_seeds        = std::move(next_seeds);
        start_control_bits = std::move(next_control_bits);
    }

    // Initialize the variables
    uint64_t current_level = 0;
    uint64_t current_idx   = 0;
    uint64_t last_depth    = std::max(static_cast<int32_t>(nu) - 3, 0);
    uint64_t last_idx      = 1U << last_depth;

    // Store the seeds and control bits
    std::array<block, 8>              expanded_seeds, output_seeds;
    std::array<block, 8>              sums = {zero_block, zero_block, zero_block, zero_block,
                                              zero_block, zero_block, zero_block, zero_block};
    std::array<bool, 8>               expanded_control_bits;
    std::vector<std::array<block, 8>> prev_seeds(last_depth + 1);
    std::vector<std::array<bool, 8>>  prev_control_bits(last_depth + 1);
    std::vector<block>                byte_expanded_seeds(64);

    // Evaluate the DPF key
    for (uint64_t i = 0; i < 8; ++i) {
        prev_seeds[0][i]        = start_seeds[i];
        prev_control_bits[0][i] = start_control_bits[i];
    }

    while (current_idx < last_idx) {
        while (current_level < last_depth) {
            // Expand the seed and control bits
            uint64_t mask        = (current_idx >> (last_depth - 1U - current_level));
            bool     current_bit = mask & 1U;

            fss::prg::Side side;
            if (current_bit) {
                side = fss::prg::Side::kRight;
            } else {
                side = fss::prg::Side::kLeft;
            }
            G_.Expand(prev_seeds[current_level], expanded_seeds, side);
            for (uint64_t i = 0; i < 8; ++i) {
                expanded_control_bits[i] = GetLsb(expanded_seeds[i]);
                SetLsbZero(expanded_seeds[i]);
            }

            // Apply correction word if control bit is true
            bool  cw_control_bit = current_bit ? key.cw_control_right[current_level + 3] : key.cw_control_left[current_level + 3];
            block cw_seed        = key.cw_seed[current_level + 3];
            expanded_seeds[0] ^= (cw_seed & zero_and_all_one[prev_control_bits[current_level][0]]);
            expanded_seeds[1] ^= (cw_seed & zero_and_all_one[prev_control_bits[current_level][1]]);
            expanded_seeds[2] ^= (cw_seed & zero_and_all_one[prev_control_bits[current_level][2]]);
            expanded_seeds[3] ^= (cw_seed & zero_and_all_one[prev_control_bits[current_level][3]]);
            expanded_seeds[4] ^= (cw_seed & zero_and_all_one[prev_control_bits[current_level][4]]);
            expanded_seeds[5] ^= (cw_seed & zero_and_all_one[prev_control_bits[current_level][5]]);
            expanded_seeds[6] ^= (cw_seed & zero_and_all_one[prev_control_bits[current_level][6]]);
            expanded_seeds[7] ^= (cw_seed & zero_and_all_one[prev_control_bits[current_level][7]]);

            for (uint64_t i = 0; i < 8; ++i) {
                // expanded_seeds[i] ^= (key.cw_seed[current_level + 3] & zero_and_all_one[prev_control_bits[current_level][i]]);
                expanded_control_bits[i] ^= (cw_control_bit & prev_control_bits[current_level][i]);
            }

            // Update the current level
            current_level++;

            // Update the previous seeds and control bits
            for (uint64_t i = 0; i < 8; ++i) {
                prev_seeds[current_level][i]        = expanded_seeds[i];
                prev_control_bits[current_level][i] = expanded_control_bits[i];
            }
        }

        // Seed expansion for the final output
        G_.Expand(prev_seeds[current_level], prev_seeds[current_level], fss::prg::Side::kLeft);

        for (uint64_t j = 0; j < 8; ++j) {
            output_seeds[j] = prev_seeds[current_level][j] ^ (zero_and_all_one[prev_control_bits[current_level][j]] & key.output);
        }

        auto dest = byte_expanded_seeds.data();

        for (uint64_t i = 0; i < 8; ++i) {
            dest[0] = all_bytes_one_mask & output_seeds[i].mm_srai_epi16(0);
            dest[1] = all_bytes_one_mask & output_seeds[i].mm_srai_epi16(1);
            dest[2] = all_bytes_one_mask & output_seeds[i].mm_srai_epi16(2);
            dest[3] = all_bytes_one_mask & output_seeds[i].mm_srai_epi16(3);
            dest[4] = all_bytes_one_mask & output_seeds[i].mm_srai_epi16(4);
            dest[5] = all_bytes_one_mask & output_seeds[i].mm_srai_epi16(5);
            dest[6] = all_bytes_one_mask & output_seeds[i].mm_srai_epi16(6);
            dest[7] = all_bytes_one_mask & output_seeds[i].mm_srai_epi16(7);

            dest += 8;
        }

        uint8_t *seed_byte0 = reinterpret_cast<uint8_t *>(byte_expanded_seeds.data()) + 128 * 0;
        uint8_t *seed_byte1 = reinterpret_cast<uint8_t *>(byte_expanded_seeds.data()) + 128 * 1;
        uint8_t *seed_byte2 = reinterpret_cast<uint8_t *>(byte_expanded_seeds.data()) + 128 * 2;
        uint8_t *seed_byte3 = reinterpret_cast<uint8_t *>(byte_expanded_seeds.data()) + 128 * 3;
        uint8_t *seed_byte4 = reinterpret_cast<uint8_t *>(byte_expanded_seeds.data()) + 128 * 4;
        uint8_t *seed_byte5 = reinterpret_cast<uint8_t *>(byte_expanded_seeds.data()) + 128 * 5;
        uint8_t *seed_byte6 = reinterpret_cast<uint8_t *>(byte_expanded_seeds.data()) + 128 * 6;
        uint8_t *seed_byte7 = reinterpret_cast<uint8_t *>(byte_expanded_seeds.data()) + 128 * 7;

        // Calculate the dot product
        for (uint64_t j = 0; j < 128; ++j) {
            size_t db_idx0 = (((0 * last_idx + current_idx) * 128) + j) ^ pr;
            size_t db_idx1 = (((1 * last_idx + current_idx) * 128) + j) ^ pr;
            size_t db_idx2 = (((2 * last_idx + current_idx) * 128) + j) ^ pr;
            size_t db_idx3 = (((3 * last_idx + current_idx) * 128) + j) ^ pr;
            size_t db_idx4 = (((4 * last_idx + current_idx) * 128) + j) ^ pr;
            size_t db_idx5 = (((5 * last_idx + current_idx) * 128) + j) ^ pr;
            size_t db_idx6 = (((6 * last_idx + current_idx) * 128) + j) ^ pr;
            size_t db_idx7 = (((7 * last_idx + current_idx) * 128) + j) ^ pr;
            auto   input0  = database[db_idx0] & zero_and_all_one[seed_byte0[j]];
            auto   input1  = database[db_idx1] & zero_and_all_one[seed_byte1[j]];
            auto   input2  = database[db_idx2] & zero_and_all_one[seed_byte2[j]];
            auto   input3  = database[db_idx3] & zero_and_all_one[seed_byte3[j]];
            auto   input4  = database[db_idx4] & zero_and_all_one[seed_byte4[j]];
            auto   input5  = database[db_idx5] & zero_and_all_one[seed_byte5[j]];
            auto   input6  = database[db_idx6] & zero_and_all_one[seed_byte6[j]];
            auto   input7  = database[db_idx7] & zero_and_all_one[seed_byte7[j]];

            sums[0] = sums[0] ^ input0;
            sums[1] = sums[1] ^ input1;
            sums[2] = sums[2] ^ input2;
            sums[3] = sums[3] ^ input3;
            sums[4] = sums[4] ^ input4;
            sums[5] = sums[5] ^ input5;
            sums[6] = sums[6] ^ input6;
            sums[7] = sums[7] ^ input7;
        }

        // Update the current index
        int shift = (current_idx + 1U) ^ current_idx;
        current_level -= Log2Floor(shift) + 1;
        current_idx++;
    }

    block blk_sum = zero_block;
    for (uint64_t i = 0; i < 8; i++) {
        blk_sum = blk_sum ^ sums[i];
    }
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "Dot product result: " + Format(blk_sum));
#endif
    return blk_sum;
}

std::pair<uint64_t, uint64_t> OblivSelect::EvaluateFullDomainThenDotProduct(
    const uint64_t                  party_id,
    const fss::dpf::DpfKey         &key_from_prev,
    const fss::dpf::DpfKey         &key_from_next,
    std::vector<block>             &uv_prev,
    std::vector<block>             &uv_next,
    const sharing::Rep3ShareView64 &database,
    const uint64_t                  pr_prev,
    const uint64_t                  pr_next) const {

    {
        auto scope = ctx_.Timers().Scope(timers_.full_domain, "[P" + ToString(party_id) + "]");
        eval_.EvaluateFullDomain(key_from_next, uv_prev);
        eval_.EvaluateFullDomain(key_from_prev, uv_next);
    }

    uint64_t dp_prev{0}, dp_next{0};
    {
        auto scope = ctx_.Timers().Scope(timers_.dot_product, "[P" + ToString(party_id) + "]");

        const uint64_t *__restrict share0 = database.share0.data();
        const uint64_t *__restrict share1 = database.share1.data();

        for (size_t i = 0; i < uv_prev.size(); ++i) {
            const uint64_t low_prev  = uv_prev[i].get<uint64_t>()[0];
            const uint64_t high_prev = uv_prev[i].get<uint64_t>()[1];
            const uint64_t low_next  = uv_next[i].get<uint64_t>()[0];
            const uint64_t high_next = uv_next[i].get<uint64_t>()[1];

            const uint64_t base = i * 128;

            for (uint64_t j = 0; j < 64; ++j) {
                const uint64_t mask_prev = 0UL - ((low_prev >> j) & 1UL);
                const uint64_t mask_next = 0UL - ((low_next >> j) & 1UL);

                const uint64_t idx_prev = (base + j) ^ pr_prev;
                const uint64_t idx_next = (base + j) ^ pr_next;

                dp_prev ^= share1[idx_prev] & mask_prev;
                dp_next ^= share0[idx_next] & mask_next;
            }

            for (uint64_t j = 0; j < 64; ++j) {
                const uint64_t mask_prev = 0UL - ((high_prev >> j) & 1UL);
                const uint64_t mask_next = 0UL - ((high_next >> j) & 1UL);

                const uint64_t idx_prev = (base + 64UL + j) ^ pr_prev;
                const uint64_t idx_next = (base + 64UL + j) ^ pr_next;

                dp_prev ^= share1[idx_prev] & mask_prev;
                dp_next ^= share0[idx_next] & mask_next;
            }
        }
    }

    return {dp_prev, dp_next};
}

std::pair<uint64_t, uint64_t> OblivSelect::ReconstructMaskedValue(Channels                   &chls,
                                                                  const OblivSelectKeyView   &key,
                                                                  const sharing::Rep3Share64 &index) const {

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "ReconstructMaskedValue for Party " + ToString(chls.party_id));
#endif

    // Set replicated sharing of random value
    uint64_t pr_prev = 0, pr_next = 0;

    // Reconstruct p ^ r_i
    if (chls.party_id == 0) {
        sharing::Rep3Share64 r_1_sh(key.rsh_from_next, 0);
        sharing::Rep3Share64 r_2_sh(0, key.rsh_from_prev);
        sharing::Rep3Share64 pr_20_sh, pr_01_sh;
        uint64_t             pr_20, pr_01;
        // p - r_1 between Party 2 (prev) and Party 0 (self)
        // p - r_2 between Party 0 (self) and Party 1 (next)
        ctx_.Brss().EvaluateXor(index, r_1_sh, pr_20_sh);
        ctx_.Brss().EvaluateXor(index, r_2_sh, pr_01_sh);
        chls.prev.send(pr_20_sh[0]);
        chls.next.send(pr_01_sh[1]);
        chls.next.recv(pr_01);
        chls.prev.recv(pr_20);
        pr_prev = pr_20 ^ pr_20_sh[0] ^ pr_20_sh[1];
        pr_next = pr_01_sh[0] ^ pr_01_sh[1] ^ pr_01;

    } else if (chls.party_id == 1) {
        sharing::Rep3Share64 r_0_sh(0, key.rsh_from_prev);
        sharing::Rep3Share64 r_2_sh(key.rsh_from_next, 0);
        sharing::Rep3Share64 pr_12_sh, pr_01_sh;
        uint64_t             pr_12, pr_01;
        // p - r_0 between Party 1 (self) and Party 2 (next)
        // p - r_2 between Party 0 (prev) and Party 1 (next)
        ctx_.Brss().EvaluateXor(index, r_0_sh, pr_12_sh);
        ctx_.Brss().EvaluateXor(index, r_2_sh, pr_01_sh);
        chls.next.send(pr_12_sh[1]);
        chls.prev.send(pr_01_sh[0]);
        chls.prev.recv(pr_01);
        chls.next.recv(pr_12);
        pr_prev = pr_01 ^ pr_01_sh[0] ^ pr_01_sh[1];
        pr_next = pr_12_sh[0] ^ pr_12_sh[1] ^ pr_12;

    } else {
        sharing::Rep3Share64 r_0_sh(key.rsh_from_next, 0);
        sharing::Rep3Share64 r_1_sh(0, key.rsh_from_prev);
        sharing::Rep3Share64 pr_12_sh, pr_20_sh;
        uint64_t             pr_12, pr_20;
        // p - r_0 between Party 1 (prev) and Party 2 (self)
        // p - r_1 between Party 2 (self) and Party 0 (next)
        ctx_.Brss().EvaluateXor(index, r_0_sh, pr_12_sh);
        ctx_.Brss().EvaluateXor(index, r_1_sh, pr_20_sh);
        chls.prev.send(pr_12_sh[0]);
        chls.next.send(pr_20_sh[1]);
        chls.prev.recv(pr_12);
        chls.next.recv(pr_20);
        pr_prev = pr_12 ^ pr_12_sh[0] ^ pr_12_sh[1];
        pr_next = pr_20_sh[0] ^ pr_20_sh[1] ^ pr_20;
    }
    return std::make_pair(pr_prev, pr_next);
}

std::array<uint64_t, 4> OblivSelect::ReconstructMaskedValue(Channels                      &chls,
                                                            const OblivSelectKeyView      &key1,
                                                            const OblivSelectKeyView      &key2,
                                                            const sharing::Rep3ShareVec64 &index) const {

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "ReconstructPR for Party " + ToString(chls.party_id));
#endif

    // Set replicated sharing of random value
    uint64_t                pr_prev1 = 0, pr_next1 = 0, pr_prev2 = 0, pr_next2 = 0;
    sharing::Rep3ShareVec64 r_0_sh(2), r_1_sh(2), r_2_sh(2);

    // Reconstruct p ^ r_i
    if (chls.party_id == 0) {
        r_1_sh.Set(0, sharing::Rep3Share64(key1.rsh_from_next, 0));
        r_2_sh.Set(0, sharing::Rep3Share64(0, key1.rsh_from_prev));
        r_1_sh.Set(1, sharing::Rep3Share64(key2.rsh_from_next, 0));
        r_2_sh.Set(1, sharing::Rep3Share64(0, key2.rsh_from_prev));
        sharing::Rep3ShareVec64 pr_20_sh(2), pr_01_sh(2);
        std::vector<uint64_t>   pr_20(2), pr_01(2);
        // p - r_1 between Party 0 and Party 2
        // p - r_2 between Party 0 and Party 1
        ctx_.Brss().EvaluateXor(index, r_1_sh, pr_20_sh);
        ctx_.Brss().EvaluateXor(index, r_2_sh, pr_01_sh);
        chls.prev.send(pr_20_sh[0]);
        chls.next.send(pr_01_sh[1]);
        chls.next.recv(pr_01);
        chls.prev.recv(pr_20);
        pr_prev1 = pr_20[0] ^ pr_20_sh[0][0] ^ pr_20_sh[1][0];
        pr_next1 = pr_01_sh[0][0] ^ pr_01_sh[1][0] ^ pr_01[0];
        pr_prev2 = pr_20[1] ^ pr_20_sh[0][1] ^ pr_20_sh[1][1];
        pr_next2 = pr_01_sh[0][1] ^ pr_01_sh[1][1] ^ pr_01[1];

    } else if (chls.party_id == 1) {
        r_0_sh.Set(0, sharing::Rep3Share64(0, key1.rsh_from_prev));
        r_2_sh.Set(0, sharing::Rep3Share64(key1.rsh_from_next, 0));
        r_0_sh.Set(1, sharing::Rep3Share64(0, key2.rsh_from_prev));
        r_2_sh.Set(1, sharing::Rep3Share64(key2.rsh_from_next, 0));
        sharing::Rep3ShareVec64 pr_12_sh(2), pr_01_sh(2);
        std::vector<uint64_t>   pr_12(2), pr_01(2);
        // p - r_0 between Party 1 and Party 2
        // p - r_2 between Party 0 and Party 1
        ctx_.Brss().EvaluateXor(index, r_0_sh, pr_12_sh);
        ctx_.Brss().EvaluateXor(index, r_2_sh, pr_01_sh);
        chls.next.send(pr_12_sh[1]);
        chls.prev.send(pr_01_sh[0]);
        chls.prev.recv(pr_01);
        chls.next.recv(pr_12);
        pr_prev1 = pr_01[0] ^ pr_01_sh[0][0] ^ pr_01_sh[1][0];
        pr_next1 = pr_12_sh[0][0] ^ pr_12_sh[1][0] ^ pr_12[0];
        pr_prev2 = pr_01[1] ^ pr_01_sh[0][1] ^ pr_01_sh[1][1];
        pr_next2 = pr_12_sh[0][1] ^ pr_12_sh[1][1] ^ pr_12[1];

    } else {
        r_0_sh.Set(0, sharing::Rep3Share64(key1.rsh_from_next, 0));
        r_1_sh.Set(0, sharing::Rep3Share64(0, key1.rsh_from_prev));
        r_0_sh.Set(1, sharing::Rep3Share64(key2.rsh_from_next, 0));
        r_1_sh.Set(1, sharing::Rep3Share64(0, key2.rsh_from_prev));
        sharing::Rep3ShareVec64 pr_12_sh(2), pr_20_sh(2);
        std::vector<uint64_t>   pr_12(2), pr_20(2);
        // p - r_0 between Party 1 and Party 2
        // p - r_1 between Party 0 and Party 2
        ctx_.Brss().EvaluateXor(index, r_0_sh, pr_12_sh);
        ctx_.Brss().EvaluateXor(index, r_1_sh, pr_20_sh);
        chls.prev.send(pr_12_sh[0]);
        chls.next.send(pr_20_sh[1]);
        chls.prev.recv(pr_12);
        chls.next.recv(pr_20);
        pr_prev1 = pr_12[0] ^ pr_12_sh[0][0] ^ pr_12_sh[1][0];
        pr_next1 = pr_20_sh[0][0] ^ pr_20_sh[1][0] ^ pr_20[0];
        pr_prev2 = pr_12[1] ^ pr_12_sh[0][1] ^ pr_12_sh[1][1];
        pr_next2 = pr_20_sh[0][1] ^ pr_20_sh[1][1] ^ pr_20[1];
    }
    return std::array<uint64_t, 4>{pr_prev1, pr_next1, pr_prev2, pr_next2};
}

void OblivSelect::EvaluateNextSeed(
    const uint64_t current_level, const block &current_seed, const bool &current_control_bit,
    std::array<block, 2> &expanded_seeds, std::array<bool, 2> &expanded_control_bits,
    const fss::dpf::DpfKey &key) const {
    // Expand the seed and control bits
    G_.DoubleExpand(current_seed, expanded_seeds);
    expanded_control_bits[fss::kLeft]  = GetLsb(expanded_seeds[fss::kLeft]);
    expanded_control_bits[fss::kRight] = GetLsb(expanded_seeds[fss::kRight]);
    SetLsbZero(expanded_seeds[fss::kLeft]);
    SetLsbZero(expanded_seeds[fss::kRight]);

    // Apply correction word if control bit is true
    const block mask = key.cw_seed[current_level] & zero_and_all_one[current_control_bit];
    expanded_seeds[fss::kLeft] ^= mask;
    expanded_seeds[fss::kRight] ^= mask;

    const bool control_mask_left  = key.cw_control_left[current_level] & current_control_bit;
    const bool control_mask_right = key.cw_control_right[current_level] & current_control_bit;
    expanded_control_bits[fss::kLeft] ^= control_mask_left;
    expanded_control_bits[fss::kRight] ^= control_mask_right;
}

}    // namespace proto
}    // namespace ringoa
