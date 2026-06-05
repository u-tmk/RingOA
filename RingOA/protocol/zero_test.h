#ifndef PROTOCOL_ZERO_TEST_H_
#define PROTOCOL_ZERO_TEST_H_

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

struct ZeroTestConfig {
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

class ZeroTestParameters {
public:
    ZeroTestParameters() = delete;
    ZeroTestParameters(const ZeroTestConfig &cfg, const sharing::ShareConfig &share)
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

struct ZeroTestKeyView {
    const fss::dpf::DpfKey &dpf_key;
    uint64_t                shr_in;

    ZeroTestKeyView(const fss::dpf::DpfKey &dpf_key_,
                    const uint64_t          shr_in_)
        : dpf_key(dpf_key_), shr_in(shr_in_) {
    }
};

struct ZeroTestKeys {
    std::vector<fss::dpf::DpfKey> dpf_key;
    std::vector<uint64_t>         shr_in;

    ZeroTestKeys() = default;
    explicit ZeroTestKeys(
        const uint64_t            party_id,
        const ZeroTestParameters &params,
        size_t                    count);
    ~ZeroTestKeys() = default;

    ZeroTestKeys(const ZeroTestKeys &)                = delete;
    ZeroTestKeys &operator=(const ZeroTestKeys &)     = delete;
    ZeroTestKeys(ZeroTestKeys &&) noexcept            = default;
    ZeroTestKeys &operator=(ZeroTestKeys &&) noexcept = default;

    bool operator==(const ZeroTestKeys &rhs) const {
        return (dpf_key == rhs.dpf_key) &&
               (shr_in == rhs.shr_in);
    }
    bool operator!=(const ZeroTestKeys &rhs) const {
        return !(*this == rhs);
    }

    size_t Size() const {
        return dpf_key.size();
    }

    ZeroTestKeyView GetView(size_t index) const {
        if (index >= Size()) {
            throw std::out_of_range("ZeroTestKeys::GetView: index out of range");
        }
        return ZeroTestKeyView(dpf_key[index], shr_in[index]);
    }

    size_t CalculateSerializedSize() const;
    void   Serialize(std::vector<uint8_t> &buffer) const;
    void   Deserialize(const std::vector<uint8_t> &buffer, const ZeroTestParameters &params);
    void   Deserialize(const std::vector<uint8_t> &buffer, const ZeroTestParameters &params, size_t &offset);
};

class ZeroTest {
public:
    ZeroTest() = delete;
    ZeroTest(const ZeroTestParameters &params,
             ProtocolContext2P        &ctx);

    std::pair<ZeroTestKeys, ZeroTestKeys> GenerateKeys(const size_t count) const;

    uint64_t EvaluateSharedInput(osuCrypto::Channel &chl, const ZeroTestKeyView &key, const uint64_t x) const;
    uint64_t EvaluateMaskedInput(const ZeroTestKeyView &key, const uint64_t x) const;

    void EvaluateSharedInputBatch(osuCrypto::Channel          &chl,
                                  const ZeroTestKeys          &key_batch,
                                  const std::vector<uint64_t> &x,
                                  std::vector<uint64_t>       &outputs) const;
    void EvaluateMaskedInputBatch(const ZeroTestKeys          &key_batch,
                                  const std::vector<uint64_t> &x,
                                  std::vector<uint64_t>       &outputs) const;

private:
    const ZeroTestParameters &params_;
    fss::dpf::DpfKeyGenerator gen_;
    fss::dpf::DpfEvaluator    eval_;
    ProtocolContext2P        &ctx_;
};

}    // namespace proto
}    // namespace ringoa

#endif    // PROTOCOL_ZERO_TEST_H_
