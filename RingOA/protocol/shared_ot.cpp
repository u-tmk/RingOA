#include "shared_ot.h"

#include <cstring>

#include "RingOA/sharing/beaver_triples_gen.h"
#include "RingOA/utils/bytes.h"
#include "RingOA/utils/logger.h"
#include "RingOA/utils/network.h"
#include "RingOA/utils/rng.h"
#include "RingOA/utils/timer.h"
#include "RingOA/utils/to_string.h"
#include "RingOA/utils/utils.h"

namespace ringoa {
namespace proto {

void SharedOtParameters::PrintParametersDebug(bool with_header, int key_width) const {
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    if (with_header) {
        Logger::DebugLog(LOC, Logger::MakeSeparatorLine());
        Logger::DebugLog(LOC, "[Shared OT Parameters]");
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

SharedOtKeys::SharedOtKeys(
    uint64_t                  party_id,
    const SharedOtParameters &params,
    size_t                    count)
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

size_t SharedOtKeys::CalculateSerializedSize() const {
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

void SharedOtKeys::Serialize(std::vector<uint8_t> &buffer) const {
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
        throw std::runtime_error(LOC + " SharedOtKeys::Serialize: serialized size mismatch: " +
                                 ToString(written) + " != " + ToString(expected));
    }
}

void SharedOtKeys::Deserialize(const std::vector<uint8_t> &buffer,
                               const SharedOtParameters   &params) {
    size_t offset = 0;
    Deserialize(buffer, params, offset);
}

void SharedOtKeys::Deserialize(const std::vector<uint8_t> &buffer,
                               const SharedOtParameters   &params,
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
        throw std::runtime_error(LOC + " SharedOtKeys::Deserialize: deserialized size mismatch: " +
                                 ToString(read) + " != " + ToString(expected));
    }
}

size_t SharedOtPreprocessMsg::CalculateSerializedSize() const {
    const size_t n = dpf_keys.size();

    size_t size = 0;

    size += sizeof(uint64_t);
    for (const auto &k : dpf_keys) {
        size += k.CalculateSerializedSize();
    }
    size += n * sizeof(uint64_t);    // r_share
    return size;
}

void SharedOtPreprocessMsg::Serialize(std::vector<uint8_t> &buffer) const {
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
        throw std::runtime_error(LOC + " SharedOtPreprocessMsg::Serialize: serialized size mismatch: " +
                                 ToString(written) + " != " + ToString(expected));
    }
}

void SharedOtPreprocessMsg::Deserialize(const std::vector<uint8_t> &buffer,
                                        const SharedOtParameters   &params) {
    size_t offset = 0;
    Deserialize(buffer, params, offset);

    if (offset != buffer.size()) {
        throw std::runtime_error(LOC + " SharedOtPreprocessMsg::Deserialize: trailing bytes: " +
                                 ToString(buffer.size() - offset));
    }
}

void SharedOtPreprocessMsg::Deserialize(const std::vector<uint8_t> &buffer,
                                        const SharedOtParameters   &params,
                                        size_t                     &offset) {
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
        throw std::runtime_error(LOC + " SharedOtPreprocessMsg::Deserialize: deserialized size mismatch: " +
                                 ToString(read) + " != " + ToString(expected));
    }
}

SharedOtPreprocessData SharedOtPreprocessData::FromMessage(
    uint64_t                  party_id,
    const SharedOtParameters &params,
    size_t                    count,
    SharedOtPreprocessMsg   &&from_prev,
    SharedOtPreprocessMsg   &&from_next) {

    SharedOtPreprocessData out;

    out.keys = SharedOtKeys(party_id, params, count);

    for (size_t i = 0; i < count; ++i) {
        out.keys.key_from_prev[i] = std::move(from_prev.dpf_keys[i]);
        out.keys.rsh_from_prev[i] = from_prev.r_share[i];

        out.keys.key_from_next[i] = std::move(from_next.dpf_keys[i]);
        out.keys.rsh_from_next[i] = from_next.r_share[i];
    }

    return out;
}

size_t SharedOtPreprocessData::CalculateSerializedSize() const {
    size_t size = 0;

    size += keys.CalculateSerializedSize();
    return size;
}

void SharedOtPreprocessData::Serialize(std::vector<uint8_t> &buffer) const {
    const size_t start = buffer.size();

    keys.Serialize(buffer);

    const size_t written  = buffer.size() - start;
    const size_t expected = CalculateSerializedSize();
    if (written != expected) {
        throw std::runtime_error(LOC + " SharedOtPreprocessData::Serialize: serialized size mismatch: " +
                                 ToString(written) + " != " + ToString(expected));
    }
}

void SharedOtPreprocessData::Deserialize(const std::vector<uint8_t> &buffer,
                                         const SharedOtParameters   &params) {
    size_t offset = 0;
    Deserialize(buffer, params, offset);

    if (offset != buffer.size()) {
        throw std::runtime_error(LOC + " SharedOtPreprocessData::Deserialize: trailing bytes: " +
                                 ToString(buffer.size() - offset));
    }
}

void SharedOtPreprocessData::Deserialize(const std::vector<uint8_t> &buffer,
                                         const SharedOtParameters   &params,
                                         size_t                     &offset) {
    const size_t start = offset;

    keys.Deserialize(buffer, params, offset);

    const size_t read     = offset - start;
    const size_t expected = CalculateSerializedSize();
    if (read != expected) {
        throw std::runtime_error(LOC + " SharedOtPreprocessData::Deserialize: deserialized size mismatch: " +
                                 ToString(read) + " != " + ToString(expected));
    }
}

SharedOt::SharedOt(
    const SharedOtParameters &params,
    ProtocolContext3P        &ctx)
    : params_(params),
      gen_(params.GetParameters()),
      eval_(params.GetParameters()),
      ctx_(ctx) {
    timers_.full_domain = ctx_.Timers().GetOrCreateTimer("SharedOT::FullDomainEval");
    timers_.dot_product = ctx_.Timers().GetOrCreateTimer("SharedOT::DotProduct");
    timers_.local_eval  = ctx_.Timers().GetOrCreateTimer("SharedOT::LocalEval");
}

SharedOtNeighborMsg SharedOt::MakePreprocessMsg(size_t count) const {
    uint64_t d = params_.GetDbIndexBits();

    SharedOtNeighborMsg out;

    // Generate SharedOT keys
    for (size_t i = 0; i < count; ++i) {
        uint64_t r    = Mod2N(GlobalRng::Rand<uint64_t>(), d);
        auto     r_sh = ctx_.AssWithPrev().Share(r);

        auto key_pairs = gen_.GenerateKeys(r, 1);

        // To next: key_pairs.first, r_sh.first, w_sh.first
        out.to_next.dpf_keys.emplace_back(std::move(key_pairs.first));
        out.to_next.r_share.emplace_back(r_sh.first);

        // To prev: key_pairs.second, r_sh.second, w_sh.second
        out.to_prev.dpf_keys.emplace_back(std::move(key_pairs.second));
        out.to_prev.r_share.emplace_back(r_sh.second);

#if LOG_LEVEL >= LOG_LEVEL_TRACE
        Logger::TraceLog(LOC, "(i=" + ToString(i) + ") r: " + ToString(r));
#endif
    }

    return out;
}

SharedOtNeighborMsgIn SharedOt::ExchangePreprocessMsg(Channels &chls, SharedOtNeighborMsg &&out) const {
    std::vector<uint8_t> to_prev, to_next;
    out.to_prev.Serialize(to_prev);
    out.to_next.Serialize(to_next);

    chls.prev.send(to_prev);
    chls.next.send(to_next);

    std::vector<uint8_t> in_prev, in_next;
    chls.next.recv(in_next);
    chls.prev.recv(in_prev);

    SharedOtNeighborMsgIn in;
    in.from_prev.Deserialize(in_prev, params_);
    in.from_next.Deserialize(in_next, params_);
    return in;
}

SharedOtPreprocessData SharedOt::BuildPreprocessData(uint64_t                party_id,
                                                     size_t                  count,
                                                     SharedOtNeighborMsgIn &&in) const {
    return SharedOtPreprocessData::FromMessage(
        party_id, params_, count,
        std::move(in.from_prev),
        std::move(in.from_next));
}

SharedOtPreprocessData SharedOt::Preprocess(Channels &chls, size_t count) const {
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "Generate SharedOt preprocess data");
#endif

