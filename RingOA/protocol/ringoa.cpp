#include "ringoa.h"

#include <cstring>

#include "RingOA/sharing/beaver_triples_gen.h"
#include "RingOA/utils/bytes.h"
#include "RingOA/utils/logger.h"
#include "RingOA/utils/network.h"
#include "RingOA/utils/rng.h"
#include "RingOA/utils/to_string.h"
#include "RingOA/utils/utils.h"

namespace ringoa {
namespace proto {

void RingOaParameters::PrintParametersDebug(bool with_header, int key_width) const {
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    if (with_header) {
        Logger::DebugLog(LOC, Logger::MakeSeparatorLine());
        Logger::DebugLog(LOC, "[RingOA Params]");
        Logger::DebugLog(LOC, Logger::MakeSeparatorLine());
    }

    Logger::DebugLog(LOC, Logger::FormatKeyValue("DatabaseIndexBits", ToString(db_index_bits_), key_width));
    Logger::DebugLog(LOC, Logger::FormatKeyValue("ShareBits", ToString(share_bitsize_), key_width));

    params_.PrintParametersDebug(false, key_width);

    if (with_header) {
        Logger::DebugLog(LOC, Logger::MakeSeparatorLine());
    }
#endif
}

RingOaKeys::RingOaKeys(
    uint64_t                party_id,
    const RingOaParameters &params,
    size_t                  count)
    : party_id(party_id),
      key_from_prev(),
      key_from_next(),
      rsh_from_prev(count, 0),
      rsh_from_next(count, 0),
      wsh_from_prev(count, 0),
      wsh_from_next(count, 0) {
    key_from_prev.reserve(count);
    key_from_next.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        key_from_prev.emplace_back(party_id, params.GetParameters());
        key_from_next.emplace_back(party_id, params.GetParameters());
    }
}

size_t RingOaKeys::CalculateSerializedSize() const {
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
    size += num * sizeof(uint64_t);    // wsh_from_prev
    size += num * sizeof(uint64_t);    // wsh_from_next
    return size;
}

void RingOaKeys::Serialize(std::vector<uint8_t> &buffer) const {
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
    append_array(buffer, wsh_from_prev.data(), static_cast<size_t>(num));
    append_array(buffer, wsh_from_next.data(), static_cast<size_t>(num));

    // Check size
    const size_t written  = buffer.size() - start;
    const size_t expected = CalculateSerializedSize();
    if (written != expected) {
        throw std::runtime_error(LOC + " RingOaKeys::Serialize: serialized size mismatch: " +
                                 ToString(written) + " != " + ToString(expected));
    }
}

void RingOaKeys::Deserialize(const std::vector<uint8_t> &buffer,
                             const RingOaParameters     &params) {
    size_t offset = 0;
    Deserialize(buffer, params, offset);
}

void RingOaKeys::Deserialize(const std::vector<uint8_t> &buffer,
                             const RingOaParameters     &params,
                             size_t                     &offset) {
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
    wsh_from_prev.clear();
    wsh_from_next.clear();

    key_from_prev.reserve(num);
    key_from_next.reserve(num);
    rsh_from_prev.resize(num);
    rsh_from_next.resize(num);
    wsh_from_prev.resize(num);
    wsh_from_next.resize(num);

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
    read_array(buffer, offset, wsh_from_prev.data(), static_cast<size_t>(num));
    read_array(buffer, offset, wsh_from_next.data(), static_cast<size_t>(num));

    // Check size
    const size_t read     = offset - start;
    const size_t expected = CalculateSerializedSize();
    if (read != expected) {
        throw std::runtime_error(LOC + " RingOaKeys::Deserialize: deserialized size mismatch: " +
                                 ToString(read) + " != " + ToString(expected));
    }
}

size_t RingOaPreprocessMsg::CalculateSerializedSize() const {
    const size_t n = dpf_keys.size();

    size_t size = 0;

    size += sizeof(uint64_t);
    for (const auto &k : dpf_keys) {
        size += k.CalculateSerializedSize();
    }
    size += n * sizeof(uint64_t);    // r_share
    size += n * sizeof(uint64_t);    // w_share
    size += triples.CalculateSerializedSize();
    return size;
}

void RingOaPreprocessMsg::Serialize(std::vector<uint8_t> &buffer) const {
    const size_t start = buffer.size();

    const size_t n = dpf_keys.size();

    append_pod(buffer, static_cast<uint64_t>(n));
    for (const auto &k : dpf_keys) {
        k.Serialize(buffer);
    }
    append_array(buffer, r_share.data(), n);
    append_array(buffer, w_share.data(), n);
    triples.Serialize(buffer);

    const size_t written  = buffer.size() - start;
    const size_t expected = CalculateSerializedSize();
    if (written != expected) {
        throw std::runtime_error(LOC + " RingOaPreprocessMsg::Serialize: serialized size mismatch: " +
                                 ToString(written) + " != " + ToString(expected));
    }
}

void RingOaPreprocessMsg::Deserialize(const std::vector<uint8_t> &buffer,
                                      const RingOaParameters     &params) {
    size_t offset = 0;
    Deserialize(buffer, params, offset);

    if (offset != buffer.size()) {
        throw std::runtime_error(LOC + " RingOaPreprocessMsg::Deserialize: trailing bytes: " +
                                 ToString(buffer.size() - offset));
    }
}

void RingOaPreprocessMsg::Deserialize(const std::vector<uint8_t> &buffer,
                                      const RingOaParameters     &params,
                                      size_t                     &offset) {
    const size_t start = offset;

    uint64_t n64 = 0;
    read_pod(buffer, offset, n64);

    const size_t n = static_cast<size_t>(n64);

    // Reset
    dpf_keys.clear();
    r_share.clear();
    w_share.clear();

    dpf_keys.reserve(n);
    r_share.resize(n);
    w_share.resize(n);

    // DPF keys
    for (size_t i = 0; i < n; ++i) {
        dpf_keys.emplace_back(/*party_id=*/0, params.GetParameters());
        dpf_keys.back().Deserialize(buffer, offset);
    }

    // shares
    read_array(buffer, offset, r_share.data(), n);
    read_array(buffer, offset, w_share.data(), n);

    // triples
    triples.Deserialize(buffer, offset);

    const size_t read     = offset - start;
    const size_t expected = CalculateSerializedSize();
    if (read != expected) {
        throw std::runtime_error(LOC + " RingOaPreprocessMsg::Deserialize: deserialized size mismatch: " +
                                 ToString(read) + " != " + ToString(expected));
    }
}

RingOaPreprocessData RingOaPreprocessData::FromMessage(
    uint64_t                party_id,
    const RingOaParameters &params,
    size_t                  count,
    RingOaPreprocessMsg   &&from_prev,
    RingOaPreprocessMsg   &&from_next) {

    RingOaPreprocessData out;

    out.keys = RingOaKeys(party_id, params, count);

    for (size_t i = 0; i < count; ++i) {
        out.keys.key_from_prev[i] = std::move(from_prev.dpf_keys[i]);
        out.keys.rsh_from_prev[i] = from_prev.r_share[i];
        out.keys.wsh_from_prev[i] = from_prev.w_share[i];

        out.keys.key_from_next[i] = std::move(from_next.dpf_keys[i]);
        out.keys.rsh_from_next[i] = from_next.r_share[i];
        out.keys.wsh_from_next[i] = from_next.w_share[i];
    }

    out.triples_with_prev = std::move(from_next.triples);
    out.triples_with_next = std::move(from_prev.triples);

    return out;
}

size_t RingOaPreprocessData::CalculateSerializedSize() const {
    size_t size = 0;

    size += keys.CalculateSerializedSize();
    size += triples_with_prev.CalculateSerializedSize();
    size += triples_with_next.CalculateSerializedSize();
    return size;
}

void RingOaPreprocessData::LogSerializedSizeBreakdown(const std::string &prefix) const {
    const size_t keys_bytes    = keys.CalculateSerializedSize();
    const size_t triples_bytes = triples_with_prev.CalculateSerializedSize() +
                                 triples_with_next.CalculateSerializedSize();
    const size_t total_bytes = keys_bytes + triples_bytes;

    auto pct = [&](size_t x) -> double {
        if (total_bytes == 0)
            return 0.0;
        return 100.0 * static_cast<double>(x) / static_cast<double>(total_bytes);
    };

    const std::string p = prefix.empty() ? "" : (prefix + " ");

    Logger::DebugLog(LOC, p + "PreprocessData serialized size breakdown (bytes):");
    Logger::DebugLog(LOC, p + "  total    = " + ToString(total_bytes));
    Logger::DebugLog(LOC, p + "  keys     = " + ToString(keys_bytes) + " (" + ToString(pct(keys_bytes)) + "%)");
    Logger::DebugLog(LOC, p + "  triples  = " + ToString(triples_bytes) + " (" + ToString(pct(triples_bytes)) + "%)");
}

void RingOaPreprocessData::Serialize(std::vector<uint8_t> &buffer) const {
    const size_t start = buffer.size();

    keys.Serialize(buffer);
    triples_with_prev.Serialize(buffer);
    triples_with_next.Serialize(buffer);

    const size_t written  = buffer.size() - start;
    const size_t expected = CalculateSerializedSize();
    if (written != expected) {
        throw std::runtime_error(LOC + " RingOaPreprocessData::Serialize: serialized size mismatch: " +
                                 ToString(written) + " != " + ToString(expected));
    }
}

void RingOaPreprocessData::Deserialize(const std::vector<uint8_t> &buffer,
                                       const RingOaParameters     &params) {
    size_t offset = 0;
    Deserialize(buffer, params, offset);

    if (offset != buffer.size()) {
        throw std::runtime_error(LOC + " RingOaPreprocessData::Deserialize: trailing bytes: " +
                                 ToString(buffer.size() - offset));
    }
}

void RingOaPreprocessData::Deserialize(const std::vector<uint8_t> &buffer,
                                       const RingOaParameters     &params,
                                       size_t                     &offset) {
    const size_t start = offset;

    keys.Deserialize(buffer, params, offset);
    triples_with_prev.Deserialize(buffer, offset);
    triples_with_next.Deserialize(buffer, offset);

    const size_t read     = offset - start;
    const size_t expected = CalculateSerializedSize();
    if (read != expected) {
        throw std::runtime_error(LOC + " RingOaPreprocessData::Deserialize: deserialized size mismatch: " +
                                 ToString(read) + " != " + ToString(expected));
    }
}

RingOa::RingOa(
    const RingOaParameters &params,
    ProtocolContext3P      &ctx)
    : params_(params),
      gen_(params.GetParameters()),
      eval_(params.GetParameters()),
      ctx_(ctx) {
    timers_.full_domain = ctx_.Timers().GetOrCreateTimer("RingOA::FullDomainEval");
    timers_.dot_product = ctx_.Timers().GetOrCreateTimer("RingOA::DotProduct");
    timers_.local_eval  = ctx_.Timers().GetOrCreateTimer("RingOA::LocalEval");
}

RingOaNeighborMsg RingOa::MakePreprocessMsg(size_t count) const {
    uint64_t d             = params_.GetDbIndexBits();
    uint64_t remaining_bit = params_.GetParameters().GetInputBitsize() - params_.GetParameters().GetTerminateBitsize();

    RingOaNeighborMsg out;

    // Generate RingOA keys
    for (size_t i = 0; i < count; ++i) {
        uint64_t r    = Mod2N(GlobalRng::Rand<uint64_t>(), d);
        auto     r_sh = ctx_.AssWithPrev().Share(r);

        block final_seed_0, final_seed_1;
        bool  final_control_bit_1 = false;

        auto key_pairs = gen_.GenerateKeys(r, 1, final_seed_0, final_seed_1, final_control_bit_1);

        uint64_t alpha_hat = GetLowerNBits(r, remaining_bit);
        uint64_t w         = ComputeSignCorrection(final_seed_0,
                                                   final_seed_1,
                                                   final_control_bit_1,
                                                   alpha_hat);
        auto     w_sh      = ctx_.AssWithPrev().Share(w);

        // To next: key_pairs.first, r_sh.first, w_sh.first
        out.to_next.dpf_keys.emplace_back(std::move(key_pairs.first));
        out.to_next.r_share.emplace_back(r_sh.first);
        out.to_next.w_share.emplace_back(w_sh.first);

        // To prev: key_pairs.second, r_sh.second, w_sh.second
        out.to_prev.dpf_keys.emplace_back(std::move(key_pairs.second));
        out.to_prev.r_share.emplace_back(r_sh.second);
        out.to_prev.w_share.emplace_back(w_sh.second);

#if LOG_LEVEL >= LOG_LEVEL_TRACE
        Logger::TraceLog(LOC, "(i=" + ToString(i) + ") r: " + ToString(r));
        Logger::TraceLog(LOC, "(i=" + ToString(i) + ") alpha_hat: " + ToString(alpha_hat));
        Logger::TraceLog(LOC, "(i=" + ToString(i) + ") w: " + ToString(w));
#endif
    }

    // Generate beaver triples
    std::pair<sharing::BeaverTriples, sharing::BeaverTriples> triples_sh = sharing::GenerateAndShareBeaverTriples(count, ctx_.Config());

    // To next: triples_sh.first, to prev: triples_sh.second
    out.to_next.triples = std::move(triples_sh.first);
    out.to_prev.triples = std::move(triples_sh.second);

    return out;
}

RingOaNeighborMsgIn RingOa::ExchangePreprocessMsg(Channels &chls, RingOaNeighborMsg &&out) const {
    std::vector<uint8_t> to_prev, to_next;
    out.to_prev.Serialize(to_prev);
    out.to_next.Serialize(to_next);

    chls.prev.send(to_prev);
    chls.next.send(to_next);

    std::vector<uint8_t> in_prev, in_next;
    chls.next.recv(in_next);
    chls.prev.recv(in_prev);

    RingOaNeighborMsgIn in;
    in.from_prev.Deserialize(in_prev, params_);
    in.from_next.Deserialize(in_next, params_);
    return in;
}

RingOaPreprocessData RingOa::BuildPreprocessData(uint64_t              party_id,
                                                 size_t                count,
                                                 RingOaNeighborMsgIn &&in) const {
    return RingOaPreprocessData::FromMessage(
        party_id, params_, count,
        std::move(in.from_prev),
        std::move(in.from_next));
}

RingOaPreprocessData RingOa::Preprocess(Channels &chls, size_t count) const {
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "Generate RingOA preprocess data");
#endif

