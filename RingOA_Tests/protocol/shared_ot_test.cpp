#include "shared_ot_test.h"

#include <cryptoTools/Common/TestCollection.h>

#include "RingOA/protocol/protocol_io.h"
#include "RingOA/protocol/shared_ot.h"
#include "RingOA/sharing/rep3_preprocess.h"
#include "RingOA/sharing/rep3_share_io.h"
#include "RingOA/sharing/rep3_sharing_ring.h"
#include "RingOA/sharing/sharing_2p_ring.h"
#include "RingOA/utils/logger.h"
#include "RingOA/utils/network.h"
#include "RingOA/utils/rng.h"
#include "RingOA/utils/to_string.h"
#include "RingOA/utils/utils.h"

namespace {

const std::string kCurrentPath = ringoa::GetCurrentDirectory();
const std::string kTestSotPath = kCurrentPath + "/data/test/protocol/shared_ot/";

struct TestCase {
    ringoa::sharing::ShareConfig  share;
    ringoa::proto::SharedOtConfig cfg;

    TestCase(ringoa::sharing::ShareConfig  s,
             ringoa::proto::SharedOtConfig e)
        : share(std::move(s)), cfg(std::move(e)) {
    }
};

std::vector<TestCase> BuildTestCases() {
    using ringoa::proto::SharedOtConfig;
    using ringoa::sharing::ShareConfig;

    std::vector<TestCase> cases;

    // 1) small arithmetic ring (arith_bits=5), db size = 5
    {
        SharedOtConfig cfg{5};
        cases.emplace_back(ShareConfig::Custom(5), cfg);
    }

    // 2) arithmetic ring 10, db size = 10
    {
        SharedOtConfig cfg{10};
        cases.emplace_back(ShareConfig::Custom(10), cfg);
    }

    // 3) arithmetic ring 32, db size = 10
    {
        SharedOtConfig cfg{10};
        cases.emplace_back(ShareConfig::Arith32(), cfg);
    }

    return cases;
}

inline std::string MakePath(const std::string &name, uint64_t d, uint64_t k) {
    return kTestSotPath + name + "_d" + ringoa::ToString(d) + "_k" + ringoa::ToString(k);
}

}    // namespace

