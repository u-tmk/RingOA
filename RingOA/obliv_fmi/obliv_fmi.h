#ifndef OBLIV_FMI_OBLIV_FMI_H_
#define OBLIV_FMI_OBLIV_FMI_H_

#include "RingOA/protocol/zero_test.h"
#include "obliv_rank.h"

namespace ringoa {
namespace fmi {

struct OblivFMIConfig {
    // User-facing: database size in bits.
    // Database size (number of items) is 2^{db_index_bits}.
    // Query size is the number of queries to be performed.
    explicit OblivFMIConfig(uint64_t db_bits, uint64_t q_size, uint64_t sigma = 3)
        : db_index_bits(db_bits),
          query_size(q_size),
          sigma(sigma) {
    }

    uint64_t db_index_bits;
    uint64_t query_size;
    uint64_t sigma;

    fss::EvalType eval_type = fss::kOptimizedEvalType;
};

class OblivFMIParameters {
public:
    OblivFMIParameters() = delete;
    OblivFMIParameters(const OblivFMIConfig &cfg, const sharing::ShareConfig &share)
        : database_bitsize_(cfg.db_index_bits),
          database_size_(1U << cfg.db_index_bits),
          query_size_(cfg.query_size),
          sigma_(cfg.sigma),
          share_bitsize_(share.arith_bits),
          orank_params_(MakeOblivRankParams(cfg, share)),
          zt_params_(MakeZeroTestParams(cfg, share)) {
    }

    uint64_t GetDbIndexBits() const {
        return database_bitsize_;
    }
    uint64_t GetDatabaseSize() const {
        return database_size_;
    }
    uint64_t GetQuerySize() const {
        return query_size_;
    }
    uint64_t GetSigma() const {
        return sigma_;
    }
    uint64_t GetShareBitsize() const {
        return share_bitsize_;
    }
    uint64_t GetNumOfRankCalls() const {
        return query_size_ * 2;    // for F and G
    }
    uint64_t GetNumOfZtCalls() const {
        return query_size_;
    }

    const OblivRankParameters &GetOblivRankParameters() const {
        return orank_params_;
    }
    const proto::ZeroTestParameters &GetZeroTestParameters() const {
        return zt_params_;
    }

    void PrintParametersDebug(bool with_header = true, int key_width = 18) const;

private:
    uint64_t                  database_bitsize_;
    uint64_t                  database_size_;
    uint64_t                  query_size_;
    uint64_t                  sigma_;
    uint64_t                  share_bitsize_;
    OblivRankParameters       orank_params_;
    proto::ZeroTestParameters zt_params_;

    static OblivRankParameters
    MakeOblivRankParams(const OblivFMIConfig &cfg, const sharing::ShareConfig &share) {
        OblivRankConfig orank_cfg(cfg.db_index_bits);
        orank_cfg.eval_type = cfg.eval_type;
        return OblivRankParameters(orank_cfg, share);
    }

    static proto::ZeroTestParameters
    MakeZeroTestParams(const OblivFMIConfig &cfg, const sharing::ShareConfig &share) {
        proto::ZeroTestConfig zt_cfg;
        if (cfg.db_index_bits == share.arith_bits) {
            zt_cfg.input_domain_bits = cfg.db_index_bits;
        }
        return proto::ZeroTestParameters(zt_cfg, share);
    }
};

struct OblivFMIKeys {
    OblivRankKeys       rank_f_keys;
    OblivRankKeys       rank_g_keys;
    proto::ZeroTestKeys zt_keys;

    OblivFMIKeys() = delete;
    OblivFMIKeys(const uint64_t id, const OblivFMIParameters &params);
    OblivFMIKeys(OblivRankKeys       &&rank_f_keys_in,
                 OblivRankKeys       &&rank_g_keys_in,
                 proto::ZeroTestKeys &&zt_keys_in);
    ~OblivFMIKeys() = default;

