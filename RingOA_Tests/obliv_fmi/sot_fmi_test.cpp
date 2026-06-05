#include "sot_fmi_test.h"

#include <random>

#include <cryptoTools/Common/TestCollection.h>

#include "RingOA/obliv_fmi/sot_fmi.h"
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

const std::string kCurrentPath    = ringoa::GetCurrentDirectory();
const std::string kTestSotFMIPath = kCurrentPath + "/data/test/obliv_fmi/sotfmi/";
const uint64_t    kFixedSeed      = 6;

std::string GenerateRandomString(size_t length, const std::string &charset = "ATGC") {
    if (charset.empty() || length == 0)
        return "";
    static thread_local std::mt19937_64                       rng(kFixedSeed);
    static thread_local std::uniform_int_distribution<size_t> dist(0, charset.size() - 1);
    std::string                                               result(length, '\0');
    for (size_t i = 0; i < length; ++i) {
        result[i] = charset[dist(rng)];
    }
    return result;
}

struct TestCase {
    ringoa::sharing::ShareConfig share;
    ringoa::fmi::SotFMIConfig    cfg;

    TestCase(ringoa::sharing::ShareConfig s,
             ringoa::fmi::SotFMIConfig    e)
        : share(std::move(s)), cfg(std::move(e)) {
    }
};

std::vector<TestCase> BuildTestCases() {
    using ringoa::fmi::SotFMIConfig;
    using ringoa::sharing::ShareConfig;

    std::vector<TestCase> cases;

    // 1) small arithmetic ring (arith_bits=10), db size = 10
    {
        SotFMIConfig cfg{10, 8};
        cases.emplace_back(ShareConfig::Custom(10), cfg);
    }

    // 2) arithmetic ring 32, db size = 10
    {
        SotFMIConfig cfg{10, 8};
        cases.emplace_back(ShareConfig::Arith32(), cfg);
    }

    return cases;
}

inline std::string MakePath(const std::string &name, uint64_t d, uint64_t qs, uint64_t k) {
    return kTestSotFMIPath + name + "_d" + ringoa::ToString(d) + "_qs" + ringoa::ToString(qs) + "_k" + ringoa::ToString(k);
}

}    // namespace

