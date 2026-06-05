#include "obliv_rank.h"

#include <cstring>

#include "RingOA/utils/bytes.h"
#include "RingOA/utils/logger.h"
#include "RingOA/utils/to_string.h"
#include "RingOA/utils/utils.h"
#include "RingOA/wm/plain_wm.h"

namespace ringoa {
namespace fmi {

void OblivRankParameters::PrintParametersDebug(bool with_header, int key_width) const {
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    if (with_header) {
        Logger::DebugLog(LOC, Logger::MakeSeparatorLine());
        Logger::DebugLog(LOC, "[Oblivious Rank Params]");
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

OblivRankKeys::OblivRankKeys(
    const uint64_t             party_id,
    const OblivRankParameters &params,
    size_t                     count) {
    oa_keys.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        oa_keys.emplace_back(party_id, params.GetParameters(), params.GetNumOfOaCalls());
    }
}

size_t OblivRankKeys::CalculateSerializedSize() const {
    size_t size = 0;

    for (const auto &key : oa_keys) {
        size += key.CalculateSerializedSize();
    }
    return size;
}

void OblivRankKeys::Serialize(std::vector<uint8_t> &buffer) const {
    const size_t start = buffer.size();

    // Number of keys
    const size_t num = oa_keys.size();
    append_pod(buffer, static_cast<uint64_t>(num));

    // Serialize the OA keys
    for (const auto &key : oa_keys) {
        key.Serialize(buffer);
    }

    // Check size
    const size_t written  = buffer.size() - start;
    const size_t expected = CalculateSerializedSize();
    if (written != expected) {
        throw std::runtime_error(LOC + " OblivRankKeys::Serialize: serialized size mismatch: " +
                                 ToString(written) + " != " + ToString(expected));
    }
}

void OblivRankKeys::Deserialize(const std::vector<uint8_t> &buffer, const OblivRankParameters &params) {
    size_t offset = 0;
    Deserialize(buffer, params, offset);
}

void OblivRankKeys::Deserialize(const std::vector<uint8_t> &buffer, const OblivRankParameters &params, size_t &offset) {
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
        throw std::runtime_error(LOC + " OblivRankKeys::Deserialize: deserialized size mismatch: " +
                                 ToString(read) + " != " + ToString(expected));
    }
}

size_t OblivRankPreprocessMsg::CalculateSerializedSize() const {
    size_t size = 0;
    size += sizeof(uint64_t);    // number of messages
    for (const auto &msg : oa_msg) {
        size += msg.CalculateSerializedSize();
    }
    return size;
}

void OblivRankPreprocessMsg::Serialize(std::vector<uint8_t> &buffer) const {
    const size_t start = buffer.size();

    // Number of messages
    const size_t num = oa_msg.size();
    append_pod(buffer, static_cast<uint64_t>(num));

    // Serialize the OA preprocess messages
    for (const auto &msg : oa_msg) {
        msg.Serialize(buffer);
    }

    // Check size
    const size_t written  = buffer.size() - start;
    const size_t expected = CalculateSerializedSize();
    if (written != expected) {
        throw std::runtime_error(LOC + " OblivRankPreprocessMsg::Serialize: serialized size mismatch: " +
                                 ToString(written) + " != " + ToString(expected));
    }
}

void OblivRankPreprocessMsg::Deserialize(const std::vector<uint8_t> &buffer, const OblivRankParameters &params) {
    size_t offset = 0;
    Deserialize(buffer, params, offset);
}

void OblivRankPreprocessMsg::Deserialize(const std::vector<uint8_t> &buffer, const OblivRankParameters &params, size_t &offset) {
    const size_t start = offset;

    // Number of messages
    uint64_t num = 0;
    read_pod(buffer, offset, num);

    // Reset containers
    oa_msg.clear();

    oa_msg.reserve(num);

    // Deserialize the OA preprocess messages
    for (size_t i = 0; i < num; ++i) {
        oa_msg.emplace_back();
        oa_msg.back().Deserialize(buffer, params.GetParameters(), offset);
    }

    // Check size
    const size_t read     = offset - start;
    const size_t expected = CalculateSerializedSize();
    if (read != expected) {
        throw std::runtime_error(LOC + " OblivRankPreprocessMsg::Deserialize: deserialized size mismatch: " +
                                 ToString(read) + " != " + ToString(expected));
    }
}

OblivRankPreprocessData OblivRankPreprocessData::FromMessage(
    uint64_t                   party_id,
    const OblivRankParameters &params,
    size_t                     count,
    OblivRankPreprocessMsg   &&from_prev,
    OblivRankPreprocessMsg   &&from_next) {

    OblivRankPreprocessData out;

    out.oa_data.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        out.oa_data.emplace_back(
            proto::RingOaPreprocessData::FromMessage(
                party_id, params.GetParameters(),
                params.GetNumOfOaCalls(),
                std::move(from_prev.oa_msg[i]),
                std::move(from_next.oa_msg[i])));
    }

