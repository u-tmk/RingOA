#include "obliv_fmi.h"

#include <cstring>

#include "RingOA/utils/bytes.h"
#include "RingOA/utils/logger.h"
#include "RingOA/utils/to_string.h"
#include "RingOA/utils/utils.h"
#include "RingOA/wm/plain_wm.h"

namespace ringoa {
namespace fmi {

void OblivFMIParameters::PrintParametersDebug(bool with_header, int key_width) const {
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    if (with_header) {
        Logger::DebugLog(LOC, Logger::MakeSeparatorLine());
        Logger::DebugLog(LOC, "[Oblivious FM-Index Params]");
        Logger::DebugLog(LOC, Logger::MakeSeparatorLine());
    }

    Logger::DebugLog(LOC, Logger::FormatKeyValue("DatabaseIndexBits", ToString(database_bitsize_), key_width));
    Logger::DebugLog(LOC, Logger::FormatKeyValue("DatabaseSize", ToString(database_size_), key_width));
    Logger::DebugLog(LOC, Logger::FormatKeyValue("Sigma", ToString(sigma_), key_width));
    Logger::DebugLog(LOC, Logger::FormatKeyValue("QuerySize", ToString(query_size_), key_width));
    Logger::DebugLog(LOC, Logger::FormatKeyValue("RankCalls", ToString(GetNumOfRankCalls()), key_width));
    Logger::DebugLog(LOC, Logger::FormatKeyValue("ZtCalls", ToString(GetNumOfZtCalls()), key_width));
    Logger::DebugLog(LOC, Logger::FormatKeyValue("ShareBits", ToString(share_bitsize_), key_width));

    orank_params_.PrintParametersDebug(false, key_width);
    zt_params_.PrintParametersDebug(false, key_width);

    if (with_header) {
        Logger::DebugLog(LOC, Logger::MakeSeparatorLine());
    }
#endif
}

OblivFMIKeys::OblivFMIKeys(const uint64_t id, const OblivFMIParameters &params)
    : rank_f_keys(id, params.GetOblivRankParameters(), params.GetNumOfRankCalls()),
      rank_g_keys(id, params.GetOblivRankParameters(), params.GetNumOfRankCalls()),
      zt_keys(id, params.GetZeroTestParameters(), params.GetNumOfZtCalls()) {
}

OblivFMIKeys::OblivFMIKeys(OblivRankKeys       &&rank_f_keys_in,
                           OblivRankKeys       &&rank_g_keys_in,
                           proto::ZeroTestKeys &&zt_keys_in)
    : rank_f_keys(std::move(rank_f_keys_in)),
      rank_g_keys(std::move(rank_g_keys_in)),
      zt_keys(std::move(zt_keys_in)) {
}

size_t OblivFMIKeys::CalculateSerializedSize() const {
    size_t size = 0;

    size += rank_f_keys.CalculateSerializedSize();
    size += rank_g_keys.CalculateSerializedSize();
    size += zt_keys.CalculateSerializedSize();
    return size;
}

void OblivFMIKeys::Serialize(std::vector<uint8_t> &buffer) const {
    const size_t start = buffer.size();

    // Serialize the rank_f_keys
    rank_f_keys.Serialize(buffer);

    // Serialize the rank_g_keys
    rank_g_keys.Serialize(buffer);

    // Serialize the ZT keys
    zt_keys.Serialize(buffer);

    // Check size
    const size_t written  = buffer.size() - start;
    const size_t expected = CalculateSerializedSize();
    if (written != expected) {
        throw std::runtime_error(LOC + " OblivFMIKeys::Serialize: serialized size mismatch: " +
                                 ToString(written) + " != " + ToString(expected));
    }
}

void OblivFMIKeys::Deserialize(const std::vector<uint8_t> &buffer, const OblivFMIParameters &params) {
    size_t offset = 0;
    Deserialize(buffer, params, offset);
}

void OblivFMIKeys::Deserialize(const std::vector<uint8_t> &buffer, const OblivFMIParameters &params, size_t &offset) {
    const size_t start = offset;

    // Deserialize the rank_f_keys
    rank_f_keys.Deserialize(buffer, params.GetOblivRankParameters(), offset);

    // Deserialize the rank_g_keys
    rank_g_keys.Deserialize(buffer, params.GetOblivRankParameters(), offset);

    // Deserialize the ZT keys
    zt_keys.Deserialize(buffer, params.GetZeroTestParameters(), offset);

    // Check size
    const size_t read     = offset - start;
    const size_t expected = CalculateSerializedSize();
    if (read != expected) {
        throw std::runtime_error(LOC + " OblivFMIKeys::Deserialize: deserialized size mismatch: " +
                                 ToString(read) + " != " + ToString(expected));
    }
}

OblivFMIKeys OblivFMIPreprocessData::ExtractKeys() && {
    return OblivFMIKeys(std::move(rank_f_data).ExtractKeys(),
                        std::move(rank_g_data).ExtractKeys(),
                        std::move(zt_keys));
}

size_t OblivFMIPreprocessData::CalculateSerializedSize() const {
    size_t size = 0;

    size += rank_f_data.CalculateSerializedSize();
    size += rank_g_data.CalculateSerializedSize();
    size += zt_keys.CalculateSerializedSize();
    return size;
}

void OblivFMIPreprocessData::LogSerializedSizeBreakdown(const std::string &prefix) const {
    const size_t rank_f_bytes = rank_f_data.CalculateSerializedSize();
    const size_t rank_g_bytes = rank_g_data.CalculateSerializedSize();
    const size_t zt_bytes     = zt_keys.CalculateSerializedSize();
    const size_t total_bytes  = rank_f_bytes + rank_g_bytes + zt_bytes;

    auto pct = [&](size_t x) -> double {
        if (total_bytes == 0)
            return 0.0;
        return 100.0 * static_cast<double>(x) / static_cast<double>(total_bytes);
    };

    const std::string p = prefix.empty() ? "" : (prefix + " ");

    Logger::DebugLog(LOC, p + "OblivFMIPreprocessData serialized size breakdown (bytes):");
    Logger::DebugLog(LOC, p + "  total      = " + ToString(total_bytes) + " bytes");
    Logger::DebugLog(LOC, p + "  rank_f_data = " + ToString(rank_f_bytes) + " bytes (" + ToString(pct(rank_f_bytes)) + "%)");
    Logger::DebugLog(LOC, p + "  rank_g_data = " + ToString(rank_g_bytes) + " bytes (" + ToString(pct(rank_g_bytes)) + "%)");
    Logger::DebugLog(LOC, p + "  zt_keys     = " + ToString(zt_bytes) + " bytes (" + ToString(pct(zt_bytes)) + "%)");
}

void OblivFMIPreprocessData::Serialize(std::vector<uint8_t> &buffer) const {
    const size_t start = buffer.size();

    // Serialize the rank_f_data
    rank_f_data.Serialize(buffer);

    // Serialize the rank_g_data
    rank_g_data.Serialize(buffer);

    // Serialize the ZT keys
    zt_keys.Serialize(buffer);

    // Check size
    const size_t written  = buffer.size() - start;
    const size_t expected = CalculateSerializedSize();
    if (written != expected) {
        throw std::runtime_error(LOC + " OblivFMIPreprocessData::Serialize: serialized size mismatch: " +
                                 ToString(written) + " != " + ToString(expected));
    }
}

void OblivFMIPreprocessData::Deserialize(const std::vector<uint8_t> &buffer, const OblivFMIParameters &params) {
    size_t offset = 0;
    Deserialize(buffer, params, offset);
}

void OblivFMIPreprocessData::Deserialize(const std::vector<uint8_t> &buffer, const OblivFMIParameters &params, size_t &offset) {
    const size_t start = offset;

    // Deserialize the rank_f_data
    rank_f_data.Deserialize(buffer, params.GetOblivRankParameters(), offset);

    // Deserialize the rank_g_data
    rank_g_data.Deserialize(buffer, params.GetOblivRankParameters(), offset);

    // Deserialize the ZT keys
    zt_keys.Deserialize(buffer, params.GetZeroTestParameters(), offset);

    // Check size
    const size_t read     = offset - start;
    const size_t expected = CalculateSerializedSize();
    if (read != expected) {
        throw std::runtime_error(LOC + " OblivFMIPreprocessData::Deserialize: deserialized size mismatch: " +
                                 ToString(read) + " != " + ToString(expected));
    }
}

OblivFMIFscKeys::OblivFMIFscKeys(const uint64_t id, const OblivFMIParameters &params)
    : rank_f_keys(id, params.GetOblivRankParameters(), params.GetNumOfRankCalls()),
      rank_g_keys(id, params.GetOblivRankParameters(), params.GetNumOfRankCalls()),
      zt_keys(id, params.GetZeroTestParameters(), params.GetNumOfZtCalls()) {
}

OblivFMIFscKeys::OblivFMIFscKeys(OblivRankFscKeys    &&rank_f_keys_in,
                                 OblivRankFscKeys    &&rank_g_keys_in,
                                 proto::ZeroTestKeys &&zt_keys_in)
    : rank_f_keys(std::move(rank_f_keys_in)),
      rank_g_keys(std::move(rank_g_keys_in)),
      zt_keys(std::move(zt_keys_in)) {
}

size_t OblivFMIFscKeys::CalculateSerializedSize() const {
    size_t size = 0;

    size += rank_f_keys.CalculateSerializedSize();
    size += rank_g_keys.CalculateSerializedSize();
    size += zt_keys.CalculateSerializedSize();
    return size;
}

void OblivFMIFscKeys::Serialize(std::vector<uint8_t> &buffer) const {
    const size_t start = buffer.size();

    // Serialize the rank_f_keys
    rank_f_keys.Serialize(buffer);

    // Serialize the rank_g_keys
    rank_g_keys.Serialize(buffer);

    // Serialize the ZT keys
    zt_keys.Serialize(buffer);

    // Check size
    const size_t written  = buffer.size() - start;
    const size_t expected = CalculateSerializedSize();
    if (written != expected) {
        throw std::runtime_error(LOC + " OblivFMIFscKeys::Serialize: serialized size mismatch: " +
                                 ToString(written) + " != " + ToString(expected));
    }
}

void OblivFMIFscKeys::Deserialize(const std::vector<uint8_t> &buffer, const OblivFMIParameters &params) {
    size_t offset = 0;
    Deserialize(buffer, params, offset);
}

void OblivFMIFscKeys::Deserialize(const std::vector<uint8_t> &buffer, const OblivFMIParameters &params, size_t &offset) {
    const size_t start = offset;

    // Deserialize the rank_f_keys
    rank_f_keys.Deserialize(buffer, params.GetOblivRankParameters(), offset);

    // Deserialize the rank_g_keys
    rank_g_keys.Deserialize(buffer, params.GetOblivRankParameters(), offset);

    // Deserialize the ZT keys
    zt_keys.Deserialize(buffer, params.GetZeroTestParameters(), offset);

    // Check size
    const size_t read     = offset - start;
    const size_t expected = CalculateSerializedSize();
    if (read != expected) {
        throw std::runtime_error(LOC + " OblivFMIFscKeys::Deserialize: deserialized size mismatch: " +
                                 ToString(read) + " != " + ToString(expected));
    }
}

OblivFMIFscKeys OblivFMIFscPreprocessData::ExtractKeys() && {
    return OblivFMIFscKeys(std::move(rank_f_data).ExtractKeys(),
                           std::move(rank_g_data).ExtractKeys(),
                           std::move(zt_keys));
}

size_t OblivFMIFscPreprocessData::CalculateSerializedSize() const {
    size_t size = 0;

    size += rank_f_data.CalculateSerializedSize();
    size += rank_g_data.CalculateSerializedSize();
    size += zt_keys.CalculateSerializedSize();
    return size;
}

void OblivFMIFscPreprocessData::LogSerializedSizeBreakdown(const std::string &prefix) const {
    const size_t rank_f_bytes = rank_f_data.CalculateSerializedSize();
    const size_t rank_g_bytes = rank_g_data.CalculateSerializedSize();
    const size_t zt_bytes     = zt_keys.CalculateSerializedSize();
    const size_t total_bytes  = rank_f_bytes + rank_g_bytes + zt_bytes;

    auto pct = [&](size_t x) -> double {
        if (total_bytes == 0)
            return 0.0;
        return 100.0 * static_cast<double>(x) / static_cast<double>(total_bytes);
    };

    const std::string p = prefix.empty() ? "" : (prefix + " ");

    Logger::DebugLog(LOC, p + "OblivFMIFscPreprocessData serialized size breakdown (bytes):");
    Logger::DebugLog(LOC, p + "  total      = " + ToString(total_bytes) + " bytes");
    Logger::DebugLog(LOC, p + "  rank_f_data = " + ToString(rank_f_bytes) + " bytes (" + ToString(pct(rank_f_bytes)) + "%)");
    Logger::DebugLog(LOC, p + "  rank_g_data = " + ToString(rank_g_bytes) + " bytes (" + ToString(pct(rank_g_bytes)) + "%)");
    Logger::DebugLog(LOC, p + "  zt_keys     = " + ToString(zt_bytes) + " bytes (" + ToString(pct(zt_bytes)) + "%)");
}

void OblivFMIFscPreprocessData::Serialize(std::vector<uint8_t> &buffer) const {
    const size_t start = buffer.size();

    // Serialize the rank_f_data
    rank_f_data.Serialize(buffer);

    // Serialize the rank_g_data
    rank_g_data.Serialize(buffer);

    // Serialize the ZT keys
    zt_keys.Serialize(buffer);

    // Check size
    const size_t written  = buffer.size() - start;
    const size_t expected = CalculateSerializedSize();
    if (written != expected) {
        throw std::runtime_error(LOC + " OblivFMIPreprocessData::Serialize: serialized size mismatch: " +
                                 ToString(written) + " != " + ToString(expected));
    }
}

void OblivFMIFscPreprocessData::Deserialize(const std::vector<uint8_t> &buffer, const OblivFMIParameters &params) {
    size_t offset = 0;
    Deserialize(buffer, params, offset);
}

void OblivFMIFscPreprocessData::Deserialize(const std::vector<uint8_t> &buffer, const OblivFMIParameters &params, size_t &offset) {
    const size_t start = offset;

    // Deserialize the rank_f_data
    rank_f_data.Deserialize(buffer, params.GetOblivRankParameters(), offset);

    // Deserialize the rank_g_data
    rank_g_data.Deserialize(buffer, params.GetOblivRankParameters(), offset);

    // Deserialize the ZT keys
    zt_keys.Deserialize(buffer, params.GetZeroTestParameters(), offset);

    // Check size
    const size_t read     = offset - start;
    const size_t expected = CalculateSerializedSize();
    if (read != expected) {
        throw std::runtime_error(LOC + " OblivFMIFscPreprocessData::Deserialize: deserialized size mismatch: " +
                                 ToString(read) + " != " + ToString(expected));
    }
}

OblivFMI::OblivFMI(
    const OblivFMIParameters &params,
    proto::ProtocolContext2P &ctx2p,
    proto::ProtocolContext3P &ctx3p)
    : params_(params),
      orank_(params.GetOblivRankParameters(), ctx3p),
      zt_(params.GetZeroTestParameters(), ctx2p),
      ctx2p_(ctx2p),
      ctx3p_(ctx3p) {
}

std::array<sharing::Rep3ShareMat64, 3> OblivFMI::GenerateDatabaseU64Share(const wm::FMIndex &fm) const {
    return orank_.GenerateDatabaseU64Share(fm);
}

void OblivFMI::GenerateDatabaseU64Share(
    const wm::FMIndex                      &fm,
    std::array<sharing::Rep3ShareMat64, 3> &db_sh,
    std::array<sharing::Rep3ShareVec64, 3> &aux_sh,
    std::array<bool, 3>                    &v_sign) const {
    orank_.GenerateDatabaseU64Share(fm, db_sh, aux_sh, v_sign);
}

std::array<sharing::Rep3ShareMat64, 3> OblivFMI::GenerateQueryU64Share(const wm::FMIndex &fm, std::string &query) const {
    std::vector<uint64_t> query_bv = fm.ConvertToBitMatrix(query);
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "Query bitvec: " + ToStringMatrix(query_bv, params_.GetQuerySize(), fm.GetWaveletMatrix().GetSigma()));
#endif
    return ctx3p_.Rss().ShareLocal(query_bv, params_.GetQuerySize(), fm.GetWaveletMatrix().GetSigma());
}

OblivFMIPreprocessData OblivFMI::Preprocess(Channels &chls) const {
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "Generate OblivFMI preprocess data");
#endif

