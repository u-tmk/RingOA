#include "equality_test.h"

#include <cryptoTools/Common/TestCollection.h>

#include "RingOA/protocol/equality.h"
#include "RingOA/protocol/protocol_io.h"
#include "RingOA/sharing/sharing_2p_ring.h"
#include "RingOA/utils/file_io.h"
#include "RingOA/utils/logger.h"
#include "RingOA/utils/network.h"
#include "RingOA/utils/rng.h"
#include "RingOA/utils/timer.h"
#include "RingOA/utils/to_string.h"
#include "RingOA/utils/utils.h"

namespace {

const std::string kCurrentPath = ringoa::GetCurrentDirectory();
const std::string kTestEqPath  = kCurrentPath + "/data/test/protocol/equality/";

struct TestCase {
    ringoa::sharing::ShareConfig  share;
    ringoa::proto::EqualityConfig cfg;

    TestCase(ringoa::sharing::ShareConfig  s,
             ringoa::proto::EqualityConfig e = ringoa::proto::EqualityConfig{})
        : share(std::move(s)), cfg(std::move(e)) {
    }
};

std::vector<TestCase> BuildTestCases() {
    using ringoa::proto::EqualityConfig;
    using ringoa::sharing::ShareConfig;

    std::vector<TestCase> cases;

    // 1) small arithmetic ring (arith_bits=5), default in/out = 5
    {
        EqualityConfig cfg;
        cases.emplace_back(ShareConfig::Custom(5), cfg);
    }

    // 2) arithmetic ring 32, dpf in = 10, out = 32
    {
        EqualityConfig cfg;
        cfg.input_domain_bits = 10;    // smaller input domain
        cases.emplace_back(ShareConfig::Arith32(), cfg);
    }

    // 3) arithmetic ring 10, dpf in = 10, boolean out = 10
    {
        EqualityConfig cfg;
        cfg.input_domain_bits = 10;    // smaller input domain
        cases.emplace_back(ShareConfig::Custom(10), cfg);
    }

    // 4) arithmetic ring 32, but boolean output (out=1), input default=32
    {
        EqualityConfig cfg;
        cfg.boolean_output = true;
        cases.emplace_back(ShareConfig::Arith32(), cfg);
    }

    return cases;
}

inline std::string KeyPath(uint64_t n, uint64_t e, uint64_t k) {
    return kTestEqPath + "eqkey_n" + ringoa::ToString(n) + "_e" + ringoa::ToString(e) + "_k" + ringoa::ToString(k);
}

inline std::string DataPath(const std::string &name, uint64_t n, uint64_t e, uint64_t k) {
    return kTestEqPath + name + "_n" + ringoa::ToString(n) + "_e" + ringoa::ToString(e) + "_k" + ringoa::ToString(k);
}

}    // namespace