    SharedOtNeighborMsg   out = MakePreprocessMsg(count);
    SharedOtNeighborMsgIn in  = ExchangePreprocessMsg(chls, std::move(out));
    return BuildPreprocessData(chls.party_id, count, std::move(in));
}

void SharedOt::ObliviousAccess(Channels                       &chls,
                               const SharedOtKeyView          &key,
                               std::vector<uint64_t>          &uv_prev,
                               std::vector<uint64_t>          &uv_next,
                               const sharing::Rep3ShareView64 &database,
                               const sharing::Rep3Share64     &index,
                               sharing::Rep3Share64           &result) const {

    uint64_t party_id = chls.party_id;
    uint64_t d        = params_.GetDbIndexBits();
    uint64_t k        = params_.GetShareBitsize();

    if (uv_prev.size() != (1UL << d) || uv_next.size() != (1UL << d)) {
        throw std::invalid_argument(LOC + " Output vector size does not match the number of nodes: " +
                                    ToString(uv_prev.size()) + " != " + ToString(1UL << d) +
                                    " or " + ToString(uv_next.size()) + " != " + ToString(1UL << d));
    }
    if (database.Size() != (1UL << d)) {
        throw std::invalid_argument(LOC + " Database size does not match the number of nodes: " +
                                    ToString(database.Size()) + " != " + ToString(1UL << d));
    }

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "Evaluate SharedOt key");
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

    uint64_t             selected_sh = Mod2N(dp_prev + dp_next, k);
    sharing::Rep3Share64 r_sh;
    ctx_.Rss().Rand(r_sh);
    result[0] = Mod2N(selected_sh + r_sh[0] - r_sh[1], k);
    chls.next.send(result[0]);
    chls.prev.recv(result[1]);
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, party_str + " result: " + ToString(result[0]) + ", " + ToString(result[1]));
#endif
}