    uint64_t party_id = chls.party_id;

    OblivRankNeighborMsg out_f = orank_.MakePreprocessMsg(params_.GetNumOfRankCalls());
    OblivRankNeighborMsg out_g = orank_.MakePreprocessMsg(params_.GetNumOfRankCalls());

    std::vector<uint8_t> to_prev, to_next;
    out_f.to_prev.Serialize(to_prev);
    out_f.to_next.Serialize(to_next);
    out_g.to_prev.Serialize(to_prev);
    out_g.to_next.Serialize(to_next);

    if (party_id == 0) {
        auto zt_key_pairs = zt_.GenerateKeys(params_.GetNumOfZtCalls());
        zt_key_pairs.first.Serialize(to_prev);
        zt_key_pairs.second.Serialize(to_next);
    }

    chls.prev.send(to_prev);
    chls.next.send(to_next);

    std::vector<uint8_t> in_prev, in_next;
    chls.prev.recv(in_prev);
    chls.next.recv(in_next);

    OblivRankNeighborMsgIn in_f, in_g;
    size_t                 offset_prev{0}, offset_next{0};
    in_f.from_prev.Deserialize(in_prev, params_.GetOblivRankParameters(), offset_prev);
    in_f.from_next.Deserialize(in_next, params_.GetOblivRankParameters(), offset_next);
    in_g.from_prev.Deserialize(in_prev, params_.GetOblivRankParameters(), offset_prev);
    in_g.from_next.Deserialize(in_next, params_.GetOblivRankParameters(), offset_next);

