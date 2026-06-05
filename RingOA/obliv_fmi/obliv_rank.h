#ifndef OBLIV_FMI_OBLIV_RANK_H_
#define OBLIV_FMI_OBLIV_RANK_H_

#include "RingOA/protocol/ringoa.h"
#include "RingOA/protocol/ringoa_fsc.h"

namespace ringoa {

namespace wm {

class FMIndex;

}    // namespace wm

namespace fmi {

struct OblivRankConfig {
    // User-facing: database size in bits.
    // Database size (number of items) is 2^{db_index_bits}.
    explicit OblivRankConfig(uint64_t db_bits, uint64_t sigma = 3)
        : db_index_bits(db_bits),
          sigma(sigma) {
    }

    uint64_t db_index_bits;
    uint64_t sigma;

    fss::EvalType eval_type = fss::kOptimizedEvalType;
};

class OblivRankParameters {
public:
    OblivRankParameters() = delete;
    OblivRankParameters(const OblivRankConfig &cfg, const sharing::ShareConfig &share)
        : database_bitsize_(cfg.db_index_bits),
          database_size_(1U << cfg.db_index_bits),
          sigma_(cfg.sigma),
          share_bitsize_(share.arith_bits),
          params_(MakeRingOaParams(cfg, share)) {
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

    const proto::RingOaParameters &GetParameters() const {
        return params_;
    }

    void PrintParametersDebug(bool with_header = true, int key_width = 18) const;

private:
    uint64_t                database_bitsize_;
    uint64_t                database_size_;
    uint64_t                sigma_;
    uint64_t                share_bitsize_;
    proto::RingOaParameters params_;

    static proto::RingOaParameters
    MakeRingOaParams(const OblivRankConfig &cfg, const sharing::ShareConfig &share) {
        proto::RingOaConfig ringoa_cfg(cfg.db_index_bits);
        ringoa_cfg.eval_type = cfg.eval_type;
        return proto::RingOaParameters(ringoa_cfg, share);
    }
};

struct OblivRankKeyView {
    const proto::RingOaKeys *keys = nullptr;

    OblivRankKeyView() = default;
    explicit OblivRankKeyView(const proto::RingOaKeys &k) : keys(&k) {
    }

    proto::RingOaKeyView GetOaKeyView(size_t sigma_index) const {
        if (!keys) {
            throw std::runtime_error("OblivRankKeyView: null");
        }
        return keys->GetView(sigma_index);
    }
};

struct OblivRankKeys {
    std::vector<proto::RingOaKeys> oa_keys;

    OblivRankKeys() = default;
    OblivRankKeys(
        const uint64_t             party_id,
        const OblivRankParameters &params,
        size_t                     count);
    explicit OblivRankKeys(std::vector<proto::RingOaKeys> &&keys)
        : oa_keys(std::move(keys)) {
    }
    ~OblivRankKeys() = default;

    OblivRankKeys(const OblivRankKeys &)                = delete;
    OblivRankKeys &operator=(const OblivRankKeys &)     = delete;
    OblivRankKeys(OblivRankKeys &&) noexcept            = default;
    OblivRankKeys &operator=(OblivRankKeys &&) noexcept = default;

    bool operator==(const OblivRankKeys &rhs) const {
        return oa_keys == rhs.oa_keys;
    }
    bool operator!=(const OblivRankKeys &rhs) const {
        return !(*this == rhs);
    }

    size_t Size() const {
        return oa_keys.size();
    }

    OblivRankKeyView GetView(size_t index) const {
        if (index >= Size()) {
            throw std::out_of_range("OblivRankKeys::GetView: index out of range");
        }
        return OblivRankKeyView(oa_keys[index]);
    }

    size_t CalculateSerializedSize() const;
    void   Serialize(std::vector<uint8_t> &buffer) const;
    void   Deserialize(const std::vector<uint8_t> &buffer, const OblivRankParameters &params);
    void   Deserialize(const std::vector<uint8_t> &buffer, const OblivRankParameters &params, size_t &offset);
};

struct OblivRankPreprocessMsg {
    std::vector<proto::RingOaPreprocessMsg> oa_msg;

    size_t CalculateSerializedSize() const;
    void   Serialize(std::vector<uint8_t> &buffer) const;
    void   Deserialize(const std::vector<uint8_t> &buffer, const OblivRankParameters &params);
    void   Deserialize(const std::vector<uint8_t> &buffer, const OblivRankParameters &params, size_t &offset);
};

struct OblivRankPreprocessData {
    std::vector<proto::RingOaPreprocessData> oa_data;