    return out;
}

OblivRankKeys OblivRankPreprocessData::ExtractKeys() && {
    std::vector<proto::RingOaKeys> keys;
    keys.reserve(oa_data.size());
    for (auto &d : oa_data) {
        keys.emplace_back(std::move(d.keys));
    }
    return OblivRankKeys(std::move(keys));
}

size_t OblivRankPreprocessData::CalculateSerializedSize() const {
    size_t size = 0;

    size += sizeof(uint64_t);    // number of data entries
    for (const auto &data : oa_data) {
        size += data.CalculateSerializedSize();
    }
    return size;
}

void OblivRankPreprocessData::Serialize(std::vector<uint8_t> &buffer) const {
    const size_t start = buffer.size();

    // Number of data entries
    const size_t num = oa_data.size();
    append_pod(buffer, static_cast<uint64_t>(num));

    // Serialize the OA preprocess data
    for (const auto &data : oa_data) {
        data.Serialize(buffer);
    }

    // Check size
    const size_t written  = buffer.size() - start;
    const size_t expected = CalculateSerializedSize();
    if (written != expected) {
        throw std::runtime_error(LOC + " OblivRankPreprocessData::Serialize: serialized size mismatch: " +
                                 ToString(written) + " != " + ToString(expected));
    }
}

void OblivRankPreprocessData::Deserialize(const std::vector<uint8_t> &buffer, const OblivRankParameters &params) {
    size_t offset = 0;
    Deserialize(buffer, params, offset);
}

void OblivRankPreprocessData::Deserialize(const std::vector<uint8_t> &buffer, const OblivRankParameters &params, size_t &offset) {
    const size_t start = offset;

    // Number of data entries
    uint64_t num = 0;
    read_pod(buffer, offset, num);

    // Reset containers
    oa_data.clear();

    oa_data.reserve(num);

    // Deserialize the OA preprocess data
    for (size_t i = 0; i < num; ++i) {
        oa_data.emplace_back();
        oa_data.back().Deserialize(buffer, params.GetParameters(), offset);
    }

    // Check size
    const size_t read     = offset - start;
    const size_t expected = CalculateSerializedSize();
    if (read != expected) {
        throw std::runtime_error(LOC + " OblivRankPreprocessData::Deserialize: deserialized size mismatch: " +
                                 ToString(read) + " != " + ToString(expected));
    }
}

OblivRankFscKeys::OblivRankFscKeys(
    const uint64_t             party_id,
    const OblivRankParameters &params,
    size_t                     count) {
    oa_keys.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        oa_keys.emplace_back(party_id, params.GetParameters(), params.GetNumOfOaCalls());
    }
}

size_t OblivRankFscKeys::CalculateSerializedSize() const {
    size_t size = 0;

    for (const auto &key : oa_keys) {
        size += key.CalculateSerializedSize();
    }
    return size;
}

