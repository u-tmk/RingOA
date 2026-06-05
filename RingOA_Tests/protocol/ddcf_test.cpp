#include "ddcf_test.h"

#include <cryptoTools/Common/TestCollection.h>

#include "RingOA/protocol/ddcf.h"
#include "RingOA/sharing/sharing_2p_binary.h"
#include "RingOA/utils/logger.h"
#include "RingOA/utils/rng.h"
#include "RingOA/utils/timer.h"
#include "RingOA/utils/to_string.h"
#include "RingOA/utils/utils.h"

namespace {

bool DdcfFullDomainCheck(const uint64_t alpha, const uint64_t beta_1, const uint64_t beta_2, const std::vector<uint64_t> &res) {
    bool check = true;
    for (uint64_t i = 0; i < res.size(); ++i) {
        if ((i < alpha && res[i] == beta_1) || (i >= alpha && res[i] == beta_2)) {
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

using ringoa::Logger;
using ringoa::Mod2N;
using ringoa::ToString;
using ringoa::proto::Ddcf;
using ringoa::proto::DdcfKey;
using ringoa::proto::DdcfParameters;

void Ddcf_EvalAt_Test() {
    Logger::DebugLog(LOC, "Ddcf_EvalAt_Test...");
    // Test parameters
    const std::vector<std::pair<uint64_t, uint64_t>> size_pair = {
        {3, 3},
        // {10, 10},
        // {10, 1},
    };

    // Test all combinations of parameters
    for (auto [n, e] : size_pair) {
        DdcfParameters param(n, e);
        param.PrintParametersDebug();
        Ddcf                        ddcf(param);
        uint64_t                    alpha  = 5;
        uint64_t                    beta_1 = 1;
        uint64_t                    beta_2 = 2;
        std::pair<DdcfKey, DdcfKey> keys   = ddcf.GenerateKeys(alpha, beta_1, beta_2);

        uint64_t x   = 3;
        uint64_t y_0 = ddcf.EvaluateAt(keys.first, x);
        uint64_t y_1 = ddcf.EvaluateAt(keys.second, x);
        uint64_t y   = Mod2N(y_0 + y_1, e);

        if (y != beta_1)
            throw osuCrypto::UnitTestFail("y is not equal to beta_1");

        x   = 7;
        y_0 = ddcf.EvaluateAt(keys.first, x);
        y_1 = ddcf.EvaluateAt(keys.second, x);
        y   = Mod2N(y_0 + y_1, e);

        if (y != beta_2)
            throw osuCrypto::UnitTestFail("y is not equal to beta_2");
    }

    Logger::DebugLog(LOC, "Ddcf_EvalAt_Test - Passed");
}

void Ddcf_Fde_Test() {
    Logger::DebugLog(LOC, "Ddcf_Fde_Test...");
    const std::vector<std::tuple<uint64_t, uint64_t>> size_pair = {
        {3, 3},
    };

    // Test all combinations of parameters
    for (auto [n, e] : size_pair) {
        DdcfParameters param(n, e);
        param.PrintParametersDebug();
        Ddcf     ddcf(param);
        uint64_t alpha  = 5;
        uint64_t beta_1 = 1;
        uint64_t beta_2 = 2;

        // Generate keys
        Logger::DebugLog(LOC, "alpha=" + ToString(alpha) + ", beta_1=" + ToString(beta_1) + ", beta_2=" + ToString(beta_2));
        std::pair<DdcfKey, DdcfKey> keys = ddcf.GenerateKeys(alpha, beta_1, beta_2);

        // Evaluate keys
        std::vector<uint64_t> outputs_0(1 << n), outputs_1(1 << n);

        for (uint64_t i = 0; i < outputs_0.size(); ++i) {
            outputs_0[i] = ddcf.EvaluateAt(keys.first, i);
            outputs_1[i] = ddcf.EvaluateAt(keys.second, i);
        }

        std::vector<uint64_t> outputs(outputs_0.size());
        for (uint64_t i = 0; i < outputs_0.size(); ++i) {
            outputs[i] = Mod2N(outputs_0[i] + outputs_1[i], e);
        }

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
        Logger::DebugLog(LOC, "Outputs=" + ToString(outputs));
#endif

        // Check FDE
        if (!DdcfFullDomainCheck(alpha, beta_1, beta_2, outputs))
            throw osuCrypto::UnitTestFail("FDE check failed");
    }
    Logger::DebugLog(LOC, "Ddcf_Fde_Test - Passed");
}

}    // namespace test_ringoa