namespace test_ringoa {

using ringoa::Channels;
using ringoa::CreateSequence;
using ringoa::FileIo;
using ringoa::Logger;
using ringoa::ThreePartyNetworkManager;
using ringoa::ToString;
using ringoa::fmi::SotFMI;
using ringoa::fmi::SotFMIKeys;
using ringoa::fmi::SotFMIParameters;
using ringoa::fmi::SotFMIPreprocessData;
using ringoa::proto::KeyIo;
using ringoa::proto::ProtocolContext2P;
using ringoa::proto::ProtocolContext3P;
using ringoa::proto::ProtocolIo;
using ringoa::sharing::Rep3Share64;
using ringoa::sharing::Rep3ShareIo;
using ringoa::sharing::Rep3ShareMat64;
using ringoa::sharing::Rep3ShareVec64;
using ringoa::sharing::Rep3ShareView64;
using ringoa::sharing::ReplicatedSharing3P;
using ringoa::wm::FMIndex;

void SotFMI_DataGen_Test() {
    Logger::DebugLog(LOC, "SotFMI_DataGen_Test...");

    const auto cases = BuildTestCases();

    for (const auto &tc : cases) {
        SotFMIParameters params(tc.cfg, tc.share);
        params.PrintParametersDebug();

        uint64_t d  = params.GetDbIndexBits();
        uint64_t k  = params.GetShareBitsize();
        uint64_t ds = params.GetDatabaseSize();
        uint64_t qs = params.GetQuerySize();

        ProtocolContext2P ctx2p(tc.share);
        ProtocolContext3P ctx3p(tc.share);
        SotFMI            sotfmi(params, ctx2p, ctx3p);

        FileIo file_io;

        // Generate the database and index
        std::string database = GenerateRandomString(ds - 2);
        FMIndex     fm(database);
        std::string query = GenerateRandomString(qs);
        Logger::DebugLog(LOC, "Database: " + database);
        Logger::DebugLog(LOC, "Query   : " + query);

        std::array<Rep3ShareMat64, 3> db_sh    = sotfmi.GenerateDatabaseU64Share(fm);
        std::array<Rep3ShareMat64, 3> query_sh = sotfmi.GenerateQueryU64Share(fm, query);
        for (size_t p = 0; p < ringoa::sharing::kThreeParties; ++p) {
            Logger::DebugLog(LOC, "Party " + ToString(p) + " rank share: " + db_sh[p].ToStringMatrix());
            Logger::DebugLog(LOC, "Party " + ToString(p) + " query share: " + query_sh[p].ToStringMatrix());
        }

        // Save data
        file_io.WriteBinary(MakePath("db", d, qs, k), database);
        file_io.WriteBinary(MakePath("query", d, qs, k), query);
        Logger::DebugLog(LOC, "Saved database to file: " + MakePath("db", d, qs, k));
        Logger::DebugLog(LOC, "Saved query to file: " + MakePath("query", d, qs, k));

        for (size_t p = 0; p < ringoa::sharing::kThreeParties; ++p) {
            Rep3ShareIo::SaveShare(MakePath("db", d, qs, k) + "_" + ToString(p), db_sh[p]);
            Rep3ShareIo::SaveShare(MakePath("query", d, qs, k) + "_" + ToString(p), query_sh[p]);
            Logger::DebugLog(LOC, "Saved party " + ToString(p) + " database share to file: " + MakePath("db", d, qs, k) + "_" + ToString(p));
            Logger::DebugLog(LOC, "Saved party " + ToString(p) + " query share to file: " + MakePath("query", d, qs, k) + "_" + ToString(p));
        }
    }
    Logger::DebugLog(LOC, "SotFMI_DataGen_Test - Passed");
}

void SotFMI_Preprocess_Test(const osuCrypto::CLP &cmd) {
    Logger::DebugLog(LOC, "SotFMI_Preprocess_Test...");

    const auto cases = BuildTestCases();

    for (const auto &tc : cases) {
        SotFMIParameters params(tc.cfg, tc.share);
        params.PrintParametersDebug();

        uint64_t d  = params.GetDbIndexBits();
        uint64_t k  = params.GetShareBitsize();
        uint64_t qs = params.GetQuerySize();

        // Define the task for each party
        auto MakeTask = [&](int party_id) {
            return [=](osuCrypto::Channel &chl_next, osuCrypto::Channel &chl_prev) {
                ProtocolContext2P ctx2p(tc.share);
                ProtocolContext3P ctx3p(tc.share);
                SotFMI            sotfmi(params, ctx2p, ctx3p);
                Channels          chls(party_id, chl_prev, chl_next);

                // Generate preprocess data
                ctx3p.StartCommStats(chls);
                auto rep3_data    = ringoa::sharing::PreprocessRep3PrfKeys(chls);
                auto preproc_data = sotfmi.Preprocess(chls);
                Logger::DebugLog(LOC, "[P" + ToString(party_id) + "] sent: " + ToString(ctx3p.StopCommStats(chls)) + " bytes");

                // Save preprocess data
                ringoa::sharing::SaveRep3PreprocessDataToFile(MakePath("rep3_prf_keys", d, qs, k) + "_" + ToString(party_id), rep3_data);
                Logger::DebugLog(LOC, "Saved Rep3 PRF preprocess data to file: " + MakePath("rep3_prf_keys", d, qs, k) + "_" + ToString(party_id));
                ProtocolIo::SaveToFile(MakePath("sot_fmi_preproc", d, qs, k) + "_" + ToString(party_id), preproc_data);
                Logger::DebugLog(LOC, "Saved SotFMI preprocess data to file: " + MakePath("sot_fmi_preproc", d, qs, k) + "_" + ToString(party_id));

                // Load preprocess data (verify)
                ringoa::sharing::Rep3PreprocessData loaded_rep3_data;
                ringoa::sharing::LoadRep3PreprocessDataFromFile(MakePath("rep3_prf_keys", d, qs, k) + "_" + ToString(party_id), loaded_rep3_data);
                Logger::DebugLog(LOC, "Loaded Rep3 PRF preprocess data from file: " + MakePath("rep3_prf_keys", d, qs, k) + "_" + ToString(party_id));
                SotFMIPreprocessData loaded_data;
                ProtocolIo::LoadFromFile(MakePath("sot_fmi_preproc", d, qs, k) + "_" + ToString(party_id), loaded_data, params);
                Logger::DebugLog(LOC, "Loaded SotFMI preprocess data from file: " + MakePath("sot_fmi_preproc", d, qs, k) + "_" + ToString(party_id));
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
    Logger::DebugLog(LOC, "SotFMI_Preprocess_Test - Passed");
}

void SotFMI_Online_Test(const osuCrypto::CLP &cmd) {
    Logger::DebugLog(LOC, "SotFMI_Online_Test...");

    const auto cases = BuildTestCases();

    for (const auto &tc : cases) {
        SotFMIParameters params(tc.cfg, tc.share);
        params.PrintParametersDebug();

        uint64_t d  = params.GetDbIndexBits();
        uint64_t k  = params.GetShareBitsize();
        uint64_t qs = params.GetQuerySize();

        FileIo file_io;

        std::vector<uint64_t> result;

        std::string database;
        std::string query;
        file_io.ReadBinary(MakePath("db", d, qs, k), database);
        file_io.ReadBinary(MakePath("query", d, qs, k), query);
        Logger::DebugLog(LOC, "Loaded database from file: " + MakePath("db", d, qs, k));
        Logger::DebugLog(LOC, "Loaded query from file: " + MakePath("query", d, qs, k));

        auto MakeTask = [&](int party_id) {
            return [=, &result](osuCrypto::Channel &chl_next, osuCrypto::Channel &chl_prev) {
                ProtocolContext2P    ctx2p(tc.share);
                ProtocolContext3P    ctx3p(tc.share);
                ReplicatedSharing3P &rss = ctx3p.Rss();
                SotFMI               sotfmi(params, ctx2p, ctx3p);
                Channels             chls(party_id, chl_prev, chl_next);

                // Load this party's shares of the database and query
                Rep3ShareMat64 db_sh;
                Rep3ShareMat64 query_sh;
                Rep3ShareIo::LoadShare(MakePath("db", d, qs, k) + "_" + ToString(party_id), db_sh);
                Rep3ShareIo::LoadShare(MakePath("query", d, qs, k) + "_" + ToString(party_id), query_sh);
                Logger::DebugLog(LOC, "Loaded shared database from file: " + MakePath("db", d, qs, k) + "_" + ToString(party_id));
                Logger::DebugLog(LOC, "Loaded shared query from file: " + MakePath("query", d, qs, k) + "_" + ToString(party_id));

                // Load preprocess data
                ringoa::sharing::Rep3PreprocessData loaded_rep3_data;
                ringoa::sharing::LoadRep3PreprocessDataFromFile(MakePath("rep3_prf_keys", d, qs, k) + "_" + ToString(party_id), loaded_rep3_data);
                Logger::DebugLog(LOC, "Loaded Rep3 PRF preprocess data from file: " + MakePath("rep3_prf_keys", d, qs, k) + "_" + ToString(party_id));
                SotFMIPreprocessData loaded_data;
                ProtocolIo::LoadFromFile(MakePath("sot_fmi_preproc", d, qs, k) + "_" + ToString(party_id), loaded_data, params);
                Logger::DebugLog(LOC, "Loaded SotFMI preprocess data from file: " + MakePath("sot_fmi_preproc", d, qs, k) + "_" + ToString(party_id));

                // Set PRF keys and OblivRange keys
                ctx3p.Rss().SetPrfKeys(loaded_rep3_data.prf_key_self, loaded_rep3_data.prf_key_next);
                SotFMIKeys key = std::move(loaded_data).ExtractKeys();

                // Evaluate the longest-prefix-match operation
                Rep3ShareVec64        result_sh(qs);
                std::vector<uint64_t> uv_prev(1U << d), uv_next(1U << d);
                sotfmi.EvaluateLPMPair(chls, key, uv_prev, uv_next, db_sh, query_sh, result_sh);

                // Open the resulting share vector to recover the final plaintext vector
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

        // Compute expected longest-prefix-match length using FM-index
        FMIndex  fmi(database);
        uint64_t expected_result = fmi.ComputeLPMfromWM(query);

        // Count how many zero entries in the returned result vector:
        // each zero indicates a matched prefix position
        uint64_t match_len = 0;
        for (size_t i = 0; i < result.size(); ++i) {
            if (result[i] == 0) {
                match_len++;
            }
        }

        if (match_len != expected_result) {
            throw osuCrypto::UnitTestFail(
                "SotFMI_Online_Test failed: result = " + ToString(match_len) +
                ", expected = " + ToString(expected_result));
        }
    }

    Logger::DebugLog(LOC, "SotFMI_Online_Test - Passed");
}

}    // namespace test_ringoa
