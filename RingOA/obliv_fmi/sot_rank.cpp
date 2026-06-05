#include "sot_rank.h"

#include <cstring>

#include "RingOA/utils/bytes.h"
#include "RingOA/utils/logger.h"
#include "RingOA/utils/to_string.h"
#include "RingOA/utils/utils.h"
#include "RingOA/wm/plain_wm.h"

namespace ringoa {
namespace fmi {

using ringoa::sharing::ReplicatedSharing3P;

void SotRankParameters::PrintParametersDebug(bool with_header, int key_width) const {
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    if (with_header) {
        Logger::DebugLog(LOC, Logger::MakeSeparatorLine());
        Logger::DebugLog(LOC, "[SotRank Params]");
        Logger::DebugLog(LOC, Logger::MakeSeparatorLine());
    }

    Logger::DebugLog(LOC, Logger::FormatKeyValue("DatabaseIndexBits", ToString(database_bitsize_), key_width));
    Logger::DebugLog(LOC, Logger::FormatKeyValue("DatabaseSize", ToString(database_size_), key_width));
    Logger::DebugLog(LOC, Logger::FormatKeyValue("Sigma", ToString(sigma_), key_width));
    Logger::DebugLog(LOC, Logger::FormatKeyValue("OaCalls", ToString(GetNumOfOaCalls()), key_width));
    Logger::DebugLog(LOC, Logger::FormatKeyValue("ShareBits", ToString(share_bitsize_), key_width));

    params_.PrintParametersDebug(false, key_width);

    if (with_header) {
        Logger::DebugLog(LOC, Logger::MakeSeparatorLine());
    }
#endif
}
SotRankKeys::SotRankKeys(
    const uint64_t           id,
    const SotRankParameters &params,
    size_t                   count) {
    oa_keys.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        oa_keys.emplace_back(id, params.GetParameters(), params.GetNumOfOaCalls());
    }
}

size_t SotRankKeys::CalculateSerializedSize() const {
    size_t size = 0;

    for (const auto &key : oa_keys) {
        size += key.CalculateSerializedSize();
    }
    return size;
}

void SotRankKeys::Serialize(std::vector<uint8_t> &buffer) const {
    const size_t start = buffer.size();

    // Number of keys
    const size_t num = oa_keys.size();
    append_pod(buffer, static_cast<uint64_t>(num));

    // Serialize each key
    for (const auto &key : oa_keys) {
        key.Serialize(buffer);
    }

    // Check size
    const size_t written  = buffer.size() - start;
    const size_t expected = CalculateSerializedSize();
    if (written != expected) {
        throw std::runtime_error(LOC + " SotRankKeys::Serialize: serialized size mismatch: " +
                                 ToString(written) + " != " + ToString(expected));
    }
}

void SotRankKeys::Deserialize(const std::vector<uint8_t> &buffer, const SotRankParameters &params) {
    size_t offset = 0;
    Deserialize(buffer, params, offset);
}

void SotRankKeys::Deserialize(const std::vector<uint8_t> &buffer, const SotRankParameters &params, size_t &offset) {
    const size_t start = offset;

    // Number of keys
    uint64_t num = 0;
    read_pod(buffer, offset, num);

    // Reset containers
    oa_keys.clear();

    oa_keys.reserve(num);

    // Deserialize the OA keys
    for (size_t i = 0; i < num; ++i) {
        oa_keys.emplace_back(0, params.GetParameters(), params.GetNumOfOaCalls());
        oa_keys.back().Deserialize(buffer, params.GetParameters(), offset);
    }

    // Check size
    const size_t read     = offset - start;
    const size_t expected = CalculateSerializedSize();
    if (read != expected) {
        throw std::runtime_error(LOC + " SotRankKeys::Deserialize: deserialized size mismatch: " +
                                 ToString(read) + " != " + ToString(expected));
    }
}

size_t SotRankPreprocessMsg::CalculateSerializedSize() const {
    size_t size = 0;
    size += sizeof(uint64_t);    // number of messages
    for (const auto &msg : oa_msg) {
        size += msg.CalculateSerializedSize();
    }
    return size;
}

void SotRankPreprocessMsg::Serialize(std::vector<uint8_t> &buffer) const {
    const size_t start = buffer.size();

    // Number of messages
    const size_t num = oa_msg.size();
    append_pod(buffer, static_cast<uint64_t>(num));

    // Serialize each message
    for (const auto &msg : oa_msg) {
        msg.Serialize(buffer);
    }

    // Check size
    const size_t written  = buffer.size() - start;
    const size_t expected = CalculateSerializedSize();
    if (written != expected) {
        throw std::runtime_error(LOC + " SotRankPreprocessMsg::Serialize: serialized size mismatch: " +
                                 ToString(written) + " != " + ToString(expected));
    }
}

void SotRankPreprocessMsg::Deserialize(const std::vector<uint8_t> &buffer, const SotRankParameters &params) {
    size_t offset = 0;
    Deserialize(buffer, params, offset);
}

void SotRankPreprocessMsg::Deserialize(const std::vector<uint8_t> &buffer, const SotRankParameters &params, size_t &offset) {
    const size_t start = offset;

    // Number of messages
    uint64_t num = 0;
    read_pod(buffer, offset, num);

    // Reset containers
    oa_msg.clear();

    oa_msg.reserve(num);

    // Deserialize each message
    for (size_t i = 0; i < num; ++i) {
        oa_msg.emplace_back();
        oa_msg.back().Deserialize(buffer, params.GetParameters(), offset);
    }

    // Check size
    const size_t read     = offset - start;
    const size_t expected = CalculateSerializedSize();
    if (read != expected) {
        throw std::runtime_error(LOC + " SotRankPreprocessMsg::Deserialize: deserialized size mismatch: " +
                                 ToString(read) + " != " + ToString(expected));
    }
}

SotRankPreprocessData SotRankPreprocessData::FromMessage(uint64_t                 party_id,
                                                         const SotRankParameters &params,
                                                         size_t                   count,
                                                         SotRankPreprocessMsg   &&from_prev,
                                                         SotRankPreprocessMsg   &&from_next) {

    SotRankPreprocessData out;

    out.oa_data.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        out.oa_data.emplace_back(
            proto::SharedOtPreprocessData::FromMessage(
                party_id,
                params.GetParameters(),
                params.GetNumOfOaCalls(),
                std::move(from_prev.oa_msg[i]),
                std::move(from_next.oa_msg[i])));
    }

