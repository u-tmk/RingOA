#include "integer_comparison_test.h"

#include <cryptoTools/Common/TestCollection.h>

#include "RingOA/protocol/integer_comparison.h"
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
const std::string kTestCompPath = kCurrentPath + "/data/test/protocol/comparison/";

struct TestCase {
    ringoa::sharing::ShareConfig           share;
    ringoa::proto::IntegerComparisonConfig cfg;

    TestCase(ringoa::sharing::ShareConfig           s,
             ringoa::proto::IntegerComparisonConfig e = ringoa::proto::IntegerComparisonConfig{})
        : share(std::move(s)), cfg(std::move(e)) {
    }
};

std::vector<TestCase> BuildTestCases() {
    using ringoa::proto::IntegerComparisonConfig;
    using ringoa::sharing::ShareConfig;

    std::vector<TestCase> cases;

    // 1) small arithmetic ring (arith_bits=5), default in/out = 5
    {
        IntegerComparisonConfig cfg;
        cases.emplace_back(ShareConfig::Custom(5), cfg);
    }

    // 2) arithmetic ring 32, dpf in = 10, out = 32
    {
        IntegerComparisonConfig cfg;
        cfg.input_domain_bits = 10;    // smaller input domain
        cases.emplace_back(ShareConfig::Arith32(), cfg);
    }

    // 3) arithmetic ring 10, dpf in = 10, boolean out = 10
    {
        IntegerComparisonConfig cfg;
        cfg.input_domain_bits = 10;    // smaller input domain
        cases.emplace_back(ShareConfig::Custom(10), cfg);
    }

    // 4) arithmetic ring 32, but boolean output (out=1), input default=32
    {
        IntegerComparisonConfig cfg;
        cfg.boolean_output = true;
        cases.emplace_back(ShareConfig::Arith32(), cfg);
    }

    return cases;
}

inline std::string KeyPath(uint64_t n, uint64_t e, uint64_t k) {
    return kTestCompPath + "ickey_n" + ringoa::ToString(n) + "_e" + ringoa::ToString(e) + "_k" + ringoa::ToString(k);
}

inline std::string DataPath(const std::string &name, uint64_t n, uint64_t e, uint64_t k) {
    return kTestCompPath + name + "_n" + ringoa::ToString(n) + "_e" + ringoa::ToString(e) + "_k" + ringoa::ToString(k);
}

}    // namespace