void OblivRankFscKeys::Serialize(std::vector<uint8_t> &buffer) const {
    const size_t start = buffer.size();

    // Number of keys
    const size_t num = oa_keys.size();
    append_pod(buffer, static_cast<uint64_t>(num));

    // Serialize the OA keys
    for (const auto &key : oa_keys) {
        key.Serialize(buffer);
    }

    // Check size
    const size_t written  = buffer.size() - start;
    const size_t expected = CalculateSerializedSize();
    if (written != expected) {
        throw std::runtime_error(LOC + " OblivRankFscKeys::Serialize: serialized size mismatch: " +
                                 ToString(written) + " != " + ToString(expected));
    }
}

void OblivRankFscKeys::Deserialize(const std::vector<uint8_t> &buffer, const OblivRankParameters &params) {
    size_t offset = 0;
    Deserialize(buffer, params, offset);
}

void OblivRankFscKeys::Deserialize(const std::vector<uint8_t> &buffer, const OblivRankParameters &params, size_t &offset) {
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
        throw std::runtime_error(LOC + " OblivRankFscKeys::Deserialize: deserialized size mismatch: " +
                                 ToString(read) + " != " + ToString(expected));
    }
}

size_t OblivRankFscPreprocessMsg::CalculateSerializedSize() const {
    size_t size = 0;
    size += sizeof(uint64_t);    // number of messages
    for (const auto &msg : oa_msg) {
        size += msg.CalculateSerializedSize();
    }
    return size;
}

void OblivRankFscPreprocessMsg::Serialize(std::vector<uint8_t> &buffer) const {
    const size_t start = buffer.size();

    // Number of messages
    const size_t num = oa_msg.size();
    append_pod(buffer, static_cast<uint64_t>(num));

    // Serialize the OA preprocess messages
    for (const auto &msg : oa_msg) {
        msg.Serialize(buffer);
    }

    // Check size
    const size_t written  = buffer.size() - start;
    const size_t expected = CalculateSerializedSize();
    if (written != expected) {
        throw std::runtime_error(LOC + " OblivRankFscPreprocessMsg::Serialize: serialized size mismatch: " +
                                 ToString(written) + " != " + ToString(expected));
    }
}

void OblivRankFscPreprocessMsg::Deserialize(const std::vector<uint8_t> &buffer, const OblivRankParameters &params) {
    size_t offset = 0;
    Deserialize(buffer, params, offset);
}

void OblivRankFscPreprocessMsg::Deserialize(const std::vector<uint8_t> &buffer, const OblivRankParameters &params, size_t &offset) {
    const size_t start = offset;

    // Number of messages
    uint64_t num = 0;
    read_pod(buffer, offset, num);

    // Reset containers
    oa_msg.clear();

    oa_msg.reserve(num);

    // Deserialize the OA preprocess messages
    for (size_t i = 0; i < num; ++i) {
        oa_msg.emplace_back();
        oa_msg.back().Deserialize(buffer, params.GetParameters(), offset);
    }

    // Check size
    const size_t read     = offset - start;
    const size_t expected = CalculateSerializedSize();
    if (read != expected) {
        throw std::runtime_error(LOC + " OblivRankFscPreprocessMsg::Deserialize: deserialized size mismatch: " +
                                 ToString(read) + " != " + ToString(expected));
    }
}

OblivRankFscPreprocessData OblivRankFscPreprocessData::FromMessage(
    uint64_t                    party_id,
    const OblivRankParameters  &params,
    size_t                      count,
    OblivRankFscPreprocessMsg &&from_prev,
    OblivRankFscPreprocessMsg &&from_next) {

    OblivRankFscPreprocessData out;

    for (size_t i = 0; i < count; ++i) {
        out.oa_data.emplace_back(
            proto::RingOaFscPreprocessData::FromMessage(
                party_id, params.GetParameters(),
                params.GetNumOfOaCalls(),
                std::move(from_prev.oa_msg[i]),
                std::move(from_next.oa_msg[i])));
    }

    return out;
}

