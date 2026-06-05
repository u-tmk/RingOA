#ifndef OBLIV_RANGE_OBLIV_RANGE_H_
#define OBLIV_RANGE_OBLIV_RANGE_H_

#include "RingOA/protocol/integer_comparison.h"
#include "RingOA/protocol/ringoa.h"
#include "RingOA/protocol/ringoa_fsc.h"

namespace ringoa {

namespace wm {

class WaveletMatrix;

}    // namespace wm

namespace range {

struct OblivRangeConfig {
    // User-facing: database size in bits.
    // Database size (number of items) is 2^{db_index_bits}.
    explicit OblivRangeConfig(uint64_t db_bits, uint64_t sigma)
        : db_index_bits(db_bits),
          sigma(sigma) {
    }

    uint64_t db_index_bits;
    uint64_t sigma;

    fss::EvalType eval_type = fss::kOptimizedEvalType;
};

class OblivRangeParameters {
public:
    OblivRangeParameters() = delete;
    OblivRangeParameters(const OblivRangeConfig &cfg, const sharing::ShareConfig &share)
        : database_bitsize_(cfg.db_index_bits),
          database_size_(1U << cfg.db_index_bits),
          share_bitsize_(share.arith_bits),
          sigma_(cfg.sigma),
          oa_params_(MakeRingOaParams(cfg, share)),
          ic_params_(MakeIcParams(cfg, share)) {
    }

    uint64_t GetDbIndexBits() const {
        return database_bitsize_;
    }
    uint64_t GetDatabaseSize() const {
        return database_size_;
    }
    uint64_t GetShareBitsize() const {
        return share_bitsize_;
    }
    uint64_t GetSigma() const {
        return sigma_;
    }
    uint64_t GetNumOfOaCalls() const {
        return sigma_ * 2;
    }
    uint64_t GetNumOfIcCalls() const {
        return sigma_;
    }

    const proto::RingOaParameters &GetOaParameters() const {
        return oa_params_;
    }
    const proto::IntegerComparisonParameters &GetIcParameters() const {
        return ic_params_;
    }

    void PrintParametersDebug(bool with_header = true, int key_width = 18) const;

private:
    uint64_t                           database_bitsize_;
    uint64_t                           database_size_;
    uint64_t                           share_bitsize_;
    uint64_t                           sigma_;
    proto::RingOaParameters            oa_params_;
    proto::IntegerComparisonParameters ic_params_;

    static proto::RingOaParameters
    MakeRingOaParams(const OblivRangeConfig &cfg, const sharing::ShareConfig &share) {
        proto::RingOaConfig ringoa_cfg(cfg.db_index_bits);
        ringoa_cfg.eval_type = cfg.eval_type;
        return proto::RingOaParameters(ringoa_cfg, share);
    }

    static proto::IntegerComparisonParameters
    MakeIcParams(const OblivRangeConfig &cfg, const sharing::ShareConfig &share) {
        if (cfg.db_index_bits >= share.arith_bits) {
            throw std::invalid_argument(
                "OblivRange requires share.arith_bits >= db_index_bits + 1 for IntegerComparison");
        }
        proto::IntegerComparisonConfig ic_cfg;
        ic_cfg.boolean_output = false;
        return proto::IntegerComparisonParameters(ic_cfg, share);
    }
};

struct OblivRangeKeys {
    proto::RingOaKeys            oa_keys;
    proto::IntegerComparisonKeys ic_keys;

    OblivRangeKeys() = delete;
    OblivRangeKeys(const uint64_t id, const OblivRangeParameters &params);
    OblivRangeKeys(proto::RingOaKeys            &&oa_keys_in,
                   proto::IntegerComparisonKeys &&ic_keys_in);
    ~OblivRangeKeys() = default;

    OblivRangeKeys(const OblivRangeKeys &)                = delete;
    OblivRangeKeys &operator=(const OblivRangeKeys &)     = delete;
    OblivRangeKeys(OblivRangeKeys &&) noexcept            = default;
    OblivRangeKeys &operator=(OblivRangeKeys &&) noexcept = default;

    bool operator==(const OblivRangeKeys &rhs) const {
        return (oa_keys == rhs.oa_keys) &&
               (ic_keys == rhs.ic_keys);
    }
    bool operator!=(const OblivRangeKeys &rhs) const {
        return !(*this == rhs);
    }

    size_t CalculateSerializedSize() const;
    void   Serialize(std::vector<uint8_t> &buffer) const;
    void   Deserialize(const std::vector<uint8_t> &buffer, const OblivRangeParameters &params);
    void   Deserialize(const std::vector<uint8_t> &buffer, const OblivRangeParameters &params, size_t &offset);
};

struct OblivRangePreprocessData {
    proto::RingOaPreprocessData  oa_data;
    proto::IntegerComparisonKeys ic_keys;

    OblivRangePreprocessData() = default;
    OblivRangePreprocessData(
        proto::RingOaPreprocessData  &&oa_data_in,
        proto::IntegerComparisonKeys &&ic_keys_in)
        : oa_data(std::move(oa_data_in)),
          ic_keys(std::move(ic_keys_in)) {
    }

    OblivRangeKeys ExtractKeys() &&;

    size_t CalculateSerializedSize() const;
    void   LogSerializedSizeBreakdown(const std::string &prefix) const;
    void   Serialize(std::vector<uint8_t> &buffer) const;
    void   Deserialize(const std::vector<uint8_t> &buffer, const OblivRangeParameters &params);
    void   Deserialize(const std::vector<uint8_t> &buffer, const OblivRangeParameters &params, size_t &offset);
};

struct OblivRangeFscKeys {
    proto::RingOaFscKeys         oa_keys;
    proto::IntegerComparisonKeys ic_keys;