namespace test_ringoa {

using ringoa::FileIo;
using ringoa::Logger;
using ringoa::ToString;
using ringoa::TwoPartyNetworkManager;
using ringoa::proto::IntegerComparison;
using ringoa::proto::IntegerComparisonConfig;
using ringoa::proto::IntegerComparisonKeys;
using ringoa::proto::IntegerComparisonParameters;
using ringoa::proto::KeyIo;
using ringoa::proto::ProtocolContext2P;
using ringoa::sharing::AdditiveSharing2P;
using ringoa::sharing::ShareConfig;

void IntegerComparison_Offline_Test() {
    Logger::DebugLog(LOC, "IntegerComparison_Offline_Test...");

    const auto cases = BuildTestCases();

    for (const auto &tc : cases) {
        IntegerComparisonParameters params(tc.cfg, tc.share);
        params.PrintParametersDebug();

        const uint64_t n = params.GetInputDomainBits();      // DPF in bits
        const uint64_t e = params.GetDdcfOutputBitsize();    // DPF out bits
        const uint64_t k = tc.share.arith_bits;              // Ring bits

        ProtocolContext2P  ctx(tc.share);
        AdditiveSharing2P &ss = ctx.Arith();
        IntegerComparison  comp(params, ctx);
        FileIo             file_io;

        // Generate keys
        auto keys = comp.GenerateKeys(5);

        // Save keys
        KeyIo::SaveKey(KeyPath(n, e, k) + "_0", keys.first);
        KeyIo::SaveKey(KeyPath(n, e, k) + "_1", keys.second);
        Logger::DebugLog(LOC, "Saved IntegerComparison batch keys to files: " + KeyPath(n, e, k) + "[_0|_1]");

        // Generate input
        const std::vector<uint64_t> x = {1, 2, 3, 4, 5};
        const std::vector<uint64_t> y = {5, 4, 3, 2, 1};

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

    Logger::DebugLog(LOC, "IntegerComparison_Offline_Test - Passed");
}

void IntegerComparison_Online_Test(const osuCrypto::CLP &cmd) {
    Logger::DebugLog(LOC, "IntegerComparison_Online_Test...");
    int party_id = cmd.getOr<int>("party", -1);

    const auto cases = BuildTestCases();

    for (const auto &tc : cases) {
        IntegerComparisonParameters params(tc.cfg, tc.share);

        const uint64_t n = params.GetInputDomainBits();      // DPF in bits
        const uint64_t e = params.GetDdcfOutputBitsize();    // DPF out bits
        const uint64_t k = tc.share.arith_bits;              // Ring bits
        FileIo         file_io;

        TwoPartyNetworkManager net_mgr("IntegerComparison_Online_Test");

        std::vector<uint64_t> z, z_batch;

        auto server_task = [&](oc::Channel &chl) {
            ProtocolContext2P  ctx(tc.share);
            AdditiveSharing2P &ss = (params.GetDdcfOutputBitsize() == 1) ? ctx.Bool() : ctx.Arith();
            IntegerComparison  comp(params, ctx);

            std::vector<uint64_t> x_0, y_0;
            std::vector<uint64_t> z_0, z_1, z_batch_0, z_batch_1;

            IntegerComparisonKeys key_0(0, params, 5);
            KeyIo::LoadKey(KeyPath(n, e, k) + "_0", key_0, params);
            Logger::DebugLog(LOC, "Loaded IntegerComparison batch key from file: " + KeyPath(n, e, k) + "_0");

            file_io.ReadBinary(DataPath("x", n, e, k) + "_0", x_0);
            file_io.ReadBinary(DataPath("y", n, e, k) + "_0", y_0);
            Logger::DebugLog(LOC, "Loaded shared batch inputs from files: " + DataPath("x", n, e, k) + "_0");

            for (size_t i = 0; i < x_0.size(); ++i) {
                z_0.push_back(comp.EvaluateSharedInput(chl, key_0.GetView(i), x_0[i], y_0[i]));
            }
            comp.EvaluateSharedInputBatch(chl, key_0, x_0, y_0, z_batch_0);

            ss.Reconst(0, chl, z_0, z_1, z);
            ss.Reconst(0, chl, z_batch_0, z_batch_1, z_batch);

            Logger::DebugLog(LOC, "[P0] z: " + ToString(z));
            Logger::DebugLog(LOC, "[P0] z_batch: " + ToString(z_batch));
        };

        auto client_task = [&](oc::Channel &chl) {
            ProtocolContext2P  ctx(tc.share);
            AdditiveSharing2P &ss = (params.GetDdcfOutputBitsize() == 1) ? ctx.Bool() : ctx.Arith();
            IntegerComparison  comp(params, ctx);

            std::vector<uint64_t> x_1, y_1;
            std::vector<uint64_t> z_0, z_1, z_batch_0, z_batch_1;

            IntegerComparisonKeys key_1(1, params, 5);
            KeyIo::LoadKey(KeyPath(n, e, k) + "_1", key_1, params);
            Logger::DebugLog(LOC, "Loaded IntegerComparison batch key from file: " + KeyPath(n, e, k) + "_1");

            file_io.ReadBinary(DataPath("x", n, e, k) + "_1", x_1);
            file_io.ReadBinary(DataPath("y", n, e, k) + "_1", y_1);
            Logger::DebugLog(LOC, "Loaded shared batch inputs from files: " + DataPath("x", n, e, k) + "_1");

            for (size_t i = 0; i < x_1.size(); ++i) {
                z_1.push_back(comp.EvaluateSharedInput(chl, key_1.GetView(i), x_1[i], y_1[i]));
            }
            comp.EvaluateSharedInputBatch(chl, key_1, x_1, y_1, z_batch_1);

            ss.Reconst(1, chl, z_0, z_1, z);
            ss.Reconst(1, chl, z_batch_0, z_batch_1, z_batch);

            Logger::DebugLog(LOC, "[P1] z: " + ToString(z));
            Logger::DebugLog(LOC, "[P1] z_batch: " + ToString(z_batch));
        };

        net_mgr.AutoConfigure(party_id, server_task, client_task);
        net_mgr.WaitForCompletion();

        if (z != std::vector<uint64_t>{0, 0, 1, 1, 1}) {
            throw oc::UnitTestFail("z is not equal to expected result");
        }
        if (z_batch != std::vector<uint64_t>{0, 0, 1, 1, 1}) {
            throw oc::UnitTestFail("z_batch is not equal to expected result");
        }

        Logger::DebugLog(LOC, "IntegerComparison_Online_Test - Passed");
    }
}

void IntegerComparison_FullDomain_Offline_Test() {
    Logger::DebugLog(LOC, "IntegerComparison_FullDomain_Offline_Test...");

    ShareConfig                 share = ShareConfig::Custom(5);
    IntegerComparisonParameters params(IntegerComparisonConfig{}, share);
    params.PrintParametersDebug();

    const uint64_t n = params.GetInputDomainBits();      // DDCF in bits
    const uint64_t e = params.GetDdcfOutputBitsize();    // DDCF out bits
    const uint64_t k = share.arith_bits;                 // Ring bits

    ProtocolContext2P  ctx(share);
    AdditiveSharing2P &ss = ctx.Arith();
    IntegerComparison  comp(params, ctx);
    FileIo             file_io;

    // Generate keys
    auto keys = comp.GenerateKeys(1);

    // Save keys
    KeyIo::SaveKey(KeyPath(n, e, k) + "_0", keys.first);
    KeyIo::SaveKey(KeyPath(n, e, k) + "_1", keys.second);
    Logger::DebugLog(LOC, "Saved IntegerComparison keys to files: " + KeyPath(n, e, k) + "[_0|_1]");

    // Generate input
    std::vector<uint64_t> x1 = ringoa::CreateSequence(0, 1 << n);
    std::vector<uint64_t> x2 = ringoa::CreateSequence(0, 1 << n);

    std::pair<std::vector<uint64_t>, std::vector<uint64_t>> x1_sh = ss.Share(x1);
    std::pair<std::vector<uint64_t>, std::vector<uint64_t>> x2_sh = ss.Share(x2);

    Logger::DebugLog(LOC, "x1: " + ToString(x1) + ", x2: " + ToString(x2));
    Logger::DebugLog(LOC, "x1_sh: " + ToString(x1_sh.first) + ", " + ToString(x1_sh.second));
    Logger::DebugLog(LOC, "x2_sh: " + ToString(x2_sh.first) + ", " + ToString(x2_sh.second));

    // Save input
    file_io.WriteBinary(DataPath("x1", n, e, k) + "_0", x1_sh.first);
    file_io.WriteBinary(DataPath("x1", n, e, k) + "_1", x1_sh.second);
    file_io.WriteBinary(DataPath("x2", n, e, k) + "_0", x2_sh.first);
    file_io.WriteBinary(DataPath("x2", n, e, k) + "_1", x2_sh.second);
    Logger::DebugLog(LOC, "Saved shared inputs to files: " + DataPath("x1", n, e, k) + "[_0|_1]");
    Logger::DebugLog(LOC, "Saved shared inputs to files: " + DataPath("x2", n, e, k) + "[_0|_1]");

    Logger::DebugLog(LOC, "IntegerComparison_FullDomain_Offline_Test - Passed");
}

void IntegerComparison_FullDomain_Online_Test(const osuCrypto::CLP &cmd) {
    Logger::DebugLog(LOC, "IntegerComparison_FullDomain_Online_Test...");
    int party_id = cmd.getOr<int>("party", -1);

    ShareConfig                 share = ShareConfig::Custom(5);
    IntegerComparisonParameters params(IntegerComparisonConfig{}, share);
    params.PrintParametersDebug();

    const uint64_t n = params.GetInputDomainBits();      // DDCF in bits
    const uint64_t e = params.GetDdcfOutputBitsize();    // DDCF out bits
    const uint64_t k = share.arith_bits;                 // Ring bits
    uint64_t       N = 1 << n;                           // Number of inputs
    FileIo         file_io;

    // Start network communication
    TwoPartyNetworkManager net_mgr("IntegerComparison_FullDomain_Online_Test");

    std::vector<uint64_t> x1(N), x2(N);
    std::vector<uint64_t> y(N * N);

    // Server task
    auto server_task = [&](oc::Channel &chl) {
        ProtocolContext2P  ctx(share);
        AdditiveSharing2P &ss_in  = ctx.Arith();
        AdditiveSharing2P &ss_out = (params.GetDdcfOutputBitsize() == 1) ? ctx.Bool() : ctx.Arith();
        IntegerComparison  comp(params, ctx);

        std::vector<uint64_t> x1_0(N), x2_0(N), x1_1(N), x2_1(N);
        std::vector<uint64_t> y_0(N * N), y_1(N * N);
        // Load keys
        IntegerComparisonKeys key_0(0, params, 1);
        KeyIo::LoadKey(KeyPath(n, e, k) + "_0", key_0, params);
        Logger::DebugLog(LOC, "Loaded IntegerComparison key from file: " + KeyPath(n, e, k) + "_0");

        // Load input
        file_io.ReadBinary(DataPath("x1", n, e, k) + "_0", x1_0);
        file_io.ReadBinary(DataPath("x2", n, e, k) + "_0", x2_0);
        Logger::DebugLog(LOC, "Loaded shared inputs from files: " + DataPath("x1", n, e, k) + "_0");
        Logger::DebugLog(LOC, "Loaded shared inputs from files: " + DataPath("x2", n, e, k) + "_0");

        // Evaluate
        for (size_t i = 0; i < x1_0.size(); ++i) {
            for (size_t j = 0; j < x2_0.size(); ++j) {
                // Compare x1_0[i] and x2_0[j]
                y_0[i * x2_0.size() + j] = comp.EvaluateSharedInput(chl, key_0.GetView(0), x1_0[i], x2_0[j]);
            }
        }

        // Send output
        ss_out.Reconst(0, chl, y_0, y_1, y);

        ss_in.Reconst(0, chl, x1_0, x1_1, x1);
        ss_in.Reconst(0, chl, x2_0, x2_1, x2);
        Logger::DebugLog(LOC, "x1: " + ToString(x1));
        Logger::DebugLog(LOC, "x2: " + ToString(x2));
        for (size_t i = 0; i < x1.size(); ++i) {
            for (size_t j = 0; j < x2.size(); ++j) {
                int64_t s1 = ringoa::UnsignedToSignedNBits(x1[i], n);
                int64_t s2 = ringoa::UnsignedToSignedNBits(x2[j], n);

                bool     signed_condition      = (std::abs(s1) + std::abs(s2)) < static_cast<int64_t>(N / 2);
                uint64_t signed_compare_result = (s1 >= s2) ? 1 : 0;
                // bool signed_compare_result = (s1 < s2) ? 1 : 0;
                bool match = (y[i * x2.size() + j] == signed_compare_result);

                std::string match_result = signed_condition ? (match ? "OK" : "NG") : "N/A";

                Logger::DebugLog(LOC,
                                 "x1[" + ToString(i) + "] = " + ToString(x1[i]) + " (" + ToString(s1) + ")" +
                                     ", x2[" + ToString(j) + "] = " + ToString(x2[j]) + " (" + ToString(s2) + ")" +
                                     ", y[" + ToString(i * x2.size() + j) + "] = " + ToString(y[i * x2.size() + j]) +
                                     ", comp: " + ToString(signed_compare_result) +
                                     ", cond: " + ToString(signed_condition) +
                                     ", ? " + match_result);

                if (match_result == "NG") {
                    Logger::FatalLog(LOC, "IntegerComparison failed at index " + ToString(i) +
                                              ", x1: " + ToString(x1[i]) + ", x2: " + ToString(x2[j]) +
                                              ", y: " + ToString(y[i * x2.size() + j]) + ", expected: " + ToString(signed_compare_result));
                }
            }
        }
        for (size_t i = 0; i < x1.size(); ++i) {
            for (size_t j = 0; j < x2.size(); ++j) {
                bool unsigned_condition      = (std::abs(int64_t(x1[i]) - int64_t(x2[j])) < static_cast<int64_t>(N / 2));
                bool unsigned_compare_result = (x1[i] >= x2[j]) ? 1 : 0;
                // bool        unsigned_compare_result = (x1[i] < x2[j]) ? 1 : 0;
                bool        unsigned_match  = (y[i * x2.size() + j] == unsigned_compare_result);
                std::string unsigned_result = unsigned_condition
                                                  ? (unsigned_match ? "OK" : "NG")
                                                  : "N/A";

                Logger::DebugLog(LOC,
                                 "x1[" + ToString(i) + "] = " + ToString(x1[i]) +
                                     ", x2[" + ToString(j) + "] = " + ToString(x2[j]) +
                                     ", y[" + ToString(i * x2.size() + j) + "] = " + ToString(y[i * x2.size() + j]) +
                                     ", comp: " + ToString(unsigned_compare_result) +
                                     ", cond: " + ToString(unsigned_condition) +
                                     ", ? " + unsigned_result);

                if (unsigned_result == "NG") {
                    Logger::FatalLog(LOC,
                                     "Unsigned comparison failed at (" +
                                         ToString(i) + "," + ToString(j) + "): " +
                                         "got=" + ToString(y[i * x2.size() + j]) +
                                         ", exp=" + ToString(unsigned_compare_result));
                }
            }
        }
    };

    // Client task
    auto client_task = [&](oc::Channel &chl) {
        ProtocolContext2P  ctx(share);
        AdditiveSharing2P &ss_in  = ctx.Arith();
        AdditiveSharing2P &ss_out = (params.GetDdcfOutputBitsize() == 1) ? ctx.Bool() : ctx.Arith();
        IntegerComparison  comp(params, ctx);

        std::vector<uint64_t> x1_1(N), x2_1(N), x1_0(N), x2_0(N);
        std::vector<uint64_t> y_0(N * N), y_1(N * N);
        // Load keys
        IntegerComparisonKeys key_1(1, params, 1);
        KeyIo::LoadKey(KeyPath(n, e, k) + "_1", key_1, params);
        Logger::DebugLog(LOC, "Loaded IntegerComparison key from file: " + KeyPath(n, e, k) + "_1");

        // Load input
        file_io.ReadBinary(DataPath("x1", n, e, k) + "_1", x1_1);
        file_io.ReadBinary(DataPath("x2", n, e, k) + "_1", x2_1);
        Logger::DebugLog(LOC, "Loaded shared inputs from files: " + DataPath("x1", n, e, k) + "_1");
        Logger::DebugLog(LOC, "Loaded shared inputs from files: " + DataPath("x2", n, e, k) + "_1");

        // Evaluate
        for (size_t i = 0; i < x1_1.size(); ++i) {
            for (size_t j = 0; j < x2_1.size(); ++j) {
                // Compare x1_1[i] and x2_1[j]
                y_1[i * x2_1.size() + j] = comp.EvaluateSharedInput(chl, key_1.GetView(0), x1_1[i], x2_1[j]);
            }
        }

        // Send output
        ss_out.Reconst(1, chl, y_0, y_1, y);

        ss_in.Reconst(1, chl, x1_0, x1_1, x1);
        ss_in.Reconst(1, chl, x2_0, x2_1, x2);
    };

    net_mgr.AutoConfigure(party_id, server_task, client_task);
    net_mgr.WaitForCompletion();
    Logger::DebugLog(LOC, "IntegerComparison_FullDomain_Online_Test - Passed");
}

}    // namespace test_ringoa
