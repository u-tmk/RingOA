#ifndef FSS_DPF_EVAL_H_
#define FSS_DPF_EVAL_H_

#include "dpf_key.h"

namespace ringoa {
namespace fss {

namespace prg {

class PseudoRandomGenerator;

}    // namespace prg

namespace dpf {

/**
 * DpfEvaluator — evaluate Distributed Point Function (DPF) keys.
 *
 * Overview
 * - EvaluateAt(key, x) -> uint64_t
 * - EvaluateFullDomain(key, outputs)
 *   • std::vector<block>& : internal 128-bit blocks for engine use
 *   • std::vector<uint64_t>& : flattened numeric outputs (e ≤ 64)
 *
 * Output semantics (match DpfParameters::GetOutputType()):
 * - OutputType::kShiftedAdditive :
 *     returns an e-bit value; combine two parties by addition mod 2^e.
 * - OutputType::kSingleBitMask :
 *     returns a 0/1 mask; combine two parties with XOR.
 *
 * Strategies (match DpfParameters::GetEvalType()):
 * - Naive: full tree, no early termination.
 * - Optimized (ET): expand only nu = GetTerminateBitsize() levels, then finish via PRG.
 * - Depth-First / Single-Batch variants exist for full-domain enumeration to trade time vs memory.
 *
 * Complexity
 * - EvaluateAt: O(n) seed expansions.
 * - Full-domain: O(2^n) evaluations; memory
 *     • recursion/single-batch: O(2^n) output storage
 *     • depth-first: O(n) working memory (+ output sink).
 *
 * Usage
 *   DpfParameters params(n, e, kOptimizedEvalType, OutputType::kShiftedAdditive);
 *   dpf::DpfEvaluator eval(params);
 *
 *   Point evaluation
 *   uint64_t y = eval.EvaluateAt(key, x);
 *
 *   Full domain (flattened to integers)
 *   std::vector<uint64_t> out;
 *   out.reserve(1ULL << n);          // optional: reduce reallocations
 *   eval.EvaluateFullDomain(key, out);
 *
 * Inputs & contracts
 * - x must satisfy 0 <= x < 2^n (n = GetInputBitsize()).
 * - key must be generated with compatible parameters (same n, e, OutputType/EvalType).
 * - Full-domain methods write exactly 2^n elements to 'outputs'; the vector may be resized.
 *
 * Determinism & PRG
 * - Deterministic for a fixed key and params. Pseudo-randomness comes from prg::PseudoRandomGenerator.
 *
 */

class DpfEvaluator {
public:
    DpfEvaluator() = delete;
    explicit DpfEvaluator(const DpfParameters &params);

    uint64_t EvaluateAt(const DpfKey &key, uint64_t x) const;
    void     EvaluateAt(const std::vector<DpfKey> &keys, const std::vector<uint64_t> &x, std::vector<uint64_t> &outputs) const;

    void EvaluateFullDomain(const DpfKey &key, std::vector<block> &outputs) const;
    void EvaluateFullDomain(const DpfKey &key, std::vector<uint64_t> &outputs) const;

private:
    DpfParameters               params_;
    prg::PseudoRandomGenerator &G_;

    bool ValidateInput(const uint64_t x) const;

    uint64_t EvaluateAtNaive(const DpfKey &key, uint64_t x) const;
    uint64_t EvaluateAtOptimized(const DpfKey &key, uint64_t x) const;

    void EvaluateNextSeed(
        const uint64_t current_level, const block &current_seed, const bool &current_control_bit,
        std::array<block, 2> &expanded_seeds, std::array<bool, 2> &expanded_control_bits,
        const DpfKey &key) const;

    void FullDomainRecursive(const DpfKey &key, std::vector<block> &outputs) const;
    void FullDomainHybridBatched(const DpfKey &key, std::vector<block> &outputs) const;
    // void FullDomainHybridBatchedSubtreeOMP3(const DpfKey &key, std::vector<block> &outputs) const;
    // void FullDomainHybridBatchedSubtreeOMP4(const DpfKey &key, std::vector<block> &outputs) const;
    void FullDomainHybridBatchedFullDepth(const DpfKey &key, std::vector<uint64_t> &outputs) const;
    void FullDomainIterativeFullDepth(const DpfKey &key, std::vector<uint64_t> &outputs) const;
    void FullDomainBruteForce(const DpfKey &key, std::vector<uint64_t> &outputs) const;

    void Traverse(const block  &current_seed,
                  const bool    current_control_bit,
                  const DpfKey &key,
                  uint64_t i, uint64_t j,
                  std::vector<block> &outputs) const;

    block ComputeOutputBlock(const block &final_seed, bool final_control_bit, const DpfKey &key) const;
};

}    // namespace dpf
}    // namespace fss
}    // namespace ringoa

#endif    // FSS_DPF_EVAL_H_
