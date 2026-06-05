#include "sot_range.h"

#include <cstring>

#include "RingOA/utils/bytes.h"
#include "RingOA/utils/logger.h"
#include "RingOA/utils/to_string.h"
#include "RingOA/utils/utils.h"
#include "RingOA/wm/plain_wm.h"

namespace ringoa {
namespace range {

void SotRangeParameters::PrintParametersDebug(bool with_header, int key_width) const {
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    if (with_header) {
        Logger::DebugLog(LOC, Logger::MakeSeparatorLine());
        Logger::DebugLog(LOC, "[Oblivious Range Params]");
        Logger::DebugLog(LOC, Logger::MakeSeparatorLine());
    }

    Logger::DebugLog(LOC, Logger::FormatKeyValue("DatabaseIndexBits", ToString(database_bitsize_), key_width));
    Logger::DebugLog(LOC, Logger::FormatKeyValue("DatabaseSize", ToString(database_size_), key_width));
    Logger::DebugLog(LOC, Logger::FormatKeyValue("Sigma", ToString(sigma_), key_width));
    Logger::DebugLog(LOC, Logger::FormatKeyValue("OaCalls", ToString(GetNumOfOaCalls()), key_width));
    Logger::DebugLog(LOC, Logger::FormatKeyValue("ShareBits", ToString(share_bitsize_), key_width));

    oa_params_.PrintParametersDebug(false, key_width);
    ic_params_.PrintParametersDebug(false, key_width);

    if (with_header) {
        Logger::DebugLog(LOC, Logger::MakeSeparatorLine());
    }
#endif
}

SotRangeKeys::SotRangeKeys(const uint64_t id, const SotRangeParameters &params)
    : oa_keys(id, params.GetSharedOtParameters(), params.GetNumOfOaCalls()),
      ic_keys(id, params.GetIcParameters(), params.GetNumOfIcCalls()) {
}

SotRangeKeys::SotRangeKeys(proto::SharedOtKeys &&oa_keys_in, proto::IntegerComparisonKeys &&ic_keys_in)
    : oa_keys(std::move(oa_keys_in)),
      ic_keys(std::move(ic_keys_in)) {
}

size_t SotRangeKeys::CalculateSerializedSize() const {
    size_t size = 0;

    size += oa_keys.CalculateSerializedSize();
    size += ic_keys.CalculateSerializedSize();
    return size;
}

void SotRangeKeys::Serialize(std::vector<uint8_t> &buffer) const {
    const size_t start = buffer.size();

    // Serialize the OA keys
    oa_keys.Serialize(buffer);

    // Serialize the IC keys
    ic_keys.Serialize(buffer);

    // Check size
    const size_t written  = buffer.size() - start;
    const size_t expected = CalculateSerializedSize();
    if (written != expected) {
        throw std::runtime_error(LOC + " SotRangeKeys::Serialize: serialized size mismatch: " +
                                 ToString(written) + " != " + ToString(expected));
    }
}

void SotRangeKeys::Deserialize(const std::vector<uint8_t> &buffer, const SotRangeParameters &params) {
    size_t offset = 0;
    Deserialize(buffer, params, offset);
}

void SotRangeKeys::Deserialize(const std::vector<uint8_t> &buffer, const SotRangeParameters &params, size_t &offset) {
    const size_t start = offset;

    // Deserialize the OA keys
    oa_keys.Deserialize(buffer, params.GetSharedOtParameters(), offset);

    // Deserialize the IC keys
    ic_keys.Deserialize(buffer, params.GetIcParameters(), offset);

    // Check size
    const size_t read     = offset - start;
    const size_t expected = CalculateSerializedSize();
    if (read != expected) {
        throw std::runtime_error(LOC + " SotRangeKeys::Deserialize: deserialized size mismatch: " +
                                 ToString(read) + " != " + ToString(expected));
    }
}

SotRangeKeys SotRangePreprocessData::ExtractKeys() && {
    return SotRangeKeys(std::move(oa_data.keys), std::move(ic_keys));
}

size_t SotRangePreprocessData::CalculateSerializedSize() const {
    size_t size = 0;

    size += oa_data.CalculateSerializedSize();
    size += ic_keys.CalculateSerializedSize();
    return size;
}

void SotRangePreprocessData::LogSerializedSizeBreakdown(const std::string &prefix) const {
    const size_t oa_data_bytes = oa_data.CalculateSerializedSize();
    const size_t ic_keys_bytes = ic_keys.CalculateSerializedSize();
    const size_t total_bytes   = oa_data_bytes + ic_keys_bytes;

    auto pct = [&](size_t x) -> double {
        if (total_bytes == 0)
            return 0.0;
        return 100.0 * static_cast<double>(x) / static_cast<double>(total_bytes);
    };

    const std::string p = prefix.empty() ? "" : (prefix + " ");

    Logger::DebugLog(LOC, p + "SotRangePreprocessData serialized size breakdown (bytes):");
    Logger::DebugLog(LOC, p + "  total    = " + ToString(total_bytes));
    Logger::DebugLog(LOC, p + "  OA data  = " + ToString(oa_data_bytes) + " (" + ToString(pct(oa_data_bytes)) + "%)");
    Logger::DebugLog(LOC, p + "  IC keys  = " + ToString(ic_keys_bytes) + " (" + ToString(pct(ic_keys_bytes)) + "%)");
}

void SotRangePreprocessData::Serialize(std::vector<uint8_t> &buffer) const {
    const size_t start = buffer.size();

    oa_data.Serialize(buffer);
    ic_keys.Serialize(buffer);

    const size_t written  = buffer.size() - start;
    const size_t expected = CalculateSerializedSize();
    if (written != expected) {
        throw std::runtime_error(LOC + " SotRangePreprocessData::Serialize: serialized size mismatch: " +
                                 ToString(written) + " != " + ToString(expected));
    }
}

void SotRangePreprocessData::Deserialize(const std::vector<uint8_t> &buffer, const SotRangeParameters &params) {
    size_t offset = 0;
    Deserialize(buffer, params, offset);

    if (offset != buffer.size()) {
        throw std::runtime_error(LOC + " SotRangePreprocessData::Deserialize: trailing bytes: " +
                                 ToString(buffer.size() - offset));
    }
}

void SotRangePreprocessData::Deserialize(const std::vector<uint8_t> &buffer, const SotRangeParameters &params, size_t &offset) {
    const size_t start = offset;

    oa_data.Deserialize(buffer, params.GetSharedOtParameters(), offset);
    ic_keys.Deserialize(buffer, params.GetIcParameters(), offset);

    const size_t read     = offset - start;
    const size_t expected = CalculateSerializedSize();
    if (read != expected) {
        throw std::runtime_error(LOC + " SotRangePreprocessData::Deserialize: deserialized size mismatch: " +
                                 ToString(read) + " != " + ToString(expected));
    }
}

SotRange::SotRange(
    const SotRangeParameters &params,
    proto::ProtocolContext2P &ctx2p,
    proto::ProtocolContext3P &ctx3p)
    : params_(params),
      sot_(params.GetSharedOtParameters(), ctx3p),
      ic_(params.GetIcParameters(), ctx2p),
      ctx2p_(ctx2p),
      ctx3p_(ctx3p) {
}

std::array<sharing::Rep3ShareMat64, 3> SotRange::GenerateDatabaseU64Share(const wm::WaveletMatrix &wm) const {
    if (wm.GetLength() + 1 != params_.GetDatabaseSize()) {
        throw std::invalid_argument("WaveletMatrix length does not match the database size in SotRangeParameters");
    }
    const std::vector<uint64_t> &rank0_tables = wm.GetRank0Tables();
    return ctx3p_.Rss().ShareLocal(rank0_tables, wm.GetSigma(), wm.GetLength() + 1);
}

SotRangePreprocessData SotRange::Preprocess(Channels &chls) const {
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "Generate SotRange preprocess data");
#endif