OblivRankFscKeys OblivRankFscPreprocessData::ExtractKeys() && {
    std::vector<proto::RingOaFscKeys> keys;
    keys.reserve(oa_data.size());
    for (auto &d : oa_data) {
        keys.emplace_back(std::move(d.keys));
    }
    return OblivRankFscKeys(std::move(keys));
}

size_t OblivRankFscPreprocessData::CalculateSerializedSize() const {
    size_t size = 0;

    size += sizeof(uint64_t);    // number of data entries
    for (const auto &data : oa_data) {
        size += data.CalculateSerializedSize();
    }
    return size;
}

void OblivRankFscPreprocessData::Serialize(std::vector<uint8_t> &buffer) const {
    const size_t start = buffer.size();

    // Number of data entries
    const size_t num = oa_data.size();
    append_pod(buffer, static_cast<uint64_t>(num));

    // Serialize the OA preprocess data
    for (const auto &data : oa_data) {
        data.Serialize(buffer);
    }

    // Check size
    const size_t written  = buffer.size() - start;
    const size_t expected = CalculateSerializedSize();
    if (written != expected) {
        throw std::runtime_error(LOC + " OblivRankFscPreprocessData::Serialize: serialized size mismatch: " +
                                 ToString(written) + " != " + ToString(expected));
    }
}

void OblivRankFscPreprocessData::Deserialize(const std::vector<uint8_t> &buffer, const OblivRankParameters &params) {
    size_t offset = 0;
    Deserialize(buffer, params, offset);
}

void OblivRankFscPreprocessData::Deserialize(const std::vector<uint8_t> &buffer, const OblivRankParameters &params, size_t &offset) {
    const size_t start = offset;

    // Number of data entries
    uint64_t num = 0;
    read_pod(buffer, offset, num);

    // Reset containers
    oa_data.clear();

    oa_data.reserve(num);

    // Deserialize the OA preprocess data
    for (size_t i = 0; i < num; ++i) {
        oa_data.emplace_back();
        oa_data.back().Deserialize(buffer, params.GetParameters(), offset);
    }

    // Check size
    const size_t read     = offset - start;
    const size_t expected = CalculateSerializedSize();
    if (read != expected) {
        throw std::runtime_error(LOC + " OblivRankFscPreprocessData::Deserialize: deserialized size mismatch: " +
                                 ToString(read) + " != " + ToString(expected));
    }
}

OblivRank::OblivRank(
    const OblivRankParameters &params,
    proto::ProtocolContext3P  &ctx)
    : params_(params),
      ringoa_(params.GetParameters(), ctx),
      ringoa_fsc_(params.GetParameters(), ctx),
      ctx_(ctx) {
}

std::array<sharing::Rep3ShareMat64, 3> OblivRank::GenerateDatabaseU64Share(const wm::FMIndex &fm) const {
    if (fm.GetWaveletMatrix().GetLength() + 1 != params_.GetDatabaseSize()) {
        throw std::invalid_argument(LOC + " FMIndex length does not match the database size in OblivRankParameters");
    }
    const std::vector<uint64_t> &rank0_tables = fm.GetRank0Tables();
    return ctx_.Rss().ShareLocal(rank0_tables, fm.GetWaveletMatrix().GetSigma(), fm.GetWaveletMatrix().GetLength() + 1);
}