    RingOaNeighborMsg   out = MakePreprocessMsg(count);
    RingOaNeighborMsgIn in  = ExchangePreprocessMsg(chls, std::move(out));
    return BuildPreprocessData(chls.party_id, count, std::move(in));
}

std::array<RingOaKeys, 3> RingOa::GenerateKeys(size_t count) const {
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "Generating RingOA keys, count = " + ToString(count));
#endif

    uint64_t d             = params_.GetDbIndexBits();
    uint64_t remaining_bit = params_.GetParameters().GetInputBitsize() - params_.GetParameters().GetTerminateBitsize();

    std::array<RingOaKeys, 3> keys = {
        RingOaKeys(0, params_, count),
        RingOaKeys(1, params_, count),
        RingOaKeys(2, params_, count),
    };

    for (size_t i = 0; i < count; ++i) {
        std::array<uint64_t, 3>                                    rands;
        std::array<std::pair<uint64_t, uint64_t>, 3>               rand_shs;
        std::array<uint64_t, 3>                                    w;
        std::array<std::pair<uint64_t, uint64_t>, 3>               w_shs;
        std::vector<std::pair<fss::dpf::DpfKey, fss::dpf::DpfKey>> key_pairs;
        key_pairs.reserve(3);

        block final_seed_0, final_seed_1;
        bool  final_control_bit_1;

        for (uint64_t j = 0; j < 3; ++j) {
            rands[j]    = Mod2N(GlobalRng::Rand<uint64_t>(), d);
            rand_shs[j] = ctx_.AssWithPrev().Share(rands[j]);
            key_pairs.push_back(gen_.GenerateKeys(rands[j], 1, final_seed_0, final_seed_1, final_control_bit_1));

#if LOG_LEVEL >= LOG_LEVEL_TRACE
            Logger::TraceLog(LOC, "final_seed_0: " + Format(final_seed_0));
            Logger::TraceLog(LOC, "final_seed_1: " + Format(final_seed_1));
            Logger::TraceLog(LOC, "final_control_bit_1: " + ToString(final_control_bit_1));
#endif

            uint64_t alpha_hat = GetLowerNBits(rands[j], remaining_bit);
            w[j]               = ComputeSignCorrection(final_seed_0,
                                                       final_seed_1,
                                                       final_control_bit_1,
                                                       alpha_hat);
            w_shs[j]           = ctx_.AssWithPrev().Share(w[j]);
        }

        for (uint64_t j = 0; j < 3; ++j) {
            uint64_t prev            = (j + 2) % 3;
            uint64_t next            = (j + 1) % 3;
            keys[j].key_from_prev[i] = std::move(key_pairs[prev].first);
            keys[j].rsh_from_prev[i] = rand_shs[prev].first;
            keys[j].wsh_from_prev[i] = w_shs[prev].first;
            keys[j].key_from_next[i] = std::move(key_pairs[next].second);
            keys[j].rsh_from_next[i] = rand_shs[next].second;
            keys[j].wsh_from_next[i] = w_shs[next].second;
        }
    }

    // Return the generated keys as an array
    return keys;
}

