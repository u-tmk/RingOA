#ifndef PROTOCOL_DDCF_H_
#define PROTOCOL_DDCF_H_

#include "RingOA/fss/dcf_eval.h"
#include "RingOA/fss/dcf_gen.h"
#include "RingOA/fss/dcf_key.h"

namespace ringoa {
namespace proto {

class DdcfParameters {
public:
    DdcfParameters() = delete;
    explicit DdcfParameters(const uint64_t n, const uint64_t e)
        : params_(n, e) {
    }

    uint64_t GetInputBitsize() const {
        return params_.GetInputBitsize();
    }
    uint64_t GetOutputBitsize() const {
        return params_.GetOutputBitsize();
    }

    const fss::dcf::DcfParameters GetParameters() const {
        return params_;
    }

    void PrintParametersDebug(bool with_header = true, int key_width = 12) const;

private:
    fss::dcf::DcfParameters params_; /**< DCF parameters for the DDCF. */
};

struct DdcfKey {
    fss::dcf::DcfKey dcf_key;
    uint64_t         mask;

    DdcfKey() = delete;
    explicit DdcfKey(const uint64_t id, const DdcfParameters &params);
    ~DdcfKey() = default;

    DdcfKey(const DdcfKey &)                = delete;
    DdcfKey &operator=(const DdcfKey &)     = delete;
    DdcfKey(DdcfKey &&) noexcept            = default;
    DdcfKey &operator=(DdcfKey &&) noexcept = default;

    bool operator==(const DdcfKey &rhs) const {
        return dcf_key == rhs.dcf_key &&
               mask == rhs.mask;
    }
    bool operator!=(const DdcfKey &rhs) const {
        return !(*this == rhs);
    }

    size_t CalculateSerializedSize() const;
    void   Serialize(std::vector<uint8_t> &buffer) const;
    void   Deserialize(const std::vector<uint8_t> &buffer);
    void   Deserialize(const std::vector<uint8_t> &buffer, size_t &offset);

    std::string GetKeyInfo() const;
    void        PrintKey() const;
};

class Ddcf {
public:
    Ddcf() = delete;
    explicit Ddcf(const DdcfParameters &params);

    std::pair<DdcfKey, DdcfKey> GenerateKeys(uint64_t alpha, uint64_t beta_1, uint64_t beta_2) const;

    uint64_t EvaluateAt(const DdcfKey &key, uint64_t x) const;
    void     EvaluateAt(const std::vector<DdcfKey> &keys, const std::vector<uint64_t> &x, std::vector<uint64_t> &outputs) const;

private:
    const DdcfParameters     &params_;
    fss::dcf::DcfKeyGenerator gen_;
    fss::dcf::DcfEvaluator    eval_;
};

}    // namespace proto
}    // namespace ringoa

#endif    // PROTOCOL_DDCF_H_