    if (party_id == 0) {
        return OblivFMIPreprocessData(
            orank_.BuildPreprocessData(party_id, params_.GetNumOfRankCalls(), std::move(in_f)),
            orank_.BuildPreprocessData(party_id, params_.GetNumOfRankCalls(), std::move(in_g)),
            proto::ZeroTestKeys{});
    } else if (party_id == 1) {
        proto::ZeroTestKeys zt_keys;
        zt_keys.Deserialize(in_prev, params_.GetZeroTestParameters(), offset_prev);
        return OblivFMIPreprocessData(
            orank_.BuildPreprocessData(party_id, params_.GetNumOfRankCalls(), std::move(in_f)),
            orank_.BuildPreprocessData(party_id, params_.GetNumOfRankCalls(), std::move(in_g)),
            std::move(zt_keys));
    } else if (party_id == 2) {
        proto::ZeroTestKeys zt_keys;
        zt_keys.Deserialize(in_next, params_.GetZeroTestParameters(), offset_next);
        return OblivFMIPreprocessData(
            orank_.BuildPreprocessData(party_id, params_.GetNumOfRankCalls(), std::move(in_f)),
            orank_.BuildPreprocessData(party_id, params_.GetNumOfRankCalls(), std::move(in_g)),
            std::move(zt_keys));
    } else {
        throw std::runtime_error(LOC + " OblivFMI::Preprocess: invalid party id: " + ToString(party_id));
    }
}

