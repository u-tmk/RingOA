#include "fss.h"

#include "RingOA/utils/to_string.h"

namespace ringoa {
namespace fss {

std::string GetEvalTypeString(const EvalType eval_type) {
    switch (eval_type) {
        case EvalType::kBruteForce:
            return "BruteForce";
        case EvalType::kRecursive:
            return "Recursive";
        case EvalType::kHybridBatched:
            return "HybridBatched";
        case EvalType::kIterativeFullDepth:
            return "IterativeFullDepth";
        case EvalType::kHybridBatchedFullDepth:
            return "HybridBatchedFullDepth";
        default:
            return "Unknown";
    }
}

std::string GetOutputTypeString(const OutputType mode) {
    switch (mode) {
        case OutputType::kShiftedAdditive:
            return "Additive";
        case OutputType::kSingleBitMask:
            return "BinaryPoint";
        default:
            return "Unknown";
    }
}

uint64_t Convert(const block &b, const uint64_t bitsize) {
    return b.get<uint64_t>()[0] & ((1U << bitsize) - 1U);
}

void SplitBlockToFieldVector(
    const std::vector<block> &blks,
    uint64_t                  chunk_exp,
    uint64_t                  field_bits,
    std::vector<uint64_t>    &out) {
    const size_t   num_blocks  = blks.size();
    const size_t   chunk_count = size_t{1} << chunk_exp;
    const uint64_t mask        = (field_bits >= 64)
                                     ? ~0ULL
                                     : ((1ULL << field_bits) - 1ULL);

    out.resize(num_blocks * chunk_count);

    alignas(16) uint8_t raw_bytes[16];

    switch (chunk_exp) {
        case 2: {    // 32bit ×4
            for (size_t i = 0; i < num_blocks; ++i) {
                _mm_store_si128(reinterpret_cast<__m128i *>(raw_bytes), blks[i]);
                auto   data32 = reinterpret_cast<const uint32_t *>(raw_bytes);
                size_t base   = i * chunk_count;
                out[base + 0] = data32[0] & mask;
                out[base + 1] = data32[1] & mask;
                out[base + 2] = data32[2] & mask;
                out[base + 3] = data32[3] & mask;
            }
            break;
        }
        case 3: {    // 16bit ×8
            for (size_t i = 0; i < num_blocks; ++i) {
                _mm_store_si128(reinterpret_cast<__m128i *>(raw_bytes), blks[i]);
                auto   data16 = reinterpret_cast<const uint16_t *>(raw_bytes);
                size_t base   = i * chunk_count;
                out[base + 0] = data16[0] & mask;
                out[base + 1] = data16[1] & mask;
                out[base + 2] = data16[2] & mask;
                out[base + 3] = data16[3] & mask;
                out[base + 4] = data16[4] & mask;
                out[base + 5] = data16[5] & mask;
                out[base + 6] = data16[6] & mask;
                out[base + 7] = data16[7] & mask;
            }
            break;
        }
        case 7: {
            // 1bit ×128
            for (size_t i = 0; i < num_blocks; ++i) {
                _mm_store_si128(reinterpret_cast<__m128i *>(raw_bytes), blks[i]);
                size_t base = i * chunk_count;
                for (size_t bit = 0; bit < 128; ++bit) {
                    uint8_t  byte   = raw_bytes[bit / 8];
                    uint64_t bitval = uint64_t((byte >> (bit & 7)) & 1);
                    out[base + bit] = bitval & mask;
                }
            }
            break;
        }
        default:
            throw std::invalid_argument("Unsupported chunk_exp: " + ToString(chunk_exp));
    }
}

uint64_t GetSplitBlockValue(
    const block &blk,
    uint64_t     chunk_exp,
    uint64_t     element_idx,
    OutputType   mode) {

    const size_t count = size_t{1} << chunk_exp;
    if (element_idx >= count)
        throw std::out_of_range("element_idx out of range");

    alignas(16) uint8_t bytes[16];
    _mm_store_si128(reinterpret_cast<__m128i *>(bytes), blk);

    switch (chunk_exp) {
        case 2: {    // 4 element × 32bit
            auto data32 = reinterpret_cast<const uint32_t *>(bytes);
            return uint64_t(data32[element_idx]);
        }
        case 3: {    // 8 element × 16bit
            auto data16 = reinterpret_cast<const uint16_t *>(bytes);
            return uint64_t(data16[element_idx]);
        }
        case 7: {    // 128 element × 1bit
            if (mode == OutputType::kShiftedAdditive) {
                uint64_t low  = blk.get<uint64_t>()[0];
                uint64_t high = blk.get<uint64_t>()[1];
                if (element_idx < 64) {
                    return (low >> element_idx) & 0x01;
                } else {
                    return (high >> (element_idx - 64)) & 0x01;
                }
            } else if (mode == OutputType::kSingleBitMask) {
                uint64_t byte_idx   = element_idx % 16;
                uint64_t bit_idx    = element_idx / 16;
                auto     seed_bytes = reinterpret_cast<const uint8_t *>(&blk);
                return (seed_bytes[byte_idx] >> bit_idx) & 0x01;
            } else {
                throw std::invalid_argument("Unsupported OutputType for chunk_exp 7: " + GetOutputTypeString(mode));
            }
        }
        default:
            throw std::invalid_argument("Unsupported chunk_exp: " + ToString(chunk_exp));
    }
}

}    // namespace fss
}    // namespace ringoa