    uint64_t party_id = chls.party_id;

    proto::SharedOtNeighborMsg out = sot_.MakePreprocessMsg(params_.GetNumOfOaCalls());

    std::vector<uint8_t> to_prev, to_next;
    out.to_prev.Serialize(to_prev);
    out.to_next.Serialize(to_next);

    if (party_id == 0) {
        auto ic_key_pairs = ic_.GenerateKeys(params_.GetNumOfIcCalls());
        ic_key_pairs.first.Serialize(to_prev);
        ic_key_pairs.second.Serialize(to_next);
    }

    chls.prev.send(to_prev);
    chls.next.send(to_next);

    std::vector<uint8_t> in_prev, in_next;
    chls.prev.recv(in_prev);
    chls.next.recv(in_next);

    proto::SharedOtNeighborMsgIn in;
    size_t                       offset_prev{0}, offset_next{0};
    in.from_prev.Deserialize(in_prev, params_.GetSharedOtParameters(), offset_prev);
    in.from_next.Deserialize(in_next, params_.GetSharedOtParameters(), offset_next);

    if (party_id == 0) {
        return SotRangePreprocessData(
            sot_.BuildPreprocessData(party_id, params_.GetNumOfOaCalls(), std::move(in)),
            proto::IntegerComparisonKeys{});
    } else if (party_id == 1) {
        proto::IntegerComparisonKeys ic_keys;
        ic_keys.Deserialize(in_prev, params_.GetIcParameters(), offset_prev);
        return SotRangePreprocessData(
            sot_.BuildPreprocessData(party_id, params_.GetNumOfOaCalls(), std::move(in)),
            std::move(ic_keys));
    } else if (party_id == 2) {
        proto::IntegerComparisonKeys ic_keys;
        ic_keys.Deserialize(in_next, params_.GetIcParameters(), offset_next);
        return SotRangePreprocessData(
            sot_.BuildPreprocessData(party_id, params_.GetNumOfOaCalls(), std::move(in)),
            std::move(ic_keys));
    } else {
        throw std::runtime_error(LOC + " Invalid party ID: " + ToString(party_id));
    }
}

void SotRange::EvaluateRange(Channels                      &chls,
                             const SotRangeKeys            &key,
                             std::vector<uint64_t>         &uv_prev,
                             std::vector<uint64_t>         &uv_next,
                             const sharing::Rep3ShareMat64 &wm_tables,
                             sharing::Rep3Share64          &left_sh,
                             sharing::Rep3Share64          &right_sh,
                             sharing::Rep3Share64          &k_sh,
                             sharing::Rep3Share64          &result) const {

    uint64_t s        = params_.GetShareBitsize();
    uint64_t sigma    = params_.GetSigma();
    uint64_t party_id = chls.party_id;

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "Evaluate SotRange key");
    Logger::DebugLog(LOC, "Share size: " + ToString(s));
    Logger::DebugLog(LOC, "Sigma: " + ToString(sigma));
    Logger::DebugLog(LOC, "Rows: " + ToString(wm_tables.rows) + ", Columns: " + ToString(wm_tables.cols));
    std::string party_str = "[P" + ToString(party_id) + "] ";
#endif

