#include "dcf_eval.h"

#include <stack>

#include "RingOA/utils/logger.h"
#include "RingOA/utils/timer.h"
#include "RingOA/utils/to_string.h"
#include "RingOA/utils/utils.h"
#include "prg.h"

namespace ringoa {
namespace fss {
namespace dcf {

DcfEvaluator::DcfEvaluator(const DcfParameters &params)
    : params_(params),
      G_(prg::PseudoRandomGenerator::GetInstance()) {
}

uint64_t DcfEvaluator::EvaluateAt(const DcfKey &key, uint64_t x) const {
    if (!ValidateInput(x)) {
        throw std::invalid_argument(LOC + " DcfEvaluator::EvaluateAt: invalid input x=" + ToString(x));
    }

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(
        LOC,
        "Evaluate DCF keys: x=" + ToString(x) +
            ", (n, e)=" + "(" + ToString(params_.GetInputBitsize()) + ", " + ToString(params_.GetOutputBitsize()) + ")");
#endif

    uint64_t n = params_.GetInputBitsize();
    uint64_t e = params_.GetOutputBitsize();

    // Get the seed and control bit from the given DCF key
    block    seed        = key.init_seed;
    bool     control_bit = key.party_id != 0;
    uint64_t value       = 0;

    // Evaluate the DCF key
    std::array<block, 2> expanded_seeds;           // expanded_seeds[keep or lose]
    std::array<bool, 2>  expanded_control_bits;    // expanded_control_bits[keep or lose]
    std::array<block, 2> expanded_values;          // expanded_values[keep or lose]

    for (uint64_t i = 0; i < n; ++i) {
        EvaluateNextSeed(i, seed, control_bit, expanded_seeds, expanded_values, expanded_control_bits, key);

        // Update the seed and control bit based on the current bit
        bool current_bit = (x & (1U << (n - i - 1))) != 0;
        value            = Mod2N(Sign(key.party_id != 0) * (Convert(expanded_values[current_bit], e) + (control_bit * key.cw_value[i])) + value, e);
        seed             = expanded_seeds[current_bit];
        control_bit      = expanded_control_bits[current_bit];

#if LOG_LEVEL >= LOG_LEVEL_TRACE
        std::string level_str = "|Level=" + ToString(i) + "| ";
        Logger::TraceLog(LOC, level_str + "Current bit: " + ToString(current_bit));
        Logger::TraceLog(LOC, level_str + "Next seed: " + Format(seed));
        Logger::TraceLog(LOC, level_str + "Next control bit: " + ToString(control_bit));
        Logger::TraceLog(LOC, level_str + "Next value: " + ToString(value));
#endif
    }
    // Compute the final output
    G_.Expand(seed, seed, prg::Side::kLeft);
    uint64_t output = Sign(key.party_id != 0) * (Convert(seed, e) + (control_bit * key.output)) + value;
    return Mod2N(output, e);
}

void DcfEvaluator::EvaluateAt(const std::vector<DcfKey> &keys, const std::vector<uint64_t> &x, std::vector<uint64_t> &outputs) const {
    if (keys.size() != x.size()) {
        throw std::invalid_argument(LOC + " DcfEvaluator::EvaluateAt: keys.size() != x.size()");
    }
    if (outputs.size() != x.size()) {
        outputs.resize(x.size());
    }

    for (std::size_t i = 0; i < keys.size(); ++i) {
        outputs[i] = EvaluateAt(keys[i], x[i]);
    }
}

bool DcfEvaluator::ValidateInput(const uint64_t x) const {
    bool valid = true;
    if (x >= (1UL << params_.GetInputBitsize())) {
        valid = false;
    }
    return valid;
}

void DcfEvaluator::EvaluateNextSeed(
    const uint64_t current_level, const block &current_seed, const bool &current_control_bit,
    std::array<block, 2> &expanded_seeds, std::array<block, 2> &expanded_values, std::array<bool, 2> &expanded_control_bits,
    const DcfKey &key) const {

    // Expand the seed and control bits
    G_.DoubleExpand(current_seed, expanded_seeds);
    G_.DoubleExpandValue(current_seed, expanded_values);
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
    Logger::TraceLog(LOC, level_str + "Expanded value (L): " + Format(expanded_values[kLeft]));
    Logger::TraceLog(LOC, level_str + "Expanded value (R): " + Format(expanded_values[kRight]));
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

}    // namespace dcf
}    // namespace fss
}    // namespace ringoa