    return out;
}

SotRankKeys SotRankPreprocessData::ExtractKeys() && {
    std::vector<proto::SharedOtKeys> keys;
    keys.reserve(oa_data.size());
    for (auto &data : oa_data) {
        keys.push_back(std::move(data.keys));
    }
    return SotRankKeys(std::move(keys));
}

size_t SotRankPreprocessData::CalculateSerializedSize() const {
    size_t size = 0;
    size += sizeof(uint64_t);    // number of data entries
    for (const auto &data : oa_data) {
        size += data.CalculateSerializedSize();
    }
    return size;
}

void SotRankPreprocessData::Serialize(std::vector<uint8_t> &buffer) const {
    const size_t start = buffer.size();

    // Number of data entries
    const size_t num = oa_data.size();
    append_pod(buffer, static_cast<uint64_t>(num));

    // Serialize each data entry
    for (const auto &data : oa_data) {
        data.Serialize(buffer);
    }

    // Check size
    const size_t written  = buffer.size() - start;
    const size_t expected = CalculateSerializedSize();
    if (written != expected) {
        throw std::runtime_error(LOC + " SotRankPreprocessData::Serialize: serialized size mismatch: " +
                                 ToString(written) + " != " + ToString(expected));
    }
}

void SotRankPreprocessData::Deserialize(const std::vector<uint8_t> &buffer, const SotRankParameters &params) {
    size_t offset = 0;
    Deserialize(buffer, params, offset);
}

void SotRankPreprocessData::Deserialize(const std::vector<uint8_t> &buffer, const SotRankParameters &params, size_t &offset) {
    const size_t start = offset;

    // Number of data entries
    uint64_t num = 0;
    read_pod(buffer, offset, num);

    // Reset containers
    oa_data.clear();

    oa_data.reserve(num);

    // Deserialize each data entry
    for (size_t i = 0; i < num; ++i) {
        oa_data.emplace_back();
        oa_data.back().Deserialize(buffer, params.GetParameters(), offset);
    }

    // Check size
    const size_t read     = offset - start;
    const size_t expected = CalculateSerializedSize();
    if (read != expected) {
        throw std::runtime_error(LOC + " SotRankPreprocessData::Deserialize: deserialized size mismatch: " +
                                 ToString(read) + " != " + ToString(expected));
    }
}