    OblivRangeFscKeys() = delete;
    OblivRangeFscKeys(const uint64_t id, const OblivRangeParameters &params);
    OblivRangeFscKeys(proto::RingOaFscKeys         &&oa_keys_in,
                      proto::IntegerComparisonKeys &&ic_keys_in);
    ~OblivRangeFscKeys() = default;

    OblivRangeFscKeys(const OblivRangeFscKeys &)                = delete;
    OblivRangeFscKeys &operator=(const OblivRangeFscKeys &)     = delete;
    OblivRangeFscKeys(OblivRangeFscKeys &&) noexcept            = default;
    OblivRangeFscKeys &operator=(OblivRangeFscKeys &&) noexcept = default;

    bool operator==(const OblivRangeFscKeys &rhs) const {
        return (oa_keys == rhs.oa_keys) &&
               (ic_keys == rhs.ic_keys);
    }
    bool operator!=(const OblivRangeFscKeys &rhs) const {
        return !(*this == rhs);
    }

    size_t CalculateSerializedSize() const;
    void   Serialize(std::vector<uint8_t> &buffer) const;
    void   Deserialize(const std::vector<uint8_t> &buffer, const OblivRangeParameters &params);
    void   Deserialize(const std::vector<uint8_t> &buffer, const OblivRangeParameters &params, size_t &offset);
};

struct OblivRangeFscPreprocessData {
    proto::RingOaFscPreprocessData oa_data;
    proto::IntegerComparisonKeys   ic_keys;

    OblivRangeFscPreprocessData() = default;
    OblivRangeFscPreprocessData(
        proto::RingOaFscPreprocessData &&oa_data_in,
        proto::IntegerComparisonKeys   &&ic_keys_in)
        : oa_data(std::move(oa_data_in)),
          ic_keys(std::move(ic_keys_in)) {
    }

    OblivRangeFscKeys ExtractKeys() &&;

    size_t CalculateSerializedSize() const;
    void   LogSerializedSizeBreakdown(const std::string &prefix) const;
    void   Serialize(std::vector<uint8_t> &buffer) const;
    void   Deserialize(const std::vector<uint8_t> &buffer, const OblivRangeParameters &params);
    void   Deserialize(const std::vector<uint8_t> &buffer, const OblivRangeParameters &params, size_t &offset);
};

class OblivRange {
public:
    OblivRange() = delete;
    OblivRange(
        const OblivRangeParameters &params,
        proto::ProtocolContext2P   &ctx2p,
        proto::ProtocolContext3P   &ctx3p);

    std::array<sharing::Rep3ShareMat64, 3> GenerateDatabaseU64Share(const wm::WaveletMatrix &wm) const;
    void                                   GenerateDatabaseU64Share(const wm::WaveletMatrix                &wm,
                                                                    std::array<sharing::Rep3ShareMat64, 3> &db_sh,
                                                                    std::array<sharing::Rep3ShareVec64, 3> &aux_sh,
                                                                    std::array<bool, 3>                    &v_sign) const;

    // Preprocessing phase
    OblivRangePreprocessData    Preprocess(Channels &chls) const;
    OblivRangeFscPreprocessData PreprocessFsc(Channels &chls, bool v_sign) const;

    // Online phase
    void ConsumePreprocessData(OblivRangePreprocessData &data) const;

    void EvaluateRange(Channels                      &chls,
                       const OblivRangeKeys          &key,
                       std::vector<block>            &uv_prev,
                       std::vector<block>            &uv_next,
                       const sharing::Rep3ShareMat64 &wm_tables,
                       sharing::Rep3Share64          &left_sh,
                       sharing::Rep3Share64          &right_sh,
                       sharing::Rep3Share64          &k_sh,
                       sharing::Rep3Share64          &result) const;

    void EvaluateRangePair(Channels                      &chls,
                           const OblivRangeKeys          &key,
                           std::vector<block>            &uv_prev,
                           std::vector<block>            &uv_next,
                           const sharing::Rep3ShareMat64 &wm_tables,
                           sharing::Rep3Share64          &left_sh,
                           sharing::Rep3Share64          &right_sh,
                           sharing::Rep3Share64          &k_sh,
                           sharing::Rep3Share64          &result) const;

    void EvaluateRangeFsc(Channels                       &chls,
                          const OblivRangeFscKeys        &key,
                          std::vector<block>             &uv_prev,
                          std::vector<block>             &uv_next,
                          const sharing::Rep3ShareMat64  &wm_tables,
                          const sharing::Rep3ShareView64 &aux_sh,
                          sharing::Rep3Share64           &left_sh,
                          sharing::Rep3Share64           &right_sh,
                          sharing::Rep3Share64           &k_sh,
                          sharing::Rep3Share64           &result) const;

    void EvaluateRangeFscPair(Channels                       &chls,
                              const OblivRangeFscKeys        &key,
                              std::vector<block>             &uv_prev,
                              std::vector<block>             &uv_next,
                              const sharing::Rep3ShareMat64  &wm_tables,
                              const sharing::Rep3ShareView64 &aux_sh,
                              sharing::Rep3Share64           &left_sh,
                              sharing::Rep3Share64           &right_sh,
                              sharing::Rep3Share64           &k_sh,
                              sharing::Rep3Share64           &result) const;

private:
    OblivRangeParameters      params_;
    proto::RingOa             ringoa_;
    proto::RingOaFsc          ringoa_fsc_;
    proto::IntegerComparison  ic_;
    proto::ProtocolContext2P &ctx2p_;
    proto::ProtocolContext3P &ctx3p_;
};

}    // namespace range
}    // namespace ringoa

#endif    // OBLIV_RANGE_OBLIV_RANGE_H_