void SharedOt::ObliviousAccessPair(Channels                       &chls,
                                   const SharedOtKeyView          &key1,
                                   const SharedOtKeyView          &key2,
                                   std::vector<uint64_t>          &uv_prev,
                                   std::vector<uint64_t>          &uv_next,
                                   const sharing::Rep3ShareView64 &database,
                                   const sharing::Rep3ShareVec64  &index,
                                   sharing::Rep3ShareVec64        &result) const {

    uint64_t party_id = chls.party_id;
    uint64_t d        = params_.GetDbIndexBits();
    uint64_t k        = params_.GetShareBitsize();

    if (uv_prev.size() != (1UL << d) || uv_next.size() != (1UL << d)) {
        throw std::invalid_argument(LOC + " Output vector size does not match the number of nodes: " +
                                    ToString(uv_prev.size()) + " != " + ToString(1UL << d) +
                                    " or " + ToString(uv_next.size()) + " != " + ToString(1UL << d));
    }
    if (database.Size() != (1UL << d)) {
        throw std::invalid_argument(LOC + " Database size does not match the number of nodes: " +
                                    ToString(database.Size()) + " != " + ToString(1UL << d));
    }

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "Evaluate SharedOt key");
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
        auto scope                   = ctx_.Timers().Scope(timers_.local_eval, "[P" + ToString(party_id) + "]");
        std::tie(dp_prev1, dp_next1) = EvaluateFullDomainThenDotProduct(
            party_id, key1.key_from_prev, key1.key_from_next, uv_prev, uv_next, database, pr[0], pr[1]);
        std::tie(dp_prev2, dp_next2) = EvaluateFullDomainThenDotProduct(
            party_id, key2.key_from_prev, key2.key_from_next, uv_prev, uv_next, database, pr[2], pr[3]);
    }
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, party_str + "dp_prev1: " + ToString(dp_prev1) + ", dp_next1: " + ToString(dp_next1));
    Logger::DebugLog(LOC, party_str + "dp_prev2: " + ToString(dp_prev2) + ", dp_next2: " + ToString(dp_next2));
#endif

    uint64_t             selected1_sh = Mod2N(dp_prev1 + dp_next1, k);
    uint64_t             selected2_sh = Mod2N(dp_prev2 + dp_next2, k);
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

std::pair<uint64_t, uint64_t> SharedOt::EvaluateFullDomainThenDotProduct(
    const uint64_t                  party_id,
    const fss::dpf::DpfKey         &key_from_prev,
    const fss::dpf::DpfKey         &key_from_next,
    std::vector<uint64_t>          &uv_prev,
    std::vector<uint64_t>          &uv_next,
    const sharing::Rep3ShareView64 &database,
    const uint64_t                  pr_prev,
    const uint64_t                  pr_next) const {

    uint64_t d = params_.GetDbIndexBits();
    uint64_t k = params_.GetShareBitsize();

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "[P" + ToString(party_id) + "] key_from_prev ID: " + ToString(key_from_prev.party_id));
    Logger::DebugLog(LOC, "[P" + ToString(party_id) + "] key_from_next ID: " + ToString(key_from_next.party_id));
#endif

    {
        auto scope = ctx_.Timers().Scope(timers_.full_domain, "[P" + ToString(party_id) + "]");
        // Evaluate DPF (uv_prev and uv_next are std::vector<uint64_t>, where uint64_t is the value of the node)
        eval_.EvaluateFullDomain(key_from_next, uv_prev);
        eval_.EvaluateFullDomain(key_from_prev, uv_next);
    }

    uint64_t dp_prev = 0, dp_next = 0;

    {
        auto scope = ctx_.Timers().Scope(timers_.dot_product, "[P" + ToString(party_id) + "]");
        for (size_t i = 0; i < uv_prev.size(); ++i) {
            dp_prev = Mod2N(dp_prev + database.share1[Mod2N(i + pr_prev, d)] * uv_prev[i], k);
            dp_next = Mod2N(dp_next + database.share0[Mod2N(i + pr_next, d)] * uv_next[i], k);
        }
    }
    return std::make_pair(dp_prev, dp_next);
}

std::pair<uint64_t, uint64_t> SharedOt::ReconstructMaskedValue(Channels                   &chls,
                                                               const SharedOtKeyView      &key,
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

std::array<uint64_t, 4> SharedOt::ReconstructMaskedValue(Channels                      &chls,
                                                         const SharedOtKeyView         &key1,
                                                         const SharedOtKeyView         &key2,
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
