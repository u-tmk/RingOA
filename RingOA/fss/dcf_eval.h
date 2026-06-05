#ifndef FSS_DCF_EVAL_H_
#define FSS_DCF_EVAL_H_

#include "dcf_key.h"

namespace ringoa {
namespace fss {

namespace prg {

class PseudoRandomGenerator;

}    // namespace prg

namespace dcf {

/**
 * DcfEvaluator — evaluate Distributed Comparison Function (DCF) keys.
 *
 * Purpose
 * - For a threshold alpha and payload beta encoded in a DCF key, returns the party’s share of:
 *     f_{alpha, beta}(x) = beta · [x < alpha]
 *   Semantics follow DcfParameters::GetOutputType().
 *
 * Output semantics
 * - OutputType::kShiftedAdditive : returns an e-bit share; two parties add mod 2^e to reconstruct beta·[x < alpha].
 * - OutputType::kSingleBitMask   : returns a 0/1 share; two parties XOR to reconstruct [x < alpha].
 *
 * Usage
 *   DcfParameters params(n, e, ** eval type **, ** output type **);
 *   dcf::DcfEvaluator eval(params);
 *   uint64_t y = eval.EvaluateAt(key, x);
 *
 * Strategy (driven by params.GetEvalType())
 * - Naive: expand all n tree levels.
 * - Optimized with early termination: expand only nu = params.GetTerminateBitsize() levels, then finish from the final seed/control bit.
 *
 * Contracts
 * - key must be generated with the same (n, e, EvalType, OutputType) configuration.
 * - Input domain: 0 <= x < 2^n where n = params.GetInputBitsize().
 * - Return value:
 *     • ShiftedAdditive: fits e bits (caller may mask to 2^e if desired).
 *     • SingleBitMask  : is 0 or 1.
 *
 * Complexity
 * - EvaluateAt: O(n) seed expansions; O(1) auxiliary memory.
 *
 * Determinism
 * - Deterministic for a fixed key and parameters; randomness derives solely from the PRG used during key generation and expansion.
 *
 */

class DcfEvaluator {
public:
    DcfEvaluator() = delete;
    explicit DcfEvaluator(const DcfParameters &params);

    uint64_t EvaluateAt(const DcfKey &key, uint64_t x) const;
    void     EvaluateAt(const std::vector<DcfKey> &keys, const std::vector<uint64_t> &x, std::vector<uint64_t> &outputs) const;

private:
    DcfParameters               params_;
    prg::PseudoRandomGenerator &G_;

    bool ValidateInput(const uint64_t x) const;

    void EvaluateNextSeed(
        const uint64_t current_level, const block &current_seed, const bool &current_control_bit,
        std::array<block, 2> &expanded_seeds, std::array<block, 2> &expanded_values, std::array<bool, 2> &expanded_control_bits,
        const DcfKey &key) const;
};

}    // namespace dcf
}    // namespace fss
}    // namespace ringoa

#endif    // FSS_DCF_EVAL_H_
