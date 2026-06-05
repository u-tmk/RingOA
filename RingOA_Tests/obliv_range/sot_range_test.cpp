#include "obliv_range_test.h"

#include <random>

#include <cryptoTools/Common/TestCollection.h>

#include "RingOA/obliv_range/sot_range.h"
#include "RingOA/protocol/protocol_io.h"
#include "RingOA/sharing/rep3_preprocess.h"
#include "RingOA/sharing/rep3_share_io.h"
#include "RingOA/sharing/rep3_sharing_ring.h"
#include "RingOA/sharing/sharing_2p_ring.h"
#include "RingOA/utils/logger.h"
#include "RingOA/utils/network.h"
#include "RingOA/utils/timer.h"
#include "RingOA/utils/to_string.h"
#include "RingOA/utils/utils.h"
#include "RingOA/wm/plain_wm.h"

namespace {

const std::string kCurrentPath      = ringoa::GetCurrentDirectory();
const std::string kTestSotRangePath = kCurrentPath + "/data/test/obliv_range/sotrange/";
const uint64_t    kFixedSeed        = 6;

std::vector<uint64_t> GenerateRandomVector(size_t length, uint64_t sigma) {
    if (length == 0)
        return {};
    static thread_local std::mt19937_64                       rng(kFixedSeed);
    static thread_local std::uniform_int_distribution<size_t> dist(0, 1U << sigma);
    std::vector<uint64_t>                                     result(length);
    for (size_t i = 0; i < length; ++i) {
        result[i] = dist(rng);
    }
    return result;
}

struct TestCase {
    ringoa::sharing::ShareConfig  share;
    ringoa::range::SotRangeConfig cfg;

    TestCase(ringoa::sharing::ShareConfig  s,
             ringoa::range::SotRangeConfig e)
        : share(std::move(s)), cfg(std::move(e)) {
    }
};

std::vector<TestCase> BuildTestCases() {
    using ringoa::range::SotRangeConfig;
    using ringoa::sharing::ShareConfig;

    std::vector<TestCase> cases;

    // 1) small arithmetic ring (arith_bits=10), db size = 10
    {
        SotRangeConfig cfg{/* db_bits=*/10, /* sigma=*/7};
        cases.emplace_back(ShareConfig::Custom(11), cfg);
    }

    // 2) arithmetic ring 32, db size = 10
    {
        SotRangeConfig cfg{/* db_bits=*/10, /* sigma=*/7};
        cases.emplace_back(ShareConfig::Arith32(), cfg);
    }

    return cases;
}

inline std::string MakePath(const std::string &name, uint64_t d, uint64_t k) {
    return kTestSotRangePath + name + "_d" + ringoa::ToString(d) + "_k" + ringoa::ToString(k);
}

}    // namespace