SotRank::SotRank(
    const SotRankParameters  &params,
    proto::ProtocolContext3P &ctx)
    : params_(params),
      sot_(params.GetParameters(), ctx),
      ctx_(ctx) {
}

std::array<sharing::Rep3ShareMat64, 3> SotRank::GenerateDatabaseU64Share(const wm::FMIndex &fm) const {
    if (fm.GetWaveletMatrix().GetLength() + 1 != params_.GetDatabaseSize()) {
        throw std::invalid_argument("FMIndex length does not match the database size in SotRankParameters");
    }
    const std::vector<uint64_t> &rank0_tables = fm.GetRank0Tables();
    return ctx_.Rss().ShareLocal(rank0_tables, fm.GetWaveletMatrix().GetSigma(), fm.GetWaveletMatrix().GetLength() + 1);
}

SotRankNeighborMsg SotRank::MakePreprocessMsg(size_t count) const {
    SotRankNeighborMsg out;

    out.to_prev.oa_msg.reserve(count);
    out.to_next.oa_msg.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        proto::SharedOtNeighborMsg oa_msg = sot_.MakePreprocessMsg(params_.GetNumOfOaCalls());
        out.to_prev.oa_msg.push_back(std::move(oa_msg.to_prev));
        out.to_next.oa_msg.push_back(std::move(oa_msg.to_next));
    }

    return out;
}

SotRankNeighborMsgIn SotRank::ExchangePreprocessMsg(Channels &chls, SotRankNeighborMsg &&out) const {
    std::vector<uint8_t> to_prev, to_next;
    out.to_prev.Serialize(to_prev);
    out.to_next.Serialize(to_next);

    chls.prev.send(to_prev);
    chls.next.send(to_next);

    std::vector<uint8_t> in_prev, in_next;
    chls.next.recv(in_next);
    chls.prev.recv(in_prev);

    SotRankNeighborMsgIn in;
    in.from_prev.Deserialize(in_prev, params_);
    in.from_next.Deserialize(in_next, params_);
    return in;
}

SotRankPreprocessData SotRank::BuildPreprocessData(uint64_t               party_id,
                                                   size_t                 count,
                                                   SotRankNeighborMsgIn &&in) const {
    return SotRankPreprocessData::FromMessage(
        party_id, params_, count,
        std::move(in.from_prev),
        std::move(in.from_next));
}

SotRankPreprocessData SotRank::Preprocess(Channels &chls, size_t count) const {
    SotRankNeighborMsg   out = MakePreprocessMsg(count);
    SotRankNeighborMsgIn in  = ExchangePreprocessMsg(chls, std::move(out));
    return BuildPreprocessData(chls.party_id, count, std::move(in));
}

void SotRank::EvaluateRankCF(Channels                       &chls,
                             const SotRankKeyView           &key,
                             std::vector<uint64_t>          &uv_prev,
                             std::vector<uint64_t>          &uv_next,
                             const sharing::Rep3ShareMat64  &wm_tables,
                             const sharing::Rep3ShareView64 &char_sh,
                             sharing::Rep3Share64           &position_sh,
                             sharing::Rep3Share64           &result) const {

    uint64_t d        = params_.GetDbIndexBits();
    uint64_t ds       = params_.GetDatabaseSize();
    uint64_t sigma    = params_.GetSigma();
    uint64_t party_id = chls.party_id;

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "Evaluate SotRank key");
    Logger::DebugLog(LOC, "Database bit size: " + ToString(d));
    Logger::DebugLog(LOC, "Database size: " + ToString(ds));
    Logger::DebugLog(LOC, "Sigma: " + ToString(sigma));
    Logger::DebugLog(LOC, "Rows: " + ToString(wm_tables.rows) + ", Columns: " + ToString(wm_tables.cols));
    std::string party_str = "[P" + ToString(party_id) + "] ";