void OblivRank::GenerateDatabaseU64Share(const wm::FMIndex                      &fm,
                                         std::array<sharing::Rep3ShareMat64, 3> &db_sh,
                                         std::array<sharing::Rep3ShareVec64, 3> &aux_sh,
                                         std::array<bool, 3>                    &v_sign) const {
    if (fm.GetWaveletMatrix().GetLength() + 1 != params_.GetDatabaseSize()) {
        throw std::invalid_argument("FMIndex length does not match the database size in OblivRankFscParameters");
    }
    const std::vector<uint64_t> &rank0_tables = fm.GetRank0Tables();
    ringoa_fsc_.GenerateDatabaseShare(rank0_tables, db_sh, fm.GetWaveletMatrix().GetSigma(), fm.GetWaveletMatrix().GetLength() + 1, v_sign);

    const size_t          stride = fm.GetWaveletMatrix().GetLength() + 1;
    std::vector<uint64_t> total_zero(fm.GetWaveletMatrix().GetSigma());
    for (size_t i = 0; i < fm.GetWaveletMatrix().GetSigma(); ++i) {
        total_zero[i] = rank0_tables[(i + 1) * stride - 1];
    }
    aux_sh = ctx_.Rss().ShareLocal(total_zero);
}

OblivRankNeighborMsg OblivRank::MakePreprocessMsg(size_t count) const {
    OblivRankNeighborMsg out;

    out.to_prev.oa_msg.reserve(count);
    out.to_next.oa_msg.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        proto::RingOaNeighborMsg oa_msg = ringoa_.MakePreprocessMsg(params_.GetNumOfOaCalls());
        out.to_prev.oa_msg.push_back(std::move(oa_msg.to_prev));
        out.to_next.oa_msg.push_back(std::move(oa_msg.to_next));
    }

    return out;
}

OblivRankNeighborMsgIn OblivRank::ExchangePreprocessMsg(Channels &chls, OblivRankNeighborMsg &&out) const {
    std::vector<uint8_t> to_prev, to_next;
    out.to_prev.Serialize(to_prev);
    out.to_next.Serialize(to_next);

    chls.prev.send(to_prev);
    chls.next.send(to_next);

    std::vector<uint8_t> in_prev, in_next;
    chls.next.recv(in_next);
    chls.prev.recv(in_prev);

    OblivRankNeighborMsgIn in;
    in.from_prev.Deserialize(in_prev, params_);
    in.from_next.Deserialize(in_next, params_);
    return in;
}

OblivRankPreprocessData OblivRank::BuildPreprocessData(uint64_t                 party_id,
                                                       size_t                   count,
                                                       OblivRankNeighborMsgIn &&in) const {
    return OblivRankPreprocessData::FromMessage(
        party_id, params_, count,
        std::move(in.from_prev),
        std::move(in.from_next));
}

OblivRankPreprocessData OblivRank::Preprocess(Channels &chls, size_t count) const {
    OblivRankNeighborMsg   out = MakePreprocessMsg(count);
    OblivRankNeighborMsgIn in  = ExchangePreprocessMsg(chls, std::move(out));
    return BuildPreprocessData(chls.party_id, count, std::move(in));
}

OblivRankFscNeighborMsg OblivRank::MakePreprocessMsgFsc(size_t count, bool v_sign) const {
    OblivRankFscNeighborMsg out;

    out.to_prev.oa_msg.reserve(count);
    out.to_next.oa_msg.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        proto::RingOaFscNeighborMsg oa_msg = ringoa_fsc_.MakePreprocessMsg(params_.GetNumOfOaCalls(), v_sign);
        out.to_prev.oa_msg.push_back(std::move(oa_msg.to_prev));
        out.to_next.oa_msg.push_back(std::move(oa_msg.to_next));
    }

    return out;
}

OblivRankFscNeighborMsgIn OblivRank::ExchangePreprocessMsgFsc(Channels &chls, OblivRankFscNeighborMsg &&out) const {
    std::vector<uint8_t> to_prev, to_next;
    out.to_prev.Serialize(to_prev);
    out.to_next.Serialize(to_next);

    chls.prev.send(to_prev);
    chls.next.send(to_next);

    std::vector<uint8_t> in_prev, in_next;
    chls.next.recv(in_next);
    chls.prev.recv(in_prev);

    OblivRankFscNeighborMsgIn in;
    in.from_prev.Deserialize(in_prev, params_);
    in.from_next.Deserialize(in_next, params_);
    return in;
}

