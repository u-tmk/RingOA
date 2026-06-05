#ifndef FSS_DCF_GEN_H_
#define FSS_DCF_GEN_H_

#include "dcf_key.h"

namespace ringoa {
namespace fss {

namespace prg {

class PseudoRandomGenerator;

}    // namespace prg

namespace dcf {

/**
 * DcfKeyGenerator — build key pairs for a Distributed Comparison Function (DCF).
 *
 * Purpose
 * - Encodes a comparison predicate at threshold alpha with payload beta.
 *   Typical semantics: f_{alpha,beta}(x) = beta if (x < alpha), else 0.
 *   The exact combine rule follows DcfParameters::GetOutputType().
 *
 * Usage
 *   DcfParameters params(n, e, kOptimizedEvalType, OutputType::kShiftedAdditive);
 *   dcf::DcfKeyGenerator gen(params);
 *   auto [k0, k1] = gen.GenerateKeys(alpha, beta);
 *   // Evaluate with the matching DCF evaluator using the same params.
 *
 * Output semantics
 * - OutputType::kShiftedAdditive : parties add their outputs mod 2^e to reconstruct beta·[x < alpha].
 * - OutputType::kSingleBitMask   : parties XOR 0/1 masks for [x < alpha].
 *
 * Strategy (driven by params.GetEvalType())
 * - Naive: full-tree construction.
 * - Optimized (early termination): generate only up to nu = params.GetTerminateBitsize() and compress the tail.
 *
 * Contracts
 * - Domain size: n = params.GetInputBitsize(); inputs satisfy 0 <= alpha, x < 2^n.
 * - Payload: beta fits in e = params.GetOutputBitsize() (1–64 typical).
 * - Keys must be evaluated with the same (n, e, EvalType, OutputType) configuration.
 * - GenerateKeys() validates inputs (see ValidateInput); invalid ranges are rejected.
 *
 * Notes
 * - Deterministic for fixed params, alpha, beta, and PRG seed(s).
 * - The second overload of GenerateKeys exposes final seeds/control-bit for testing and debugging.
 */
class DcfKeyGenerator {
public:
    DcfKeyGenerator() = delete;
    explicit DcfKeyGenerator(const DcfParameters &params);

    std::pair<DcfKey, DcfKey> GenerateKeys(uint64_t alpha, uint64_t beta) const;

private:
    DcfParameters               params_;
    prg::PseudoRandomGenerator &G_;

    bool ValidateInput(const uint64_t alpha, const uint64_t beta) const;
};

}    // namespace dcf
}    // namespace fss
}    // namespace ringoa

#endif    // FSS_DCF_GEN_H_
