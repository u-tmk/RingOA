#include "dcf_gen.h"

#include "RingOA/utils/logger.h"
#include "RingOA/utils/rng.h"
#include "RingOA/utils/to_string.h"
#include "RingOA/utils/utils.h"
#include "prg.h"

namespace ringoa {
namespace fss {
namespace dcf {

DcfKeyGenerator::DcfKeyGenerator(const DcfParameters &params)
    : params_(params),
      G_(prg::PseudoRandomGenerator::GetInstance()) {
}

std::pair<DcfKey, DcfKey> DcfKeyGenerator::GenerateKeys(uint64_t alpha, uint64_t beta) const {
    // Validate the input values
    if (!ValidateInput(alpha, beta)) {
        throw std::invalid_argument(LOC + " DcfKeyGenerator::GenerateKeys: invalid input alpha=" + ToString(alpha) + ", beta=" + ToString(beta));
    }

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(
        LOC,
        "Generate DCF keys: (alpha, beta)=(" + ToString(alpha) + ", " + ToString(beta) + ")" +
            ", (n, e)=" + "(" + ToString(params_.GetInputBitsize()) + ", " + ToString(params_.GetOutputBitsize()) + ")");
#endif

    uint64_t n = params_.GetInputBitsize();
    uint64_t e = params_.GetOutputBitsize();

    std::array<DcfKey, 2>     keys     = {DcfKey(0, params_), DcfKey(1, params_)};
    std::pair<DcfKey, DcfKey> key_pair = std::make_pair(std::move(keys[0]), std::move(keys[1]));

    // Set the initial seed and control bits
    block seed_0              = GlobalRng::Rand<block>();
    block seed_1              = GlobalRng::Rand<block>();
    bool  control_bit_0       = 0;
    bool  control_bit_1       = 1;
    key_pair.first.init_seed  = seed_0;
    key_pair.second.init_seed = seed_1;
    uint64_t value            = 0;

#if LOG_LEVEL >= LOG_LEVEL_TRACE
    Logger::TraceLog(LOC, "[P0] Initial seed: " + Format(seed_0));
    Logger::TraceLog(LOC, "[P0] Control bit: " + ToString(control_bit_0));
    Logger::TraceLog(LOC, "[P1] Initial seed: " + Format(seed_1));
    Logger::TraceLog(LOC, "[P1] Control bit: " + ToString(control_bit_1));
    Logger::TraceLog(LOC, "Initial value: " + ToString(value));
#endif

    std::array<block, 2> expanded_seed_0;           // expanded_seed_0[keep or lose]
    std::array<block, 2> expanded_seed_1;           // expanded_seed_1[keep or lose]
    std::array<bool, 2>  expanded_control_bit_0;    // expanded_control_bit_0[keep or lose]
    std::array<bool, 2>  expanded_control_bit_1;    // expanded_control_bit_1[keep or lose]
    std::array<block, 2> expanded_value_0;          // expanded_value_0[keep or lose]
    std::array<block, 2> expanded_value_1;          // expanded_value_1[keep or lose]
    block                seed_correction;           // seed_correction
    std::array<bool, 2>  control_bit_correction;    // control_bit_correction[keep or lose]
    uint64_t             value_correction;

    for (uint64_t i = 0; i < n; i++) {
        // Expand the seed and control bits
        G_.DoubleExpand(seed_0, expanded_seed_0);
        G_.DoubleExpand(seed_1, expanded_seed_1);
        G_.DoubleExpandValue(seed_0, expanded_value_0);
        G_.DoubleExpandValue(seed_1, expanded_value_1);
        expanded_control_bit_0[kLeft]  = GetLsb(expanded_seed_0[kLeft]);
        expanded_control_bit_0[kRight] = GetLsb(expanded_seed_0[kRight]);
        expanded_control_bit_1[kLeft]  = GetLsb(expanded_seed_1[kLeft]);
        expanded_control_bit_1[kRight] = GetLsb(expanded_seed_1[kRight]);
        SetLsbZero(expanded_seed_0[kLeft]);
        SetLsbZero(expanded_seed_0[kRight]);
        SetLsbZero(expanded_seed_1[kLeft]);
        SetLsbZero(expanded_seed_1[kRight]);

#if LOG_LEVEL >= LOG_LEVEL_TRACE
        std::string level_str = "|Level=" + ToString(i) + "| ";
        Logger::TraceLog(LOC, level_str + "[P0] Expanded seed (L) : " + Format(expanded_seed_0[kLeft]));
        Logger::TraceLog(LOC, level_str + "[P0] Expanded seed (R) : " + Format(expanded_seed_0[kRight]));
        Logger::TraceLog(LOC, level_str + "[P0] Expanded value (L): " + Format(expanded_value_0[kLeft]));
        Logger::TraceLog(LOC, level_str + "[P0] Expanded value (R): " + Format(expanded_value_0[kRight]));
        Logger::TraceLog(LOC, level_str + "[P0] Expanded control bit (L, R): " + ToString(expanded_control_bit_0[kLeft]) + ", " + ToString(expanded_control_bit_0[kRight]));
        Logger::TraceLog(LOC, level_str + "[P1] Expanded seed (L) : " + Format(expanded_seed_1[kLeft]));
        Logger::TraceLog(LOC, level_str + "[P1] Expanded seed (R) : " + Format(expanded_seed_1[kRight]));
        Logger::TraceLog(LOC, level_str + "[P1] Expanded value (L): " + Format(expanded_value_1[kLeft]));
        Logger::TraceLog(LOC, level_str + "[P1] Expanded value (R): " + Format(expanded_value_1[kRight]));
        Logger::TraceLog(LOC, level_str + "[P1] Expanded control bit (L, R): " + ToString(expanded_control_bit_1[kLeft]) + ", " + ToString(expanded_control_bit_1[kRight]));
#endif

        // Choose keep or lose path
        bool current_bit = (alpha & (1U << (n - i - 1))) != 0;
        bool keep = current_bit, lose = !current_bit;

        // Compute seed correction
        seed_correction = expanded_seed_0[lose] ^ expanded_seed_1[lose];

        // Value correction
        value_correction = Mod2N(Sign(control_bit_1) * (Convert(expanded_value_1[lose], e) - Convert(expanded_value_0[lose], e) - value), e);
        if (lose == kLeft) {
            value_correction = Mod2N(value_correction + Sign(control_bit_1) * beta, e);
        }

        // Compute control bit correction
        control_bit_correction[kLeft]  = expanded_control_bit_0[kLeft] ^ expanded_control_bit_1[kLeft] ^ current_bit ^ 1;
        control_bit_correction[kRight] = expanded_control_bit_0[kRight] ^ expanded_control_bit_1[kRight] ^ current_bit;

#if LOG_LEVEL >= LOG_LEVEL_TRACE
        Logger::TraceLog(LOC, level_str + "Current bit: " + ToString(current_bit) + " (Keep: " + ToString(keep) + ", Lose: " + ToString(lose) + ")");
        Logger::TraceLog(LOC, level_str + "Seed correction: " + Format(seed_correction));
        Logger::TraceLog(LOC, level_str + "Correction control bit (L, R): " + ToString(control_bit_correction[kLeft]) + ", " + ToString(control_bit_correction[kRight]));
        Logger::TraceLog(LOC, level_str + "Value correction: " + ToString(value_correction));
#endif

        // Set the correction word
        key_pair.first.cw_seed[i]           = seed_correction;
        key_pair.first.cw_control_left[i]   = control_bit_correction[kLeft];
        key_pair.first.cw_control_right[i]  = control_bit_correction[kRight];
        key_pair.first.cw_value[i]          = value_correction;
        key_pair.second.cw_seed[i]          = seed_correction;
        key_pair.second.cw_control_left[i]  = control_bit_correction[kLeft];
        key_pair.second.cw_control_right[i] = control_bit_correction[kRight];
        key_pair.second.cw_value[i]         = value_correction;

        // Update seed and control bits
        value         = Mod2N(value - Convert(expanded_value_1[keep], e) + Convert(expanded_value_0[keep], e) + (Sign(control_bit_1) * value_correction), e);
        seed_0        = expanded_seed_0[keep] ^ (seed_correction & zero_and_all_one[control_bit_0]);
        seed_1        = expanded_seed_1[keep] ^ (seed_correction & zero_and_all_one[control_bit_1]);
        control_bit_0 = expanded_control_bit_0[keep] ^ (control_bit_0 & control_bit_correction[keep]);
        control_bit_1 = expanded_control_bit_1[keep] ^ (control_bit_1 & control_bit_correction[keep]);

#if LOG_LEVEL >= LOG_LEVEL_TRACE
        Logger::TraceLog(LOC, level_str + "[P0] Next seed: " + Format(seed_0));
        Logger::TraceLog(LOC, level_str + "[P0] Next control bit: " + ToString(control_bit_0));
        Logger::TraceLog(LOC, level_str + "[P1] Next seed: " + Format(seed_1));
        Logger::TraceLog(LOC, level_str + "[P1] Next control bit: " + ToString(control_bit_1));
        Logger::TraceLog(LOC, level_str + "Next value: " + ToString(value));
#endif
    }

    // Set the output
    G_.Expand(seed_0, seed_0, prg::Side::kLeft);
    G_.Expand(seed_1, seed_1, prg::Side::kLeft);

    uint64_t output        = Mod2N(Sign(control_bit_1) * (Convert(seed_1, e) - Convert(seed_0, e) - value), e);
    key_pair.first.output  = output;
    key_pair.second.output = output;

#if LOG_LEVEL >= LOG_LEVEL_TRACE
    Logger::DebugLog(LOC, "Output: " + ToString(output));
    key_pair.first.PrintKeyTrace();
    key_pair.second.PrintKeyTrace();
#endif

    // Return the generated keys as a pair.
    return key_pair;
}

bool DcfKeyGenerator::ValidateInput(const uint64_t alpha, const uint64_t beta) const {
    bool valid = true;
    if (alpha >= (1UL << params_.GetInputBitsize()) || beta >= (1UL << params_.GetOutputBitsize())) {
        valid = false;
    }
    return valid;
}

}    // namespace dcf
}    // namespace fss
}    // namespace ringoa