OblivRankFscPreprocessData OblivRank::BuildPreprocessDataFsc(uint64_t                    party_id,
                                                             size_t                      count,
                                                             OblivRankFscNeighborMsgIn &&in) const {
    return OblivRankFscPreprocessData::FromMessage(
        party_id, params_, count,
        std::move(in.from_prev),
        std::move(in.from_next));
}

OblivRankFscPreprocessData OblivRank::PreprocessFsc(Channels &chls, size_t count, bool v_sign) const {
    OblivRankFscNeighborMsg   out = MakePreprocessMsgFsc(count, v_sign);
    OblivRankFscNeighborMsgIn in  = ExchangePreprocessMsgFsc(chls, std::move(out));
    return BuildPreprocessDataFsc(chls.party_id, count, std::move(in));
}

void OblivRank::ConsumePreprocessData(OblivRankPreprocessData &data) const {
    sharing::BeaverTriples all_prev, all_next;

    for (auto &oa_data : data.oa_data) {
        all_prev.Append(std::move(oa_data.triples_with_prev));
        all_next.Append(std::move(oa_data.triples_with_next));
    }

    ctx_.AssWithPrev().SetTriples(std::move(all_prev));
    ctx_.AssWithNext().SetTriples(std::move(all_next));
}

void OblivRank::EvaluateRankCF(Channels                       &chls,
                               const OblivRankKeyView         &key,
                               std::vector<block>             &uv_prev,
                               std::vector<block>             &uv_next,
                               const sharing::Rep3ShareMat64  &wm_tables,
                               const sharing::Rep3ShareView64 &char_sh,
                               sharing::Rep3Share64           &position_sh,
                               sharing::Rep3Share64           &result) const {

    uint64_t d        = params_.GetDbIndexBits();
    uint64_t ds       = params_.GetDatabaseSize();
    uint64_t sigma    = params_.GetSigma();
    uint64_t party_id = chls.party_id;

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "Evaluate OblivRank key");
    Logger::DebugLog(LOC, "Database bit size: " + ToString(d) + " (size: " + ToString(ds) + ")");
    Logger::DebugLog(LOC, "Sigma: " + ToString(sigma));
    Logger::DebugLog(LOC, "Rows: " + ToString(wm_tables.rows) + ", Columns: " + ToString(wm_tables.cols));
    std::string party_str = "[P" + ToString(party_id) + "] ";
