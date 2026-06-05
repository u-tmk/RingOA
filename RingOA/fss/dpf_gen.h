#ifndef FSS_DPF_GEN_H_
#define FSS_DPF_GEN_H_

#include "dpf_key.h"

namespace ringoa {
namespace fss {

namespace prg {

class PseudoRandomGenerator;

}    // namespace prg

namespace dpf {

/**
 * DpfKeyGenerator builds DPF key pairs for a given (alpha, beta).
 *
 * Usage:
 *   prg::PseudoRandomGenerator G(...);
 *   DpfParameters params(n, e, kOptimizedEvalType, OutputType::kShiftedAdditive);
 *   DpfKeyGenerator gen(params, G);
 *   auto [k0, k1] = gen.GenerateKeys(alpha, beta);
 *
 * Behavior:
 *   - Dispatches to a strategy based on params.GetEvalType()
 *     (Naive / Optimized with early termination).
 *   - Output semantics depend on params.GetOutputType()
 *     (ShiftedAdditive vs SingleBitMask).
 *
 * Notes:
 *   - Not thread-safe.
 *   - Requires 0 <= alpha < 2^n; beta fits e bits (caller responsibility).
 *   - Specialized GenerateKeysNaive/Optimized are provided mainly for testing.
 */
class DpfKeyGenerator {
public:
    DpfKeyGenerator() = delete;
    explicit DpfKeyGenerator(const DpfParameters &params);

    std::pair<DpfKey, DpfKey> GenerateKeys(const uint64_t alpha, const uint64_t beta) const;
    std::pair<DpfKey, DpfKey> GenerateKeys(const uint64_t alpha, const uint64_t beta, block &final_seed_0, block &final_seed_1, bool &final_control_bit_1) const;

    void GenerateKeysNaive(const uint64_t alpha, const uint64_t beta, std::pair<DpfKey, DpfKey> &key_pair) const;
    void GenerateKeysNaive(const uint64_t alpha, const uint64_t beta, block &final_seed_0, block &final_seed_1,
                           bool &final_control_bit_1, std::pair<DpfKey, DpfKey> &key_pair) const;

    void GenerateKeysOptimized(const uint64_t alpha, const uint64_t beta, std::pair<DpfKey, DpfKey> &key_pair) const;
    void GenerateKeysOptimized(const uint64_t alpha, const uint64_t beta, block &final_seed_0, block &final_seed_1,
                               bool &final_control_bit_1, std::pair<DpfKey, DpfKey> &key_pair) const;

private:
    DpfParameters               params_;
    prg::PseudoRandomGenerator &G_;

    bool ValidateInput(const uint64_t alpha, const uint64_t beta) const;

    void GenerateNextSeed(const uint64_t current_level, const bool current_bit,
                          block &current_seed_0, bool &current_control_bit_0,
                          block &current_seed_1, bool &current_control_bit_1,
                          std::pair<DpfKey, DpfKey> &key_pair) const;

    void ComputeAdditiveShiftedOutput(uint64_t alpha, uint64_t beta,
                                      block &final_seed_0, block &final_seed_1, bool final_control_bit_1,
                                      std::pair<DpfKey, DpfKey> &key_pair) const;

    void ComputeSingleBitMaskOutput(uint64_t alpha, block &final_seed_0, block &final_seed_1,
                                    std::pair<DpfKey, DpfKey> &key_pair) const;
};

}    // namespace dpf
}    // namespace fss
}    // namespace ringoa

#endif    // FSS_DPF_GEN_H_