uint64_t RingOa::ComputeSignCorrection(
    block   &final_seed_0,
    block   &final_seed_1,
    bool     final_control_bit_1,
    uint64_t alpha_hat) const {
    uint64_t k = params_.GetShareBitsize();

    // --- Select bit depending on final_control_bit_1 ---
    bool selected_bit = final_control_bit_1
                            ? GetBit(final_seed_0, alpha_hat)
                            : GetBit(final_seed_1, alpha_hat);

    // --- Compute sign correction value (mod 2^s) ---
    uint64_t w = (selected_bit ^ final_control_bit_1) ? Mod2N(-1, k) : 1;

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    std::string src_seed = final_control_bit_1 ? "seed0" : "seed1";
    std::string msg      = "ComputeSignCorrection: "
                           "alpha_hat=" +
                      ToString(alpha_hat) +
                      ", control_bit=" + ToString(final_control_bit_1) +
                      ", src=" + src_seed +
                      ", selected_bit=" + ToString(selected_bit) +
                      ", w=" + ToString(w);
    Logger::DebugLog(LOC, msg);
#endif

    return w;
}

void RingOa::ConsumePreprocessData(RingOaPreprocessData &data) const {
    // Set beaver triples
    ctx_.AssWithPrev().SetTriples(std::move(data.triples_with_prev));
    ctx_.AssWithNext().SetTriples(std::move(data.triples_with_next));
}

