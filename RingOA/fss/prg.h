#ifndef FSS_PRG_H_
#define FSS_PRG_H_

#include <cryptoTools/Crypto/AES.h>

#include "RingOA/utils/block.h"

namespace ringoa {
namespace fss {
namespace prg {

// Clear side selection (avoid bool ambiguity).
enum class Side : uint8_t
{
    kLeft  = 0,
    kRight = 1
};

// Pseudo-random generator based on osuCrypto::AES.
// Usage:
//   auto& prg = PseudoRandomGenerator::GetInstance();
//   block out;
//   prg.Expand(seed, out, Side::Left);   // PRG(seed; key=seed_left)
// Notes:
//   - Not thread-safe.
//   - Keys are fixed for the singleton instance.
//   - AES backend: osuCrypto::AES.

class PseudoRandomGenerator {
public:
    PseudoRandomGenerator(block seedL, block seedR, block valueL, block valueR);

    // PRG for a single block with "seed" keys.
    void Expand(const block &in, block &out, Side side) noexcept;

    // PRG for a single block with "value" keys.
    void ExpandValue(const block &in, block &out, Side side) noexcept;

    // PRG for N blocks with "seed" keys.
    template <size_t N>
    void Expand(const std::array<block, N> &in,
                std::array<block, N>       &out,
                Side                        side) noexcept;

    // Expand with both "seed" keys: out[0]=PRG_left(in), out[1]=PRG_right(in).
    void DoubleExpand(const block &in, std::array<block, 2> &out) noexcept;

    // Expand with both "value" keys.
    void DoubleExpandValue(const block &in, std::array<block, 2> &out) noexcept;

    static PseudoRandomGenerator &GetInstance() noexcept;

private:
    std::array<osuCrypto::AES, 2> aes_seed_;  /**< AES instances for the PRG from osuCrypto. */
    std::array<osuCrypto::AES, 2> aes_value_; /**< AES instances for the PRG from osuCrypto. */
};

}    // namespace prg
}    // namespace fss
}    // namespace ringoa

#endif    // PRG_PRG_H_