    OblivRankPreprocessData() = default;

    static OblivRankPreprocessData FromMessage(uint64_t                   party_id,
                                               const OblivRankParameters &params,
                                               size_t                     count,
                                               OblivRankPreprocessMsg   &&from_prev,
                                               OblivRankPreprocessMsg   &&from_next);

    OblivRankKeys ExtractKeys() &&;

    size_t CalculateSerializedSize() const;
    void   Serialize(std::vector<uint8_t> &buffer) const;
    void   Deserialize(const std::vector<uint8_t> &buffer, const OblivRankParameters &params);
    void   Deserialize(const std::vector<uint8_t> &buffer, const OblivRankParameters &params, size_t &offset);
};

struct OblivRankNeighborMsg {
    OblivRankPreprocessMsg to_prev;
    OblivRankPreprocessMsg to_next;
};

struct OblivRankNeighborMsgIn {
    OblivRankPreprocessMsg from_prev;
    OblivRankPreprocessMsg from_next;
};

struct OblivRankFscKeyView {
    const proto::RingOaFscKeys *keys = nullptr;

    OblivRankFscKeyView() = default;
    explicit OblivRankFscKeyView(const proto::RingOaFscKeys &k) : keys(&k) {
    }

    size_t Sigma() const {
        if (!keys) {
            throw std::runtime_error("OblivRankFscKeyView: null");
        }
        return keys->Size();
    }

    proto::RingOaFscKeyView GetOaKeyView(size_t sigma_index) const {
        if (!keys) {
            throw std::runtime_error("OblivRankFscKeyView: null");
        }
        return keys->GetView(sigma_index);
    }
};

struct OblivRankFscKeys {
    std::vector<proto::RingOaFscKeys> oa_keys;

    OblivRankFscKeys() = default;
    OblivRankFscKeys(
        const uint64_t             party_id,
        const OblivRankParameters &params,
        size_t                     count);
    explicit OblivRankFscKeys(std::vector<proto::RingOaFscKeys> &&keys)
        : oa_keys(std::move(keys)) {
    }
    ~OblivRankFscKeys() = default;

    OblivRankFscKeys(const OblivRankFscKeys &)                = delete;
    OblivRankFscKeys &operator=(const OblivRankFscKeys &)     = delete;
    OblivRankFscKeys(OblivRankFscKeys &&) noexcept            = default;
    OblivRankFscKeys &operator=(OblivRankFscKeys &&) noexcept = default;

    bool operator==(const OblivRankFscKeys &rhs) const {
        return oa_keys == rhs.oa_keys;
    }
    bool operator!=(const OblivRankFscKeys &rhs) const {
        return !(*this == rhs);
    }

    size_t Size() const {
        return oa_keys.size();
    }

    OblivRankFscKeyView GetView(size_t index) const {
        if (index >= Size()) {
            throw std::out_of_range("OblivRankFscKeys::GetView: index out of range");
        }
        return OblivRankFscKeyView(oa_keys[index]);
    }

    size_t CalculateSerializedSize() const;
    void   Serialize(std::vector<uint8_t> &buffer) const;
    void   Deserialize(const std::vector<uint8_t> &buffer, const OblivRankParameters &params);
    void   Deserialize(const std::vector<uint8_t> &buffer, const OblivRankParameters &params, size_t &offset);
};

struct OblivRankFscPreprocessMsg {
    std::vector<proto::RingOaFscPreprocessMsg> oa_msg;

    size_t CalculateSerializedSize() const;
    void   Serialize(std::vector<uint8_t> &buffer) const;
    void   Deserialize(const std::vector<uint8_t> &buffer, const OblivRankParameters &params);
    void   Deserialize(const std::vector<uint8_t> &buffer, const OblivRankParameters &params, size_t &offset);
};

struct OblivRankFscPreprocessData {
    std::vector<proto::RingOaFscPreprocessData> oa_data;

    OblivRankFscPreprocessData() = default;

    static OblivRankFscPreprocessData FromMessage(uint64_t                    party_id,
                                                  const OblivRankParameters  &params,
                                                  size_t                      count,
                                                  OblivRankFscPreprocessMsg &&from_prev,
                                                  OblivRankFscPreprocessMsg &&from_next);

    OblivRankFscKeys ExtractKeys() &&;

