#ifndef UTILS_RNG_H_
#define UTILS_RNG_H_

#include <cryptoTools/Crypto/PRNG.h>

#include "block.h"

namespace ringoa {

class GlobalRng {
public:
    // Call once at program startup.
    // You can override the default seed by passing your own.
    // e.g. std::random_device rd;
    //      GlobalRng::Initialize(osuCrypto::toBlock(rd(), rd()));
    static void Initialize(
        const block seed = osuCrypto::toBlock(0xDEADBEEF, 0xFEEDFACE)) {
        prng().SetSeed(seed);
    }

    // Generate a POD of type T.
    template <typename T>
    static T Rand() {
        static_assert(std::is_standard_layout_v<T> && std::is_trivial_v<T>,
                      "GlobalRng::Rand<T> requires a POD type");
        return prng().get<T>();
    }

    // Generate a random bit.
    static bool RandBit() {
        return prng().getBit() != 0;
    }

private:
    // Thread-local AES-NI PRNG instance.
    static inline osuCrypto::PRNG &prng() {
        static thread_local osuCrypto::PRNG instance;
        return instance;
    }
};

}    // namespace ringoa

#endif    // UTILS_RNG_H_
