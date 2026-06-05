#ifndef FSS_FSS_H_
#define FSS_FSS_H_

#include <cstdint>
#include <vector>

#include "RingOA/utils/block.h"

namespace ringoa {
namespace fss {

constexpr uint64_t kSecurityParameter = 128;
constexpr uint64_t kLeft              = 0;
constexpr uint64_t kRight             = 1;

enum class EvalType
{
    kBruteForce,                  // Naive full-domain evaluation using EvaluateAtNaive() for all x
    kIterativeFullDepth,          // Iterative (loop-based) depth-first traversal without recursion
    kHybridBatchedFullDepth,      // Hybrid method with full-depth BFS + batched AES, no recursion
    kRecursive,                   // Recursive traversal with AES expansion at each level
    kHybridBatched,               // Hybrid method: BFS for first levels + batched AES thereafter
};

enum class OutputType
{
    kShiftedAdditive,
    kSingleBitMask,
};

inline constexpr EvalType kOptimizedEvalType = EvalType::kHybridBatched;

std::string GetEvalTypeString(EvalType t);
std::string GetOutputTypeString(OutputType m);

// Converts a 128-bit block to a uint64_t with lower 'bitsize' bits.
uint64_t Convert(const block &b, uint64_t bitsize);

// Split 128-bit blocks into 2^chunk_exp lanes, mask to 'field_bits', append to 'out'.
// Supported chunk_exp: 2, 3, 7 (4, 8, 128 lanes).
void SplitBlockToFieldVector(const std::vector<block> &blks,
                             uint64_t                  chunk_exp,
                             uint64_t                  field_bits,
                             std::vector<uint64_t>    &out);

// Get lane value after splitting a block by 2^chunk_exp.
uint64_t GetSplitBlockValue(const block &blk,
                            uint64_t     chunk_exp,
                            uint64_t     element_idx,
                            OutputType   mode);

}    // namespace fss
}    // namespace ringoa

#endif    // FSS_FSS_H_