#endif

    sharing::Rep3Share64 rank0_sh(0, 0), rank1_sh(0, 0);
    sharing::Rep3Share64 total_zeros;
    sharing::Rep3Share64 p_sub_rank0_sh;

    for (uint64_t i = 0; i < sigma; ++i) {
        sot_.ObliviousAccess(chls, key.GetOaKeyView(i), uv_prev, uv_next, wm_tables.RowView(i), position_sh, rank0_sh);

        total_zeros = wm_tables.RowView(i).At(wm_tables.RowView(i).Size() - 1);
        ctx_.Rss().EvaluateSub(position_sh, rank0_sh, p_sub_rank0_sh);
        ctx_.Rss().EvaluateAdd(p_sub_rank0_sh, total_zeros, rank1_sh);
        ctx_.Rss().EvaluateSelect(chls, rank0_sh, rank1_sh, char_sh.At(i), position_sh);

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
        uint64_t total_zersot_rec;
        uint64_t p_sub_rank0;
        ctx_.Rss().Open(chls, total_zeros, total_zersot_rec);
        ctx_.Rss().Open(chls, p_sub_rank0_sh, p_sub_rank0);
        Logger::DebugLog(LOC, party_str + "total_zersot_rec: " + ToString(total_zersot_rec));
        Logger::DebugLog(LOC, party_str + "p_sub_rank0: " + ToString(p_sub_rank0));
        Logger::DebugLog(LOC, party_str + "Rank0 share: " + ToString(rank0_sh[0]) + ", " + ToString(rank0_sh[1]));
        Logger::DebugLog(LOC, party_str + "Rank1 share: " + ToString(rank1_sh[0]) + ", " + ToString(rank1_sh[1]));
        uint64_t open_position = 0;
        ctx_.Rss().Open(chls, position_sh, open_position);
        Logger::DebugLog(LOC, party_str + "Rank CF for character " + ToString(i) + ": " + ToString(open_position));
#endif
    }
    result = position_sh;
}

void SotRank::EvaluateRankCFPair(Channels                       &chls,
                                 const SotRankKeyView           &key1,
                                 const SotRankKeyView           &key2,
                                 std::vector<uint64_t>          &uv_prev,
                                 std::vector<uint64_t>          &uv_next,
                                 const sharing::Rep3ShareMat64  &wm_tables,
                                 const sharing::Rep3ShareView64 &char_sh,
                                 sharing::Rep3ShareVec64        &position_sh,
                                 sharing::Rep3ShareVec64        &result) const {
    uint64_t d        = params_.GetDbIndexBits();
    uint64_t ds       = params_.GetDatabaseSize();
    uint64_t sigma    = params_.GetSigma();
    uint64_t party_id = chls.party_id;

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "Evaluate SotRank key");
    Logger::DebugLog(LOC, "Database bit size: " + ToString(d));
    Logger::DebugLog(LOC, "Database size: " + ToString(ds));
    Logger::DebugLog(LOC, "Sigma: " + ToString(sigma));
    Logger::DebugLog(LOC, "Rows: " + ToString(wm_tables.rows) + ", Columns: " + ToString(wm_tables.cols));
    std::string party_str = "[P" + ToString(party_id) + "] ";
#endif

    sharing::Rep3ShareVec64 rank0_sh(2), rank1_sh(2);
    sharing::Rep3ShareVec64 total_zeros(2);
    sharing::Rep3ShareVec64 p_sub_rank0_sh(2);
    for (uint64_t i = 0; i < sigma; ++i) {
        sot_.ObliviousAccessPair(chls, key1.GetOaKeyView(i), key2.GetOaKeyView(i), uv_prev, uv_next, wm_tables.RowView(i), position_sh, rank0_sh);
        total_zeros.Set(0, wm_tables.RowView(i).At(wm_tables.RowView(i).Size() - 1));
        total_zeros.Set(1, wm_tables.RowView(i).At(wm_tables.RowView(i).Size() - 1));
        ctx_.Rss().EvaluateSub(position_sh, rank0_sh, p_sub_rank0_sh);
        ctx_.Rss().EvaluateAdd(p_sub_rank0_sh, total_zeros, rank1_sh);
        ctx_.Rss().EvaluateSelect(chls, rank0_sh, rank1_sh, char_sh.At(i), position_sh);

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
        Logger::DebugLog(LOC, party_str + "Rank0 share: " + ToString(rank0_sh[0]) + ", " + ToString(rank0_sh[1]));
        Logger::DebugLog(LOC, party_str + "Rank1 share: " + ToString(rank1_sh[0]) + ", " + ToString(rank1_sh[1]));
        std::vector<uint64_t> open_position(2);
        ctx_.Rss().Open(chls, position_sh, open_position);
        Logger::DebugLog(LOC, party_str + "Rank CF for character " + ToString(i) + ": " + ToString(open_position[0]) + ", " + ToString(open_position[1]));
#endif
    }
    result = position_sh;
}

}    // namespace fmi
}    // namespace ringoa