OblivFMIFscPreprocessData OblivFMI::PreprocessFsc(Channels &chls, bool v_sign) const {
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "Generate OblivFMIFsc preprocess data");
#endif

    uint64_t party_id = chls.party_id;

    OblivRankFscNeighborMsg out_f = orank_.MakePreprocessMsgFsc(params_.GetNumOfRankCalls(), v_sign);
    OblivRankFscNeighborMsg out_g = orank_.MakePreprocessMsgFsc(params_.GetNumOfRankCalls(), v_sign);

    std::vector<uint8_t> to_prev, to_next;
    out_f.to_prev.Serialize(to_prev);
    out_f.to_next.Serialize(to_next);
    out_g.to_prev.Serialize(to_prev);
    out_g.to_next.Serialize(to_next);

    if (party_id == 0) {
        auto zt_key_pairs = zt_.GenerateKeys(params_.GetNumOfZtCalls());
        zt_key_pairs.first.Serialize(to_prev);
        zt_key_pairs.second.Serialize(to_next);
    }

    chls.prev.send(to_prev);
    chls.next.send(to_next);

    std::vector<uint8_t> in_prev, in_next;
    chls.prev.recv(in_prev);
    chls.next.recv(in_next);

    OblivRankFscNeighborMsgIn in_f, in_g;
    size_t                    offset_prev{0}, offset_next{0};
    in_f.from_prev.Deserialize(in_prev, params_.GetOblivRankParameters(), offset_prev);
    in_f.from_next.Deserialize(in_next, params_.GetOblivRankParameters(), offset_next);
    in_g.from_prev.Deserialize(in_prev, params_.GetOblivRankParameters(), offset_prev);
    in_g.from_next.Deserialize(in_next, params_.GetOblivRankParameters(), offset_next);

    if (party_id == 0) {
        return OblivFMIFscPreprocessData(
            orank_.BuildPreprocessDataFsc(party_id, params_.GetNumOfRankCalls(), std::move(in_f)),
            orank_.BuildPreprocessDataFsc(party_id, params_.GetNumOfRankCalls(), std::move(in_g)),
            proto::ZeroTestKeys{});
    } else if (party_id == 1) {
        proto::ZeroTestKeys zt_keys;
        zt_keys.Deserialize(in_prev, params_.GetZeroTestParameters(), offset_prev);
        return OblivFMIFscPreprocessData(
            orank_.BuildPreprocessDataFsc(party_id, params_.GetNumOfRankCalls(), std::move(in_f)),
            orank_.BuildPreprocessDataFsc(party_id, params_.GetNumOfRankCalls(), std::move(in_g)),
            std::move(zt_keys));
    } else if (party_id == 2) {
        proto::ZeroTestKeys zt_keys;
        zt_keys.Deserialize(in_next, params_.GetZeroTestParameters(), offset_next);
        return OblivFMIFscPreprocessData(
            orank_.BuildPreprocessDataFsc(party_id, params_.GetNumOfRankCalls(), std::move(in_f)),
            orank_.BuildPreprocessDataFsc(party_id, params_.GetNumOfRankCalls(), std::move(in_g)),
            std::move(zt_keys));
    } else {
        throw std::runtime_error(LOC + " OblivFMI::Preprocess: invalid party id: " + ToString(party_id));
    }
}

void OblivFMI::ConsumePreprocessData(OblivFMIPreprocessData &data) const {
    orank_.ConsumePreprocessData(data.rank_f_data);
    orank_.ConsumePreprocessData(data.rank_g_data);
}

void OblivFMI::EvaluateLPM(Channels                      &chls,
                           const OblivFMIKeys            &key,
                           std::vector<block>            &uv_prev,
                           std::vector<block>            &uv_next,
                           const sharing::Rep3ShareMat64 &wm_tables,
                           const sharing::Rep3ShareMat64 &query,
                           sharing::Rep3ShareVec64       &result) const {

    uint64_t d        = params_.GetDbIndexBits();
    uint64_t ds       = params_.GetDatabaseSize();
    uint64_t qs       = params_.GetQuerySize();
    uint64_t k        = params_.GetShareBitsize();
    uint64_t sigma    = params_.GetSigma();
    uint64_t party_id = chls.party_id;

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "Evaluate OblivFMI key");
    Logger::DebugLog(LOC, "Database bit size: " + ToString(d));
    Logger::DebugLog(LOC, "Database size: " + ToString(ds));
    Logger::DebugLog(LOC, "Query size: " + ToString(qs));
    Logger::DebugLog(LOC, "Share size: " + ToString(k));
    Logger::DebugLog(LOC, "Sigma: " + ToString(sigma));
    std::string party_str = "[P" + ToString(party_id) + "] ";
