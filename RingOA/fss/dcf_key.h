#ifndef FSS_DCF_KEY_H_
#define FSS_DCF_KEY_H_

#include <memory>

#include "fss.h"

namespace ringoa {
namespace fss {
namespace dcf {

/**
 * @brief A class to hold params for the Distributed Comparison Function (DCF).
 */
class DcfParameters {
public:
    DcfParameters() = delete;
    explicit DcfParameters(const uint64_t n, const uint64_t e);
    void ValidateParameters() const;

    uint64_t GetInputBitsize() const {
        return input_bitsize_;
    }
    uint64_t GetOutputBitsize() const {
        return element_bitsize_;
    }

    void PrintParametersDebug(bool with_header = true, int key_width = 12) const;

private:
    uint64_t input_bitsize_;
    uint64_t element_bitsize_;
};

struct DcfKey {
    uint64_t                    party_id;
    block                       init_seed;
    uint64_t                    cw_length;
    std::unique_ptr<block[]>    cw_seed;
    std::unique_ptr<bool[]>     cw_control_left;
    std::unique_ptr<bool[]>     cw_control_right;
    std::unique_ptr<uint64_t[]> cw_value;
    uint64_t                    output;

    DcfKey() = delete;
    explicit DcfKey(const uint64_t id, const DcfParameters &params);
    ~DcfKey() = default;

    DcfKey(const DcfKey &)                = delete;
    DcfKey &operator=(const DcfKey &)     = delete;
    DcfKey(DcfKey &&) noexcept            = default;
    DcfKey &operator=(DcfKey &&) noexcept = default;

    bool operator==(const DcfKey &rhs) const;
    bool operator!=(const DcfKey &rhs) const {
        return !(*this == rhs);
    }

    size_t CalculateSerializedSize() const;
    void   Serialize(std::vector<uint8_t> &buffer) const;
    void   Deserialize(const std::vector<uint8_t> &buffer);
    void   Deserialize(const std::vector<uint8_t> &buffer, size_t &offset);

    std::string GetKeyInfo() const;
    void        PrintKeyTrace() const;
};

}    // namespace dcf
}    // namespace fss
}    // namespace ringoa

#endif    // FSS_DCF_KEY_H_