    result = sharing::Rep3Share64(0, 0);
    sharing::Rep3Share64 zeroleft_sh(0, 0), zeroright_sh(0, 0);
    sharing::Rep3Share64 total_zeros(0, 0);
    sharing::Rep3Share64 zerocount_sh(0, 0);
    sharing::Rep3Share64 comp_sh(0, 0);

    size_t oa_key_idx = 0;
    for (uint64_t i = sigma; i > 0; --i) {
        const size_t bit = i - 1;
        sot_.ObliviousAccess(chls, key.oa_keys.GetView(oa_key_idx), uv_prev, uv_next, wm_tables.RowView(bit), left_sh, zeroleft_sh);
        sot_.ObliviousAccess(chls, key.oa_keys.GetView(oa_key_idx + 1), uv_prev, uv_next, wm_tables.RowView(bit), right_sh, zeroright_sh);
        oa_key_idx += 2;

        total_zeros = wm_tables.RowView(bit).At(wm_tables.RowView(bit).Size() - 1);
        ctx3p_.Rss().EvaluateSub(zeroright_sh, zeroleft_sh, zerocount_sh);

        // Convert RSS to (2, 2)-sharing between P1 and P2 and Evaluate IntegerComparison
        uint64_t             ic_0{0}, ic_1{0};
        sharing::Rep3Share64 r1_sh, r2_sh;
        ctx3p_.Rss().Rand(r1_sh);
        ctx3p_.Rss().Rand(r2_sh);
        if (party_id == 1) {
            uint64_t k_0         = Mod2N(k_sh.data[0] + k_sh.data[1] + r1_sh.data[1], s);
            uint64_t zerocount_0 = Mod2N(zerocount_sh.data[0] + zerocount_sh.data[1] + r2_sh.data[1], s);
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
            Logger::DebugLog(LOC, party_str + " k_0: " + ToString(k_0) + ", zerocount_0: " + ToString(zerocount_0));
#endif
            ic_0 = ic_.EvaluateSharedInput(chls.next, key.ic_keys.GetView(bit), k_0, zerocount_0);
        } else if (party_id == 2) {
            uint64_t k_1         = Mod2N(k_sh.data[0] - r1_sh.data[0], s);
            uint64_t zerocount_1 = Mod2N(zerocount_sh.data[0] - r2_sh.data[0], s);
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
            Logger::DebugLog(LOC, party_str + " k_1: " + ToString(k_1) + ", zerocount_1: " + ToString(zerocount_1));
#endif
            ic_1 = ic_.EvaluateSharedInput(chls.prev, key.ic_keys.GetView(bit), k_1, zerocount_1);
        }

        // Convert (2, 2)-sharing to RSS
        if (party_id == 0) {
            ctx3p_.Rss().Rand(r1_sh);
            comp_sh[0] = Mod2N(r1_sh[1] - r1_sh[0], s);
        } else if (party_id == 1) {
            ctx3p_.Rss().Rand(r1_sh);
            comp_sh[0] = Mod2N(ic_0 + r1_sh[1] - r1_sh[0], s);
        } else if (party_id == 2) {
            ctx3p_.Rss().Rand(r1_sh);
            comp_sh[0] = Mod2N(ic_1 + r1_sh[1] - r1_sh[0], s);
        }
        chls.next.send(comp_sh[0]);
        chls.prev.recv(comp_sh[1]);

        // Update left_sh / right_sh and k_sh
        sharing::Rep3Share64 oneleft_sh(0, 0), oneright_sh(0, 0);
        ctx3p_.Rss().EvaluateAdd(total_zeros, left_sh, oneleft_sh);
        ctx3p_.Rss().EvaluateSub(oneleft_sh, zeroleft_sh, oneleft_sh);
        ctx3p_.Rss().EvaluateAdd(total_zeros, right_sh, oneright_sh);
        ctx3p_.Rss().EvaluateSub(oneright_sh, zeroright_sh, oneright_sh);

        sharing::Rep3Share64 update_sh(0, 0);
        ctx3p_.Rss().EvaluateSub(k_sh, zerocount_sh, update_sh);

        sharing::Rep3ShareVec64 zerolrk_sh(3), onelrk_sh(3), select_sh(3);
        zerolrk_sh.Set(0, zeroleft_sh);
        zerolrk_sh.Set(1, zeroright_sh);
        zerolrk_sh.Set(2, k_sh);
        onelrk_sh.Set(0, oneleft_sh);
        onelrk_sh.Set(1, oneright_sh);
        onelrk_sh.Set(2, update_sh);
        ctx3p_.Rss().EvaluateSelect(chls, zerolrk_sh, onelrk_sh, comp_sh, select_sh);
        left_sh  = select_sh.At(0);
        right_sh = select_sh.At(1);
        k_sh     = select_sh.At(2);

        // Update result
        sharing::Rep3Share64 cond_sh(0, 0);
        cond_sh[0] = Mod2N(comp_sh[0] * (1UL << bit), s);
        cond_sh[1] = Mod2N(comp_sh[1] * (1UL << bit), s);
        ctx3p_.Rss().EvaluateAdd(result, cond_sh, result);

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
        uint64_t total_zero_rec, zeroleft_rec, zeroright_rec, zerocount_rec;
        uint64_t comp_rec;
        uint64_t k_rec;
        uint64_t left_rec, right_rec;
        uint64_t result_rec;
        ctx3p_.Rss().Open(chls, total_zeros, total_zero_rec);
        ctx3p_.Rss().Open(chls, zeroleft_sh, zeroleft_rec);
        ctx3p_.Rss().Open(chls, zeroright_sh, zeroright_rec);
        ctx3p_.Rss().Open(chls, zerocount_sh, zerocount_rec);
        ctx3p_.Rss().Open(chls, comp_sh, comp_rec);
        ctx3p_.Rss().Open(chls, k_sh, k_rec);
        ctx3p_.Rss().Open(chls, left_sh, left_rec);
        ctx3p_.Rss().Open(chls, right_sh, right_rec);
        ctx3p_.Rss().Open(chls, result, result_rec);
        Logger::DebugLog(LOC, party_str + "total_zero_rec: " + ToString(total_zero_rec));
        Logger::DebugLog(LOC, party_str + "zeroleft_rec: " + ToString(zeroleft_rec));
        Logger::DebugLog(LOC, party_str + "zeroright_rec: " + ToString(zeroright_rec));
        Logger::DebugLog(LOC, party_str + "zerocount_rec: " + ToString(zerocount_rec));
        Logger::DebugLog(LOC, party_str + "comp_rec: " + ToString(comp_rec));
        Logger::DebugLog(LOC, party_str + "k_rec: " + ToString(k_rec));
        Logger::DebugLog(LOC, party_str + "left_rec: " + ToString(left_rec));
        Logger::DebugLog(LOC, party_str + "right_rec: " + ToString(right_rec));
        Logger::DebugLog(LOC, party_str + "result_rec: " + ToString(result_rec));
#endif
    }
}