#endif

    sharing::Rep3Share64    f_sh(0, 0), g_sh(0, 0);
    sharing::Rep3Share64    f_next_sh(0, 0), g_next_sh(0, 0);
    sharing::Rep3ShareVec64 interval_sh(qs);

    if (party_id == 0) {
        g_sh.data[0] = wm_tables.RowView(0).Size() - 1;
    } else if (party_id == 1) {
        g_sh.data[1] = wm_tables.RowView(0).Size() - 1;
    }

    for (uint64_t i = 0; i < qs; ++i) {
        orank_.EvaluateRankCF(chls, key.rank_f_keys.GetView(i), uv_prev, uv_next, wm_tables, query.RowView(i), f_sh, f_next_sh);
        orank_.EvaluateRankCF(chls, key.rank_g_keys.GetView(i), uv_prev, uv_next, wm_tables, query.RowView(i), g_sh, g_next_sh);
        f_sh = f_next_sh;
        g_sh = g_next_sh;
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
        uint64_t f, g;
        ctx3p_.Rss().Open(chls, f_sh, f);
        ctx3p_.Rss().Open(chls, g_sh, g);
        Logger::InfoLog(LOC, party_str + "f(" + ToString(i) + "): " + ToString(f));
        Logger::InfoLog(LOC, party_str + "g(" + ToString(i) + "): " + ToString(g));
#endif
        sharing::Rep3Share64 fg_sub_sh;
        ctx3p_.Rss().EvaluateSub(g_sh, f_sh, fg_sub_sh);
        interval_sh.Set(i, fg_sub_sh);
    }
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    std::vector<uint64_t> interval;
    ctx3p_.Rss().Open(chls, interval_sh, interval);
    Logger::DebugLog(LOC, party_str + "Interval: " + ToString(interval));
#endif

    // Convert RSS to (2, 2)-sharing between P1 and P2 and Evaluate ZeroTest
    std::vector<uint64_t> masked_intervals_0(qs), masked_intervals_1(qs), masked_intervals(qs);
    std::vector<uint64_t> zt_0(qs), zt_1(qs), recon_zt(qs);
    sharing::Rep3Share64  r_sh;
    ctx3p_.Rss().Rand(r_sh);
    if (party_id == 1) {
        for (uint64_t i = 0; i < qs; ++i) {
            uint64_t interval_0 = Mod2N(interval_sh.data[0][i] + interval_sh.data[1][i] + r_sh.data[1], k);
            uint64_t masked_interval_0;
            ctx3p_.AssWithNext().EvaluateAdd(interval_0, key.zt_keys.GetView(i).shr_in, masked_interval_0);
            masked_intervals_0[i] = masked_interval_0;
        }
        ctx3p_.AssWithNext().Reconst(0, chls.next, masked_intervals_0, masked_intervals_1, masked_intervals);
        for (uint64_t i = 0; i < qs; ++i) {
            zt_0[i] = zt_.EvaluateMaskedInput(key.zt_keys.GetView(i), masked_intervals[i]);
        }
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
        ctx3p_.AssWithNext().Reconst(0, chls.next, zt_0, zt_1, recon_zt);
        Logger::DebugLog(LOC, party_str + "Reconstructed ZT: " + ToString(recon_zt));
#endif
    } else if (party_id == 2) {
        for (uint64_t i = 0; i < qs; ++i) {
            uint64_t interval_1 = Mod2N(interval_sh.data[0][i] - r_sh.data[0], k);
            uint64_t masked_interval_1;
            ctx3p_.AssWithPrev().EvaluateAdd(interval_1, key.zt_keys.GetView(i).shr_in, masked_interval_1);
            masked_intervals_1[i] = masked_interval_1;
        }
        ctx3p_.AssWithPrev().Reconst(1, chls.prev, masked_intervals_0, masked_intervals_1, masked_intervals);
        for (uint64_t i = 0; i < qs; ++i) {
            zt_1[i] = zt_.EvaluateMaskedInput(key.zt_keys.GetView(i), masked_intervals[i]);
        }
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
        ctx3p_.AssWithPrev().Reconst(1, chls.prev, zt_0, zt_1, recon_zt);
        Logger::DebugLog(LOC, party_str + "Reconstructed ZT: " + ToString(recon_zt));
#endif
    }

    // Convert (2, 2)-sharing to RSS
    if (party_id == 0) {
        for (uint64_t i = 0; i < qs; ++i) {
            ctx3p_.Rss().Rand(r_sh);
            result[0][i] = Mod2N(r_sh.data[1] - r_sh.data[0], k);
        }
    } else if (party_id == 1) {
        for (uint64_t i = 0; i < qs; ++i) {
            ctx3p_.Rss().Rand(r_sh);
            result[0][i] = Mod2N(zt_0[i] + r_sh.data[1] - r_sh.data[0], k);
        }
    } else {
        for (uint64_t i = 0; i < qs; ++i) {
            ctx3p_.Rss().Rand(r_sh);
            result[0][i] = Mod2N(zt_1[i] + r_sh.data[1] - r_sh.data[0], k);
        }
    }
    chls.next.send(result[0]);
    chls.prev.recv(result[1]);
}

void OblivFMI::EvaluateLPMPair(Channels                      &chls,
                               const OblivFMIKeys            &key,
                               std::vector<block>            &uv_prev,
                               std::vector<block>            &uv_next,
                               const sharing::Rep3ShareMat64 &wm_tables,
                               const sharing::Rep3ShareMat64 &query,
                               sharing::Rep3ShareVec64       &result) const {

    uint64_t d        = params_.GetDbIndexBits();
    uint64_t ds       = params_.GetDatabaseSize();
    uint64_t qs       = params_.GetQuerySize();
    uint64_t k        = params_.GetShareBitsize();
    uint64_t sigma    = params_.GetSigma();
    uint64_t party_id = chls.party_id;

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "Evaluate OblivFMI key");
    Logger::DebugLog(LOC, "Database bit size: " + ToString(d));
    Logger::DebugLog(LOC, "Database size: " + ToString(ds));
    Logger::DebugLog(LOC, "Query size: " + ToString(qs));
    Logger::DebugLog(LOC, "Share size: " + ToString(k));
    Logger::DebugLog(LOC, "Sigma: " + ToString(sigma));
    std::string party_str = "[P" + ToString(party_id) + "] ";
