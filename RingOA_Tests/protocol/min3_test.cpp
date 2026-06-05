#include "min3_test.h"

#include <cryptoTools/Common/TestCollection.h>

#include "RingOA/protocol/min3.h"
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

const std::string kCurrentPath  = ringoa::GetCurrentDirectory();
const std::string kTestMin3Path = kCurrentPath + "/data/test/protocol/min3/";

struct TestCase {
    ringoa::sharing::ShareConfig share;
    ringoa::proto::Min3Config    cfg;

    TestCase(ringoa::sharing::ShareConfig s,
             ringoa::proto::Min3Config    e = ringoa::proto::Min3Config{})
        : share(std::move(s)), cfg(std::move(e)) {
    }
};

std::vector<TestCase> BuildTestCases() {
    using ringoa::proto::Min3Config;
    using ringoa::sharing::ShareConfig;

    std::vector<TestCase> cases;

    // 1) small arithmetic ring (arith_bits=5), default in/out = 5
    {
        Min3Config cfg;
        cases.emplace_back(ShareConfig::Custom(5), cfg);
    }

    // 2) arithmetic ring 32, dpf in = 10, out = 32
    {
        Min3Config cfg;
        cfg.input_domain_bits = 10;    // smaller input domain
        cases.emplace_back(ShareConfig::Arith32(), cfg);
    }

    // 3) arithmetic ring 10, dpf in = 10, boolean out = 10
    {
        Min3Config cfg;
        cfg.input_domain_bits = 10;    // smaller input domain
        cases.emplace_back(ShareConfig::Custom(10), cfg);
    }

    return cases;
}

inline std::string KeyPath(uint64_t n, uint64_t e, uint64_t k) {
    return kTestMin3Path + "min3key_n" + ringoa::ToString(n) + "_e" + ringoa::ToString(e) + "_k" + ringoa::ToString(k);
}

inline std::string DataPath(const std::string &name, uint64_t n, uint64_t e, uint64_t k) {
    return kTestMin3Path + name + "_n" + ringoa::ToString(n) + "_e" + ringoa::ToString(e) + "_k" + ringoa::ToString(k);
}

}    // namespace

