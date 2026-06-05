#include "dpf_test.h"

#include <cryptoTools/Common/TestCollection.h>

#include "RingOA/fss/dpf_eval.h"
#include "RingOA/fss/dpf_gen.h"
#include "RingOA/fss/dpf_key.h"
#include "RingOA/utils/logger.h"
#include "RingOA/utils/rng.h"
#include "RingOA/utils/timer.h"
#include "RingOA/utils/to_string.h"
#include "RingOA/utils/utils.h"

namespace {

bool DpfFullDomainCheck(const uint64_t alpha, const uint64_t beta, const std::vector<uint64_t> &res) {
    bool check = true;
    for (uint64_t i = 0; i < res.size(); ++i) {
        if ((i == alpha && res[i] == beta) || (i != alpha && res[i] == 0)) {
            check &= true;
        } else {
            check &= false;
            ringoa::Logger::DebugLog(LOC, "FDE check failed at x=" + ringoa::ToString(i) + " -> Result: " + ringoa::ToString(res[i]));
        }
    }
    return check;
}

// Check if the XOR sum of all blocks matches the expected block (Note: Can detect only error exists, not the position)
bool DpfFullDomainCheckOneBit(const uint64_t alpha, const uint64_t beta, const std::vector<ringoa::block> &res, const ringoa::fss::OutputType mode) {
    // Compute XOR sum of all blocks
    ringoa::block xor_sum = ringoa::zero_block;
    for (const auto &r : res) {
        xor_sum = xor_sum ^ r;
    }
    if (mode == ringoa::fss::OutputType::kShiftedAdditive) {
        // Calculate bit position
        uint64_t bit_position = alpha % (sizeof(ringoa::block) * 8);
        // Generate block with only the bit_position set to 1
        uint64_t high = 0, low = 0;
        // Set the bit_position to 1
        if (bit_position < 64) {
            low = 1ULL << bit_position;
        } else {
            high = 1ULL << (bit_position - 64);
        }
        ringoa::block expected_block = ringoa::MakeBlock(high, low);
        // Check if XOR sum matches the expected block
        bool is_match = xor_sum == expected_block;
        if (!is_match) {
            ringoa::Logger::DebugLog(LOC, "FDE check failed for alpha=" + ringoa::ToString(alpha) + " and beta=" + ringoa::ToString(beta));
        }
        return is_match;
    } else {
        // Calculate bit position
        uint64_t bit_position = alpha % 128;
        uint64_t byte_idx     = bit_position % 16;
        uint64_t bit_idx      = bit_position / 16;

        ringoa::block expected_block = ringoa::zero_block;
        auto          expected_byte  = reinterpret_cast<uint8_t *>(&expected_block);
        expected_byte[byte_idx] ^= static_cast<uint8_t>(1) << bit_idx;

        // Check if XOR sum matches the expected block
        bool is_match = xor_sum == expected_block;
        if (!is_match) {
            ringoa::Logger::DebugLog(LOC, "FDE check failed for alpha=" + ringoa::ToString(alpha) + " and beta=" + ringoa::ToString(beta));
        }
        return is_match;
    }
}

}    // namespace