#endif

    sharing::Rep3Share64 rank0_sh(0, 0), rank1_sh(0, 0);
    sharing::Rep3Share64 total_zeros;
    sharing::Rep3Share64 p_sub_rank0_sh;

    for (uint64_t i = 0; i < sigma; ++i) {
        ringoa_.ObliviousAccess(chls, key.GetOaKeyView(i), uv_prev, uv_next, wm_tables.RowView(i), position_sh, rank0_sh);

        total_zeros = wm_tables.RowView(i).At(wm_tables.RowView(i).Size() - 1);
        ctx_.Rss().EvaluateSub(position_sh, rank0_sh, p_sub_rank0_sh);
        ctx_.Rss().EvaluateAdd(p_sub_rank0_sh, total_zeros, rank1_sh);
        ctx_.Rss().EvaluateSelect(chls, rank0_sh, rank1_sh, char_sh.At(i), position_sh);

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
        uint64_t total_zero_rec;
        uint64_t p_sub_rank0;
        ctx_.Rss().Open(chls, total_zeros, total_zero_rec);
        ctx_.Rss().Open(chls, p_sub_rank0_sh, p_sub_rank0);
        Logger::DebugLog(LOC, party_str + "total_zero_rec: " + ToString(total_zero_rec));
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

void OblivRank::EvaluateRankCFPair(Channels                       &chls,
                                   const OblivRankKeyView         &key1,
                                   const OblivRankKeyView         &key2,
                                   std::vector<block>             &uv_prev,
                                   std::vector<block>             &uv_next,
                                   const sharing::Rep3ShareMat64  &wm_tables,
                                   const sharing::Rep3ShareView64 &char_sh,
                                   sharing::Rep3ShareVec64        &position_sh,
                                   sharing::Rep3ShareVec64        &result) const {
    uint64_t d        = params_.GetDbIndexBits();
    uint64_t ds       = params_.GetDatabaseSize();
    uint64_t sigma    = params_.GetSigma();
    uint64_t party_id = chls.party_id;

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "Evaluate OblivRank key");
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
        ringoa_.ObliviousAccessPair(chls, key1.GetOaKeyView(i), key2.GetOaKeyView(i), uv_prev, uv_next, wm_tables.RowView(i), position_sh, rank0_sh);
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

void OblivRank::EvaluateRankCFFsc(Channels                       &chls,
                                  const OblivRankFscKeyView      &key,
                                  std::vector<block>             &uv_prev,
                                  std::vector<block>             &uv_next,
                                  const sharing::Rep3ShareMat64  &wm_tables,
                                  const sharing::Rep3ShareView64 &aux_sh,
                                  const sharing::Rep3ShareView64 &char_sh,
                                  sharing::Rep3Share64           &position_sh,
                                  sharing::Rep3Share64           &result) const {

    uint64_t d        = params_.GetDbIndexBits();
    uint64_t ds       = params_.GetDatabaseSize();
    uint64_t sigma    = params_.GetSigma();
    uint64_t party_id = chls.party_id;

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "Evaluate OblivRankFsc key");
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
        ringoa_fsc_.ObliviousAccess(chls, key.GetOaKeyView(i), uv_prev, uv_next, wm_tables.RowView(i), position_sh, rank0_sh);

        total_zeros = aux_sh.At(i);
        ctx_.Rss().EvaluateSub(position_sh, rank0_sh, p_sub_rank0_sh);
        ctx_.Rss().EvaluateAdd(p_sub_rank0_sh, total_zeros, rank1_sh);
        ctx_.Rss().EvaluateSelect(chls, rank0_sh, rank1_sh, char_sh.At(i), position_sh);

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
        uint64_t total_zero_rec;
        uint64_t p_sub_rank0;
        ctx_.Rss().Open(chls, total_zeros, total_zero_rec);
        ctx_.Rss().Open(chls, p_sub_rank0_sh, p_sub_rank0);
        Logger::DebugLog(LOC, party_str + "total_zero_rec: " + ToString(total_zero_rec));
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

void OblivRank::EvaluateRankCFFscPair(Channels                       &chls,
                                      const OblivRankFscKeyView      &key1,
                                      const OblivRankFscKeyView      &key2,
                                      std::vector<block>             &uv_prev,
                                      std::vector<block>             &uv_next,
                                      const sharing::Rep3ShareMat64  &wm_tables,
                                      const sharing::Rep3ShareView64 &aux_sh,
                                      const sharing::Rep3ShareView64 &char_sh,
                                      sharing::Rep3ShareVec64        &position_sh,
                                      sharing::Rep3ShareVec64        &result) const {
    uint64_t d        = params_.GetDbIndexBits();
    uint64_t ds       = params_.GetDatabaseSize();
    uint64_t sigma    = params_.GetSigma();
    uint64_t party_id = chls.party_id;

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "Evaluate OblivRankFsc key");
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
        ringoa_fsc_.ObliviousAccessPair(chls, key1.GetOaKeyView(i), key2.GetOaKeyView(i), uv_prev, uv_next, wm_tables.RowView(i), position_sh, rank0_sh);
        total_zeros.Set(0, aux_sh.At(i));
        total_zeros.Set(1, aux_sh.At(i));
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
