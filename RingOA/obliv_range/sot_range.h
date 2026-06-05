#ifndef OBLIV_RANGE_SOT_RANGE_H_
#define OBLIV_RANGE_SOT_RANGE_H_

#include "RingOA/protocol/integer_comparison.h"
#include "RingOA/protocol/shared_ot.h"

namespace ringoa {

namespace wm {

class WaveletMatrix;

}    // namespace wm

namespace range {

struct SotRangeConfig {
    // User-facing: database size in bits.
    // Database size (number of items) is 2^{db_index_bits}.
    explicit SotRangeConfig(uint64_t db_bits, uint64_t sigma)
        : db_index_bits(db_bits),
          sigma(sigma) {
    }

    uint64_t db_index_bits;
    uint64_t sigma;

    fss::EvalType eval_type = fss::kOptimizedEvalType;
};

class SotRangeParameters {
public:
    SotRangeParameters() = delete;
    SotRangeParameters(const SotRangeConfig &cfg, const sharing::ShareConfig &share)
        : database_bitsize_(cfg.db_index_bits),
          database_size_(1U << cfg.db_index_bits),
          share_bitsize_(share.arith_bits),
          sigma_(cfg.sigma),
          oa_params_(MakeSharedOtParams(cfg, share)),
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

    const proto::SharedOtParameters &GetSharedOtParameters() const {
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
    proto::SharedOtParameters          oa_params_;
    proto::IntegerComparisonParameters ic_params_;

    static proto::SharedOtParameters
    MakeSharedOtParams(const SotRangeConfig &cfg, const sharing::ShareConfig &share) {
        proto::SharedOtConfig sharedot_cfg(cfg.db_index_bits);

        sharedot_cfg.eval_type = cfg.eval_type;
        return proto::SharedOtParameters(sharedot_cfg, share);
    }

    static proto::IntegerComparisonParameters
    MakeIcParams(const SotRangeConfig &cfg, const sharing::ShareConfig &share) {
        if (cfg.db_index_bits >= share.arith_bits) {
            throw std::invalid_argument(
                "SotRange requires share.arith_bits >= db_index_bits + 1 for IntegerComparison");
        }
        proto::IntegerComparisonConfig ic_cfg;
        ic_cfg.boolean_output = false;
        return proto::IntegerComparisonParameters(ic_cfg, share);
    }
};

struct SotRangeKeys {
    proto::SharedOtKeys          oa_keys;
    proto::IntegerComparisonKeys ic_keys;

    SotRangeKeys() = delete;
    SotRangeKeys(const uint64_t id, const SotRangeParameters &params);
    SotRangeKeys(proto::SharedOtKeys          &&oa_keys_in,
                 proto::IntegerComparisonKeys &&ic_keys_in);
    ~SotRangeKeys() = default;

    SotRangeKeys(const SotRangeKeys &)                = delete;
    SotRangeKeys &operator=(const SotRangeKeys &)     = delete;
    SotRangeKeys(SotRangeKeys &&) noexcept            = default;
    SotRangeKeys &operator=(SotRangeKeys &&) noexcept = default;

    bool operator==(const SotRangeKeys &rhs) const {
        return (oa_keys == rhs.oa_keys) &&
               (ic_keys == rhs.ic_keys);
    }
    bool operator!=(const SotRangeKeys &rhs) const {
        return !(*this == rhs);
    }

    size_t CalculateSerializedSize() const;
    void   Serialize(std::vector<uint8_t> &buffer) const;
    void   Deserialize(const std::vector<uint8_t> &buffer, const SotRangeParameters &params);
    void   Deserialize(const std::vector<uint8_t> &buffer, const SotRangeParameters &params, size_t &offset);
};

struct SotRangePreprocessData {
    proto::SharedOtPreprocessData oa_data;
    proto::IntegerComparisonKeys  ic_keys;

    SotRangePreprocessData() = default;
    SotRangePreprocessData(
        proto::SharedOtPreprocessData &&oa_data_in,
        proto::IntegerComparisonKeys  &&ic_keys_in)
        : oa_data(std::move(oa_data_in)),
          ic_keys(std::move(ic_keys_in)) {
    }

    SotRangeKeys ExtractKeys() &&;

    size_t CalculateSerializedSize() const;
    void   LogSerializedSizeBreakdown(const std::string &prefix) const;
    void   Serialize(std::vector<uint8_t> &buffer) const;
    void   Deserialize(const std::vector<uint8_t> &buffer, const SotRangeParameters &params);
    void   Deserialize(const std::vector<uint8_t> &buffer, const SotRangeParameters &params, size_t &offset);
};

class SotRange {
public:
    SotRange() = delete;
    SotRange(
        const SotRangeParameters &params,
        proto::ProtocolContext2P &ctx2p,
        proto::ProtocolContext3P &ctx3p);

    std::array<sharing::Rep3ShareMat64, 3> GenerateDatabaseU64Share(const wm::WaveletMatrix &wm) const;

    // Preprocessing phase
    SotRangePreprocessData Preprocess(Channels &chls) const;

    // Online phase
    void EvaluateRange(Channels                      &chls,
                       const SotRangeKeys            &key,
                       std::vector<uint64_t>         &uv_prev,
                       std::vector<uint64_t>         &uv_next,
                       const sharing::Rep3ShareMat64 &wm_tables,
                       sharing::Rep3Share64          &left_sh,
                       sharing::Rep3Share64          &right_sh,
                       sharing::Rep3Share64          &k_sh,
                       sharing::Rep3Share64          &result) const;

    void EvaluateRangePair(Channels                      &chls,
                           const SotRangeKeys            &key,
                           std::vector<uint64_t>         &uv_prev,
                           std::vector<uint64_t>         &uv_next,
                           const sharing::Rep3ShareMat64 &wm_tables,
                           sharing::Rep3Share64          &left_sh,
                           sharing::Rep3Share64          &right_sh,
                           sharing::Rep3Share64          &k_sh,
                           sharing::Rep3Share64          &result) const;

private:
    SotRangeParameters        params_;
    proto::SharedOt           sot_;
    proto::IntegerComparison  ic_;
    proto::ProtocolContext2P &ctx2p_;
    proto::ProtocolContext3P &ctx3p_;
};

}    // namespace range
}    // namespace ringoa

#endif    // OBLIV_RANGE_SOT_RANGE_H_