void RingOa::ObliviousAccess(Channels                       &chls,
                             const RingOaKeyView            &key,
                             std::vector<block>             &uv_prev,
                             std::vector<block>             &uv_next,
                             const sharing::Rep3ShareView64 &database,
                             const sharing::Rep3Share64     &index,
                             sharing::Rep3Share64           &result) const {

    uint64_t party_id = chls.party_id;
    uint64_t d        = params_.GetDbIndexBits();
    uint64_t k        = params_.GetShareBitsize();
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
    Logger::DebugLog(LOC, "Evaluate RingOa key");
    std::string party_str = "[P" + ToString(party_id) + "] ";
    Logger::DebugLog(LOC, party_str + " idx: " + index.ToString());
    Logger::DebugLog(LOC, party_str + " db: " + database.ToString());
#endif

    // Reconstruct p - r_i
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

    uint64_t ext_dp_prev, ext_dp_next;
    if (party_id == 0) {
        ctx_.AssWithPrev().EvaluateMult(1, chls.prev, dp_prev, key.wsh_from_next, ext_dp_prev);    // P0 <-> P2
        ctx_.AssWithNext().EvaluateMult(0, chls.next, dp_next, key.wsh_from_prev, ext_dp_next);    // P0 <-> P1
    } else if (party_id == 1) {
        ctx_.AssWithNext().EvaluateMult(0, chls.next, dp_next, key.wsh_from_prev, ext_dp_next);    // P1 <-> P2
        ctx_.AssWithPrev().EvaluateMult(1, chls.prev, dp_prev, key.wsh_from_next, ext_dp_prev);    // P1 <-> P0
    } else {
        ctx_.AssWithPrev().EvaluateMult(1, chls.prev, dp_prev, key.wsh_from_next, ext_dp_prev);    // P1 <-> P2
        ctx_.AssWithNext().EvaluateMult(0, chls.next, dp_next, key.wsh_from_prev, ext_dp_next);    // P0 <-> P2
    }
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, party_str + "ext_dp_prev: " + ToString(ext_dp_prev) + ", ext_dp_next: " + ToString(ext_dp_next));
#endif

    uint64_t             selected_sh = Mod2N(ext_dp_prev + ext_dp_next, k);
    sharing::Rep3Share64 r_sh;
    ctx_.Rss().Rand(r_sh);
    result[0] = Mod2N(selected_sh + r_sh[0] - r_sh[1], k);
    chls.next.send(result[0]);
    chls.prev.recv(result[1]);
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, party_str + " result: " + ToString(result[0]) + ", " + ToString(result[1]));
#endif
}

