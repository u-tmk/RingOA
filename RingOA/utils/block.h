#ifndef UTILS_BLOCK_H_
#define UTILS_BLOCK_H_

#include <cryptoTools/Common/block.h>

namespace ringoa {

using block = osuCrypto::block;

/**
 * Small helpers around osuCrypto::block (128-bit).
 * - Portable fallbacks where SSE4.1 might be unavailable.
 */
inline block MakeBlock(uint64_t high, uint64_t low) {
    return osuCrypto::toBlock(high, low);
}

inline bool GetLsb(const block &x) {
    // Extract low 64 bits then check bit-0
    return (_mm_extract_epi64(x.mData, 0) & 1) != 0;
}

inline void SetLsbZero(block &x) {
    // Clear bit 0 of the low 64-bit lane.
    x.mData = _mm_andnot_si128(_mm_set_epi64x(0, 1), x.mData);
}

inline bool GetBit(block &block, uint64_t bit_position) {
    if (bit_position < 64) {
        return (block.get<uint64_t>()[0] >> bit_position) & 1;
    } else {
        return (block.get<uint64_t>()[1] >> (bit_position - 64)) & 1;
    }
}

// inline variables (ODR-safe single definition across TUs)
inline const block                zero_block         = MakeBlock(0, 0);
inline const block                one_block          = MakeBlock(0, 1);
inline const block                not_one_block      = MakeBlock(~0ull, ~1ull);
inline const block                all_one_block      = MakeBlock(~0ull, ~0ull);
inline const std::array<block, 2> zero_and_all_one   = {zero_block, all_one_block};
inline const block                all_bytes_one_mask = block{_mm_set1_epi8(0x01)};

}    // namespace ringoa

#endif    // UTILS_BLOCK_H_
