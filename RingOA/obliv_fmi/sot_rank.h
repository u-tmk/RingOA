#ifndef OBLIV_FMI_SOT_RANK_H_
#define OBLIV_FMI_SOT_RANK_H_

#include "RingOA/protocol/shared_ot.h"

namespace ringoa {

namespace wm {

class FMIndex;

}    // namespace wm

namespace fmi {

struct SotRankConfig {
    // User-facing: database size in bits.
    // Database size (number of items) is 2^{db_index_bits}.
    explicit SotRankConfig(uint64_t db_bits, uint64_t sigma = 3)
        : db_index_bits(db_bits),
          sigma(sigma) {
    }

    uint64_t db_index_bits;
    uint64_t sigma;

    fss::EvalType eval_type = fss::kOptimizedEvalType;
};

class SotRankParameters {
public:
    SotRankParameters() = delete;
    SotRankParameters(const SotRankConfig &cfg, const sharing::ShareConfig &share)
        : database_bitsize_(cfg.db_index_bits),
          database_size_(1U << cfg.db_index_bits),
          sigma_(cfg.sigma),
          share_bitsize_(share.arith_bits),
          params_(MakeSharedOtParams(cfg, share)) {
    }

    uint64_t GetDbIndexBits() const {
        return database_bitsize_;
    }
    uint64_t GetDatabaseSize() const {
        return database_size_;
    }
    uint64_t GetSigma() const {
        return sigma_;
    }
    uint64_t GetShareBitsize() const {
        return share_bitsize_;
    }
    uint64_t GetNumOfOaCalls() const {
        return sigma_;
    }

    const proto::SharedOtParameters &GetParameters() const {
        return params_;
    }

    void PrintParametersDebug(bool with_header = true, int key_width = 18) const;

private:
    uint64_t                  database_bitsize_;
    uint64_t                  database_size_;
    uint64_t                  sigma_;
    uint64_t                  share_bitsize_;
    proto::SharedOtParameters params_;

    static proto::SharedOtParameters
    MakeSharedOtParams(const SotRankConfig &cfg, const sharing::ShareConfig &share) {
        proto::SharedOtConfig sot_cfg(cfg.db_index_bits);
        sot_cfg.eval_type = cfg.eval_type;
        return proto::SharedOtParameters(sot_cfg, share);
    }
};

struct SotRankKeyView {
    const proto::SharedOtKeys *keys = nullptr;

    SotRankKeyView() = default;
    explicit SotRankKeyView(const proto::SharedOtKeys &k) : keys(&k) {
    }

    proto::SharedOtKeyView GetOaKeyView(size_t sigma_index) const {
        if (!keys) {
            throw std::runtime_error("SotRankKeyView: null");
        }
        return keys->GetView(sigma_index);
    }
};

struct SotRankKeys {
    std::vector<proto::SharedOtKeys> oa_keys;

    SotRankKeys() = delete;
    SotRankKeys(const uint64_t           id,
                const SotRankParameters &params,
                size_t                   count);
    explicit SotRankKeys(std::vector<proto::SharedOtKeys> &&keys)
        : oa_keys(std::move(keys)) {
    }
    ~SotRankKeys() = default;

    SotRankKeys(const SotRankKeys &)                = delete;
    SotRankKeys &operator=(const SotRankKeys &)     = delete;
    SotRankKeys(SotRankKeys &&) noexcept            = default;
    SotRankKeys &operator=(SotRankKeys &&) noexcept = default;

    bool operator==(const SotRankKeys &rhs) const {
        return oa_keys == rhs.oa_keys;
    }
    bool operator!=(const SotRankKeys &rhs) const {
        return !(*this == rhs);
    }

    size_t Size() const {
        return oa_keys.size();
    }

    SotRankKeyView GetView(size_t index) const {
        if (index >= Size()) {
            throw std::out_of_range("SotRankKeys::GetView: index out of range");
        }
        return SotRankKeyView(oa_keys[index]);
    }