void SotRange::EvaluateRangePair(Channels                      &chls,
                                 const SotRangeKeys            &key,
                                 std::vector<uint64_t>         &uv_prev,
                                 std::vector<uint64_t>         &uv_next,
                                 const sharing::Rep3ShareMat64 &wm_tables,
                                 sharing::Rep3Share64          &left_sh,
                                 sharing::Rep3Share64          &right_sh,
                                 sharing::Rep3Share64          &k_sh,
                                 sharing::Rep3Share64          &result) const {

    uint64_t s        = params_.GetShareBitsize();
    uint64_t sigma    = params_.GetSigma();
    uint64_t party_id = chls.party_id;

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "Evaluate SotRange_Parallel key");
    Logger::DebugLog(LOC, "Share size: " + ToString(s));
    Logger::DebugLog(LOC, "Sigma: " + ToString(sigma));
    Logger::DebugLog(LOC, "Rows: " + ToString(wm_tables.rows) + ", Columns: " + ToString(wm_tables.cols));
    std::string party_str = "[P" + ToString(party_id) + "] ";
#endif

    result = sharing::Rep3Share64(0, 0);
    sharing::Rep3ShareVec64 lr_sh(2), zerolr_sh(2);
    sharing::Rep3Share64    total_zeros(0, 0);
    sharing::Rep3Share64    zerocount_sh(0, 0);
    sharing::Rep3Share64    comp_sh(0, 0);

    size_t oa_key_idx = 0;
    for (uint64_t i = sigma; i > 0; --i) {
        const size_t bit = i - 1;
        lr_sh.Set(0, left_sh);
        lr_sh.Set(1, right_sh);
        sot_.ObliviousAccessPair(chls, key.oa_keys.GetView(oa_key_idx), key.oa_keys.GetView(oa_key_idx + 1), uv_prev, uv_next, wm_tables.RowView(bit), lr_sh, zerolr_sh);
        oa_key_idx += 2;

        total_zeros = wm_tables.RowView(bit).At(wm_tables.RowView(bit).Size() - 1);
        ctx3p_.Rss().EvaluateSub(zerolr_sh.At(1), zerolr_sh.At(0), zerocount_sh);

        // Convert RSS to (2, 2)-sharing between P1 and P2 and Evaluate IntegerComparison
        uint64_t             ic_0{0}, ic_1{0};
        sharing::Rep3Share64 r1_sh, r2_sh;
        ctx3p_.Rss().Rand(r1_sh);
        ctx3p_.Rss().Rand(r2_sh);
        if (party_id == 1) {
            uint64_t k_0         = Mod2N(k_sh.data[0] + k_sh.data[1] + r1_sh.data[1], s);
            uint64_t zerocount_0 = Mod2N(zerocount_sh.data[0] + zerocount_sh.data[1] + r2_sh.data[1], s);
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
            Logger::DebugLog(LOC, party_str + " k_0: " + ToString(k_0) + ", zerocount_0: " + ToString(zerocount_0));
#endif
            ic_0 = ic_.EvaluateSharedInput(chls.next, key.ic_keys.GetView(bit), k_0, zerocount_0);
        } else if (party_id == 2) {
            uint64_t k_1         = Mod2N(k_sh.data[0] - r1_sh.data[0], s);
            uint64_t zerocount_1 = Mod2N(zerocount_sh.data[0] - r2_sh.data[0], s);
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
            Logger::DebugLog(LOC, party_str + " k_1: " + ToString(k_1) + ", zerocount_1: " + ToString(zerocount_1));
#endif
            ic_1 = ic_.EvaluateSharedInput(chls.prev, key.ic_keys.GetView(bit), k_1, zerocount_1);
        }

        // Convert (2, 2)-sharing to RSS
        if (party_id == 0) {
            ctx3p_.Rss().Rand(r1_sh);
            comp_sh[0] = Mod2N(r1_sh[1] - r1_sh[0], s);
        } else if (party_id == 1) {
            ctx3p_.Rss().Rand(r1_sh);
            comp_sh[0] = Mod2N(ic_0 + r1_sh[1] - r1_sh[0], s);
        } else if (party_id == 2) {
            ctx3p_.Rss().Rand(r1_sh);
            comp_sh[0] = Mod2N(ic_1 + r1_sh[1] - r1_sh[0], s);
        }
        chls.next.send(comp_sh[0]);
        chls.prev.recv(comp_sh[1]);

        // Update left_sh / right_sh and k_sh
        sharing::Rep3Share64 oneleft_sh(0, 0), oneright_sh(0, 0);
        ctx3p_.Rss().EvaluateAdd(total_zeros, left_sh, oneleft_sh);
        ctx3p_.Rss().EvaluateSub(oneleft_sh, zerolr_sh.At(0), oneleft_sh);
        ctx3p_.Rss().EvaluateAdd(total_zeros, right_sh, oneright_sh);
        ctx3p_.Rss().EvaluateSub(oneright_sh, zerolr_sh.At(1), oneright_sh);

        sharing::Rep3Share64 update_sh(0, 0);
        ctx3p_.Rss().EvaluateSub(k_sh, zerocount_sh, update_sh);

        sharing::Rep3ShareVec64 zerolrk_sh(3), onelrk_sh(3), select_sh(3);
        zerolrk_sh.Set(0, zerolr_sh.At(0));
        zerolrk_sh.Set(1, zerolr_sh.At(1));
        zerolrk_sh.Set(2, k_sh);
        onelrk_sh.Set(0, oneleft_sh);
        onelrk_sh.Set(1, oneright_sh);
        onelrk_sh.Set(2, update_sh);
        ctx3p_.Rss().EvaluateSelect(chls, zerolrk_sh, onelrk_sh, comp_sh, select_sh);
        left_sh  = select_sh.At(0);
        right_sh = select_sh.At(1);
        k_sh     = select_sh.At(2);

        // Update result
        sharing::Rep3Share64 cond_sh(0, 0);
        cond_sh[0] = Mod2N(comp_sh[0] * (1UL << bit), s);
        cond_sh[1] = Mod2N(comp_sh[1] * (1UL << bit), s);
        ctx3p_.Rss().EvaluateAdd(result, cond_sh, result);

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
        uint64_t total_zero_rec, zeroleft_rec, zeroright_rec, zerocount_rec;
        uint64_t comp_rec;
        uint64_t k_rec;
        uint64_t left_rec, right_rec;
        uint64_t result_rec;
        ctx3p_.Rss().Open(chls, total_zeros, total_zero_rec);
        ctx3p_.Rss().Open(chls, zerolr_sh.At(0), zeroleft_rec);
        ctx3p_.Rss().Open(chls, zerolr_sh.At(1), zeroright_rec);
        ctx3p_.Rss().Open(chls, zerocount_sh, zerocount_rec);
        ctx3p_.Rss().Open(chls, comp_sh, comp_rec);
        ctx3p_.Rss().Open(chls, k_sh, k_rec);
        ctx3p_.Rss().Open(chls, left_sh, left_rec);
        ctx3p_.Rss().Open(chls, right_sh, right_rec);
        ctx3p_.Rss().Open(chls, result, result_rec);
        Logger::DebugLog(LOC, party_str + "total_zero_rec: " + ToString(total_zero_rec));
        Logger::DebugLog(LOC, party_str + "zeroleft_rec: " + ToString(zeroleft_rec));
        Logger::DebugLog(LOC, party_str + "zeroright_rec: " + ToString(zeroright_rec));
        Logger::DebugLog(LOC, party_str + "zerocount_rec: " + ToString(zerocount_rec));
        Logger::DebugLog(LOC, party_str + "comp_rec: " + ToString(comp_rec));
        Logger::DebugLog(LOC, party_str + "k_rec: " + ToString(k_rec));
        Logger::DebugLog(LOC, party_str + "left_rec: " + ToString(left_rec));
        Logger::DebugLog(LOC, party_str + "right_rec: " + ToString(right_rec));
        Logger::DebugLog(LOC, party_str + "result_rec: " + ToString(result_rec));
#endif
    }
}

}    // namespace range
}    // namespace ringoa