namespace test_ringoa {

using ringoa::block;
using ringoa::FormatType;
using ringoa::GlobalRng;
using ringoa::Logger;
using ringoa::Mod2N;
using ringoa::TimerManager;
using ringoa::ToString, ringoa::Format;
using ringoa::fss::EvalType, ringoa::fss::OutputType;
using ringoa::fss::dpf::DpfEvaluator;
using ringoa::fss::dpf::DpfKey;
using ringoa::fss::dpf::DpfKeyGenerator;
using ringoa::fss::dpf::DpfParameters;

void Dpf_Params_Test() {
    Logger::DebugLog(LOC, "Dpf_Params_Test...");

    // Test parameters
    const std::vector<std::pair<uint64_t, uint64_t>> size_pair = {
        {3, 3},
        {3, 1},
        {9, 1},
        {10, 1},
        {8, 8},
        {9, 9},
        {17, 17},
        {29, 29},
    };
    const std::vector<EvalType> evals = {
        EvalType::kBruteForce,
        EvalType::kRecursive,
        EvalType::kHybridBatched};

    // Test all combinations of parameters
    for (auto [n, e] : size_pair) {
        for (auto ev : evals) {
            DpfParameters params(n, e, ev);
            params.PrintParametersDebug();
            DpfKeyGenerator gen(params);
            DpfEvaluator    eval(params);
        }
    }

    Logger::DebugLog(LOC, "Dpf_Params_Test - Passed");
}

void Dpf_EvalAt_Test() {
    Logger::DebugLog(LOC, "Dpf_EvalAt_Test...");
    // Test parameters
    const std::vector<std::pair<uint64_t, uint64_t>> size_pair = {
        {3, 3},
        {3, 1},
        {9, 1},
        {10, 1},
        {8, 8},
        {9, 9},
        {17, 17},
        {29, 29},
    };
    const std::vector<EvalType> evals = {
        EvalType::kBruteForce,
        EvalType::kHybridBatched};

    // Test all combinations of parameters
    for (auto [n, e] : size_pair) {
        for (auto ev : evals) {
            DpfParameters param(n, e, ev);
            param.PrintParametersDebug();
            uint64_t                  e = param.GetOutputBitsize();
            DpfKeyGenerator           gen(param);
            DpfEvaluator              eval(param);
            uint64_t                  alpha = 5;
            uint64_t                  beta  = 1;
            std::pair<DpfKey, DpfKey> keys  = gen.GenerateKeys(alpha, beta);

            uint64_t x   = 5;
            uint64_t y_0 = eval.EvaluateAt(keys.first, x);
            uint64_t y_1 = eval.EvaluateAt(keys.second, x);
            uint64_t y   = Mod2N(y_0 + y_1, e);

            if (y != beta)
                throw osuCrypto::UnitTestFail("y is not equal to beta");

            x   = 7;
            y_0 = eval.EvaluateAt(keys.first, x);
            y_1 = eval.EvaluateAt(keys.second, x);
            y   = Mod2N(y_0 + y_1, e);

            if (y != 0)
                throw osuCrypto::UnitTestFail("y is not equal to 0");
        }
    }

    Logger::DebugLog(LOC, "Dpf_EvalAt_Test - Passed");
}

void Dpf_Fde_Test() {
    Logger::DebugLog(LOC, "Dpf_Fde_Test...");
    const std::vector<std::tuple<uint64_t, uint64_t, EvalType>> fde_param = {
        {3, 3, EvalType::kBruteForce},
        {8, 8, EvalType::kRecursive},
        {8, 8, EvalType::kHybridBatchedFullDepth},
        {8, 8, EvalType::kHybridBatched},
        {9, 9, EvalType::kIterativeFullDepth},
        {9, 9, EvalType::kRecursive},
        {9, 9, EvalType::kHybridBatched},
        // {10, 10, EvalType::kHybridBatchedSubtreeOMP3},
        // {10, 10, EvalType::kHybridBatchedSubtreeOMP4},
        {17, 17, EvalType::kRecursive},
        {17, 17, EvalType::kIterativeFullDepth},
        {17, 17, EvalType::kHybridBatchedFullDepth},
        {17, 17, EvalType::kHybridBatched},
    };

    // Test all combinations of parameters
    for (auto [n, e, eval_type] : fde_param) {
        DpfParameters param(n, e, eval_type);
        param.PrintParametersDebug();
        DpfKeyGenerator gen(param);
        DpfEvaluator    eval(param);
        uint64_t        alpha = Mod2N(GlobalRng::Rand<uint64_t>(), n);
        uint64_t        beta  = Mod2N(GlobalRng::Rand<uint64_t>(), e);

        // Generate keys
        Logger::DebugLog(LOC, "alpha=" + ToString(alpha) + ", beta=" + ToString(beta));
        std::pair<DpfKey, DpfKey> keys = gen.GenerateKeys(alpha, beta);

        // Evaluate keys
        std::vector<uint64_t> outputs_0(1 << n), outputs_1(1 << n);
        eval.EvaluateFullDomain(keys.first, outputs_0);
        eval.EvaluateFullDomain(keys.second, outputs_1);

        std::vector<uint64_t> outputs(outputs_0.size());
        for (uint64_t i = 0; i < outputs_0.size(); ++i) {
            outputs[i] = Mod2N(outputs_0[i] + outputs_1[i], e);
        }

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
        Logger::DebugLog(LOC, "Outputs=" + ToString(outputs));
#endif

        // Check FDE
        if (!DpfFullDomainCheck(alpha, beta, outputs))
            throw osuCrypto::UnitTestFail("FDE check failed");
    }
    Logger::DebugLog(LOC, "Dpf_Fde_Test - Passed");
}

void Dpf_Fde_One_Test() {
    Logger::DebugLog(LOC, "Dpf_Fde_One_Test...");
    const std::vector<std::tuple<uint64_t, uint64_t, EvalType>> fde_param = {
        {3, 1, EvalType::kBruteForce},
        {10, 1, EvalType::kRecursive},
        {10, 1, EvalType::kHybridBatched},
    };
    // Test all combinations of parameters
    for (auto [n, e, eval_type] : fde_param) {
        DpfParameters param(n, e, eval_type);
        param.PrintParametersDebug();
        DpfKeyGenerator gen(param);
        DpfEvaluator    eval(param);
        uint64_t        alpha = Mod2N(GlobalRng::Rand<uint64_t>(), n);
        uint64_t        beta  = 1;

        // Generate keys
        Logger::DebugLog(LOC, "alpha=" + ToString(alpha) + ", beta=" + ToString(beta));
        std::pair<DpfKey, DpfKey> keys = gen.GenerateKeys(alpha, beta);

        // Evaluate keys
        if (param.GetEvalType() == EvalType::kBruteForce) {
            std::vector<uint64_t> outputs_0(1 << n), outputs_1(1 << n);
            eval.EvaluateFullDomain(keys.first, outputs_0);
            eval.EvaluateFullDomain(keys.second, outputs_1);
            std::vector<uint64_t> outputs(outputs_0.size());
            for (uint64_t i = 0; i < outputs_0.size(); ++i) {
                outputs[i] = Mod2N(outputs_0[i] + outputs_1[i], e);
            }

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
            Logger::DebugLog(LOC, "Outputs=" + ToString(outputs));
#endif
            // Check FDE
            if (!DpfFullDomainCheck(alpha, beta, outputs))
                throw osuCrypto::UnitTestFail("FDE check failed");

        } else {
            std::vector<block> outputs_0(1 << param.GetTerminateBitsize()), outputs_1(1 << param.GetTerminateBitsize());
            eval.EvaluateFullDomain(keys.first, outputs_0);
            eval.EvaluateFullDomain(keys.second, outputs_1);
            std::vector<block> outputs(outputs_0.size());
            for (uint64_t i = 0; i < outputs_0.size(); ++i) {
                outputs[i] = outputs_0[i] ^ outputs_1[i];
            }
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
            for (uint64_t i = 0; i < outputs.size(); ++i) {
                Logger::DebugLog(LOC, "Outputs[" + ToString(i) + "]  =" + Format(outputs[i], FormatType::kBin));
            }
#endif
            // Check FDE
            if (!DpfFullDomainCheckOneBit(alpha, beta, outputs, param.GetOutputType()))
                throw osuCrypto::UnitTestFail("FDE check failed");
        }
    }
    Logger::DebugLog(LOC, "Dpf_Fde_One_Test - Passed");
}

}    // namespace test_ringoa