    size_t CalculateSerializedSize() const;
    void   Serialize(std::vector<uint8_t> &buffer) const;
    void   Deserialize(const std::vector<uint8_t> &buffer, const SotRankParameters &params);
    void   Deserialize(const std::vector<uint8_t> &buffer, const SotRankParameters &params, size_t &offset);
};

struct SotRankPreprocessMsg {
    std::vector<proto::SharedOtPreprocessMsg> oa_msg;

    size_t CalculateSerializedSize() const;
    void   Serialize(std::vector<uint8_t> &buffer) const;
    void   Deserialize(const std::vector<uint8_t> &buffer, const SotRankParameters &params);
    void   Deserialize(const std::vector<uint8_t> &buffer, const SotRankParameters &params, size_t &offset);
};

struct SotRankPreprocessData {
    std::vector<proto::SharedOtPreprocessData> oa_data;

    SotRankPreprocessData() = default;

    static SotRankPreprocessData FromMessage(uint64_t                 party_id,
                                             const SotRankParameters &params,
                                             size_t                   count,
                                             SotRankPreprocessMsg   &&from_prev,
                                             SotRankPreprocessMsg   &&from_next);

    SotRankKeys ExtractKeys() &&;

    size_t CalculateSerializedSize() const;
    void   Serialize(std::vector<uint8_t> &buffer) const;
    void   Deserialize(const std::vector<uint8_t> &buffer, const SotRankParameters &params);
    void   Deserialize(const std::vector<uint8_t> &buffer, const SotRankParameters &params, size_t &offset);
};

struct SotRankNeighborMsg {
    SotRankPreprocessMsg to_prev;
    SotRankPreprocessMsg to_next;
};

struct SotRankNeighborMsgIn {
    SotRankPreprocessMsg from_prev;
    SotRankPreprocessMsg from_next;
};

class SotRank {
public:
    SotRank() = delete;
    SotRank(const SotRankParameters  &params,
            proto::ProtocolContext3P &ctx);

    std::array<sharing::Rep3ShareMat64, 3> GenerateDatabaseU64Share(const wm::FMIndex &fm) const;

    // Preprocessing phase
    SotRankNeighborMsg    MakePreprocessMsg(size_t count) const;
    SotRankNeighborMsgIn  ExchangePreprocessMsg(Channels &chls, SotRankNeighborMsg &&out) const;
    SotRankPreprocessData BuildPreprocessData(uint64_t               party_id,
                                              size_t                 count,
                                              SotRankNeighborMsgIn &&in) const;
    SotRankPreprocessData Preprocess(Channels &chls, size_t count) const;

    // Online phase
    void EvaluateRankCF(Channels                       &chls,
                        const SotRankKeyView           &key,
                        std::vector<uint64_t>          &uv_prev,
                        std::vector<uint64_t>          &uv_next,
                        const sharing::Rep3ShareMat64  &wm_tables,
                        const sharing::Rep3ShareView64 &char_sh,
                        sharing::Rep3Share64           &position_sh,
                        sharing::Rep3Share64           &result) const;

    void EvaluateRankCFPair(Channels                       &chls,
                            const SotRankKeyView           &key1,
                            const SotRankKeyView           &key2,
                            std::vector<uint64_t>          &uv_prev,
                            std::vector<uint64_t>          &uv_next,
                            const sharing::Rep3ShareMat64  &wm_tables,
                            const sharing::Rep3ShareView64 &char_sh,
                            sharing::Rep3ShareVec64        &position_sh,
                            sharing::Rep3ShareVec64        &result) const;

private:
    SotRankParameters         params_;
    proto::SharedOt           sot_;
    proto::ProtocolContext3P &ctx_;
};

}    // namespace fmi
}    // namespace ringoa

#endif    // OBLIV_FMI_SOT_RANK_H_