namespace test_ringoa {

using ringoa::Channels;
using ringoa::FileIo;
using ringoa::Logger;
using ringoa::ThreePartyNetworkManager;
using ringoa::ToString, ringoa::ToStringMatrix;
using ringoa::proto::ProtocolContext2P;
using ringoa::proto::ProtocolContext3P;
using ringoa::proto::ProtocolIo;
using ringoa::range::SotRange;
using ringoa::range::SotRangeKeys;
using ringoa::range::SotRangeParameters;
using ringoa::range::SotRangePreprocessData;
using ringoa::sharing::Rep3Share64;
using ringoa::sharing::Rep3ShareIo;
using ringoa::sharing::Rep3ShareMat64;
using ringoa::sharing::Rep3ShareVec64;
using ringoa::sharing::Rep3ShareView64;
using ringoa::sharing::ReplicatedSharing3P;
using ringoa::wm::WaveletMatrix;

void SotRange_DataGen_Test() {
    Logger::DebugLog(LOC, "SotRange_DataGen_Test...");

    const auto cases = BuildTestCases();

    for (const auto &tc : cases) {
        SotRangeParameters params(tc.cfg, tc.share);
        params.PrintParametersDebug();

        uint64_t d  = params.GetDbIndexBits();
        uint64_t ds = params.GetDatabaseSize();
        uint64_t k  = params.GetShareBitsize();

        ProtocolContext2P    ctx2p(tc.share);
        ProtocolContext3P    ctx3p(tc.share);
        ReplicatedSharing3P &rss = ctx3p.Rss();
        SotRange             sot_range(params, ctx2p, ctx3p);

        FileIo file_io;

        // Generate the database and index
        std::vector<uint64_t>
                              database = GenerateRandomVector(ds - 1, params.GetSigma());
        std::vector<uint64_t> q_arg    = {/* left = */ 100, /* right = */ 150, /* k = */ 49};
        WaveletMatrix         wm(database, params.GetSigma());
        Logger::DebugLog(LOC, "Database: " + ToString(database));
        Logger::DebugLog(LOC, "Left: " + ToString(q_arg[0]) + ", Right: " + ToString(q_arg[1]) + ", k: " + ToString(q_arg[2]));

        // Generate replicated shares for the database, query, and position
        std::array<Rep3ShareMat64, 3> db_sh    = sot_range.GenerateDatabaseU64Share(wm);
        std::array<Rep3ShareVec64, 3> q_arg_sh = rss.ShareLocal(q_arg);
        for (size_t p = 0; p < ringoa::sharing::kThreeParties; ++p) {
            Logger::DebugLog(LOC, "Party " + ToString(p) + " rank share: " + db_sh[p].ToStringMatrix());
            Logger::DebugLog(LOC, "Party " + ToString(p) + " (left, right, k) share: " + q_arg_sh[p].ToString());
        }

        // Save data
        file_io.WriteBinary(MakePath("db", d, k), database);
        file_io.WriteBinary(MakePath("query", d, k), q_arg);
        Logger::DebugLog(LOC, "Saved database to file: " + MakePath("db", d, k));
        Logger::DebugLog(LOC, "Saved query to file: " + MakePath("query", d, k));

        for (size_t p = 0; p < ringoa::sharing::kThreeParties; ++p) {
            Rep3ShareIo::SaveShare(MakePath("db", d, k) + "_" + ToString(p), db_sh[p]);
            Rep3ShareIo::SaveShare(MakePath("query", d, k) + "_" + ToString(p), q_arg_sh[p]);
            Logger::DebugLog(LOC, "Saved party " + ToString(p) + " database share to file: " + MakePath("db", d, k) + "_" + ToString(p));
            Logger::DebugLog(LOC, "Saved party " + ToString(p) + " query share to file: " + MakePath("query", d, k) + "_" + ToString(p));
        }
    }
    Logger::DebugLog(LOC, "SotRange_DataGen_Test - Passed");
}

void SotRange_Preprocess_Test(const osuCrypto::CLP &cmd) {
    Logger::DebugLog(LOC, "SotRange_Preprocess_Test...");

    const auto cases = BuildTestCases();

    for (const auto &tc : cases) {
        SotRangeParameters params(tc.cfg, tc.share);
        params.PrintParametersDebug();

        uint64_t d = params.GetDbIndexBits();
        uint64_t k = params.GetShareBitsize();

        // Define the task for each party
        auto MakeTask = [&](int party_id) {
            return [=](osuCrypto::Channel &chl_next, osuCrypto::Channel &chl_prev) {
                ProtocolContext2P ctx2p(tc.share);
                ProtocolContext3P ctx3p(tc.share);
                SotRange          sot_range(params, ctx2p, ctx3p);
                Channels          chls(party_id, chl_prev, chl_next);

                // Generate preprocess data
                ctx3p.StartCommStats(chls);
                auto rep3_data    = ringoa::sharing::PreprocessRep3PrfKeys(chls);
                auto preproc_data = sot_range.Preprocess(chls);
                Logger::DebugLog(LOC, "[P" + ToString(party_id) + "] sent: " + ToString(ctx3p.StopCommStats(chls)) + " bytes");

                // Save preprocess data
                ringoa::sharing::SaveRep3PreprocessDataToFile(MakePath("rep3_prf_keys", d, k) + "_" + ToString(party_id), rep3_data);
                Logger::DebugLog(LOC, "Saved Rep3 PRF preprocess data to file: " + MakePath("rep3_prf_keys", d, k) + "_" + ToString(party_id));
                ProtocolIo::SaveToFile(MakePath("sot_range_preproc", d, k) + "_" + ToString(party_id), preproc_data);
                Logger::DebugLog(LOC, "Saved SotRange preprocess data to file: " + MakePath("sot_range_preproc", d, k) + "_" + ToString(party_id));

                // Load preprocess data (verify)
                ringoa::sharing::Rep3PreprocessData loaded_rep3_data;
                ringoa::sharing::LoadRep3PreprocessDataFromFile(MakePath("rep3_prf_keys", d, k) + "_" + ToString(party_id), loaded_rep3_data);
                Logger::DebugLog(LOC, "Loaded Rep3 PRF preprocess data from file: " + MakePath("rep3_prf_keys", d, k) + "_" + ToString(party_id));
                SotRangePreprocessData loaded_data;
                ProtocolIo::LoadFromFile(MakePath("sot_range_preproc", d, k) + "_" + ToString(party_id), loaded_data, params);
                Logger::DebugLog(LOC, "Loaded SotRange preprocess data from file: " + MakePath("sot_range_preproc", d, k) + "_" + ToString(party_id));
            };
        };

        // Create tasks for each party
        auto task_p0 = MakeTask(0);
        auto task_p1 = MakeTask(1);
        auto task_p2 = MakeTask(2);

        ThreePartyNetworkManager net_mgr;
        // Configure network based on party ID and wait for completion
        int party_id = cmd.getOr<int>("party", -1);
        net_mgr.AutoConfigure(party_id, task_p0, task_p1, task_p2);
        net_mgr.WaitForCompletion();
    }
    Logger::DebugLog(LOC, "SotRange_Preprocess_Test - Passed");
}

void SotRange_Online_Test(const osuCrypto::CLP &cmd) {
    Logger::DebugLog(LOC, "SotRange_Online_Test...");

    const auto cases = BuildTestCases();

    for (const auto &tc : cases) {
        SotRangeParameters params(tc.cfg, tc.share);
        params.PrintParametersDebug();

        uint64_t d = params.GetDbIndexBits();
        uint64_t k = params.GetShareBitsize();

        FileIo file_io;

        uint64_t result{0};

        std::vector<uint64_t> database;
        std::vector<uint64_t> q_arg;
        file_io.ReadBinary(MakePath("db", d, k), database);
        file_io.ReadBinary(MakePath("query", d, k), q_arg);

        auto MakeTask = [&](int party_id) {
            return [=, &result](osuCrypto::Channel &chl_next, osuCrypto::Channel &chl_prev) {
                ProtocolContext2P    ctx2p(tc.share);
                ProtocolContext3P    ctx3p(tc.share);
                ReplicatedSharing3P &rss = ctx3p.Rss();
                SotRange             sot_range(params, ctx2p, ctx3p);
                Channels             chls(party_id, chl_prev, chl_next);

                // Load this party's shares of the database and query
                Rep3ShareMat64 db_sh;
                Rep3ShareVec64 q_arg_sh;
                Rep3ShareIo::LoadShare(MakePath("db", d, k) + "_" + ToString(party_id), db_sh);
                Rep3ShareIo::LoadShare(MakePath("query", d, k) + "_" + ToString(party_id), q_arg_sh);
                Logger::DebugLog(LOC, "Loaded shared database from file: " + MakePath("db", d, k) + "_" + ToString(party_id));
                Logger::DebugLog(LOC, "Loaded shared query from file: " + MakePath("query", d, k) + "_" + ToString(party_id));

                // Load preprocess data
                ringoa::sharing::Rep3PreprocessData loaded_rep3_data;
                ringoa::sharing::LoadRep3PreprocessDataFromFile(MakePath("rep3_prf_keys", d, k) + "_" + ToString(party_id), loaded_rep3_data);
                Logger::DebugLog(LOC, "Loaded Rep3 PRF preprocess data from file: " + MakePath("rep3_prf_keys", d, k) + "_" + ToString(party_id));
                SotRangePreprocessData preproc_data;
                ProtocolIo::LoadFromFile(MakePath("sot_range_preproc", d, k) + "_" + ToString(party_id), preproc_data, params);
                Logger::DebugLog(LOC, "Loaded SotRange preprocess data from file: " + MakePath("sot_range_preproc", d, k) + "_" + ToString(party_id));

                // Set PRF keys and SotRange keys
                ctx3p.Rss().SetPrfKeys(loaded_rep3_data.prf_key_self, loaded_rep3_data.prf_key_next);
                SotRangeKeys key = std::move(preproc_data).ExtractKeys();

                // Evaluate the rank operation
                Rep3Share64           result_sh;
                Rep3Share64           left_sh = q_arg_sh.At(0), right_sh = q_arg_sh.At(1), k_sh = q_arg_sh.At(2);
                std::vector<uint64_t> uv_prev(1U << d), uv_next(1U << d);
                sot_range.EvaluateRangePair(chls, key, uv_prev, uv_next,
                                            db_sh, left_sh, right_sh, k_sh, result_sh);

                // Open the resulting share to recover the final value
                rss.Open(chls, result_sh, result);
            };
        };

        // Instantiate tasks for parties 0, 1, and 2
        auto task_p0 = MakeTask(0);
        auto task_p1 = MakeTask(1);
        auto task_p2 = MakeTask(2);

        ThreePartyNetworkManager net_mgr;
        int                      party_id = cmd.isSet("party") ? cmd.get<int>("party") : -1;
        net_mgr.AutoConfigure(party_id, task_p0, task_p1, task_p2);
        net_mgr.WaitForCompletion();

        Logger::DebugLog(LOC, "Result: " + ToString(result));

        // Verify against the plain-wavelet-matrix rank computation
        WaveletMatrix wm(database, params.GetSigma());
        uint64_t      expected_result = wm.Quantile(q_arg[0], q_arg[1], q_arg[2]);
        if (result != expected_result) {
            throw osuCrypto::UnitTestFail(
                "SotRange_Online_Test failed: result = " + ToString(result) +
                ", expected = " + ToString(expected_result));
        }
    }

    Logger::DebugLog(LOC, "SotRange_Online_Test - Passed");
}

}    // namespace test_ringoa