void RingOa::ObliviousAccessPair(Channels                       &chls,
                                 const RingOaKeyView            &key1,
                                 const RingOaKeyView            &key2,
                                 std::vector<block>             &uv_prev,
                                 std::vector<block>             &uv_next,
                                 const sharing::Rep3ShareView64 &database,
                                 const sharing::Rep3ShareVec64  &index,
                                 sharing::Rep3ShareVec64        &result) const {

    uint64_t party_id = chls.party_id;
    uint64_t d        = params_.GetDbIndexBits();
    uint64_t k        = params_.GetShareBitsize();
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
    Logger::DebugLog(LOC, "Evaluate RingOa key");
    std::string party_str = "[P" + ToString(party_id) + "] ";
    Logger::DebugLog(LOC, party_str + " idx: " + index.ToString());
    Logger::DebugLog(LOC, party_str + " db: " + database.ToString());
#endif

    // Reconstruct p - r_i
    // pr: [pr_prev1, pr_next1, pr_prev2, pr_next2]
    std::array<uint64_t, 4> pr = ReconstructMaskedValue(chls, key1, key2, index);
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, party_str + " pr_prev1: " + ToString(pr[0]) + ", pr_next1: " + ToString(pr[1]) +
                              ", pr_prev2: " + ToString(pr[2]) + ", pr_next2: " + ToString(pr[3]));
#endif

    // Evaluate DPF (uv_prev and uv_next are std::vector<block>, where block
    uint64_t dp_prev1, dp_next1, dp_prev2, dp_next2;
    {
        auto scope                = ctx_.Timers().Scope(timers_.local_eval, "[P" + ToString(party_id) + "]");
        std::tie(dp_prev1, dp_next1) = EvaluateFullDomainThenDotProduct(
            party_id, key1.key_from_prev, key1.key_from_next, uv_prev, uv_next, database, pr[0], pr[1]);
        std::tie(dp_prev2, dp_next2) = EvaluateFullDomainThenDotProduct(
            party_id, key2.key_from_prev, key2.key_from_next, uv_prev, uv_next, database, pr[2], pr[3]);
    }
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, party_str + "dp_prev1: " + ToString(dp_prev1) + ", dp_next1: " + ToString(dp_next1));
    Logger::DebugLog(LOC, party_str + "dp_prev2: " + ToString(dp_prev2) + ", dp_next2: " + ToString(dp_next2));
#endif

    std::array<uint64_t, 2> ext_dp_prev, ext_dp_next;
    if (party_id == 0) {
        ctx_.AssWithPrev().EvaluateMult(1, chls.prev, {dp_prev1, dp_prev2}, {key1.wsh_from_next, key2.wsh_from_next}, ext_dp_prev);    // P0 <-> P2
        ctx_.AssWithNext().EvaluateMult(0, chls.next, {dp_next1, dp_next2}, {key1.wsh_from_prev, key2.wsh_from_prev}, ext_dp_next);    // P0 <-> P1
    } else if (party_id == 1) {
        ctx_.AssWithNext().EvaluateMult(0, chls.next, {dp_next1, dp_next2}, {key1.wsh_from_prev, key2.wsh_from_prev}, ext_dp_next);    // P1 <-> P2
        ctx_.AssWithPrev().EvaluateMult(1, chls.prev, {dp_prev1, dp_prev2}, {key1.wsh_from_next, key2.wsh_from_next}, ext_dp_prev);    // P1 <-> P0
    } else {
        ctx_.AssWithPrev().EvaluateMult(1, chls.prev, {dp_prev1, dp_prev2}, {key1.wsh_from_next, key2.wsh_from_next}, ext_dp_prev);    // P1 <-> P2
        ctx_.AssWithNext().EvaluateMult(0, chls.next, {dp_next1, dp_next2}, {key1.wsh_from_prev, key2.wsh_from_prev}, ext_dp_next);    // P0 <-> P2
    }
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, party_str + "ext_dp_prev1: " + ToString(ext_dp_prev[0]) + ", ext_dp_prev2: " + ToString(ext_dp_prev[1]) +
                              ", ext_dp_next1: " + ToString(ext_dp_next[0]) + ", ext_dp_next2: " + ToString(ext_dp_next[1]));
#endif

    uint64_t             selected1_sh = Mod2N(ext_dp_prev[0] + ext_dp_next[0], k);
    uint64_t             selected2_sh = Mod2N(ext_dp_prev[1] + ext_dp_next[1], k);
    sharing::Rep3Share64 r1_sh, r2_sh;
    ctx_.Rss().Rand(r1_sh);
    ctx_.Rss().Rand(r2_sh);
    result[0][0] = Mod2N(selected1_sh + r1_sh[0] - r1_sh[1], k);
    result[0][1] = Mod2N(selected2_sh + r2_sh[0] - r2_sh[1], k);
    chls.next.send(result[0]);
    chls.prev.recv(result[1]);
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, party_str + " result: " + ToString(result[0]) + ", " + ToString(result[1]));
#endif
}

std::pair<uint64_t, uint64_t> RingOa::EvaluateFullDomainThenDotProduct(
    const uint64_t                  party_id,
    const fss::dpf::DpfKey         &key_from_prev,
    const fss::dpf::DpfKey         &key_from_next,
    std::vector<block>             &uv_prev,
    std::vector<block>             &uv_next,
    const sharing::Rep3ShareView64 &database,
    const uint64_t                  pr_prev,
    const uint64_t                  pr_next) const {

    const uint64_t d = params_.GetDbIndexBits();
    const uint64_t k = params_.GetShareBitsize();

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "[P" + ToString(party_id) + "] key_from_prev ID: " + ToString(key_from_prev.party_id));
    Logger::DebugLog(LOC, "[P" + ToString(party_id) + "] key_from_next ID: " + ToString(key_from_next.party_id));
