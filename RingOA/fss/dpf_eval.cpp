#include "dpf_eval.h"

#include <omp.h>
#include <stack>

#include "RingOA/utils/logger.h"
#include "RingOA/utils/timer.h"
#include "RingOA/utils/to_string.h"
#include "RingOA/utils/utils.h"
#include "prg.h"

namespace ringoa {
namespace fss {
namespace dpf {

DpfEvaluator::DpfEvaluator(const DpfParameters &params)
    : params_(params),
      G_(prg::PseudoRandomGenerator::GetInstance()) {
}

uint64_t DpfEvaluator::EvaluateAt(const DpfKey &key, uint64_t x) const {
    if (!ValidateInput(x)) {
        throw std::invalid_argument("DpfEvaluator::EvaluateAt: invalid input x=" + ToString(x) +
                                    " (expected 0 <= x < 2^" + ToString(params_.GetInputBitsize()) + ")");
    }

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(
        LOC,
        "Evaluate DPF keys: " + std::string(params_.GetEnableEarlyTermination() ? "(optimized)" : "(naive)") +
            ", x=" + ToString(x) +
            ", (n, e)=" + "(" + ToString(params_.GetInputBitsize()) + ", " + ToString(params_.GetOutputBitsize()) + ")");
#endif

    if (params_.GetEnableEarlyTermination()) {
        return EvaluateAtOptimized(key, x);
    } else {
        return EvaluateAtNaive(key, x);
    }
}

void DpfEvaluator::EvaluateAt(const std::vector<DpfKey> &keys, const std::vector<uint64_t> &x, std::vector<uint64_t> &outputs) const {
    if (keys.size() != x.size()) {
        throw std::invalid_argument(
            "DpfEvaluator::EvaluateAt: keys.size() != x.size() (" +
            ToString(keys.size()) + " vs " + ToString(x.size()) + ")");
    }

    if (outputs.size() != x.size()) {
        outputs.resize(x.size());
    }

    for (std::size_t i = 0; i < keys.size(); ++i) {
        outputs[i] = EvaluateAt(keys[i], x[i]);    // may throw; let it propagate
    }
}

uint64_t DpfEvaluator::EvaluateAtNaive(const DpfKey &key, uint64_t x) const {
    uint64_t n = params_.GetInputBitsize();
    uint64_t e = params_.GetOutputBitsize();

    // Get the seed and control bit from the given DPF key
    block seed        = key.init_seed;
    bool  control_bit = key.party_id != 0;

#if LOG_LEVEL >= LOG_LEVEL_TRACE
    Logger::TraceLog(LOC, "Evaluating DPF key (naive)");
    Logger::TraceLog(LOC, "Initial seed: " + Format(seed));
    Logger::TraceLog(LOC, "Initial control bit: " + ToString(control_bit));
#endif

    // Evaluate the DPF key
    std::array<block, 2> expanded_seeds;           // expanded_seeds[keep or lose]
    std::array<bool, 2>  expanded_control_bits;    // expanded_control_bits[keep or lose]

    for (uint64_t i = 0; i < n; ++i) {
        EvaluateNextSeed(i, seed, control_bit, expanded_seeds, expanded_control_bits, key);

        // Update the seed and control bit based on the current bit
        bool current_bit = (x & (1UL << (n - i - 1))) != 0;
        seed             = expanded_seeds[current_bit];
        control_bit      = expanded_control_bits[current_bit];

#if LOG_LEVEL >= LOG_LEVEL_TRACE
        std::string level_str = "|Level=" + ToString(i) + "| ";
        Logger::TraceLog(LOC, level_str + "Current bit: " + ToString(current_bit));
        Logger::TraceLog(LOC, level_str + "Next seed: " + Format(seed));
        Logger::TraceLog(LOC, level_str + "Next control bit: " + ToString(control_bit));
#endif
    }
    // Compute the final output
    G_.Expand(seed, seed, prg::Side::kLeft);
    uint64_t output = Sign(key.party_id) * (Convert(seed, e) + (control_bit * Convert(key.output, e)));
    return Mod2N(output, e);
}

uint64_t DpfEvaluator::EvaluateAtOptimized(const DpfKey &key, uint64_t x) const {
    uint64_t   n    = params_.GetInputBitsize();
    uint64_t   e    = params_.GetOutputBitsize();
    uint64_t   nu   = params_.GetTerminateBitsize();
    OutputType mode = params_.GetOutputType();

    // Get the seed and control bit from the given DPF key
    block seed        = key.init_seed;
    bool  control_bit = key.party_id != 0;

#if LOG_LEVEL >= LOG_LEVEL_TRACE
    Logger::TraceLog(LOC, "Evaluating DPF key (optimized)");
    Logger::TraceLog(LOC, "Initial seed: " + Format(seed));
    Logger::TraceLog(LOC, "Initial control bit: " + ToString(control_bit));
#endif

    // Evaluate the DPF key
    std::array<block, 2> expanded_seeds;           // expanded_seeds[keep or lose]
    std::array<bool, 2>  expanded_control_bits;    // expanded_control_bits[keep or lose]

    for (uint64_t i = 0; i < nu; ++i) {
        EvaluateNextSeed(i, seed, control_bit, expanded_seeds, expanded_control_bits, key);

        // Update the seed and control bit based on the current bit
        bool current_bit = (x & (1UL << (n - i - 1))) != 0;
        seed             = expanded_seeds[current_bit];
        control_bit      = expanded_control_bits[current_bit];

#if LOG_LEVEL >= LOG_LEVEL_TRACE
        std::string level_str = "|Level=" + ToString(i) + "| ";
        Logger::TraceLog(LOC, level_str + "Current bit: " + ToString(current_bit));
        Logger::TraceLog(LOC, level_str + "Next seed: " + Format(seed));
        Logger::TraceLog(LOC, level_str + "Next control bit: " + ToString(control_bit));
#endif
    }

    // Compute the final output
    block    output_block = ComputeOutputBlock(seed, control_bit, key);
    uint64_t x_hat        = GetLowerNBits(x, n - nu);
    uint64_t output       = GetSplitBlockValue(output_block, n - nu, x_hat, mode);
    return Mod2N(output, e);
}

bool DpfEvaluator::ValidateInput(const uint64_t x) const {
    bool valid = true;
    if (x >= (1ULL << params_.GetInputBitsize())) {
        valid = false;
    }
    return valid;
}

void DpfEvaluator::EvaluateFullDomain(const DpfKey &key, std::vector<block> &outputs) const {
    uint64_t nu        = params_.GetTerminateBitsize();
    EvalType fde_type  = params_.GetEvalType();
    uint64_t num_nodes = 1UL << nu;

    // Check output vector size
    if (outputs.size() != num_nodes) {
        throw std::invalid_argument(LOC + " DpfEvaluator::EvaluateFullDomain: output vector size does not match the number of nodes: " +
                                    ToString(num_nodes));
    }

    // Evaluate the DPF key for all possible x values
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(
        LOC,
        "[P" + ToString(key.party_id) + "] Full domain evaluation for block output: " + std::string(params_.GetEnableEarlyTermination() ? "(optimized)" : "(naive)") +
            ", (n, e)=" + "(" + ToString(params_.GetInputBitsize()) + ", " + ToString(params_.GetOutputBitsize()) + ")");
#endif

    switch (fde_type) {
        case EvalType::kBruteForce:
            throw std::invalid_argument(LOC + " DpfEvaluator::EvaluateFullDomain: naive approach is not supported for the block output");

        case EvalType::kRecursive:
            FullDomainRecursive(key, outputs);
            break;

        case EvalType::kHybridBatched:
            FullDomainHybridBatched(key, outputs);
            break;

        default:
            throw std::invalid_argument(LOC + " DpfEvaluator::EvaluateFullDomain: invalid evaluation type: " + GetEvalTypeString(fde_type));
    }
}

void DpfEvaluator::EvaluateFullDomain(const DpfKey &key, std::vector<uint64_t> &outputs) const {
    uint64_t n         = params_.GetInputBitsize();
    uint64_t nu        = params_.GetTerminateBitsize();
    EvalType fde_type  = params_.GetEvalType();
    uint64_t num_nodes = 1UL << nu;

    // Check output vector size
    if (outputs.size() != 1UL << params_.GetInputBitsize()) {
        throw std::invalid_argument(LOC + " DpfEvaluator::EvaluateFullDomain: output vector size does not match the number of nodes: " +
                                    ToString(1UL << params_.GetInputBitsize()));
    }

// Evaluate the DPF key for all possible x values
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(
        LOC,
        "[P" + ToString(key.party_id) + "] Full domain evaluation for uint64_t output: " + std::string(params_.GetEnableEarlyTermination() ? "(optimized)" : "(naive)") +
            ", (n, e)=" + "(" + ToString(params_.GetInputBitsize()) + ", " + ToString(params_.GetOutputBitsize()) + ")");
#endif

    switch (fde_type) {
        case EvalType::kBruteForce: {
            FullDomainBruteForce(key, outputs);
            break;
        }

        case EvalType::kRecursive: {
            std::vector<block> outputs_block(num_nodes);
            FullDomainRecursive(key, outputs_block);
            SplitBlockToFieldVector(outputs_block, n - nu, params_.GetOutputBitsize(), outputs);
            break;
        }

        case EvalType::kHybridBatched: {
            std::vector<block> outputs_block(num_nodes);
            FullDomainHybridBatched(key, outputs_block);
            SplitBlockToFieldVector(outputs_block, n - nu, params_.GetOutputBitsize(), outputs);
            break;
        }

        case EvalType::kIterativeFullDepth: {
            FullDomainIterativeFullDepth(key, outputs);
            break;
        }

        case EvalType::kHybridBatchedFullDepth: {
            FullDomainHybridBatchedFullDepth(key, outputs);
            break;
        }

        default:
            throw std::invalid_argument(LOC + " DpfEvaluator::EvaluateFullDomain: invalid evaluation type: " + GetEvalTypeString(fde_type));
    }
}

void DpfEvaluator::EvaluateNextSeed(
    const uint64_t current_level, const block &current_seed, const bool &current_control_bit,
    std::array<block, 2> &expanded_seeds, std::array<bool, 2> &expanded_control_bits,
    const DpfKey &key) const {
    // Expand the seed and control bits
    G_.DoubleExpand(current_seed, expanded_seeds);
    expanded_control_bits[kLeft]  = GetLsb(expanded_seeds[kLeft]);
    expanded_control_bits[kRight] = GetLsb(expanded_seeds[kRight]);
    SetLsbZero(expanded_seeds[kLeft]);
    SetLsbZero(expanded_seeds[kRight]);

#if LOG_LEVEL >= LOG_LEVEL_TRACE
    std::string level_str = "|Level=" + ToString(current_level) + "| ";
    Logger::TraceLog(LOC, level_str + "Current seed: " + Format(current_seed));
    Logger::TraceLog(LOC, level_str + "Current control bit: " + ToString(current_control_bit));
    Logger::TraceLog(LOC, level_str + "Expanded seed (L): " + Format(expanded_seeds[kLeft]));
    Logger::TraceLog(LOC, level_str + "Expanded seed (R): " + Format(expanded_seeds[kRight]));
    Logger::TraceLog(LOC, level_str + "Expanded control bit (L, R): " + ToString(expanded_control_bits[kLeft]) + ", " + ToString(expanded_control_bits[kRight]));
#endif

    // Apply correction word if control bit is true
    const block mask = key.cw_seed[current_level] & zero_and_all_one[current_control_bit];
    expanded_seeds[kLeft] ^= mask;
    expanded_seeds[kRight] ^= mask;

    const bool control_mask_left  = key.cw_control_left[current_level] & current_control_bit;
    const bool control_mask_right = key.cw_control_right[current_level] & current_control_bit;
    expanded_control_bits[kLeft] ^= control_mask_left;
    expanded_control_bits[kRight] ^= control_mask_right;
}

void DpfEvaluator::FullDomainRecursive(const DpfKey &key, std::vector<block> &outputs) const {
    uint64_t nu = params_.GetTerminateBitsize();

    // Get the seed and control bit from the given DPF key
    block seed        = key.init_seed;
    bool  control_bit = key.party_id != 0;

    Traverse(seed, control_bit, key, nu, 0, outputs);
}

void DpfEvaluator::FullDomainHybridBatched(const DpfKey &key, std::vector<block> &outputs) const {
    uint64_t nu            = params_.GetTerminateBitsize();
    uint64_t remaining_bit = params_.GetInputBitsize() - nu;

    // Breadth-first traversal for 8 nodes
    std::vector<block> start_seeds{key.init_seed}, next_seeds;
    std::vector<bool>  start_control_bits{key.party_id != 0}, next_control_bits;

    for (uint64_t i = 0; i < 3; ++i) {
        std::array<block, 2> expanded_seeds;
        std::array<bool, 2>  expanded_control_bits;
        next_seeds.resize(1UL << (i + 1));
        next_control_bits.resize(1UL << (i + 1));
        for (size_t j = 0; j < start_seeds.size(); j++) {
            EvaluateNextSeed(i, start_seeds[j], start_control_bits[j], expanded_seeds, expanded_control_bits, key);
            next_seeds[j * 2]            = expanded_seeds[kLeft];
            next_seeds[j * 2 + 1]        = expanded_seeds[kRight];
            next_control_bits[j * 2]     = expanded_control_bits[kLeft];
            next_control_bits[j * 2 + 1] = expanded_control_bits[kRight];
        }
        start_seeds        = std::move(next_seeds);
        start_control_bits = std::move(next_control_bits);
    }

    // Initialize the variables
    uint64_t current_level = 0;
    uint64_t current_idx   = 0;
    uint64_t last_depth    = std::max(static_cast<int32_t>(nu) - 3, 0);
    uint64_t last_idx      = 1UL << last_depth;

    // Store the seeds and control bits
    std::array<block, 8>              expanded_seeds;
    std::array<bool, 8>               expanded_control_bits;
    std::vector<std::array<block, 8>> prev_seeds(last_depth + 1);
    std::vector<std::array<bool, 8>>  prev_control_bits(last_depth + 1);

    // Evaluate the DPF key
    for (uint64_t i = 0; i < 8; ++i) {
        prev_seeds[0][i]        = start_seeds[i];
        prev_control_bits[0][i] = start_control_bits[i];
    }

    while (current_idx < last_idx) {
        while (current_level < last_depth) {
            // Expand the seed and control bits
            uint64_t mask        = (current_idx >> (last_depth - 1UL - current_level));
            bool     current_bit = mask & 1UL;

            prg::Side side;
            if (current_bit) {
                side = prg::Side::kRight;
            } else {
                side = prg::Side::kLeft;
            }
            G_.Expand(prev_seeds[current_level], expanded_seeds, side);
            for (uint64_t i = 0; i < 8; ++i) {
                expanded_control_bits[i] = GetLsb(expanded_seeds[i]);
                SetLsbZero(expanded_seeds[i]);
            }

#if LOG_LEVEL >= LOG_LEVEL_TRACE
            std::string level_str = "|Level=" + ToString(current_level) + "| ";
            for (uint64_t i = 0; i < 8; ++i) {
                Logger::TraceLog(LOC, level_str + "Current bit: " + ToString(current_bit));
                Logger::TraceLog(LOC, level_str + "Current seed (" + ToString(i) + "): " + Format(prev_seeds[current_level][i]));
                Logger::TraceLog(LOC, level_str + "Current control bit (" + ToString(i) + "): " + ToString(prev_control_bits[current_level][i]));
                Logger::TraceLog(LOC, level_str + "Expanded seed (" + ToString(i) + "): " + Format(expanded_seeds[i]));
                Logger::TraceLog(LOC, level_str + "Expanded control bit (" + ToString(i) + "): " + ToString(expanded_control_bits[i]));
            }
#endif

            // Apply correction word if control bit is true
            bool  cw_control_bit = current_bit ? key.cw_control_right[current_level + 3] : key.cw_control_left[current_level + 3];
            block cw_seed        = key.cw_seed[current_level + 3];
            expanded_seeds[0] ^= (cw_seed & zero_and_all_one[prev_control_bits[current_level][0]]);
            expanded_seeds[1] ^= (cw_seed & zero_and_all_one[prev_control_bits[current_level][1]]);
            expanded_seeds[2] ^= (cw_seed & zero_and_all_one[prev_control_bits[current_level][2]]);
            expanded_seeds[3] ^= (cw_seed & zero_and_all_one[prev_control_bits[current_level][3]]);
            expanded_seeds[4] ^= (cw_seed & zero_and_all_one[prev_control_bits[current_level][4]]);
            expanded_seeds[5] ^= (cw_seed & zero_and_all_one[prev_control_bits[current_level][5]]);
            expanded_seeds[6] ^= (cw_seed & zero_and_all_one[prev_control_bits[current_level][6]]);
            expanded_seeds[7] ^= (cw_seed & zero_and_all_one[prev_control_bits[current_level][7]]);

            for (uint64_t i = 0; i < 8; ++i) {
                // expanded_seeds[i] ^= (key.cw_seed[current_level + 3] & zero_and_all_one[prev_control_bits[current_level][i]]);
                expanded_control_bits[i] ^= (cw_control_bit & prev_control_bits[current_level][i]);
            }

            // Update the current level
            current_level++;

            // Update the previous seeds and control bits
            for (uint64_t i = 0; i < 8; ++i) {
                prev_seeds[current_level][i]        = expanded_seeds[i];
                prev_control_bits[current_level][i] = expanded_control_bits[i];
            }
        }

        // Seed expansion for the final output
        G_.Expand(prev_seeds[current_level], prev_seeds[current_level], prg::Side::kLeft);

        if (remaining_bit == 2) {
            if (key.party_id) {
                for (uint64_t i = 0; i < 8; ++i) {
                    outputs[i * last_idx + current_idx] = _mm_sub_epi32(zero_block, _mm_add_epi32(prev_seeds[current_level][i], zero_and_all_one[prev_control_bits[current_level][i]] & key.output));
                }
            } else {
                for (uint64_t i = 0; i < 8; ++i) {
                    outputs[i * last_idx + current_idx] = _mm_add_epi32(prev_seeds[current_level][i], zero_and_all_one[prev_control_bits[current_level][i]] & key.output);
                }
            }
        } else if (remaining_bit == 3) {
            if (key.party_id) {
                for (uint64_t i = 0; i < 8; ++i) {
                    outputs[i * last_idx + current_idx] = _mm_sub_epi16(zero_block, _mm_add_epi16(prev_seeds[current_level][i], zero_and_all_one[prev_control_bits[current_level][i]] & key.output));
                }
            } else {
                for (uint64_t i = 0; i < 8; ++i) {
                    outputs[i * last_idx + current_idx] = _mm_add_epi16(prev_seeds[current_level][i], zero_and_all_one[prev_control_bits[current_level][i]] & key.output);
                }
            }
        } else if (remaining_bit == 7) {
            for (uint64_t i = 0; i < 8; ++i) {
                outputs[i * last_idx + current_idx] = prev_seeds[current_level][i] ^ (zero_and_all_one[prev_control_bits[current_level][i]] & key.output);
            }
        } else {
            throw std::invalid_argument(LOC + " DpfEvaluator::EvaluateFullDomain: invalid remaining bit: " + ToString(remaining_bit));
        }

        // Update the current index
        int shift = (current_idx + 1UL) ^ current_idx;
        current_level -= Log2Floor(shift) + 1;
        current_idx++;
    }

#if LOG_LEVEL >= LOG_LEVEL_TRACE
    for (uint64_t i = 0; i < (outputs.size() > 16 ? 16 : outputs.size()); ++i) {
        Logger::TraceLog(LOC, "Output seed (" + ToString(i) + "): " + Format(outputs[i]));
    }
#endif
}

void DpfEvaluator::FullDomainIterativeFullDepth(const DpfKey &key, std::vector<uint64_t> &outputs) const {
    uint64_t n = params_.GetInputBitsize();
    uint64_t e = params_.GetOutputBitsize();

    // Initialize the variables
    uint64_t current_level = 0;
    uint64_t current_idx   = 0;
    uint64_t last_depth    = std::max(static_cast<int32_t>(n), 0);
    uint64_t last_idx      = 1UL << last_depth;

    // Store the seeds and control bits
    block              expanded_seeds;
    bool               expanded_control_bits;
    std::vector<block> prev_seeds(last_depth + 1);
    std::vector<bool>  prev_control_bits(last_depth + 1);

    // Evaluate the DPF key
    prev_seeds[0]        = key.init_seed;
    prev_control_bits[0] = key.party_id != 0;

    while (current_idx < last_idx) {
        while (current_level < last_depth) {
            // Expand the seed and control bits
            uint64_t mask        = (current_idx >> (last_depth - 1UL - current_level));
            bool     current_bit = mask & 1UL;

            prg::Side side;
            if (current_bit) {
                side = prg::Side::kRight;
            } else {
                side = prg::Side::kLeft;
            }
            G_.Expand(prev_seeds[current_level], expanded_seeds, side);
            expanded_control_bits = GetLsb(expanded_seeds);
            SetLsbZero(expanded_seeds);

#if LOG_LEVEL >= LOG_LEVEL_TRACE
            std::string level_str = "|Level=" + ToString(current_level) + "| ";
            Logger::TraceLog(LOC, level_str + "Current bit: " + ToString(current_bit));
            Logger::TraceLog(LOC, level_str + "Current seed: " + Format(prev_seeds[current_level]));
            Logger::TraceLog(LOC, level_str + "Current control bit: " + ToString(static_cast<bool>(prev_control_bits[current_level])));
            Logger::TraceLog(LOC, level_str + "Expanded seed: " + Format(expanded_seeds));
            Logger::TraceLog(LOC, level_str + "Expanded control bit: " + ToString(static_cast<bool>(expanded_control_bits)));
#endif

            // Apply correction word if control bit is true
            bool  cw_control_bit = current_bit ? key.cw_control_right[current_level] : key.cw_control_left[current_level];
            block cw_seed        = key.cw_seed[current_level];
            expanded_seeds ^= (cw_seed & zero_and_all_one[prev_control_bits[current_level]]);

            expanded_control_bits ^= (cw_control_bit & prev_control_bits[current_level]);

            // Update the current level
            current_level++;

            // Update the previous seeds and control bits
            prev_seeds[current_level]        = expanded_seeds;
            prev_control_bits[current_level] = expanded_control_bits;
        }

        // Seed expansion for the final output
        G_.Expand(prev_seeds[current_level], prev_seeds[current_level], prg::Side::kLeft);

        outputs[current_idx] = Sign(key.party_id) * (Convert(prev_seeds[current_level], e) + (prev_control_bits[current_level] * Convert(key.output, e)));

        // Update the current index
        int shift = (current_idx + 1UL) ^ current_idx;
        current_level -= Log2Floor(shift) + 1;
        current_idx++;
    }

#if LOG_LEVEL >= LOG_LEVEL_TRACE
    for (uint64_t i = 0; i < (outputs.size() > 16 ? 16 : outputs.size()); ++i) {
        Logger::TraceLog(LOC, "Output seed (" + ToString(i) + "): " + ToString(outputs[i]));
    }
#endif
}

void DpfEvaluator::FullDomainHybridBatchedFullDepth(const DpfKey &key, std::vector<uint64_t> &outputs) const {
    uint64_t n = params_.GetInputBitsize();
    uint64_t e = params_.GetOutputBitsize();

    // Breadth-first traversal for 8 nodes
    std::vector<block> start_seeds{key.init_seed}, next_seeds;
    std::vector<bool>  start_control_bits{key.party_id != 0}, next_control_bits;

    for (uint64_t i = 0; i < 3; ++i) {
        std::array<block, 2> expanded_seeds;
        std::array<bool, 2>  expanded_control_bits;
        next_seeds.resize(1UL << (i + 1));
        next_control_bits.resize(1UL << (i + 1));
        for (size_t j = 0; j < start_seeds.size(); j++) {
            EvaluateNextSeed(i, start_seeds[j], start_control_bits[j], expanded_seeds, expanded_control_bits, key);
            next_seeds[j * 2]            = expanded_seeds[kLeft];
            next_seeds[j * 2 + 1]        = expanded_seeds[kRight];
            next_control_bits[j * 2]     = expanded_control_bits[kLeft];
            next_control_bits[j * 2 + 1] = expanded_control_bits[kRight];
        }
        start_seeds        = std::move(next_seeds);
        start_control_bits = std::move(next_control_bits);
    }

    // Initialize the variables
    uint64_t current_level = 0;
    uint64_t current_idx   = 0;
    uint64_t last_depth    = std::max(static_cast<int32_t>(n) - 3, 0);
    uint64_t last_idx      = 1UL << last_depth;

    // Store the seeds and control bits
    std::array<block, 8>              expanded_seeds;
    std::array<bool, 8>               expanded_control_bits;
    std::vector<std::array<block, 8>> prev_seeds(last_depth + 1);
    std::vector<std::array<bool, 8>>  prev_control_bits(last_depth + 1);

    // Evaluate the DPF key
    for (uint64_t i = 0; i < 8; ++i) {
        prev_seeds[0][i]        = start_seeds[i];
        prev_control_bits[0][i] = start_control_bits[i];
    }

    while (current_idx < last_idx) {
        while (current_level < last_depth) {
            // Expand the seed and control bits
            uint64_t mask        = (current_idx >> (last_depth - 1UL - current_level));
            bool     current_bit = mask & 1UL;

            prg::Side side;
            if (current_bit) {
                side = prg::Side::kRight;
            } else {
                side = prg::Side::kLeft;
            }
            G_.Expand(prev_seeds[current_level], expanded_seeds, side);
            for (uint64_t i = 0; i < 8; ++i) {
                expanded_control_bits[i] = GetLsb(expanded_seeds[i]);
                SetLsbZero(expanded_seeds[i]);
            }

#if LOG_LEVEL >= LOG_LEVEL_TRACE
            std::string level_str = "|Level=" + ToString(current_level) + "| ";
            for (uint64_t i = 0; i < 8; ++i) {
                Logger::TraceLog(LOC, level_str + "Current bit: " + ToString(current_bit));
                Logger::TraceLog(LOC, level_str + "Current seed (" + ToString(i) + "): " + Format(prev_seeds[current_level][i]));
                Logger::TraceLog(LOC, level_str + "Current control bit (" + ToString(i) + "): " + ToString(prev_control_bits[current_level][i]));
                Logger::TraceLog(LOC, level_str + "Expanded seed (" + ToString(i) + "): " + Format(expanded_seeds[i]));
                Logger::TraceLog(LOC, level_str + "Expanded control bit (" + ToString(i) + "): " + ToString(expanded_control_bits[i]));
            }
#endif

            // Apply correction word if control bit is true
            bool  cw_control_bit = current_bit ? key.cw_control_right[current_level + 3] : key.cw_control_left[current_level + 3];
            block cw_seed        = key.cw_seed[current_level + 3];
            expanded_seeds[0] ^= (cw_seed & zero_and_all_one[prev_control_bits[current_level][0]]);
            expanded_seeds[1] ^= (cw_seed & zero_and_all_one[prev_control_bits[current_level][1]]);
            expanded_seeds[2] ^= (cw_seed & zero_and_all_one[prev_control_bits[current_level][2]]);
            expanded_seeds[3] ^= (cw_seed & zero_and_all_one[prev_control_bits[current_level][3]]);
            expanded_seeds[4] ^= (cw_seed & zero_and_all_one[prev_control_bits[current_level][4]]);
            expanded_seeds[5] ^= (cw_seed & zero_and_all_one[prev_control_bits[current_level][5]]);
            expanded_seeds[6] ^= (cw_seed & zero_and_all_one[prev_control_bits[current_level][6]]);
            expanded_seeds[7] ^= (cw_seed & zero_and_all_one[prev_control_bits[current_level][7]]);

            for (uint64_t i = 0; i < 8; ++i) {
                // expanded_seeds[i] ^= (key.cw_seed[current_level + 3] & zero_and_all_one[prev_control_bits[current_level][i]]);
                expanded_control_bits[i] ^= (cw_control_bit & prev_control_bits[current_level][i]);
            }

            // Update the current level
            current_level++;

            // Update the previous seeds and control bits
            for (uint64_t i = 0; i < 8; ++i) {
                prev_seeds[current_level][i]        = expanded_seeds[i];
                prev_control_bits[current_level][i] = expanded_control_bits[i];
            }
        }

        // Seed expansion for the final output
        G_.Expand(prev_seeds[current_level], prev_seeds[current_level], prg::Side::kLeft);

        for (uint64_t i = 0; i < 8; ++i) {
            outputs[current_idx + i * last_idx] = Sign(key.party_id) * (Convert(prev_seeds[current_level][i], e) + (prev_control_bits[current_level][i] * Convert(key.output, e)));
        }

        // Update the current index
        int shift = (current_idx + 1UL) ^ current_idx;
        current_level -= Log2Floor(shift) + 1;
        current_idx++;
    }

#if LOG_LEVEL >= LOG_LEVEL_TRACE
    for (uint64_t i = 0; i < (outputs.size() > 16 ? 16 : outputs.size()); ++i) {
        Logger::TraceLog(LOC, "Output seed (" + ToString(i) + "): " + Format(outputs[i]));
    }
#endif
}

void DpfEvaluator::FullDomainBruteForce(const DpfKey &key, std::vector<uint64_t> &outputs) const {
    // Evaluate the DPF key for all possible x values
    for (uint64_t x = 0; x < (1UL << params_.GetInputBitsize()); x++) {
        outputs[x] = EvaluateAtNaive(key, x);
    }
}

void DpfEvaluator::Traverse(const block &current_seed, const bool current_control_bit, const DpfKey &key, uint64_t i, uint64_t j, std::vector<block> &outputs) const {
    uint64_t nu = params_.GetTerminateBitsize();

    if (i > 0) {
        // Evaluate the DPF key
        std::array<block, 2> expanded_seeds;           // expanded_seeds[keep or lose]
        std::array<bool, 2>  expanded_control_bits;    // expanded_control_bits[keep or lose]

        EvaluateNextSeed(nu - i, current_seed, current_control_bit, expanded_seeds, expanded_control_bits, key);

        // Traverse the left and right subtrees
        Traverse(expanded_seeds[kLeft], expanded_control_bits[kLeft], key, i - 1, j, outputs);
        Traverse(expanded_seeds[kRight], expanded_control_bits[kRight], key, i - 1, j + (1UL << (i - 1)), outputs);
    } else {
        // Compute the output block
        outputs[j] = ComputeOutputBlock(current_seed, current_control_bit, key);
    }
}

// Compute the mask block and remaining bits
block DpfEvaluator::ComputeOutputBlock(const block &final_seed, bool final_control_bit, const DpfKey &key) const {
    // Compute the remaining bits
    block    mask          = zero_and_all_one[final_control_bit];
    uint64_t remaining_bit = params_.GetInputBitsize() - params_.GetTerminateBitsize();
    block    output        = zero_block;

    // Seed expansion for the final output
    block expanded_seed;
    G_.Expand(final_seed, expanded_seed, prg::Side::kLeft);

    if (remaining_bit == 2) {
        // Reduce 2 levels (2^2=4 nodes) of the tree (Additive share)
        if (key.party_id) {
            output = _mm_sub_epi32(zero_block, _mm_add_epi32(expanded_seed, (mask & key.output)));
        } else {
            output = _mm_add_epi32(expanded_seed, (mask & key.output));
        }
    } else if (remaining_bit == 3) {
        // Reduce 3 levels (2^3=8 nodes) of the tree (Additive share)
        if (key.party_id) {
            output = _mm_sub_epi16(zero_block, _mm_add_epi16(expanded_seed, (mask & key.output)));
        } else {
            output = _mm_add_epi16(expanded_seed, (mask & key.output));
        }
    } else if (remaining_bit == 7) {
        // Reduce 7 levels (2^7=128 nodes) of the tree (Additive share)
        output = expanded_seed ^ (mask & key.output);
    } else {
        throw std::invalid_argument(LOC + " DpfEvaluator::EvaluateFullDomain: unsupported termination bitsize: " + ToString(remaining_bit));
    }

    return output;
}

}    // namespace dpf
}    // namespace fss
}    // namespace ringoa