#endif

    sharing::Rep3ShareVec64 fg_sh(2);
    sharing::Rep3ShareVec64 fg_next_sh(2);
    sharing::Rep3ShareVec64 interval_sh(qs);

    if (party_id == 0) {
        fg_sh.data[0][1] = wm_tables.RowView(0).Size() - 1;
    } else if (party_id == 1) {
        fg_sh.data[1][1] = wm_tables.RowView(0).Size() - 1;
    }

    for (uint64_t i = 0; i < qs; ++i) {
        orank_.EvaluateRankCFPair(chls, key.rank_f_keys.GetView(i), key.rank_g_keys.GetView(i),
                                  uv_prev, uv_next, wm_tables,
                                  query.RowView(i), fg_sh, fg_next_sh);
        fg_sh = fg_next_sh;
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
        std::vector<uint64_t> fg(2);
        ctx3p_.Rss().Open(chls, fg_sh, fg);
        Logger::InfoLog(LOC, party_str + "f(" + ToString(i) + "): " + ToString(fg[0]));
        Logger::InfoLog(LOC, party_str + "g(" + ToString(i) + "): " + ToString(fg[1]));
#endif
        sharing::Rep3Share64 fg_sub_sh;
        ctx3p_.Rss().EvaluateSub(fg_sh.At(1), fg_sh.At(0), fg_sub_sh);
        interval_sh.Set(i, fg_sub_sh);
    }
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    std::vector<uint64_t> interval;
    ctx3p_.Rss().Open(chls, interval_sh, interval);
    Logger::DebugLog(LOC, party_str + "Interval: " + ToString(interval));
#endif

    // Convert RSS to (2, 2)-sharing between P1 and P2 and Evaluate ZeroTest
    std::vector<uint64_t> masked_intervals_0(qs), masked_intervals_1(qs), masked_intervals(qs);
    std::vector<uint64_t> zt_0(qs), zt_1(qs), recon_zt(qs);
    sharing::Rep3Share64  r_sh;
    ctx3p_.Rss().Rand(r_sh);
    if (party_id == 1) {
        for (uint64_t i = 0; i < qs; ++i) {
            uint64_t interval_0 = Mod2N(interval_sh.data[0][i] + interval_sh.data[1][i] + r_sh.data[1], k);
            uint64_t masked_interval_0;
            ctx3p_.AssWithNext().EvaluateAdd(interval_0, key.zt_keys.GetView(i).shr_in, masked_interval_0);
            masked_intervals_0[i] = masked_interval_0;
        }
        ctx3p_.AssWithNext().Reconst(0, chls.next, masked_intervals_0, masked_intervals_1, masked_intervals);
        for (uint64_t i = 0; i < qs; ++i) {
            zt_0[i] = zt_.EvaluateMaskedInput(key.zt_keys.GetView(i), masked_intervals[i]);
        }
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
        ctx3p_.AssWithNext().Reconst(0, chls.next, zt_0, zt_1, recon_zt);
        Logger::DebugLog(LOC, party_str + "Reconstructed ZT: " + ToString(recon_zt));
#endif
    } else if (party_id == 2) {
        for (uint64_t i = 0; i < qs; ++i) {
            uint64_t interval_1 = Mod2N(interval_sh.data[0][i] - r_sh.data[0], k);
            uint64_t masked_interval_1;
            ctx3p_.AssWithPrev().EvaluateAdd(interval_1, key.zt_keys.GetView(i).shr_in, masked_interval_1);
            masked_intervals_1[i] = masked_interval_1;
        }
        ctx3p_.AssWithPrev().Reconst(1, chls.prev, masked_intervals_0, masked_intervals_1, masked_intervals);
        for (uint64_t i = 0; i < qs; ++i) {
            zt_1[i] = zt_.EvaluateMaskedInput(key.zt_keys.GetView(i), masked_intervals[i]);
        }
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
        ctx3p_.AssWithPrev().Reconst(1, chls.prev, zt_0, zt_1, recon_zt);
        Logger::DebugLog(LOC, party_str + "Reconstructed ZT: " + ToString(recon_zt));
#endif
    }

    // Convert (2, 2)-sharing to RSS
    if (party_id == 0) {
        for (uint64_t i = 0; i < qs; ++i) {
            ctx3p_.Rss().Rand(r_sh);
            result[0][i] = Mod2N(r_sh.data[1] - r_sh.data[0], k);
        }
    } else if (party_id == 1) {
        for (uint64_t i = 0; i < qs; ++i) {
            ctx3p_.Rss().Rand(r_sh);
            result[0][i] = Mod2N(zt_0[i] + r_sh.data[1] - r_sh.data[0], k);
        }
    } else {
        for (uint64_t i = 0; i < qs; ++i) {
            ctx3p_.Rss().Rand(r_sh);
            result[0][i] = Mod2N(zt_1[i] + r_sh.data[1] - r_sh.data[0], k);
        }
    }
    chls.next.send(result[0]);
    chls.prev.recv(result[1]);
}

void OblivFMI::EvaluateLPMFsc(Channels                       &chls,
                              const OblivFMIFscKeys          &key,
                              std::vector<block>             &uv_prev,
                              std::vector<block>             &uv_next,
                              const sharing::Rep3ShareMat64  &wm_tables,
                              const sharing::Rep3ShareView64 &aux_sh,
                              const sharing::Rep3ShareMat64  &query,
                              sharing::Rep3ShareVec64        &result) const {

    uint64_t d        = params_.GetDbIndexBits();
    uint64_t ds       = params_.GetDatabaseSize();
    uint64_t qs       = params_.GetQuerySize();
    uint64_t k        = params_.GetShareBitsize();
    uint64_t sigma    = params_.GetSigma();
    uint64_t party_id = chls.party_id;

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "Evaluate OblivFMIFsc key");
    Logger::DebugLog(LOC, "Database bit size: " + ToString(d));
    Logger::DebugLog(LOC, "Database size: " + ToString(ds));
    Logger::DebugLog(LOC, "Query size: " + ToString(qs));
    Logger::DebugLog(LOC, "Share size: " + ToString(k));
    Logger::DebugLog(LOC, "Sigma: " + ToString(sigma));
    std::string party_str = "[P" + ToString(party_id) + "] ";