#endif

    {
        auto scope = ctx_.Timers().Scope(timers_.full_domain, "[P" + ToString(party_id) + "]");
        eval_.EvaluateFullDomain(key_from_next, uv_prev);
        eval_.EvaluateFullDomain(key_from_prev, uv_next);
    }

    uint64_t dp_prev{0}, dp_next{0};
    {
        auto scope                        = ctx_.Timers().Scope(timers_.dot_product, "[P" + ToString(party_id) + "]");
        const uint64_t *__restrict share0 = database.share0.data();
        const uint64_t *__restrict share1 = database.share1.data();

        uint64_t acc_prev = 0;
        uint64_t acc_next = 0;

        for (size_t i = 0; i < uv_prev.size(); ++i) {
            const uint64_t low_prev  = uv_prev[i].get<uint64_t>()[0];
            const uint64_t high_prev = uv_prev[i].get<uint64_t>()[1];
            const uint64_t low_next  = uv_next[i].get<uint64_t>()[0];
            const uint64_t high_next = uv_next[i].get<uint64_t>()[1];

            const uint64_t base = i * 128;

            for (uint64_t j = 0; j < 64; ++j) {
                const uint64_t mask_prev = 0UL - ((low_prev >> j) & 1UL);
                const uint64_t mask_next = 0UL - ((low_next >> j) & 1UL);

                const uint64_t idx_prev = Mod2N(base + j + pr_prev, d);
                const uint64_t idx_next = Mod2N(base + j + pr_next, d);

                acc_prev += share1[idx_prev] & mask_prev;
                acc_next += share0[idx_next] & mask_next;
            }

            for (uint64_t j = 0; j < 64; ++j) {
                const uint64_t mask_prev = 0UL - ((high_prev >> j) & 1UL);
                const uint64_t mask_next = 0UL - ((high_next >> j) & 1UL);

                const uint64_t idx_prev = Mod2N(base + 64UL + j + pr_prev, d);
                const uint64_t idx_next = Mod2N(base + 64UL + j + pr_next, d);

                acc_prev += share1[idx_prev] & mask_prev;
                acc_next += share0[idx_next] & mask_next;
            }
        }

        const bool neg_prev_contrib = (key_from_next.party_id == 1);
        const bool neg_next_contrib = (key_from_prev.party_id == 1);

        dp_prev = neg_prev_contrib ? (0ULL - acc_prev) : acc_prev;
        dp_next = neg_next_contrib ? (0ULL - acc_next) : acc_next;

        dp_prev = Mod2N(dp_prev, k);
        dp_next = Mod2N(dp_next, k);

        // const int64_t s_prev = Sign(key_from_prev.party_id);
        // const int64_t s_next = Sign(key_from_next.party_id);

        //     for (size_t i = 0; i < uv_prev.size(); ++i) {
        //     const uint64_t low_prev  = uv_prev[i].get<uint64_t>()[0];
        //     const uint64_t high_prev = uv_prev[i].get<uint64_t>()[1];
        //     const uint64_t low_next  = uv_next[i].get<uint64_t>()[0];
        //     const uint64_t high_next = uv_next[i].get<uint64_t>()[1];

        //     for (int j = 0; j < 64; ++j) {
        //         const uint64_t mask_prev = 0ULL - ((low_prev >> j) & 1ULL);
        //         const uint64_t mask_next = 0ULL - ((low_next >> j) & 1ULL);
        //         dp_prev                  = Mod2N(dp_prev + s_next * (database.share1[Mod2N((i * 128 + j) + pr_prev, d)] & mask_prev), k);
        //         dp_next                  = Mod2N(dp_next + s_prev * (database.share0[Mod2N((i * 128 + j) + pr_next, d)] & mask_next), k);
        //     }
        //     for (int j = 0; j < 64; ++j) {
        //         const uint64_t mask_prev = 0ULL - ((high_prev >> j) & 1ULL);
        //         const uint64_t mask_next = 0ULL - ((high_next >> j) & 1ULL);
        //         dp_prev                  = Mod2N(dp_prev + s_next * (database.share1[Mod2N((i * 128 + 64 + j) + pr_prev, d)] & mask_prev), k);
        //         dp_next                  = Mod2N(dp_next + s_prev * (database.share0[Mod2N((i * 128 + 64 + j) + pr_next, d)] & mask_next), k);
    }

    return {dp_prev, dp_next};
}

std::pair<uint64_t, uint64_t> RingOa::EvaluateFullDomainThenDotProduct(
    const uint64_t                  party_id,
    const fss::dpf::DpfKey         &key_from_prev,
    const fss::dpf::DpfKey         &key_from_next,
    std::vector<block>             &uv_prev,
    std::vector<block>             &uv_next,
    const sharing::Rep3ShareView32 &database,
    const uint64_t                  pr_prev,
    const uint64_t                  pr_next) const {

    const uint64_t d = params_.GetDbIndexBits();
    const uint64_t k = params_.GetShareBitsize();

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "[P" + ToString(party_id) + "] key_from_prev ID: " + ToString(key_from_prev.party_id));
    Logger::DebugLog(LOC, "[P" + ToString(party_id) + "] key_from_next ID: " + ToString(key_from_next.party_id));
