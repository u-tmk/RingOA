#include "dpf_gen.h"

#include "RingOA/utils/logger.h"
#include "RingOA/utils/rng.h"
#include "RingOA/utils/to_string.h"
#include "RingOA/utils/utils.h"
#include "prg.h"

namespace ringoa {
namespace fss {
namespace dpf {

DpfKeyGenerator::DpfKeyGenerator(const DpfParameters &params)
    : params_(params),
      G_(prg::PseudoRandomGenerator::GetInstance()) {
}

std::pair<DpfKey, DpfKey> DpfKeyGenerator::GenerateKeys(const uint64_t alpha, const uint64_t beta) const {
    // Validate the input values
    if (!ValidateInput(alpha, beta)) {
        throw std::invalid_argument(LOC + " DpfKeyGenerator::GenerateKeys: invalid input alpha=" + ToString(alpha) + ", beta=" + ToString(beta));
    }

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(
        LOC,
        "Generate DPF keys: " + std::string(params_.GetEnableEarlyTermination() ? "(optimized)" : "(naive)") +
            ", (alpha, beta)=" + "(" + ToString(alpha) + ", " + ToString(beta) + ")" +
            ", (n, e)=" + "(" + ToString(params_.GetInputBitsize()) + ", " + ToString(params_.GetOutputBitsize()) + ")");
#endif

    // Initialize the DPF keys
    DpfKey                    key_0(0, params_);
    DpfKey                    key_1(1, params_);
    std::pair<DpfKey, DpfKey> key_pair = std::make_pair(std::move(key_0), std::move(key_1));

    // Generate the DPF key
    if (params_.GetEnableEarlyTermination()) {
        GenerateKeysOptimized(alpha, beta, key_pair);
    } else {
        GenerateKeysNaive(alpha, beta, key_pair);
    }

    return key_pair;
}

std::pair<DpfKey, DpfKey> DpfKeyGenerator::GenerateKeys(const uint64_t alpha, const uint64_t beta, block &final_seed_0, block &final_seed_1, bool &final_control_bit_1) const {
    // Validate the input values
    if (!ValidateInput(alpha, beta)) {
        throw std::invalid_argument(LOC + " DpfKeyGenerator::GenerateKeys: invalid input alpha=" + ToString(alpha) + ", beta=" + ToString(beta));
    }

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(
        LOC,
        "Generate DPF keys: " + std::string(params_.GetEnableEarlyTermination() ? "(optimized)" : "(naive)") +
            ", (alpha, beta)=" + "(" + ToString(alpha) + ", " + ToString(beta) + ")" +
            ", (n, e)=" + "(" + ToString(params_.GetInputBitsize()) + ", " + ToString(params_.GetOutputBitsize()) + ")");
#endif

    // Initialize the DPF keys
    DpfKey                    key_0(0, params_);
    DpfKey                    key_1(1, params_);
    std::pair<DpfKey, DpfKey> key_pair = std::make_pair(std::move(key_0), std::move(key_1));

    // Generate the DPF key
    if (params_.GetEnableEarlyTermination()) {
        GenerateKeysOptimized(alpha, beta, final_seed_0, final_seed_1, final_control_bit_1, key_pair);
    } else {
        GenerateKeysNaive(alpha, beta, final_seed_0, final_seed_1, final_control_bit_1, key_pair);
    }

    return key_pair;
}

void DpfKeyGenerator::GenerateKeysNaive(const uint64_t alpha, const uint64_t beta, std::pair<DpfKey, DpfKey> &key_pair) const {
    block final_seed_0, final_seed_1;
    bool  final_control_bit_1;
    GenerateKeysNaive(alpha, beta, final_seed_0, final_seed_1, final_control_bit_1, key_pair);
}

void DpfKeyGenerator::GenerateKeysNaive(const uint64_t alpha, const uint64_t beta, block &final_seed_0, block &final_seed_1, bool &final_control_bit_1,
                                        std::pair<DpfKey, DpfKey> &key_pair) const {
    uint64_t n = params_.GetInputBitsize();
    uint64_t e = params_.GetOutputBitsize();

    // Set the initial seed and control bits
    block seed_0              = GlobalRng::Rand<block>();
    block seed_1              = GlobalRng::Rand<block>();
    bool  control_bit_0       = 0;
    bool  control_bit_1       = 1;
    key_pair.first.init_seed  = seed_0;
    key_pair.second.init_seed = seed_1;

#if LOG_LEVEL >= LOG_LEVEL_TRACE
    Logger::TraceLog(LOC, Logger::StrWithSep("Generate DPF keys (naive)"));
    Logger::TraceLog(LOC, "[P0] Initial seed: " + Format(seed_0));
    Logger::TraceLog(LOC, "[P0] Control bit: " + ToString(control_bit_0));
    Logger::TraceLog(LOC, "[P1] Initial seed: " + Format(seed_1));
    Logger::TraceLog(LOC, "[P1] Control bit: " + ToString(control_bit_1));
#endif

    // Generate next seed and compute correction words
    for (uint64_t i = 0; i < n; ++i) {
        bool current_bit = (alpha & (1U << (n - i - 1))) != 0;
        GenerateNextSeed(i, current_bit, seed_0, control_bit_0, seed_1, control_bit_1, key_pair);
    }

    // Set the output
    G_.Expand(seed_0, final_seed_0, prg::Side::kLeft);
    G_.Expand(seed_1, final_seed_1, prg::Side::kLeft);
    final_control_bit_1 = control_bit_1;

    uint64_t result        = Mod2N(Sign(final_control_bit_1) * (beta - Convert(final_seed_0, e) + Convert(final_seed_1, e)), e);
    block    output        = MakeBlock(0, result);
    key_pair.first.output  = output;
    key_pair.second.output = output;

#if LOG_LEVEL >= LOG_LEVEL_TRACE
    Logger::DebugLog(LOC, "Output: " + Format(output));
    key_pair.first.PrintKeyTrace();
    key_pair.second.PrintKeyTrace();
#endif
}

void DpfKeyGenerator::GenerateKeysOptimized(const uint64_t alpha, const uint64_t beta, std::pair<DpfKey, DpfKey> &key_pair) const {
    block final_seed_0, final_seed_1;
    bool  final_control_bit_1;
    GenerateKeysOptimized(alpha, beta, final_seed_0, final_seed_1, final_control_bit_1, key_pair);
}

void DpfKeyGenerator::GenerateKeysOptimized(const uint64_t alpha, const uint64_t beta, block &final_seed_0, block &final_seed_1,
                                            bool &final_control_bit_1, std::pair<DpfKey, DpfKey> &key_pair) const {
    uint64_t   n    = params_.GetInputBitsize();
    uint64_t   nu   = this->params_.GetTerminateBitsize();
    OutputType mode = this->params_.GetOutputType();

    // Set the initial seed and control bits
    block seed_0              = GlobalRng::Rand<block>();
    block seed_1              = GlobalRng::Rand<block>();
    bool  control_bit_0       = 0;
    bool  control_bit_1       = 1;
    key_pair.first.init_seed  = seed_0;
    key_pair.second.init_seed = seed_1;

#if LOG_LEVEL >= LOG_LEVEL_TRACE
    Logger::TraceLog(LOC, Logger::StrWithSep("Generate DPF keys (optimized)"));
    Logger::TraceLog(LOC, "[P0] Initial seed: " + Format(seed_0));
    Logger::TraceLog(LOC, "[P0] Control bit: " + ToString(control_bit_0));
    Logger::TraceLog(LOC, "[P1] Initial seed: " + Format(seed_1));
    Logger::TraceLog(LOC, "[P1] Control bit: " + ToString(control_bit_1));
#endif

    // Generate next seed and compute correction words
    for (uint64_t i = 0; i < nu; ++i) {
        bool current_bit = (alpha & (1U << (n - i - 1))) != 0;
        GenerateNextSeed(i, current_bit, seed_0, control_bit_0, seed_1, control_bit_1, key_pair);
    }
    final_seed_0        = seed_0;
    final_seed_1        = seed_1;
    final_control_bit_1 = control_bit_1;

    // Set the output
    if (mode == OutputType::kShiftedAdditive) {
        ComputeAdditiveShiftedOutput(alpha, beta, final_seed_0, final_seed_1, final_control_bit_1, key_pair);
    } else if (mode == OutputType::kSingleBitMask) {
        ComputeSingleBitMaskOutput(alpha, final_seed_0, final_seed_1, key_pair);
    } else {
        throw std::invalid_argument(LOC + " DpfKeyGenerator::GenerateKeys: invalid output mode: " + GetOutputTypeString(mode));
    }

#if LOG_LEVEL >= LOG_LEVEL_TRACE
    key_pair.first.PrintKeyTrace();
    key_pair.second.PrintKeyTrace();
#endif
}

bool DpfKeyGenerator::ValidateInput(const uint64_t alpha, const uint64_t beta) const {
    bool valid = true;
    if (alpha >= (1UL << params_.GetInputBitsize()) || beta >= (1UL << params_.GetOutputBitsize())) {
        valid = false;
    }
    return valid;
}

void DpfKeyGenerator::GenerateNextSeed(const uint64_t current_level, const bool current_bit,
                                       block &current_seed_0, bool &current_control_bit_0,
                                       block &current_seed_1, bool &current_control_bit_1,
                                       std::pair<DpfKey, DpfKey> &key_pair) const {
    std::array<block, 2> expanded_seed_0;           // expanded_seed_0[keep or lose]
    std::array<block, 2> expanded_seed_1;           // expanded_seed_1[keep or lose]
    std::array<bool, 2>  expanded_control_bit_0;    // expanded_control_bit_0[keep or lose]
    std::array<bool, 2>  expanded_control_bit_1;    // expanded_control_bit_1[keep or lose]
    block                seed_correction;           // seed_correction
    std::array<bool, 2>  control_bit_correction;    // control_bit_correction[keep or lose]

    // Expand the seed and control bits
    G_.DoubleExpand(current_seed_0, expanded_seed_0);
    G_.DoubleExpand(current_seed_1, expanded_seed_1);
    expanded_control_bit_0[kLeft]  = GetLsb(expanded_seed_0[kLeft]);
    expanded_control_bit_0[kRight] = GetLsb(expanded_seed_0[kRight]);
    expanded_control_bit_1[kLeft]  = GetLsb(expanded_seed_1[kLeft]);
    expanded_control_bit_1[kRight] = GetLsb(expanded_seed_1[kRight]);
    SetLsbZero(expanded_seed_0[kLeft]);
    SetLsbZero(expanded_seed_0[kRight]);
    SetLsbZero(expanded_seed_1[kLeft]);
    SetLsbZero(expanded_seed_1[kRight]);

#if LOG_LEVEL >= LOG_LEVEL_TRACE
    std::string level_str = "|Level=" + ToString(current_level) + "| ";
    Logger::TraceLog(LOC, level_str + "[P0] Expanded seed (L): " + Format(expanded_seed_0[kLeft]));
    Logger::TraceLog(LOC, level_str + "[P0] Expanded seed (R): " + Format(expanded_seed_0[kRight]));
    Logger::TraceLog(LOC, level_str + "[P0] Expanded control bit (L, R): " + ToString(expanded_control_bit_0[kLeft]) + ", " + ToString(expanded_control_bit_0[kRight]));
    Logger::TraceLog(LOC, level_str + "[P1] Expanded seed (L): " + Format(expanded_seed_1[kLeft]));
    Logger::TraceLog(LOC, level_str + "[P1] Expanded seed (R): " + Format(expanded_seed_1[kRight]));
    Logger::TraceLog(LOC, level_str + "[P1] Expanded control bit (L, R): " + ToString(expanded_control_bit_1[kLeft]) + ", " + ToString(expanded_control_bit_1[kRight]));
#endif

    // Choose keep or lose path
    bool keep = current_bit, lose = !current_bit;

    // Compute seed correction
    seed_correction = expanded_seed_0[lose] ^ expanded_seed_1[lose];

    // Compute control bit correction
    control_bit_correction[kLeft]  = expanded_control_bit_0[kLeft] ^ expanded_control_bit_1[kLeft] ^ current_bit ^ 1;
    control_bit_correction[kRight] = expanded_control_bit_0[kRight] ^ expanded_control_bit_1[kRight] ^ current_bit;

#if LOG_LEVEL >= LOG_LEVEL_TRACE
    Logger::TraceLog(LOC, level_str + "Current bit: " + ToString(current_bit) + " (Keep: " + ToString(keep) + ", Lose: " + ToString(lose) + ")");
    Logger::TraceLog(LOC, level_str + "Seed correction: " + Format(seed_correction));
    Logger::TraceLog(LOC, level_str + "Correction control bit (L, R): " + ToString(control_bit_correction[kLeft]) + ", " + ToString(control_bit_correction[kRight]));
#endif

    // Set the correction word
    key_pair.first.cw_seed[current_level]           = seed_correction;
    key_pair.first.cw_control_left[current_level]   = control_bit_correction[kLeft];
    key_pair.first.cw_control_right[current_level]  = control_bit_correction[kRight];
    key_pair.second.cw_seed[current_level]          = seed_correction;
    key_pair.second.cw_control_left[current_level]  = control_bit_correction[kLeft];
    key_pair.second.cw_control_right[current_level] = control_bit_correction[kRight];

    // Update seed and control bits
    current_seed_0        = expanded_seed_0[keep];
    current_seed_1        = expanded_seed_1[keep];
    current_seed_0        = current_seed_0 ^ (seed_correction & zero_and_all_one[current_control_bit_0]);
    current_seed_1        = current_seed_1 ^ (seed_correction & zero_and_all_one[current_control_bit_1]);
    current_control_bit_0 = expanded_control_bit_0[keep] ^ (current_control_bit_0 & control_bit_correction[keep]);
    current_control_bit_1 = expanded_control_bit_1[keep] ^ (current_control_bit_1 & control_bit_correction[keep]);

#if LOG_LEVEL >= LOG_LEVEL_TRACE
    Logger::TraceLog(LOC, level_str + "[P0] Next seed: " + Format(current_seed_0));
    Logger::TraceLog(LOC, level_str + "[P0] Next control bit: " + ToString(current_control_bit_0));
    Logger::TraceLog(LOC, level_str + "[P1] Next seed: " + Format(current_seed_1));
    Logger::TraceLog(LOC, level_str + "[P1] Next control bit: " + ToString(current_control_bit_1));
#endif
}

void DpfKeyGenerator::ComputeAdditiveShiftedOutput(uint64_t alpha, uint64_t beta,
                                                   block &final_seed_0, block &final_seed_1, bool final_control_bit_1,
                                                   std::pair<DpfKey, DpfKey> &key_pair) const {
    // Compute the remaining bits and alpha_hat
    uint64_t remaining_bit = params_.GetInputBitsize() - params_.GetTerminateBitsize();
    uint64_t alpha_hat     = GetLowerNBits(alpha, remaining_bit);
    block    output        = zero_block;

    // Seed expansion for the final output
    G_.Expand(final_seed_0, final_seed_0, prg::Side::kLeft);
    G_.Expand(final_seed_1, final_seed_1, prg::Side::kLeft);

    block beta_block = MakeBlock(0, beta);

    // Shift the beta block
    uint8_t shift_amount = (kSecurityParameter / (1U << remaining_bit)) * alpha_hat;
    if (shift_amount >= 64) {
        beta_block = beta_block.mm_slli_si128<8>();    // Shift left by 8 bytes (64 bits)
        beta_block = beta_block << (shift_amount - 64);
    } else {
        // The shift of the upper bits is not necessary because the beta is 32 bits or less.
        beta_block = beta_block << shift_amount;
    }

#if LOG_LEVEL >= LOG_LEVEL_TRACE
    Logger::TraceLog(LOC, "Remaining bits: " + ToString(remaining_bit));
    Logger::TraceLog(LOC, "Alpha_hat: " + ToString(alpha_hat));
    Logger::TraceLog(LOC, "Shift amount: " + ToString(shift_amount));
    Logger::TraceLog(LOC, "Beta block: " + Format(beta_block));
#endif

    // Set the output block
    if (remaining_bit == 2) {
        if (final_control_bit_1) {
            // Reduce 2 levels (2^2=4 nodes) of the tree (Additive share)
            output = _mm_sub_epi32(zero_block, _mm_add_epi32(_mm_sub_epi32(beta_block, final_seed_0), final_seed_1));
        } else {
            output = _mm_add_epi32(_mm_sub_epi32(beta_block, final_seed_0), final_seed_1);
        }
    } else if (remaining_bit == 3) {
        // Reduce 3 levels (2^3=8 nodes) of the tree (Additive share)
        if (final_control_bit_1) {
            output = _mm_sub_epi16(zero_block, _mm_add_epi16(_mm_sub_epi16(beta_block, final_seed_0), final_seed_1));
        } else {
            output = _mm_add_epi16(_mm_sub_epi16(beta_block, final_seed_0), final_seed_1);
        }
    } else if (remaining_bit == 7) {
        output = beta_block ^ final_seed_0 ^ final_seed_1;
    } else {
        throw std::invalid_argument(LOC + " DpfKeyGenerator::GenerateKeys: unsupported termination bitsize: " + ToString(remaining_bit));
    }

    key_pair.first.output  = output;
    key_pair.second.output = output;
}

void DpfKeyGenerator::ComputeSingleBitMaskOutput(uint64_t alpha, block &final_seed_0, block &final_seed_1,
                                                 std::pair<DpfKey, DpfKey> &key_pair) const {
    // Compute the remaining bits and alpha_hat
    uint64_t remaining_bit = params_.GetInputBitsize() - params_.GetTerminateBitsize();
    uint64_t alpha_hat     = GetLowerNBits(alpha, remaining_bit);
    block    output        = zero_block;

    // Seed expansion for the final output
    G_.Expand(final_seed_0, final_seed_0, prg::Side::kLeft);
    G_.Expand(final_seed_1, final_seed_1, prg::Side::kLeft);

    final_seed_0 ^= final_seed_1;    // XOR the final seeds for 7 bits

    uint64_t byte_idx = alpha_hat % 16;
    uint64_t bit_idx  = alpha_hat / 16;
#if LOG_LEVEL >= LOG_LEVEL_TRACE
    Logger::TraceLog(LOC, "Remaining bits: " + ToString(remaining_bit));
    Logger::TraceLog(LOC, "Alpha_hat: " + ToString(alpha_hat));
    Logger::TraceLog(LOC, "byte_idx: " + ToString(byte_idx) + ", bit_idx: " + ToString(bit_idx));
#endif

    auto seed_bytes = reinterpret_cast<uint8_t *>(&final_seed_0);
    seed_bytes[byte_idx] ^= static_cast<uint8_t>(1) << bit_idx;

    output = final_seed_0;

    key_pair.first.output  = output;
    key_pair.second.output = output;
}

}    // namespace dpf
}    // namespace fss
}    // namespace ringoa
