#include "dcf_test.h"

#include <cryptoTools/Common/TestCollection.h>

#include "RingOA/fss/dcf_eval.h"
#include "RingOA/fss/dcf_gen.h"
#include "RingOA/fss/dcf_key.h"
#include "RingOA/utils/logger.h"
#include "RingOA/utils/rng.h"
#include "RingOA/utils/timer.h"
#include "RingOA/utils/to_string.h"
#include "RingOA/utils/utils.h"

namespace {

bool DcfFullDomainCheck(const uint64_t alpha, const uint64_t beta, const std::vector<uint64_t> &res) {
    bool check = true;
    for (uint64_t i = 0; i < res.size(); ++i) {
        if ((i < alpha && res[i] == beta) || (i >= alpha && res[i] == 0)) {
            check &= true;
        } else {
            check &= false;
            ringoa::Logger::DebugLog(LOC, "FDE check failed at x=" + ringoa::ToString(i) + " -> Result: " + ringoa::ToString(res[i]));
        }
    }
    return check;
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
using ringoa::fss::dcf::DcfEvaluator;
using ringoa::fss::dcf::DcfKey;
using ringoa::fss::dcf::DcfKeyGenerator;
using ringoa::fss::dcf::DcfParameters;

void Dcf_EvalAt_Test() {
    Logger::DebugLog(LOC, "Dcf_EvalAt_Test...");
    // Test parameters
    const std::vector<std::pair<uint64_t, uint64_t>> size_pair = {
        {3, 3},
        // {10, 10},
        // {10, 1},
    };

    // Test all combinations of parameters
    for (auto [n, e] : size_pair) {
        DcfParameters param(n, e);
        param.PrintParametersDebug();
        DcfKeyGenerator           gen(param);
        DcfEvaluator              eval(param);
        uint64_t                  alpha = 5;
        uint64_t                  beta  = 1;
        std::pair<DcfKey, DcfKey> keys  = gen.GenerateKeys(alpha, beta);

        uint64_t x   = 3;
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

    Logger::DebugLog(LOC, "Dcf_EvalAt_Test - Passed");
}

void Dcf_Fde_Test() {
    Logger::DebugLog(LOC, "Dcf_Fde_Test...");
    const std::vector<std::tuple<uint64_t, uint64_t>> size_pair = {
        {3, 3},
    };

    // Test all combinations of parameters
    for (auto [n, e] : size_pair) {
        DcfParameters param(n, e);
        param.PrintParametersDebug();
        DcfKeyGenerator gen(param);
        DcfEvaluator    eval(param);
        uint64_t        alpha = Mod2N(GlobalRng::Rand<uint64_t>(), n);
        uint64_t        beta  = Mod2N(GlobalRng::Rand<uint64_t>(), e);

        // Generate keys
        Logger::DebugLog(LOC, "alpha=" + ToString(alpha) + ", beta=" + ToString(beta));
        std::pair<DcfKey, DcfKey> keys = gen.GenerateKeys(alpha, beta);

        // Evaluate keys
        std::vector<uint64_t> outputs_0(1 << n), outputs_1(1 << n);

        for (uint64_t i = 0; i < outputs_0.size(); ++i) {
            outputs_0[i] = eval.EvaluateAt(keys.first, i);
            outputs_1[i] = eval.EvaluateAt(keys.second, i);
        }

        std::vector<uint64_t> outputs(outputs_0.size());
        for (uint64_t i = 0; i < outputs_0.size(); ++i) {
            outputs[i] = Mod2N(outputs_0[i] + outputs_1[i], e);
        }

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
        Logger::DebugLog(LOC, "Outputs=" + ToString(outputs));
#endif

        // Check FDE
        if (!DcfFullDomainCheck(alpha, beta, outputs))
            throw osuCrypto::UnitTestFail("FDE check failed");
    }
    Logger::DebugLog(LOC, "Dcf_Fde_Test - Passed");
}

}    // namespace test_ringoa