#endif

    sharing::Rep3Share64    f_sh(0, 0), g_sh(0, 0);
    sharing::Rep3Share64    f_next_sh(0, 0), g_next_sh(0, 0);
    sharing::Rep3ShareVec64 interval_sh(qs);

    if (party_id == 0) {
        g_sh.data[0] = wm_tables.RowView(0).Size() - 1;
    } else if (party_id == 1) {
        g_sh.data[1] = wm_tables.RowView(0).Size() - 1;
    }

    for (uint64_t i = 0; i < qs; ++i) {
        orank_.EvaluateRankCFFsc(chls, key.rank_f_keys.GetView(i), uv_prev, uv_next, wm_tables,
                                 aux_sh, query.RowView(i), f_sh, f_next_sh);
        orank_.EvaluateRankCFFsc(chls, key.rank_g_keys.GetView(i), uv_prev, uv_next, wm_tables,
                                 aux_sh, query.RowView(i), g_sh, g_next_sh);
        f_sh = f_next_sh;
        g_sh = g_next_sh;
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
        uint64_t f, g;
        ctx3p_.Rss().Open(chls, f_sh, f);
        ctx3p_.Rss().Open(chls, g_sh, g);
        Logger::InfoLog(LOC, party_str + "f(" + ToString(i) + "): " + ToString(f));
        Logger::InfoLog(LOC, party_str + "g(" + ToString(i) + "): " + ToString(g));
#endif
        sharing::Rep3Share64 fg_sub_sh;
        ctx3p_.Rss().EvaluateSub(g_sh, f_sh, fg_sub_sh);
        interval_sh.Set(i, fg_sub_sh);
    }
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    std::vector<uint64_t> interval;
    ctx3p_.Rss().Open(chls, interval_sh, interval);
    Logger::DebugLog(LOC, party_str + "Interval: " + ToString(interval));
#endif

    // Convert RSS to (2, 2)-sharing between P1 and P2 and Evaluate ZeroTest
    std::vector<uint64_t> masked_intervals_0(qs), masked_intervals_1(qs), masked_intervals(qs);
    std::vector<uint64_t> zt_0(qs), zt_1(qs), recon_zt(qs);
    sharing::Rep3Share64  r_sh;
    ctx3p_.Rss().Rand(r_sh);
    if (party_id == 1) {
        for (uint64_t i = 0; i < qs; ++i) {
            uint64_t interval_0 = Mod2N(interval_sh.data[0][i] + interval_sh.data[1][i] + r_sh.data[1], k);
            uint64_t masked_interval_0;
            ctx3p_.AssWithNext().EvaluateAdd(interval_0, key.zt_keys.GetView(i).shr_in, masked_interval_0);
            masked_intervals_0[i] = masked_interval_0;
        }
        ctx3p_.AssWithNext().Reconst(0, chls.next, masked_intervals_0, masked_intervals_1, masked_intervals);
        for (uint64_t i = 0; i < qs; ++i) {
            zt_0[i] = zt_.EvaluateMaskedInput(key.zt_keys.GetView(i), masked_intervals[i]);
        }
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
        ctx3p_.AssWithNext().Reconst(0, chls.next, zt_0, zt_1, recon_zt);
        Logger::DebugLog(LOC, party_str + "Reconstructed ZT: " + ToString(recon_zt));
#endif
    } else if (party_id == 2) {
        for (uint64_t i = 0; i < qs; ++i) {
            uint64_t interval_1 = Mod2N(interval_sh.data[0][i] - r_sh.data[0], k);
            uint64_t masked_interval_1;
            ctx3p_.AssWithPrev().EvaluateAdd(interval_1, key.zt_keys.GetView(i).shr_in, masked_interval_1);
            masked_intervals_1[i] = masked_interval_1;
        }
        ctx3p_.AssWithPrev().Reconst(1, chls.prev, masked_intervals_0, masked_intervals_1, masked_intervals);
        for (uint64_t i = 0; i < qs; ++i) {
            zt_1[i] = zt_.EvaluateMaskedInput(key.zt_keys.GetView(i), masked_intervals[i]);
        }
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
        ctx3p_.AssWithPrev().Reconst(1, chls.prev, zt_0, zt_1, recon_zt);
        Logger::DebugLog(LOC, party_str + "Reconstructed ZT: " + ToString(recon_zt));
#endif
    }

    // Convert (2, 2)-sharing to RSS
    if (party_id == 0) {
        for (uint64_t i = 0; i < qs; ++i) {
            ctx3p_.Rss().Rand(r_sh);
            result[0][i] = Mod2N(r_sh.data[1] - r_sh.data[0], k);
        }
    } else if (party_id == 1) {
        for (uint64_t i = 0; i < qs; ++i) {
            ctx3p_.Rss().Rand(r_sh);
            result[0][i] = Mod2N(zt_0[i] + r_sh.data[1] - r_sh.data[0], k);
        }
    } else {
        for (uint64_t i = 0; i < qs; ++i) {
            ctx3p_.Rss().Rand(r_sh);
            result[0][i] = Mod2N(zt_1[i] + r_sh.data[1] - r_sh.data[0], k);
        }
    }
    chls.next.send(result[0]);
    chls.prev.recv(result[1]);
}