#endif

    {
        auto scope = ctx_.Timers().Scope(timers_.full_domain, "[P" + ToString(party_id) + "]");
        eval_.EvaluateFullDomain(key_from_next, uv_prev);
        eval_.EvaluateFullDomain(key_from_prev, uv_next);
    }

    uint64_t dp_prev{0}, dp_next{0};
    {
        auto scope = ctx_.Timers().Scope(timers_.dot_product, "[P" + ToString(party_id) + "]");

        const uint32_t *__restrict share0 = database.share0.data();
        const uint32_t *__restrict share1 = database.share1.data();

        uint32_t acc_prev = 0;
        uint32_t acc_next = 0;

        for (size_t i = 0; i < uv_prev.size(); ++i) {
            const uint64_t low_prev  = uv_prev[i].get<uint64_t>()[0];
            const uint64_t high_prev = uv_prev[i].get<uint64_t>()[1];
            const uint64_t low_next  = uv_next[i].get<uint64_t>()[0];
            const uint64_t high_next = uv_next[i].get<uint64_t>()[1];

            const uint64_t base = i * 128;

            for (uint64_t j = 0; j < 64; ++j) {
                const uint32_t mask_prev = 0U - static_cast<uint32_t>((low_prev >> j) & 1UL);
                const uint32_t mask_next = 0U - static_cast<uint32_t>((low_next >> j) & 1UL);

                const uint64_t idx_prev = Mod2N(base + j + pr_prev, d);
                const uint64_t idx_next = Mod2N(base + j + pr_next, d);

                acc_prev += share1[idx_prev] & mask_prev;
                acc_next += share0[idx_next] & mask_next;
            }

            for (uint64_t j = 0; j < 64; ++j) {
                const uint32_t mask_prev = 0U - static_cast<uint32_t>((high_prev >> j) & 1UL);
                const uint32_t mask_next = 0U - static_cast<uint32_t>((high_next >> j) & 1UL);

                const uint64_t idx_prev = Mod2N(base + 64UL + j + pr_prev, d);
                const uint64_t idx_next = Mod2N(base + 64UL + j + pr_next, d);

                acc_prev += share1[idx_prev] & mask_prev;
                acc_next += share0[idx_next] & mask_next;
            }
        }

        const bool neg_prev_contrib = (key_from_next.party_id == 1);
        const bool neg_next_contrib = (key_from_prev.party_id == 1);

        dp_prev = neg_prev_contrib ? (0ULL - acc_prev) : acc_prev;
        dp_next = neg_next_contrib ? (0ULL - acc_next) : acc_next;

        dp_prev = Mod2N(dp_prev, k);
        dp_next = Mod2N(dp_next, k);
    }

    return {dp_prev, dp_next};
}

std::pair<uint64_t, uint64_t> RingOa::ReconstructMaskedValue(Channels                   &chls,
                                                             const RingOaKeyView        &key,
                                                             const sharing::Rep3Share64 &index) const {

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "ReconstructMaskedValue for Party " + ToString(chls.party_id));
#endif

    // Set replicated sharing of random value
    uint64_t k       = params_.GetShareBitsize();
    uint64_t pr_prev = 0, pr_next = 0;

    // Reconstruct p - r_i
    if (chls.party_id == 0) {
        sharing::Rep3Share64 r_1_sh(key.rsh_from_next, 0);
        sharing::Rep3Share64 r_2_sh(0, key.rsh_from_prev);
        sharing::Rep3Share64 pr_20_sh, pr_01_sh;
        uint64_t             pr_20, pr_01;
        // p - r_1 between Party 2 (prev) and Party 0 (self)
        // p - r_2 between Party 0 (self) and Party 1 (next)
        ctx_.Rss().EvaluateSub(index, r_1_sh, pr_20_sh);
        ctx_.Rss().EvaluateSub(index, r_2_sh, pr_01_sh);
        chls.prev.send(pr_20_sh[0]);
        chls.next.send(pr_01_sh[1]);
        chls.next.recv(pr_01);
        chls.prev.recv(pr_20);
        pr_prev = Mod2N(pr_20 + pr_20_sh[0] + pr_20_sh[1], k);
        pr_next = Mod2N(pr_01_sh[0] + pr_01_sh[1] + pr_01, k);

    } else if (chls.party_id == 1) {
        sharing::Rep3Share64 r_0_sh(0, key.rsh_from_prev);
        sharing::Rep3Share64 r_2_sh(key.rsh_from_next, 0);
        sharing::Rep3Share64 pr_12_sh, pr_01_sh;
        uint64_t             pr_12, pr_01;
        // p - r_0 between Party 1 (self) and Party 2 (next)
        // p - r_2 between Party 0 (prev) and Party 1 (next)
        ctx_.Rss().EvaluateSub(index, r_0_sh, pr_12_sh);
        ctx_.Rss().EvaluateSub(index, r_2_sh, pr_01_sh);
        chls.next.send(pr_12_sh[1]);
        chls.prev.send(pr_01_sh[0]);
        chls.prev.recv(pr_01);
        chls.next.recv(pr_12);
        pr_prev = Mod2N(pr_01 + pr_01_sh[0] + pr_01_sh[1], k);
        pr_next = Mod2N(pr_12_sh[0] + pr_12_sh[1] + pr_12, k);

    } else {
        sharing::Rep3Share64 r_0_sh(key.rsh_from_next, 0);
        sharing::Rep3Share64 r_1_sh(0, key.rsh_from_prev);
        sharing::Rep3Share64 pr_12_sh, pr_20_sh;
        uint64_t             pr_12, pr_20;
        // p - r_0 between Party 1 (prev) and Party 2 (self)
        // p - r_1 between Party 2 (self) and Party 0 (next)
        ctx_.Rss().EvaluateSub(index, r_0_sh, pr_12_sh);
        ctx_.Rss().EvaluateSub(index, r_1_sh, pr_20_sh);
        chls.prev.send(pr_12_sh[0]);
        chls.next.send(pr_20_sh[1]);
        chls.prev.recv(pr_12);
        chls.next.recv(pr_20);
        pr_prev = Mod2N(pr_12 + pr_12_sh[0] + pr_12_sh[1], k);
        pr_next = Mod2N(pr_20_sh[0] + pr_20_sh[1] + pr_20, k);
    }
    return std::make_pair(pr_prev, pr_next);
}

