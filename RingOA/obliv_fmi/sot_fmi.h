#ifndef OBLIV_FMI_SOT_FMI_H_
#define OBLIV_FMI_SOT_FMI_H_

#include "RingOA/protocol/zero_test.h"
#include "sot_rank.h"

namespace ringoa {
namespace fmi {

struct SotFMIConfig {
    // User-facing: database size in bits.
    // Database size (number of items) is 2^{db_index_bits}.
    // Query size is the number of queries to be performed.
    explicit SotFMIConfig(uint64_t db_bits, uint64_t q_size, uint64_t sigma = 3)
        : db_index_bits(db_bits),
          query_size(q_size),
          sigma(sigma) {
    }

    uint64_t db_index_bits;
    uint64_t query_size;
    uint64_t sigma;

    fss::EvalType eval_type = fss::kOptimizedEvalType;
};

class SotFMIParameters {
public:
    SotFMIParameters() = delete;
    SotFMIParameters(const SotFMIConfig &cfg, const sharing::ShareConfig &share)
        : database_bitsize_(cfg.db_index_bits),
          database_size_(1U << cfg.db_index_bits),
          query_size_(cfg.query_size),
          sigma_(cfg.sigma),
          share_bitsize_(share.arith_bits),
          sotrank_params_(MakeSotRankParams(cfg, share)),
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
        return query_size_;
    }
    uint64_t GetNumOfOaCalls() const {
        return sigma_ * query_size_ * 2;
    }
    uint64_t GetNumOfZtCalls() const {
        return query_size_;
    }

    const SotRankParameters &GetSotRankParameters() const {
        return sotrank_params_;
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
    SotRankParameters         sotrank_params_;
    proto::ZeroTestParameters zt_params_;

    static SotRankParameters
    MakeSotRankParams(const SotFMIConfig &cfg, const sharing::ShareConfig &share) {
        SotRankConfig sotrank_cfg(cfg.db_index_bits, cfg.sigma);
        sotrank_cfg.eval_type = cfg.eval_type;
        return SotRankParameters(sotrank_cfg, share);
    }

    static proto::ZeroTestParameters
    MakeZeroTestParams(const SotFMIConfig &cfg, const sharing::ShareConfig &share) {
        proto::ZeroTestConfig zt_cfg;
        if (cfg.db_index_bits == share.arith_bits) {
            zt_cfg.input_domain_bits = cfg.db_index_bits;
        }
        return proto::ZeroTestParameters(zt_cfg, share);
    }
};

struct SotFMIKeys {
    SotRankKeys         rank_f_keys;
    SotRankKeys         rank_g_keys;
    proto::ZeroTestKeys zt_keys;

    SotFMIKeys() = delete;
    SotFMIKeys(const uint64_t id, const SotFMIParameters &params);
    SotFMIKeys(SotRankKeys         &&rank_f_keys_in,
               SotRankKeys         &&rank_g_keys_in,
               proto::ZeroTestKeys &&zt_keys_in)
        : rank_f_keys(std::move(rank_f_keys_in)),
          rank_g_keys(std::move(rank_g_keys_in)),
          zt_keys(std::move(zt_keys_in)) {
    }
    ~SotFMIKeys() = default;

    SotFMIKeys(const SotFMIKeys &other)            = delete;
    SotFMIKeys &operator=(const SotFMIKeys &other) = delete;
    SotFMIKeys(SotFMIKeys &&) noexcept             = default;
    SotFMIKeys &operator=(SotFMIKeys &&) noexcept  = default;

    bool operator==(const SotFMIKeys &rhs) const {
        return (rank_f_keys == rhs.rank_f_keys) &&
               (rank_g_keys == rhs.rank_g_keys) &&
               (zt_keys == rhs.zt_keys);
    }
    bool operator!=(const SotFMIKeys &rhs) const {
        return !(*this == rhs);
    }

    size_t Size() const {
        return rank_f_keys.Size();
    }

    size_t CalculateSerializedSize() const;
    void   Serialize(std::vector<uint8_t> &buffer) const;
    void   Deserialize(const std::vector<uint8_t> &buffer, const SotFMIParameters &params);
    void   Deserialize(const std::vector<uint8_t> &buffer, const SotFMIParameters &params, size_t &offset);
};

struct SotFMIPreprocessData {
    SotRankPreprocessData rank_f_data;
    SotRankPreprocessData rank_g_data;
    proto::ZeroTestKeys   zt_keys;

    SotFMIPreprocessData() = default;
    SotFMIPreprocessData(SotRankPreprocessData &&rank_f_data_in,
                         SotRankPreprocessData &&rank_g_data_in,
                         proto::ZeroTestKeys   &&zt_keys_in)
        : rank_f_data(std::move(rank_f_data_in)),
          rank_g_data(std::move(rank_g_data_in)),
          zt_keys(std::move(zt_keys_in)) {
    }

    SotFMIKeys ExtractKeys() &&;

    size_t CalculateSerializedSize() const;
    void   LogSerializedSizeBreakdown(const std::string &prefix) const;
    void   Serialize(std::vector<uint8_t> &buffer) const;
    void   Deserialize(const std::vector<uint8_t> &buffer, const SotFMIParameters &params);
    void   Deserialize(const std::vector<uint8_t> &buffer, const SotFMIParameters &params, size_t &offset);
};

class SotFMI {
public:
    SotFMI() = delete;
    SotFMI(
        const SotFMIParameters   &params,
        proto::ProtocolContext2P &ctx2p,
        proto::ProtocolContext3P &ctx3p);

    std::array<sharing::Rep3ShareMat64, 3> GenerateDatabaseU64Share(const wm::FMIndex &fm) const;
    std::array<sharing::Rep3ShareMat64, 3> GenerateQueryU64Share(const wm::FMIndex &fm, std::string &query) const;

    // Preprocessing phase
    SotFMIPreprocessData Preprocess(Channels &chls) const;

    // Online phase
    void EvaluateLPM(Channels                      &chls,
                     const SotFMIKeys              &key,
                     std::vector<uint64_t>         &uv_prev,
                     std::vector<uint64_t>         &uv_next,
                     const sharing::Rep3ShareMat64 &wm_tables,
                     const sharing::Rep3ShareMat64 &query,
                     sharing::Rep3ShareVec64       &result) const;

    void EvaluateLPMPair(Channels                      &chls,
                         const SotFMIKeys              &key,
                         std::vector<uint64_t>         &uv_prev,
                         std::vector<uint64_t>         &uv_next,
                         const sharing::Rep3ShareMat64 &wm_tables,
                         const sharing::Rep3ShareMat64 &query,
                         sharing::Rep3ShareVec64       &result) const;

private:
    SotFMIParameters          params_;
    SotRank                   sot_rank_;
    proto::ZeroTest           zt_;
    proto::ProtocolContext2P &ctx2p_;
    proto::ProtocolContext3P &ctx3p_;
};

}    // namespace fmi
}    // namespace ringoa

#endif    // OBLIV_FMI_SOT_FMI_H_
