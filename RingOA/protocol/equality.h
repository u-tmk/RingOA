#ifndef PROTOCOL_EQUALITY_H_
#define PROTOCOL_EQUALITY_H_

#include <optional>

#include "RingOA/fss/dpf_eval.h"
#include "RingOA/fss/dpf_gen.h"
#include "RingOA/fss/dpf_key.h"
#include "RingOA/sharing/share_config.h"
#include "context_2p.h"

namespace osuCrypto {

class Channel;

}    // namespace osuCrypto

namespace ringoa {

namespace sharing {

class AdditiveSharing2P;

}    // namespace sharing

namespace proto {

struct EqualityConfig {
    // User-facing: DPF input domain bits.
    // If not set, defaults to ShareConfig.arith_bits.
    // Use smaller value if x is known to be in [0, 2^n).
    std::optional<uint64_t> input_domain_bits;

    // Output format:
    //   false -> output is a share in Z_{2^k}
    //   true  -> output is a 1-bit share
    bool boolean_output = false;

    uint64_t ResolvedDpfInBits(const sharing::ShareConfig &share) const {
        return input_domain_bits.value_or(share.arith_bits);
    }
    uint64_t ResolvedDpfOutBits(const sharing::ShareConfig &share) const {
        return boolean_output ? 1ULL : share.arith_bits;
    }
};

class EqualityParameters {
public:
    EqualityParameters() = delete;
    EqualityParameters(const EqualityConfig &cfg, const sharing::ShareConfig &share)
        : input_domain_bits_(cfg.ResolvedDpfInBits(share)),
          dpf_out_bits_(cfg.ResolvedDpfOutBits(share)),
          params_(input_domain_bits_, dpf_out_bits_) {
    }

    uint64_t GetInputDomainBits() const {
        return input_domain_bits_;
    }
    uint64_t GetDpfOutputBitsize() const {
        return dpf_out_bits_;
    }

    const fss::dpf::DpfParameters &GetParameters() const {
        return params_;
    }

    void PrintParametersDebug(bool with_header = true, int key_width = 18) const;

private:
    uint64_t                input_domain_bits_ = 0;
    uint64_t                dpf_out_bits_      = 0;
    fss::dpf::DpfParameters params_;
};

struct EqualityKeyView {
    const fss::dpf::DpfKey &dpf_key;
    uint64_t                shr1_in;
    uint64_t                shr2_in;

    EqualityKeyView(const fss::dpf::DpfKey &dpf_key_,
                    const uint64_t          shr1_in_,
                    const uint64_t          shr2_in_)
        : dpf_key(dpf_key_),
          shr1_in(shr1_in_),
          shr2_in(shr2_in_) {
    }
};

struct EqualityKeys {
    std::vector<fss::dpf::DpfKey> dpf_key;
    std::vector<uint64_t>         shr1_in;
    std::vector<uint64_t>         shr2_in;

    EqualityKeys() = default;
    explicit EqualityKeys(
        const uint64_t            party_id,
        const EqualityParameters &params,
        size_t                    count);
    ~EqualityKeys() = default;

    EqualityKeys(const EqualityKeys &)                = delete;
    EqualityKeys &operator=(const EqualityKeys &)     = delete;
    EqualityKeys(EqualityKeys &&) noexcept            = default;
    EqualityKeys &operator=(EqualityKeys &&) noexcept = default;

    bool operator==(const EqualityKeys &rhs) const {
        return (dpf_key == rhs.dpf_key) &&
               (shr1_in == rhs.shr1_in) &&
               (shr2_in == rhs.shr2_in);
    }
    bool operator!=(const EqualityKeys &rhs) const {
        return !(*this == rhs);
    }

    size_t Size() const {
        return dpf_key.size();
    }

    EqualityKeyView GetView(size_t index) const {
        if (index >= Size()) {
            throw std::out_of_range("EqualityKeys::GetView: index out of range");
        }
        return EqualityKeyView(dpf_key[index], shr1_in[index], shr2_in[index]);
    }

    size_t CalculateSerializedSize() const;
    void   Serialize(std::vector<uint8_t> &buffer) const;
    void   Deserialize(const std::vector<uint8_t> &buffer, const EqualityParameters &params);
    void   Deserialize(const std::vector<uint8_t> &buffer, const EqualityParameters &params, size_t &offset);
};

class Equality {
public:
    Equality(const EqualityParameters &params, ProtocolContext2P &ctx);

    std::pair<EqualityKeys, EqualityKeys> GenerateKeys(const size_t count) const;

    uint64_t EvaluateSharedInput(osuCrypto::Channel &chl, const EqualityKeyView &key, const uint64_t x1, const uint64_t x2) const;
    uint64_t EvaluateMaskedInput(const EqualityKeyView &key, const uint64_t x1, const uint64_t x2) const;

    void EvaluateSharedInputBatch(osuCrypto::Channel          &chl,
                                  const EqualityKeys          &keys,
                                  const std::vector<uint64_t> &x1,
                                  const std::vector<uint64_t> &x2,
                                  std::vector<uint64_t>       &out) const;
    void EvaluateMaskedInputBatch(const EqualityKeys          &keys,
                                  const std::vector<uint64_t> &x1,
                                  const std::vector<uint64_t> &x2,
                                  std::vector<uint64_t>       &out) const;

private:
    const EqualityParameters &params_;
    fss::dpf::DpfKeyGenerator gen_;
    fss::dpf::DpfEvaluator    eval_;
    ProtocolContext2P        &ctx_;
};

}    // namespace proto
}    // namespace ringoa

#endif    // PROTOCOL_EQUALITY_H_