namespace test_ringoa {

using ringoa::FileIo;
using ringoa::Logger;
using ringoa::ToString;
using ringoa::TwoPartyNetworkManager;
using ringoa::proto::Equality;
using ringoa::proto::EqualityKeys;
using ringoa::proto::EqualityParameters;
using ringoa::proto::KeyIo;
using ringoa::proto::ProtocolContext2P;
using ringoa::sharing::AdditiveSharing2P;

void Equality_Offline_Test() {
    Logger::DebugLog(LOC, "Equality_Offline_Test...");

    const auto cases = BuildTestCases();

    for (const auto &tc : cases) {
        EqualityParameters params(tc.cfg, tc.share);
        params.PrintParametersDebug();

        const uint64_t n = params.GetInputDomainBits();     // DPF in bits
        const uint64_t e = params.GetDpfOutputBitsize();    // DPF out bits
        const uint64_t k = tc.share.arith_bits;             // Ring bits

        ProtocolContext2P  ctx(tc.share);
        AdditiveSharing2P &ss = ctx.Arith();
        Equality           eq(params, ctx);
        FileIo             file_io;

        // Generate keys
        auto keys = eq.GenerateKeys(5);

        // Save keys
        KeyIo::SaveKey(KeyPath(n, e, k) + "_0", keys.first);
        KeyIo::SaveKey(KeyPath(n, e, k) + "_1", keys.second);
        Logger::DebugLog(LOC, "Saved Equality keys to files: " + KeyPath(n, e, k) + "[_0|_1]");

        // Generate input
        const std::vector<uint64_t> x = {1, 2, 3, 4, 5};
        const std::vector<uint64_t> y = {1, 0, 3, 0, 5};    // x == y for indices 0, 2, 4

        const auto x_sh = ss.Share(x);
        const auto y_sh = ss.Share(y);

        Logger::DebugLog(LOC, "x: " + ToString(x) + ", y: " + ToString(y));
        Logger::DebugLog(LOC, "x_sh: " + ToString(x_sh.first) + ", " + ToString(x_sh.second));
        Logger::DebugLog(LOC, "y_sh: " + ToString(y_sh.first) + ", " + ToString(y_sh.second));

        // Save input
        file_io.WriteBinary(DataPath("x", n, e, k) + "_0", x_sh.first);
        file_io.WriteBinary(DataPath("x", n, e, k) + "_1", x_sh.second);
        file_io.WriteBinary(DataPath("y", n, e, k) + "_0", y_sh.first);
        file_io.WriteBinary(DataPath("y", n, e, k) + "_1", y_sh.second);
        Logger::DebugLog(LOC, "Saved shared inputs to files: " + DataPath("x", n, e, k) + "[_0|_1]");
        Logger::DebugLog(LOC, "Saved shared inputs to files: " + DataPath("y", n, e, k) + "[_0|_1]");
    }

    Logger::DebugLog(LOC, "Equality_Offline_Test - Passed");
}

void Equality_Online_Test(const osuCrypto::CLP &cmd) {
    Logger::DebugLog(LOC, "Equality_Online_Test...");
    int party_id = cmd.getOr<int>("party", -1);

    const auto cases = BuildTestCases();

    for (const auto &tc : cases) {
        EqualityParameters params(tc.cfg, tc.share);

        const uint64_t n = params.GetInputDomainBits();     // DPF in bits
        const uint64_t e = params.GetDpfOutputBitsize();    // DPF out bits
        const uint64_t k = tc.share.arith_bits;             // Ring bits
        FileIo         file_io;

        TwoPartyNetworkManager net_mgr("Equality_Online_Test");

        std::vector<uint64_t> z, z_batch;

        auto server_task = [&](oc::Channel &chl) {
            ProtocolContext2P  ctx(tc.share);
            AdditiveSharing2P &ss = (params.GetDpfOutputBitsize() == 1) ? ctx.Bool() : ctx.Arith();
            Equality           eq(params, ctx);

            std::vector<uint64_t> x_0, y_0;
            std::vector<uint64_t> z_0, z_1, z_batch_0, z_batch_1;

            EqualityKeys key_0(0, params, 5);
            KeyIo::LoadKey(KeyPath(n, e, k) + "_0", key_0, params);
            Logger::DebugLog(LOC, "Loaded Equality key from file: " + KeyPath(n, e, k) + "_0");

            file_io.ReadBinary(DataPath("x", n, e, k) + "_0", x_0);
            file_io.ReadBinary(DataPath("y", n, e, k) + "_0", y_0);
            Logger::DebugLog(LOC, "Loaded shared inputs from files: " + DataPath("x", n, e, k) + "_0");
            Logger::DebugLog(LOC, "Loaded shared inputs from files: " + DataPath("y", n, e, k) + "_0");

            for (size_t i = 0; i < x_0.size(); ++i) {
                z_0.push_back(eq.EvaluateSharedInput(chl, key_0.GetView(i), x_0[i], y_0[i]));
            }
            eq.EvaluateSharedInputBatch(chl, key_0, x_0, y_0, z_batch_0);

            ss.Reconst(0, chl, z_0, z_1, z);
            ss.Reconst(0, chl, z_batch_0, z_batch_1, z_batch);

            Logger::DebugLog(LOC, "[P0] z: " + ToString(z));
            Logger::DebugLog(LOC, "[P0] z_batch: " + ToString(z_batch));
        };

        auto client_task = [&](oc::Channel &chl) {
            ProtocolContext2P  ctx(tc.share);
            AdditiveSharing2P &ss = (params.GetDpfOutputBitsize() == 1) ? ctx.Bool() : ctx.Arith();
            Equality           eq(params, ctx);

            std::vector<uint64_t> x_1, y_1;
            std::vector<uint64_t> z_0, z_1, z_batch_0, z_batch_1;

            EqualityKeys key_1(1, params, 5);
            KeyIo::LoadKey(KeyPath(n, e, k) + "_1", key_1, params);
            Logger::DebugLog(LOC, "Loaded Equality key from file: " + KeyPath(n, e, k) + "_1");

            file_io.ReadBinary(DataPath("x", n, e, k) + "_1", x_1);
            file_io.ReadBinary(DataPath("y", n, e, k) + "_1", y_1);
            Logger::DebugLog(LOC, "Loaded shared inputs from files: " + DataPath("x", n, e, k) + "_1");
            Logger::DebugLog(LOC, "Loaded shared inputs from files: " + DataPath("y", n, e, k) + "_1");

            for (size_t i = 0; i < x_1.size(); ++i) {
                z_1.push_back(eq.EvaluateSharedInput(chl, key_1.GetView(i), x_1[i], y_1[i]));
            }
            eq.EvaluateSharedInputBatch(chl, key_1, x_1, y_1, z_batch_1);

            ss.Reconst(1, chl, z_0, z_1, z);
            ss.Reconst(1, chl, z_batch_0, z_batch_1, z_batch);

            Logger::DebugLog(LOC, "[P1] z: " + ToString(z));
            Logger::DebugLog(LOC, "[P1] z_batch: " + ToString(z_batch));
        };

        net_mgr.AutoConfigure(party_id, server_task, client_task);
        net_mgr.WaitForCompletion();

        if (z != std::vector<uint64_t>{1, 0, 1, 0, 1}) {
            throw oc::UnitTestFail("z is not equal to expected result");
        }

        if (z_batch != std::vector<uint64_t>{1, 0, 1, 0, 1}) {
            throw oc::UnitTestFail("z_batch is not equal to expected result");
        }

        Logger::DebugLog(LOC, "Equality_Online_Test - Passed");
    }
}

}    // namespace test_ringoa