    OblivFMIKeys(const OblivFMIKeys &other)            = delete;
    OblivFMIKeys &operator=(const OblivFMIKeys &other) = delete;
    OblivFMIKeys(OblivFMIKeys &&) noexcept             = default;
    OblivFMIKeys &operator=(OblivFMIKeys &&) noexcept  = default;

    bool operator==(const OblivFMIKeys &rhs) const {
        return (rank_f_keys == rhs.rank_f_keys) &&
               (rank_g_keys == rhs.rank_g_keys) &&
               (zt_keys == rhs.zt_keys);
    }
    bool operator!=(const OblivFMIKeys &rhs) const {
        return !(*this == rhs);
    }

    size_t Size() const {
        return rank_f_keys.Size();
    }

    size_t CalculateSerializedSize() const;
    void   Serialize(std::vector<uint8_t> &buffer) const;
    void   Deserialize(const std::vector<uint8_t> &buffer, const OblivFMIParameters &params);
    void   Deserialize(const std::vector<uint8_t> &buffer, const OblivFMIParameters &params, size_t &offset);
};

struct OblivFMIPreprocessData {
    OblivRankPreprocessData rank_f_data;
    OblivRankPreprocessData rank_g_data;
    proto::ZeroTestKeys     zt_keys;

    OblivFMIPreprocessData() = default;
    OblivFMIPreprocessData(OblivRankPreprocessData &&rank_f_data_in,
                           OblivRankPreprocessData &&rank_g_data_in,
                           proto::ZeroTestKeys     &&zt_keys_in)
        : rank_f_data(std::move(rank_f_data_in)),
          rank_g_data(std::move(rank_g_data_in)),
          zt_keys(std::move(zt_keys_in)) {
    }

    OblivFMIKeys ExtractKeys() &&;

    size_t CalculateSerializedSize() const;
    void   LogSerializedSizeBreakdown(const std::string &prefix) const;
    void   Serialize(std::vector<uint8_t> &buffer) const;
    void   Deserialize(const std::vector<uint8_t> &buffer, const OblivFMIParameters &params);
    void   Deserialize(const std::vector<uint8_t> &buffer, const OblivFMIParameters &params, size_t &offset);
};

struct OblivFMIFscKeys {
    OblivRankFscKeys    rank_f_keys;
    OblivRankFscKeys    rank_g_keys;
    proto::ZeroTestKeys zt_keys;

    OblivFMIFscKeys() = delete;
    OblivFMIFscKeys(const uint64_t id, const OblivFMIParameters &params);
    OblivFMIFscKeys(OblivRankFscKeys    &&rank_f_keys_in,
                    OblivRankFscKeys    &&rank_g_keys_in,
                    proto::ZeroTestKeys &&zt_keys_in);
    ~OblivFMIFscKeys() = default;

    OblivFMIFscKeys(const OblivFMIFscKeys &other)            = delete;
    OblivFMIFscKeys &operator=(const OblivFMIFscKeys &other) = delete;
    OblivFMIFscKeys(OblivFMIFscKeys &&) noexcept             = default;
    OblivFMIFscKeys &operator=(OblivFMIFscKeys &&) noexcept  = default;

    bool operator==(const OblivFMIFscKeys &rhs) const {
        return (rank_f_keys == rhs.rank_f_keys) &&
               (rank_g_keys == rhs.rank_g_keys) &&
               (zt_keys == rhs.zt_keys);
    }
    bool operator!=(const OblivFMIFscKeys &rhs) const {
        return !(*this == rhs);
    }

    size_t Size() const {
        return rank_f_keys.Size();
    }

    size_t CalculateSerializedSize() const;
    void   Serialize(std::vector<uint8_t> &buffer) const;
    void   Deserialize(const std::vector<uint8_t> &buffer, const OblivFMIParameters &params);
    void   Deserialize(const std::vector<uint8_t> &buffer, const OblivFMIParameters &params, size_t &offset);
};

struct OblivFMIFscPreprocessData {
    OblivRankFscPreprocessData rank_f_data;
    OblivRankFscPreprocessData rank_g_data;
    proto::ZeroTestKeys        zt_keys;