namespace test_ringoa {

using ringoa::FileIo;
using ringoa::Logger;
using ringoa::Mod2N;
using ringoa::ToString;
using ringoa::TwoPartyNetworkManager;
using ringoa::proto::KeyIo;
using ringoa::proto::Min3;
using ringoa::proto::Min3Keys;
using ringoa::proto::Min3Parameters;
using ringoa::proto::ProtocolContext2P;
using ringoa::sharing::AdditiveSharing2P;

void Min3_Offline_Test() {
    Logger::DebugLog(LOC, "Min3_Offline_Test...");

    const auto cases = BuildTestCases();

    for (const auto &tc : cases) {
        Min3Parameters params(tc.cfg, tc.share);
        params.PrintParametersDebug();

        const uint64_t n = params.GetInputDomainBits();    // Min3 in bits
        const uint64_t e = params.GetOutputBitsize();      // Min3 out bits
        const uint64_t k = tc.share.arith_bits;            // Ring bits

        ProtocolContext2P  ctx(tc.share);
        AdditiveSharing2P &ss = ctx.Arith();
        Min3               min3(params, ctx);
        FileIo             file_io;

        // Generate keys
        auto keys = min3.GenerateKeys(1);

        // Offline Setup
        min3.OfflineSetUp(1, DataPath("min3", n, e, k));

        // Save keys
        KeyIo::SaveKey(KeyPath(n, e, k) + "_0", keys.first);
        KeyIo::SaveKey(KeyPath(n, e, k) + "_1", keys.second);
        Logger::DebugLog(LOC, "Saved Min3 keys to files: " + KeyPath(n, e, k) + "[_0|_1]");

        // Generate input
        uint64_t x1 = 3;
        uint64_t x2 = 5;
        uint64_t x3 = 8;

        auto x1_sh = ss.Share(x1);
        auto x2_sh = ss.Share(x2);
        auto x3_sh = ss.Share(x3);

        Logger::DebugLog(LOC, "x1: " + ToString(x1) + ", x2: " + ToString(x2) + ", x3: " + ToString(x3));
        Logger::DebugLog(LOC, "x1_sh: " + ToString(x1_sh.first) + ", " + ToString(x1_sh.second));
        Logger::DebugLog(LOC, "x2_sh: " + ToString(x2_sh.first) + ", " + ToString(x2_sh.second));
        Logger::DebugLog(LOC, "x3_sh: " + ToString(x3_sh.first) + ", " + ToString(x3_sh.second));

        // Save input
        file_io.WriteBinary(DataPath("x1", n, e, k) + "_0", x1_sh.first);
        file_io.WriteBinary(DataPath("x1", n, e, k) + "_1", x1_sh.second);
        file_io.WriteBinary(DataPath("x2", n, e, k) + "_0", x2_sh.first);
        file_io.WriteBinary(DataPath("x2", n, e, k) + "_1", x2_sh.second);
        file_io.WriteBinary(DataPath("x3", n, e, k) + "_0", x3_sh.first);
        file_io.WriteBinary(DataPath("x3", n, e, k) + "_1", x3_sh.second);
        Logger::DebugLog(LOC, "Saved shared inputs to files: " + DataPath("x1", n, e, k) + "[_0|_1]");
        Logger::DebugLog(LOC, "Saved shared inputs to files: " + DataPath("x2", n, e, k) + "[_0|_1]");
        Logger::DebugLog(LOC, "Saved shared inputs to files: " + DataPath("x3", n, e, k) + "[_0|_1]");
    }
    Logger::DebugLog(LOC, "Min3_Offline_Test - Passed");
}

void Min3_Online_Test(const osuCrypto::CLP &cmd) {
    Logger::DebugLog(LOC, "Min3_Online_Test...");
    int party_id = cmd.getOr<int>("party", -1);

    const auto cases = BuildTestCases();

    for (const auto &tc : cases) {
        Min3Parameters params(tc.cfg, tc.share);
        params.PrintParametersDebug();

        const uint64_t n = params.GetInputDomainBits();    // Min3 in bits
        const uint64_t e = params.GetOutputBitsize();      // Min3 out bits
        const uint64_t k = tc.share.arith_bits;            // Ring bits
        FileIo         file_io;

        // Start network communication
        TwoPartyNetworkManager net_mgr("Min3_Online_Test");

        uint64_t y{0};

        auto server_task = [&](oc::Channel &chl) {
            ProtocolContext2P  ctx(tc.share);
            AdditiveSharing2P &ss = ctx.Arith();
            Min3               min3(params, ctx);

            uint64_t x1_0{0}, x2_0{0}, x3_0{0};
            uint64_t y_0{0}, y_1{0};

            Min3Keys key_0(0, params, 1);
            KeyIo::LoadKey(KeyPath(n, e, k) + "_0", key_0, params);
            Logger::DebugLog(LOC, "Loaded Min3 key from file: " + KeyPath(n, e, k) + "_0");

            file_io.ReadBinary(DataPath("x1", n, e, k) + "_0", x1_0);
            file_io.ReadBinary(DataPath("x2", n, e, k) + "_0", x2_0);
            file_io.ReadBinary(DataPath("x3", n, e, k) + "_0", x3_0);
            Logger::DebugLog(LOC, "Loaded shared inputs from files: " + DataPath("x1", n, e, k) + "_0");
            Logger::DebugLog(LOC, "Loaded shared inputs from files: " + DataPath("x2", n, e, k) + "_0");
            Logger::DebugLog(LOC, "Loaded shared inputs from files: " + DataPath("x3", n, e, k) + "_0");

            min3.OnlineSetUp(0, DataPath("min3", n, e, k));

            y_0 = min3.EvaluateSharedInput(chl, key_0.GetView(0), {x1_0, x2_0, x3_0});

            ss.Reconst(0, chl, y_0, y_1, y);

            Logger::DebugLog(LOC, "[P0] y: " + ToString(y));
        };

        auto client_task = [&](oc::Channel &chl) {
            ProtocolContext2P  ctx(tc.share);
            AdditiveSharing2P &ss = ctx.Arith();
            Min3               min3(params, ctx);

            uint64_t x1_1{0}, x2_1{0}, x3_1{0};
            uint64_t y_0{0}, y_1{0};

            Min3Keys key_1(1, params, 1);
            KeyIo::LoadKey(KeyPath(n, e, k) + "_1", key_1, params);
            Logger::DebugLog(LOC, "Loaded Min3 key from file: " + KeyPath(n, e, k) + "_1");

            file_io.ReadBinary(DataPath("x1", n, e, k) + "_1", x1_1);
            file_io.ReadBinary(DataPath("x2", n, e, k) + "_1", x2_1);
            file_io.ReadBinary(DataPath("x3", n, e, k) + "_1", x3_1);
            Logger::DebugLog(LOC, "Loaded shared inputs from files: " + DataPath("x1", n, e, k) + "_1");
            Logger::DebugLog(LOC, "Loaded shared inputs from files: " + DataPath("x2", n, e, k) + "_1");
            Logger::DebugLog(LOC, "Loaded shared inputs from files: " + DataPath("x3", n, e, k) + "_1");

            min3.OnlineSetUp(1, DataPath("min3", n, e, k));

            y_1 = min3.EvaluateSharedInput(chl, key_1.GetView(0), {x1_1, x2_1, x3_1});

            ss.Reconst(1, chl, y_0, y_1, y);

            Logger::DebugLog(LOC, "[P1] y: " + ToString(y));
        };

        net_mgr.AutoConfigure(party_id, server_task, client_task);
        net_mgr.WaitForCompletion();

        uint64_t min_val = std::min({3ULL, 5ULL, 8ULL});
        Logger::DebugLog(LOC, "min(3, 5, 8) = " + ToString(min_val));
        if (y != min_val)
            throw oc::UnitTestFail("y is not equal to " + ToString(min_val));
    }
    Logger::DebugLog(LOC, "Min3_Online_Test - Passed");
}

}    // namespace test_ringoa