void OblivFMI::EvaluateLPMFscPair(Channels                       &chls,
                                  const OblivFMIFscKeys          &key,
                                  std::vector<block>             &uv_prev,
                                  std::vector<block>             &uv_next,
                                  const sharing::Rep3ShareMat64  &wm_tables,
                                  const sharing::Rep3ShareView64 &aux_sh,
                                  const sharing::Rep3ShareMat64  &query,
                                  sharing::Rep3ShareVec64        &result) const {

    uint64_t d        = params_.GetDbIndexBits();
    uint64_t ds       = params_.GetDatabaseSize();
    uint64_t qs       = params_.GetQuerySize();
    uint64_t k        = params_.GetShareBitsize();
    uint64_t sigma    = params_.GetSigma();
    uint64_t party_id = chls.party_id;

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "Evaluate OblivFMIFsc key");
    Logger::DebugLog(LOC, "Database bit size: " + ToString(d));
    Logger::DebugLog(LOC, "Database size: " + ToString(ds));
    Logger::DebugLog(LOC, "Query size: " + ToString(qs));
    Logger::DebugLog(LOC, "Share size: " + ToString(k));
    Logger::DebugLog(LOC, "Sigma: " + ToString(sigma));
    std::string party_str = "[P" + ToString(party_id) + "] ";
#endif

    sharing::Rep3ShareVec64 fg_sh(2);
    sharing::Rep3ShareVec64 fg_next_sh(2);
    sharing::Rep3ShareVec64 interval_sh(qs);

    if (party_id == 0) {
        fg_sh.data[0][1] = wm_tables.RowView(0).Size() - 1;
    } else if (party_id == 1) {
        fg_sh.data[1][1] = wm_tables.RowView(0).Size() - 1;
    }

    for (uint64_t i = 0; i < qs; ++i) {
        orank_.EvaluateRankCFFscPair(chls, key.rank_f_keys.GetView(i), key.rank_g_keys.GetView(i),
                                     uv_prev, uv_next, wm_tables, aux_sh,
                                     query.RowView(i), fg_sh, fg_next_sh);
        fg_sh = fg_next_sh;
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
        std::vector<uint64_t> fg(2);
        ctx3p_.Rss().Open(chls, fg_sh, fg);
        Logger::InfoLog(LOC, party_str + "f(" + ToString(i) + "): " + ToString(fg[0]));
        Logger::InfoLog(LOC, party_str + "g(" + ToString(i) + "): " + ToString(fg[1]));
#endif
        sharing::Rep3Share64 fg_sub_sh;
        ctx3p_.Rss().EvaluateSub(fg_sh.At(1), fg_sh.At(0), fg_sub_sh);
        interval_sh.Set(i, fg_sub_sh);
    }
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    std::vector<uint64_t> interval;
    ctx3p_.Rss().Open(chls, interval_sh, interval);
    Logger::DebugLog(LOC, party_str + "Interval: " + ToString(interval));
#endif

    // Convert RSS to (2, 2)-sharing between P1 and P2 and Evaluate ZeroTest
    std::vector<uint64_t> masked_intervals_0(qs), masked_intervals_1(qs), masked_intervals(qs);
    std::vector<uint64_t> zt_0(qs), zt_1(qs), recon_zt(qs);
    sharing::Rep3Share64  r_sh;
    ctx3p_.Rss().Rand(r_sh);
    if (party_id == 1) {
        for (uint64_t i = 0; i < qs; ++i) {
            uint64_t interval_0 = Mod2N(interval_sh.data[0][i] + interval_sh.data[1][i] + r_sh.data[1], k);
            uint64_t masked_interval_0;
            ctx3p_.AssWithNext().EvaluateAdd(interval_0, key.zt_keys.GetView(i).shr_in, masked_interval_0);
            masked_intervals_0[i] = masked_interval_0;
        }
        ctx3p_.AssWithNext().Reconst(0, chls.next, masked_intervals_0, masked_intervals_1, masked_intervals);
        for (uint64_t i = 0; i < qs; ++i) {
            zt_0[i] = zt_.EvaluateMaskedInput(key.zt_keys.GetView(i), masked_intervals[i]);
        }
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
        ctx3p_.AssWithNext().Reconst(0, chls.next, zt_0, zt_1, recon_zt);
        Logger::DebugLog(LOC, party_str + "Reconstructed ZT: " + ToString(recon_zt));
#endif
    } else if (party_id == 2) {
        for (uint64_t i = 0; i < qs; ++i) {
            uint64_t interval_1 = Mod2N(interval_sh.data[0][i] - r_sh.data[0], k);
            uint64_t masked_interval_1;
            ctx3p_.AssWithPrev().EvaluateAdd(interval_1, key.zt_keys.GetView(i).shr_in, masked_interval_1);
            masked_intervals_1[i] = masked_interval_1;
        }
        ctx3p_.AssWithPrev().Reconst(1, chls.prev, masked_intervals_0, masked_intervals_1, masked_intervals);
        for (uint64_t i = 0; i < qs; ++i) {
            zt_1[i] = zt_.EvaluateMaskedInput(key.zt_keys.GetView(i), masked_intervals[i]);
        }
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
        ctx3p_.AssWithPrev().Reconst(1, chls.prev, zt_0, zt_1, recon_zt);
        Logger::DebugLog(LOC, party_str + "Reconstructed ZT: " + ToString(recon_zt));
#endif
    }

    // Convert (2, 2)-sharing to RSS
    if (party_id == 0) {
        for (uint64_t i = 0; i < qs; ++i) {
            ctx3p_.Rss().Rand(r_sh);
            result[0][i] = Mod2N(r_sh.data[1] - r_sh.data[0], k);
        }
    } else if (party_id == 1) {
        for (uint64_t i = 0; i < qs; ++i) {
            ctx3p_.Rss().Rand(r_sh);
            result[0][i] = Mod2N(zt_0[i] + r_sh.data[1] - r_sh.data[0], k);
        }
    } else {
        for (uint64_t i = 0; i < qs; ++i) {
            ctx3p_.Rss().Rand(r_sh);
            result[0][i] = Mod2N(zt_1[i] + r_sh.data[1] - r_sh.data[0], k);
        }
    }
    chls.next.send(result[0]);
    chls.prev.recv(result[1]);
}

}    // namespace fmi
}    // namespace ringoa
