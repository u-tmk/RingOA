#ifndef PROTOCOL_MIN3_H_
#define PROTOCOL_MIN3_H_

#include "RingOA/sharing/share_config.h"
#include "context_2p.h"
#include "integer_comparison.h"

namespace osuCrypto {

class Channel;

}    // namespace osuCrypto

namespace ringoa {

namespace sharing {

class AdditiveSharing2P;

}    // namespace sharing

namespace proto {

struct Min3Config {
    // User-facing: DDCF input domain bits used by the internal comparisons.
    // If not set, defaults to ShareConfig.arith_bits.
    // Use smaller value if inputs are known to be in [0, 2^n).
    std::optional<uint64_t> input_domain_bits;

    // Internal comparison domain bits (DDCF input domain).
    uint64_t ResolvedDdcfInBits(const sharing::ShareConfig &share) const {
        return input_domain_bits.value_or(share.arith_bits);
    }
};

class Min3Parameters {
public:
    Min3Parameters() = delete;
    Min3Parameters(const Min3Config &cfg, const sharing::ShareConfig &share)
        : input_domain_bits_(cfg.ResolvedDdcfInBits(share)),
          params_(MakeIcParams(cfg, share)) {
    }

    uint64_t GetInputDomainBits() const {
        return input_domain_bits_;
    }
    uint64_t GetOutputBitsize() const {
        return params_.GetDdcfOutputBitsize();
    }

    const IntegerComparisonParameters &GetParameters() const {
        return params_;
    }
    void PrintParametersDebug(bool with_header = true, int key_width = 20) const;

private:
    uint64_t                    input_domain_bits_ = 0;
    IntegerComparisonParameters params_;

    static IntegerComparisonParameters
    MakeIcParams(const Min3Config &cfg, const sharing::ShareConfig &share) {
        IntegerComparisonConfig ic_cfg;
        if (cfg.input_domain_bits.has_value()) {
            ic_cfg.input_domain_bits = cfg.input_domain_bits.value();
        } else {
            ic_cfg.input_domain_bits = share.arith_bits;
        }
        return IntegerComparisonParameters(ic_cfg, share);
    }
};

struct Min3KeyView {
    IntegerComparisonKeyView ic_key_1;
    IntegerComparisonKeyView ic_key_2;

    Min3KeyView(IntegerComparisonKeyView ic_key_1_,
                IntegerComparisonKeyView ic_key_2_)
        : ic_key_1(std::move(ic_key_1_)),
          ic_key_2(std::move(ic_key_2_)) {
    }
};

struct Min3Keys {
    IntegerComparisonKeys ic_key_1;
    IntegerComparisonKeys ic_key_2;

    Min3Keys() = default;
    explicit Min3Keys(
        const uint64_t        party_id,
        const Min3Parameters &params,
        size_t                count);
    ~Min3Keys() = default;

    Min3Keys(const Min3Keys &)                = delete;
    Min3Keys &operator=(const Min3Keys &)     = delete;
    Min3Keys(Min3Keys &&) noexcept            = default;
    Min3Keys &operator=(Min3Keys &&) noexcept = default;

    bool operator==(const Min3Keys &rhs) const {
        return (ic_key_1 == rhs.ic_key_1) &&
               (ic_key_2 == rhs.ic_key_2);
    }
    bool operator!=(const Min3Keys &rhs) const {
        return !(*this == rhs);
    }

    size_t Size() const {
        return ic_key_1.Size();
    }

    Min3KeyView GetView(size_t index) const {
        if (index >= Size()) {
            throw std::out_of_range("Min3Keys::GetView: index out of range");
        }
        return Min3KeyView(ic_key_1.GetView(index), ic_key_2.GetView(index));
    }

    size_t CalculateSerializedSize() const;
    void   Serialize(std::vector<uint8_t> &buffer) const;
    void   Deserialize(const std::vector<uint8_t> &buffer, const Min3Parameters &params);
    void   Deserialize(const std::vector<uint8_t> &buffer, const Min3Parameters &params, size_t &offset);
};

class Min3 {
public:
    Min3() = delete;
    Min3(const Min3Parameters &params,
         ProtocolContext2P    &ctx);

    void OfflineSetUp(const uint64_t count, const std::string &file_path) const;

    std::pair<Min3Keys, Min3Keys> GenerateKeys(const size_t count) const;

    void OnlineSetUp(const uint64_t party_id, const std::string &file_path);

    uint64_t EvaluateSharedInput(osuCrypto::Channel &chl, const Min3KeyView &key, const std::array<uint64_t, 3> &inputs) const;

    // TODO: Sequential batch evaluation.
    void EvaluateSharedInputBatch(osuCrypto::Channel                         &chl,
                                  const Min3Keys                             &keys,
                                  const std::vector<std::array<uint64_t, 3>> &inputs,
                                  std::vector<uint64_t>                      &out) const;

private:
    const Min3Parameters &params_;
    IntegerComparison     comp_;
    ProtocolContext2P    &ctx_;
};

}    // namespace proto
}    // namespace ringoa

#endif    // PROTOCOL_MIN3_H_