namespace test_ringoa {

using ringoa::Channels;
using ringoa::FileIo;
using ringoa::GlobalRng;
using ringoa::Logger;
using ringoa::Mod2N;
using ringoa::ThreePartyNetworkManager;
using ringoa::ToString, ringoa::Format;
using ringoa::fss::EvalType;
using ringoa::proto::KeyIo;
using ringoa::proto::ProtocolContext3P;
using ringoa::proto::ProtocolIo;
using ringoa::proto::SharedOt;
using ringoa::proto::SharedOtKeys;
using ringoa::proto::SharedOtParameters;
using ringoa::proto::SharedOtPreprocessData;
using ringoa::sharing::AdditiveSharing2P;
using ringoa::sharing::Rep3Share64;
using ringoa::sharing::Rep3ShareIo;
using ringoa::sharing::Rep3ShareVec64;
using ringoa::sharing::Rep3ShareView64;
using ringoa::sharing::ReplicatedSharing3P;

void SharedOT_DataGen_Test() {
    Logger::DebugLog(LOC, "SharedOT_DataGen_Test...");

    const auto cases = BuildTestCases();

    for (const auto &tc : cases) {
        SharedOtParameters params(tc.cfg, tc.share);
        params.PrintParametersDebug();

        uint64_t d = params.GetDbIndexBits();
        uint64_t k = params.GetShareBitsize();

        ProtocolContext3P    ctx(tc.share);
        ReplicatedSharing3P &rss = ctx.Rss();

        FileIo file_io;

        // Generate the database and index
        std::vector<uint64_t> database(1U << d);
        for (size_t i = 0; i < database.size(); ++i) {
            database[i] = i;
        }
        Logger::DebugLog(LOC, "Database: " + ToString(database));

        std::array<Rep3ShareVec64, 3> database_sh = rss.ShareLocal(database);
        for (size_t p = 0; p < ringoa::sharing::kThreeParties; ++p) {
            Logger::DebugLog(LOC, "Party " + ToString(p) + " db: " + database_sh[p].ToString());
        }

        // Save data
        file_io.WriteBinary(MakePath("db", d, k), database);
        Logger::DebugLog(LOC, "Saved database to file: " + MakePath("db", d, k));

        for (size_t p = 0; p < ringoa::sharing::kThreeParties; ++p) {
            Rep3ShareIo::SaveShare(MakePath("db", d, k) + "_" + ToString(p), database_sh[p]);
            Logger::DebugLog(LOC, "Saved shared database to file: " + MakePath("db", d, k) + "_" + ToString(p));
        }

        // Generate a random index
        uint64_t index = Mod2N(GlobalRng::Rand<uint64_t>(), d);
        Logger::DebugLog(LOC, "Index: " + ToString(index));
        std::array<Rep3Share64, 3> index_sh = rss.ShareLocal(index);
        for (size_t p = 0; p < ringoa::sharing::kThreeParties; ++p) {
            Logger::DebugLog(LOC, "Party " + ToString(p) + " index share: " + index_sh[p].ToString());
        }
        file_io.WriteBinary(MakePath("idx", d, k), index);
        Logger::DebugLog(LOC, "Saved index to file: " + MakePath("idx", d, k));

        for (size_t p = 0; p < ringoa::sharing::kThreeParties; ++p) {
            Rep3ShareIo::SaveShare(MakePath("idx", d, k) + "_" + ToString(p), index_sh[p]);
            Logger::DebugLog(LOC, "Saved shared index to file: " + MakePath("idx", d, k) + "_" + ToString(p));
        }
    }
    Logger::DebugLog(LOC, "SharedOT_DataGen_Test - Passed");
}

void SharedOT_Preprocess_Test(const osuCrypto::CLP &cmd) {
    Logger::DebugLog(LOC, "SharedOT_Preprocess_Test...");

    const auto cases = BuildTestCases();

    for (const auto &tc : cases) {
        SharedOtParameters params(tc.cfg, tc.share);
        params.PrintParametersDebug();

        uint64_t d = params.GetDbIndexBits();
        uint64_t k = params.GetShareBitsize();

        // Define the task for each party
        auto MakeTask = [&](int party_id) {
            return [=](osuCrypto::Channel &chl_next, osuCrypto::Channel &chl_prev) {
                ProtocolContext3P ctx(tc.share);
                SharedOt          shared_ot(params, ctx);
                Channels          chls(party_id, chl_prev, chl_next);

                // Generate preprocess data
                size_t count = 3;
                ctx.StartCommStats(chls);
                auto rep3_data    = ringoa::sharing::PreprocessRep3PrfKeys(chls);
                auto preproc_data = shared_ot.Preprocess(chls, count);
                Logger::DebugLog(LOC, "[P" + ToString(party_id) + "] sent: " + ToString(ctx.StopCommStats(chls)) + " bytes");

                // Save preprocess data
                ringoa::sharing::SaveRep3PreprocessDataToFile(MakePath("rep3_prf_keys", d, k) + "_" + ToString(party_id), rep3_data);
                Logger::DebugLog(LOC, "Saved Rep3 PRF preprocess data to file: " + MakePath("rep3_prf_keys", d, k) + "_" + ToString(party_id));
                ProtocolIo::SaveToFile(MakePath("sot_preproc", d, k) + "_" + ToString(party_id), preproc_data);
                Logger::DebugLog(LOC, "Saved SharedOt preprocess data to file: " + MakePath("sot_preproc", d, k) + "_" + ToString(party_id));

                // Load preprocess data (verify)
                ringoa::sharing::Rep3PreprocessData loaded_rep3_data;
                ringoa::sharing::LoadRep3PreprocessDataFromFile(MakePath("rep3_prf_keys", d, k) + "_" + ToString(party_id), loaded_rep3_data);
                Logger::DebugLog(LOC, "Loaded Rep3 PRF preprocess data from file: " + MakePath("rep3_prf_keys", d, k) + "_" + ToString(party_id));
                SharedOtPreprocessData loaded_data(SharedOtKeys(party_id, params, count));
                ProtocolIo::LoadFromFile(MakePath("sot_preproc", d, k) + "_" + ToString(party_id), loaded_data, params);
                Logger::DebugLog(LOC, "Loaded SharedOt preprocess data from file: " + MakePath("sot_preproc", d, k) + "_" + ToString(party_id));
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
    Logger::DebugLog(LOC, "SharedOT_Preprocess_Test - Passed");
}

void SharedOT_Online_Test(const osuCrypto::CLP &cmd) {
    Logger::DebugLog(LOC, "SharedOT_Additive_Online_Test...");

    const auto cases = BuildTestCases();

    for (const auto &tc : cases) {
        SharedOtParameters params(tc.cfg, tc.share);
        params.PrintParametersDebug();

        uint64_t d = params.GetDbIndexBits();
        uint64_t k = params.GetShareBitsize();

        FileIo file_io;

        uint64_t              result{0};
        std::vector<uint64_t> database;
        uint64_t              index;
        file_io.ReadBinary(MakePath("db", d, k), database);
        file_io.ReadBinary(MakePath("idx", d, k), index);
        Logger::DebugLog(LOC, "Loaded database from file: " + MakePath("db", d, k));
        Logger::DebugLog(LOC, "Loaded index from file: " + MakePath("idx", d, k));

        // Define the task for each party
        auto MakeTask = [&](int party_id) {
            return [=, &result](osuCrypto::Channel &chl_next, osuCrypto::Channel &chl_prev) {
                ProtocolContext3P    ctx(tc.share);
                ReplicatedSharing3P &rss = ctx.Rss();
                SharedOt             shared_ot(params, ctx);
                Channels             chls(party_id, chl_prev, chl_next);

                // Load data
                Rep3ShareVec64 database_sh;
                Rep3Share64    index_sh;
                Rep3ShareIo::LoadShare(MakePath("db", d, k) + "_" + ToString(party_id), database_sh);
                Rep3ShareIo::LoadShare(MakePath("idx", d, k) + "_" + ToString(party_id), index_sh);
                Logger::DebugLog(LOC, "Loaded shared database from file: " + MakePath("db", d, k) + "_" + ToString(party_id));
                Logger::DebugLog(LOC, "Loaded shared index from file: " + MakePath("idx", d, k) + "_" + ToString(party_id));

                // Load preprocess data
                ringoa::sharing::Rep3PreprocessData loaded_rep3_data;
                ringoa::sharing::LoadRep3PreprocessDataFromFile(MakePath("rep3_prf_keys", d, k) + "_" + ToString(party_id), loaded_rep3_data);
                Logger::DebugLog(LOC, "Loaded Rep3 PRF preprocess data from file: " + MakePath("rep3_prf_keys", d, k) + "_" + ToString(party_id));
                SharedOtPreprocessData preproc_data(SharedOtKeys(party_id, params, 3));
                ProtocolIo::LoadFromFile(MakePath("sot_preproc", d, k) + "_" + ToString(party_id), preproc_data, params);
                Logger::DebugLog(LOC, "Loaded SharedOt preprocess data from file: " + MakePath("sot_preproc", d, k) + "_" + ToString(party_id));

                ctx.Rss().SetPrfKeys(loaded_rep3_data.prf_key_self, loaded_rep3_data.prf_key_next);
                SharedOtKeys &key = preproc_data.keys;

                std::vector<uint64_t> uv_prev(1U << d), uv_next(1U << d);

                // Evaluate
                ctx.StartCommStats(chls);
                Rep3Share64 result_sh;
                shared_ot.ObliviousAccess(chls, key.GetView(0), uv_prev, uv_next, Rep3ShareView64(database_sh), index_sh, result_sh);

                Rep3ShareVec64 index_vec_sh(2), result_vec_sh(2);
                index_vec_sh.Set(0, index_sh);
                index_vec_sh.Set(1, index_sh);
                shared_ot.ObliviousAccessPair(chls, key.GetView(1), key.GetView(2), uv_prev, uv_next, Rep3ShareView64(database_sh), index_vec_sh, result_vec_sh);
                Logger::DebugLog(LOC, "[P" + ToString(party_id) + "] sent: " + ToString(ctx.StopCommStats(chls)) + " bytes");

                // Open the result
                uint64_t              local_res = 0;
                std::vector<uint64_t> local_res_vec(2);

                rss.Open(chls, result_sh, local_res);
                rss.Open(chls, result_vec_sh, local_res_vec);
                Logger::DebugLog(LOC, "result_vec_sh: " + ToString(local_res_vec));
                result = local_res;
            };
        };

        // Create tasks for each party
        auto task_p0 = MakeTask(0);
        auto task_p1 = MakeTask(1);
        auto task_p2 = MakeTask(2);

        ThreePartyNetworkManager net_mgr;
        // Configure network based on party ID and wait for completion
        int party_id = cmd.isSet("party") ? cmd.get<int>("party") : -1;
        net_mgr.AutoConfigure(party_id, task_p0, task_p1, task_p2);
        net_mgr.WaitForCompletion();

        Logger::DebugLog(LOC, "Result: " + ToString(result));

        if (result != database[index])
            throw osuCrypto::UnitTestFail("SharedOT_Online_Test failed: result = " + ToString(result) +
                                          ", expected = " + ToString(database[index]));
    }
    Logger::DebugLog(LOC, "SharedOT_Online_Test - Passed");
}

}    // namespace test_ringoa