    OblivFMIFscPreprocessData() = default;
    OblivFMIFscPreprocessData(OblivRankFscPreprocessData &&rank_f_data_in,
                              OblivRankFscPreprocessData &&rank_g_data_in,
                              proto::ZeroTestKeys        &&zt_keys_in)
        : rank_f_data(std::move(rank_f_data_in)),
          rank_g_data(std::move(rank_g_data_in)),
          zt_keys(std::move(zt_keys_in)) {
    }

    OblivFMIFscKeys ExtractKeys() &&;

    size_t CalculateSerializedSize() const;
    void   LogSerializedSizeBreakdown(const std::string &prefix) const;
    void   Serialize(std::vector<uint8_t> &buffer) const;
    void   Deserialize(const std::vector<uint8_t> &buffer, const OblivFMIParameters &params);
    void   Deserialize(const std::vector<uint8_t> &buffer, const OblivFMIParameters &params, size_t &offset);
};

class OblivFMI {
public:
    OblivFMI() = delete;
    OblivFMI(
        const OblivFMIParameters &params,
        proto::ProtocolContext2P &ctx2p,
        proto::ProtocolContext3P &ctx3p);

    std::array<sharing::Rep3ShareMat64, 3> GenerateDatabaseU64Share(const wm::FMIndex &fm) const;
    void                                   GenerateDatabaseU64Share(const wm::FMIndex                      &fm,
                                                                    std::array<sharing::Rep3ShareMat64, 3> &db_sh,
                                                                    std::array<sharing::Rep3ShareVec64, 3> &aux_sh,
                                                                    std::array<bool, 3>                    &v_sign) const;
    std::array<sharing::Rep3ShareMat64, 3> GenerateQueryU64Share(const wm::FMIndex &fm, std::string &query) const;

    // Preprocessing phase
    OblivFMIPreprocessData    Preprocess(Channels &chls) const;
    OblivFMIFscPreprocessData PreprocessFsc(Channels &chls, bool v_sign) const;

    // Online phase
    void ConsumePreprocessData(OblivFMIPreprocessData &preprocess_data) const;

    void EvaluateLPM(Channels                      &chls,
                     const OblivFMIKeys            &key,
                     std::vector<block>            &uv_prev,
                     std::vector<block>            &uv_next,
                     const sharing::Rep3ShareMat64 &wm_tables,
                     const sharing::Rep3ShareMat64 &query,
                     sharing::Rep3ShareVec64       &result) const;

    void EvaluateLPMPair(Channels                      &chls,
                         const OblivFMIKeys            &key,
                         std::vector<block>            &uv_prev,
                         std::vector<block>            &uv_next,
                         const sharing::Rep3ShareMat64 &wm_tables,
                         const sharing::Rep3ShareMat64 &query,
                         sharing::Rep3ShareVec64       &result) const;

    void EvaluateLPMFsc(Channels                       &chls,
                        const OblivFMIFscKeys          &key,
                        std::vector<block>             &uv_prev,
                        std::vector<block>             &uv_next,
                        const sharing::Rep3ShareMat64  &wm_tables,
                        const sharing::Rep3ShareView64 &aux_sh,
                        const sharing::Rep3ShareMat64  &query,
                        sharing::Rep3ShareVec64        &result) const;

    void EvaluateLPMFscPair(Channels                       &chls,
                            const OblivFMIFscKeys          &key,
                            std::vector<block>             &uv_prev,
                            std::vector<block>             &uv_next,
                            const sharing::Rep3ShareMat64  &wm_tables,
                            const sharing::Rep3ShareView64 &aux_sh,
                            const sharing::Rep3ShareMat64  &query,
                            sharing::Rep3ShareVec64        &result) const;

private:
    OblivFMIParameters        params_;
    OblivRank                 orank_;
    proto::ZeroTest           zt_;
    proto::ProtocolContext2P &ctx2p_;
    proto::ProtocolContext3P &ctx3p_;
};

}    // namespace fmi
}    // namespace ringoa

#endif    // OBLIV_FMI_OBLIV_FMI_H_