std::array<uint64_t, 4> RingOa::ReconstructMaskedValue(Channels                      &chls,
                                                       const RingOaKeyView           &key1,
                                                       const RingOaKeyView           &key2,
                                                       const sharing::Rep3ShareVec64 &index) const {

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "ReconstructPR for Party " + ToString(chls.party_id));
#endif

    // Set replicated sharing of random value
    uint64_t                k        = params_.GetShareBitsize();
    uint64_t                pr_prev1 = 0, pr_next1 = 0, pr_prev2 = 0, pr_next2 = 0;
    sharing::Rep3ShareVec64 r_0_sh(2), r_1_sh(2), r_2_sh(2);

    // Reconstruct p - r_i
    if (chls.party_id == 0) {
        r_1_sh.Set(0, sharing::Rep3Share64(key1.rsh_from_next, 0));
        r_2_sh.Set(0, sharing::Rep3Share64(0, key1.rsh_from_prev));
        r_1_sh.Set(1, sharing::Rep3Share64(key2.rsh_from_next, 0));
        r_2_sh.Set(1, sharing::Rep3Share64(0, key2.rsh_from_prev));
        sharing::Rep3ShareVec64 pr_20_sh(2), pr_01_sh(2);
        std::vector<uint64_t>   pr_20(2), pr_01(2);
        // p - r_1 between Party 0 and Party 2
        // p - r_2 between Party 0 and Party 1
        ctx_.Rss().EvaluateSub(index, r_1_sh, pr_20_sh);
        ctx_.Rss().EvaluateSub(index, r_2_sh, pr_01_sh);
        chls.prev.send(pr_20_sh[0]);
        chls.next.send(pr_01_sh[1]);
        chls.next.recv(pr_01);
        chls.prev.recv(pr_20);
        pr_prev1 = Mod2N(pr_20[0] + pr_20_sh[0][0] + pr_20_sh[1][0], k);
        pr_next1 = Mod2N(pr_01_sh[0][0] + pr_01_sh[1][0] + pr_01[0], k);
        pr_prev2 = Mod2N(pr_20[1] + pr_20_sh[0][1] + pr_20_sh[1][1], k);
        pr_next2 = Mod2N(pr_01_sh[0][1] + pr_01_sh[1][1] + pr_01[1], k);

    } else if (chls.party_id == 1) {
        r_0_sh.Set(0, sharing::Rep3Share64(0, key1.rsh_from_prev));
        r_2_sh.Set(0, sharing::Rep3Share64(key1.rsh_from_next, 0));
        r_0_sh.Set(1, sharing::Rep3Share64(0, key2.rsh_from_prev));
        r_2_sh.Set(1, sharing::Rep3Share64(key2.rsh_from_next, 0));
        sharing::Rep3ShareVec64 pr_12_sh(2), pr_01_sh(2);
        std::vector<uint64_t>   pr_12(2), pr_01(2);
        // p - r_0 between Party 1 and Party 2
        // p - r_2 between Party 0 and Party 1
        ctx_.Rss().EvaluateSub(index, r_0_sh, pr_12_sh);
        ctx_.Rss().EvaluateSub(index, r_2_sh, pr_01_sh);
        chls.next.send(pr_12_sh[1]);
        chls.prev.send(pr_01_sh[0]);
        chls.prev.recv(pr_01);
        chls.next.recv(pr_12);
        pr_prev1 = Mod2N(pr_01[0] + pr_01_sh[0][0] + pr_01_sh[1][0], k);
        pr_next1 = Mod2N(pr_12_sh[0][0] + pr_12_sh[1][0] + pr_12[0], k);
        pr_prev2 = Mod2N(pr_01[1] + pr_01_sh[0][1] + pr_01_sh[1][1], k);
        pr_next2 = Mod2N(pr_12_sh[0][1] + pr_12_sh[1][1] + pr_12[1], k);

    } else {
        r_0_sh.Set(0, sharing::Rep3Share64(key1.rsh_from_next, 0));
        r_1_sh.Set(0, sharing::Rep3Share64(0, key1.rsh_from_prev));
        r_0_sh.Set(1, sharing::Rep3Share64(key2.rsh_from_next, 0));
        r_1_sh.Set(1, sharing::Rep3Share64(0, key2.rsh_from_prev));
        sharing::Rep3ShareVec64 pr_12_sh(2), pr_20_sh(2);
        std::vector<uint64_t>   pr_12(2), pr_20(2);
        // p - r_0 between Party 1 and Party 2
        // p - r_1 between Party 0 and Party 2
        ctx_.Rss().EvaluateSub(index, r_0_sh, pr_12_sh);
        ctx_.Rss().EvaluateSub(index, r_1_sh, pr_20_sh);
        chls.prev.send(pr_12_sh[0]);
        chls.next.send(pr_20_sh[1]);
        chls.prev.recv(pr_12);
        chls.next.recv(pr_20);
        pr_prev1 = Mod2N(pr_12[0] + pr_12_sh[0][0] + pr_12_sh[1][0], k);
        pr_next1 = Mod2N(pr_20_sh[0][0] + pr_20_sh[1][0] + pr_20[0], k);
        pr_prev2 = Mod2N(pr_12[1] + pr_12_sh[0][1] + pr_12_sh[1][1], k);
        pr_next2 = Mod2N(pr_20_sh[0][1] + pr_20_sh[1][1] + pr_20[1], k);
    }
    return std::array<uint64_t, 4>{pr_prev1, pr_next1, pr_prev2, pr_next2};
}

}    // namespace proto
}    // namespace ringoa
