#ifndef SHARING_SHARE_CONFIG_H_
#define SHARING_SHARE_CONFIG_H_

#include <cstdint>
#include <stdexcept>

namespace ringoa {
namespace sharing {

struct ShareConfig {
    const uint64_t arith_bits;

    ShareConfig() = delete;
    explicit ShareConfig(uint64_t bits) : arith_bits(bits) {
        Validate();
    }

    static ShareConfig Arith8() {
        return ShareConfig(8);
    }
    static ShareConfig Arith16() {
        return ShareConfig(16);
    }
    static ShareConfig Arith32() {
        return ShareConfig(32);
    }
    static ShareConfig Arith64() {
        return ShareConfig(64);
    }
    static ShareConfig Default() {
        return ShareConfig(32);
    }

    static ShareConfig Custom(uint64_t bits) {
        return ShareConfig(bits);
    }

    void Validate() const {
        if (arith_bits == 0 || arith_bits >= 64) {
            throw std::invalid_argument("ShareConfig.arith_bits must be in [1, 64]");
        }
    }
};

}    // namespace sharing
}    // namespace ringoa

#endif    // SHARING_SHARE_CONFIG_H_
