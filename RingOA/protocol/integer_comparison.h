#ifndef PROTOCOL_INTEGER_COMPARISON_H_
#define PROTOCOL_INTEGER_COMPARISON_H_

#include <optional>

#include "RingOA/sharing/share_config.h"
#include "context_2p.h"
#include "ddcf.h"

namespace osuCrypto {

class Channel;

}    // namespace osuCrypto

namespace ringoa {

namespace sharing {

class AdditiveSharing2P;

}    // namespace sharing

namespace proto {

struct IntegerComparisonConfig {
    // User-facing: DDCF input domain bits.
    // If not set, defaults to ShareConfig.arith_bits.
    // Use smaller value if x is known to be in [0, 2^n).
    std::optional<uint64_t> input_domain_bits;

    // Output format:
    //   false -> output is a share in Z_{2^k}
    //   true  -> output is a 1-bit share
    bool boolean_output = false;

    uint64_t ResolvedDdcfInBits(const sharing::ShareConfig &share) const {
        return input_domain_bits.value_or(share.arith_bits);
    }
    uint64_t ResolvedDdcfOutBits(const sharing::ShareConfig &share) const {
        return boolean_output ? 1ULL : share.arith_bits;
    }
};

class IntegerComparisonParameters {
public:
    IntegerComparisonParameters() = delete;
    IntegerComparisonParameters(const IntegerComparisonConfig &cfg, const sharing::ShareConfig &share)
        : input_domain_bits_(cfg.ResolvedDdcfInBits(share)),
          ddcf_out_bits_(cfg.ResolvedDdcfOutBits(share)),
          params_(input_domain_bits_, ddcf_out_bits_) {
    }

    uint64_t GetInputDomainBits() const {
        return input_domain_bits_;
    }
    uint64_t GetDdcfOutputBitsize() const {
        return ddcf_out_bits_;
    }

    const DdcfParameters &GetParameters() const {
        return params_;
    }

    void PrintParametersDebug(bool with_header = true, int key_width = 18) const;

private:
    uint64_t       input_domain_bits_ = 0;
    uint64_t       ddcf_out_bits_     = 0;
    DdcfParameters params_;
};

struct IntegerComparisonKeyView {
    const DdcfKey &ddcf_key;
    uint64_t       shr1_in;
    uint64_t       shr2_in;

    IntegerComparisonKeyView(const DdcfKey &ddcf_key_,
                             const uint64_t shr1_in_,
                             const uint64_t shr2_in_)
        : ddcf_key(ddcf_key_),
          shr1_in(shr1_in_),
          shr2_in(shr2_in_) {
    }
};

struct IntegerComparisonKeys {
    std::vector<DdcfKey>  ddcf_key;
    std::vector<uint64_t> shr1_in;
    std::vector<uint64_t> shr2_in;

    IntegerComparisonKeys() = default;
    explicit IntegerComparisonKeys(
        const uint64_t                     party_id,
        const IntegerComparisonParameters &params,
        size_t                             count);
    ~IntegerComparisonKeys() = default;

    IntegerComparisonKeys(const IntegerComparisonKeys &)                = delete;
    IntegerComparisonKeys &operator=(const IntegerComparisonKeys &)     = delete;
    IntegerComparisonKeys(IntegerComparisonKeys &&) noexcept            = default;
    IntegerComparisonKeys &operator=(IntegerComparisonKeys &&) noexcept = default;

    bool operator==(const IntegerComparisonKeys &rhs) const {
        return (ddcf_key == rhs.ddcf_key) &&
               (shr1_in == rhs.shr1_in) &&
               (shr2_in == rhs.shr2_in);
    }
    bool operator!=(const IntegerComparisonKeys &rhs) const {
        return !(*this == rhs);
    }

    size_t Size() const {
        return ddcf_key.size();
    }

    IntegerComparisonKeyView GetView(size_t index) const {
        if (index >= Size()) {
            throw std::out_of_range("IntegerComparisonKeys::GetView: index out of range");
        }
        return IntegerComparisonKeyView(ddcf_key[index], shr1_in[index], shr2_in[index]);
    }

    size_t CalculateSerializedSize() const;
    void   Serialize(std::vector<uint8_t> &buffer) const;
    void   Deserialize(const std::vector<uint8_t> &buffer, const IntegerComparisonParameters &params);
    void   Deserialize(const std::vector<uint8_t> &buffer, const IntegerComparisonParameters &params, size_t &offset);
};

class IntegerComparison {
public:
    IntegerComparison() = delete;
    IntegerComparison(const IntegerComparisonParameters &params,
                      ProtocolContext2P                 &ctx);

    std::pair<IntegerComparisonKeys, IntegerComparisonKeys> GenerateKeys(const size_t count) const;

    uint64_t EvaluateSharedInput(osuCrypto::Channel &chl, const IntegerComparisonKeyView &key, const uint64_t x1, const uint64_t x2) const;
    uint64_t EvaluateMaskedInput(const IntegerComparisonKeyView &key, const uint64_t x1, const uint64_t x2) const;

    void EvaluateSharedInputBatch(osuCrypto::Channel          &chl,
                                  const IntegerComparisonKeys &keys,
                                  const std::vector<uint64_t> &x1,
                                  const std::vector<uint64_t> &x2,
                                  std::vector<uint64_t>       &out) const;
    void EvaluateMaskedInputBatch(const IntegerComparisonKeys &keys,
                                  const std::vector<uint64_t> &x1,
                                  const std::vector<uint64_t> &x2,
                                  std::vector<uint64_t>       &out) const;

private:
    const IntegerComparisonParameters &params_;
    Ddcf                               ddcf_;
    ProtocolContext2P                 &ctx_;
};

}    // namespace proto
}    // namespace ringoa

#endif    // PROTOCOL_INTEGER_COMPARISON_H_