    size_t CalculateSerializedSize() const;
    void   Serialize(std::vector<uint8_t> &buffer) const;
    void   Deserialize(const std::vector<uint8_t> &buffer, const OblivRankParameters &params);
    void   Deserialize(const std::vector<uint8_t> &buffer, const OblivRankParameters &params, size_t &offset);
};

struct OblivRankFscNeighborMsg {
    OblivRankFscPreprocessMsg to_prev;
    OblivRankFscPreprocessMsg to_next;
};

struct OblivRankFscNeighborMsgIn {
    OblivRankFscPreprocessMsg from_prev;
    OblivRankFscPreprocessMsg from_next;
};

class OblivRank {
public:
    OblivRank() = delete;
    OblivRank(const OblivRankParameters &params,
              proto::ProtocolContext3P  &ctx);

    std::array<sharing::Rep3ShareMat64, 3> GenerateDatabaseU64Share(const wm::FMIndex &fm) const;
    void                                   GenerateDatabaseU64Share(const wm::FMIndex                      &fm,
                                                                    std::array<sharing::Rep3ShareMat64, 3> &db_sh,
                                                                    std::array<sharing::Rep3ShareVec64, 3> &aux_sh,
                                                                    std::array<bool, 3>                    &v_sign) const;

    // Preprocessing phase
    OblivRankNeighborMsg    MakePreprocessMsg(size_t count) const;
    OblivRankNeighborMsgIn  ExchangePreprocessMsg(Channels &chls, OblivRankNeighborMsg &&out) const;
    OblivRankPreprocessData BuildPreprocessData(uint64_t                 party_id,
                                                size_t                   count,
                                                OblivRankNeighborMsgIn &&in) const;
    OblivRankPreprocessData Preprocess(Channels &chls, size_t count) const;

    OblivRankFscNeighborMsg    MakePreprocessMsgFsc(size_t count, bool v_sign) const;
    OblivRankFscNeighborMsgIn  ExchangePreprocessMsgFsc(Channels &chls, OblivRankFscNeighborMsg &&out) const;
    OblivRankFscPreprocessData BuildPreprocessDataFsc(uint64_t                    party_id,
                                                      size_t                      count,
                                                      OblivRankFscNeighborMsgIn &&in) const;
    OblivRankFscPreprocessData PreprocessFsc(Channels &chls, size_t count, bool v_sign) const;

    // Online phase
    void ConsumePreprocessData(OblivRankPreprocessData &data) const;

    void EvaluateRankCF(Channels                       &chls,
                        const OblivRankKeyView         &key,
                        std::vector<block>             &uv_prev,
                        std::vector<block>             &uv_next,
                        const sharing::Rep3ShareMat64  &wm_tables,
                        const sharing::Rep3ShareView64 &char_sh,
                        sharing::Rep3Share64           &position_sh,
                        sharing::Rep3Share64           &result) const;

    void EvaluateRankCFPair(Channels                       &chls,
                            const OblivRankKeyView         &key1,
                            const OblivRankKeyView         &key2,
                            std::vector<block>             &uv_prev,
                            std::vector<block>             &uv_next,
                            const sharing::Rep3ShareMat64  &wm_tables,
                            const sharing::Rep3ShareView64 &char_sh,
                            sharing::Rep3ShareVec64        &position_sh,
                            sharing::Rep3ShareVec64        &result) const;

    void EvaluateRankCFFsc(Channels                       &chls,
                           const OblivRankFscKeyView      &key,
                           std::vector<block>             &uv_prev,
                           std::vector<block>             &uv_next,
                           const sharing::Rep3ShareMat64  &wm_tables,
                           const sharing::Rep3ShareView64 &aux_sh,
                           const sharing::Rep3ShareView64 &char_sh,
                           sharing::Rep3Share64           &position_sh,
                           sharing::Rep3Share64           &result) const;

    void EvaluateRankCFFscPair(Channels                       &chls,
                               const OblivRankFscKeyView      &key1,
                               const OblivRankFscKeyView      &key2,
                               std::vector<block>             &uv_prev,
                               std::vector<block>             &uv_next,
                               const sharing::Rep3ShareMat64  &wm_tables,
                               const sharing::Rep3ShareView64 &aux_sh,
                               const sharing::Rep3ShareView64 &char_sh,
                               sharing::Rep3ShareVec64        &position_sh,
                               sharing::Rep3ShareVec64        &result) const;

private:
    OblivRankParameters       params_;
    proto::RingOa             ringoa_;
    proto::RingOaFsc          ringoa_fsc_;
    proto::ProtocolContext3P &ctx_;
};

}    // namespace fmi
}    // namespace ringoa

#endif    // OBLIV_FMI_OBLIV_RANK_H_
