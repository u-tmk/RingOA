#include "obliv_rank_test.h"

#include <random>

#include <cryptoTools/Common/TestCollection.h>

#include "RingOA/obliv_fmi/obliv_rank.h"
#include "RingOA/protocol/protocol_io.h"
#include "RingOA/sharing/rep3_preprocess.h"
#include "RingOA/sharing/rep3_share_io.h"
#include "RingOA/sharing/rep3_sharing_ring.h"
#include "RingOA/sharing/sharing_2p_ring.h"
#include "RingOA/utils/logger.h"
#include "RingOA/utils/network.h"
#include "RingOA/utils/rng.h"
#include "RingOA/utils/timer.h"
#include "RingOA/utils/to_string.h"
#include "RingOA/utils/utils.h"
#include "RingOA/wm/plain_wm.h"

namespace {

const std::string kCurrentPath       = ringoa::GetCurrentDirectory();
const std::string kTestOblivRankPath = kCurrentPath + "/data/test/obliv_fmi/orank/";
const uint64_t    kFixedSeed         = 6;

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
    ringoa::fmi::OblivRankConfig cfg;

    TestCase(ringoa::sharing::ShareConfig s,
             ringoa::fmi::OblivRankConfig e)
        : share(std::move(s)), cfg(std::move(e)) {
    }
};

std::vector<TestCase> BuildTestCases() {
    using ringoa::fmi::OblivRankConfig;
    using ringoa::sharing::ShareConfig;

    std::vector<TestCase> cases;

    // 1) small arithmetic ring (arith_bits=10), db size = 10
    {
        OblivRankConfig cfg{10};
        cases.emplace_back(ShareConfig::Custom(10), cfg);
    }

    // 2) arithmetic ring 32, db size = 10
    {
        OblivRankConfig cfg{10};
        cases.emplace_back(ShareConfig::Arith32(), cfg);
    }

    return cases;
}

inline std::string MakePath(const std::string &name, uint64_t d, uint64_t k) {
    return kTestOblivRankPath + name + "_d" + ringoa::ToString(d) + "_k" + ringoa::ToString(k);
}

}    // namespace

namespace test_ringoa {

using ringoa::Channels;
using ringoa::FileIo;
using ringoa::GlobalRng;
using ringoa::Logger;
using ringoa::Mod2N;
using ringoa::ThreePartyNetworkManager;
using ringoa::ToString, ringoa::ToStringMatrix;
using ringoa::fmi::OblivRank;
using ringoa::fmi::OblivRankFscKeys;
using ringoa::fmi::OblivRankFscPreprocessData;
using ringoa::fmi::OblivRankKeys;
using ringoa::fmi::OblivRankParameters;
using ringoa::fmi::OblivRankPreprocessData;
using ringoa::proto::ProtocolContext3P;
using ringoa::proto::ProtocolIo;
using ringoa::sharing::Rep3Share64;
using ringoa::sharing::Rep3ShareIo;
using ringoa::sharing::Rep3ShareMat64;
using ringoa::sharing::Rep3ShareVec64;
using ringoa::sharing::Rep3ShareView64;
using ringoa::sharing::ReplicatedSharing3P;
using ringoa::wm::FMIndex;

void OblivRank_DataGen_Test() {
    Logger::DebugLog(LOC, "OblivRank_DataGen_Test...");

    const auto cases = BuildTestCases();

    for (const auto &tc : cases) {
        OblivRankParameters params(tc.cfg, tc.share);
        params.PrintParametersDebug();

        uint64_t d  = params.GetDbIndexBits();
        uint64_t ds = params.GetDatabaseSize();
        uint64_t k  = params.GetShareBitsize();

        ProtocolContext3P    ctx(tc.share);
        ReplicatedSharing3P &rss = ctx.Rss();
        OblivRank            orank(params, ctx);

        FileIo file_io;

        // Generate the database and index
        std::string           database = GenerateRandomString(ds - 2);
        FMIndex               fm(database);
        std::vector<uint64_t> query    = {0, 1, 0};
        uint64_t              position = Mod2N(GlobalRng::Rand<uint64_t>(), d);
        Logger::DebugLog(LOC, "Database: " + database);
        Logger::DebugLog(LOC, "Query   : " + ToString(query));
        Logger::DebugLog(LOC, "Position: " + ToString(position));

        // Generate replicated shares for the database, query, and position
        std::array<Rep3ShareMat64, 3> db_sh       = orank.GenerateDatabaseU64Share(fm);
        std::array<Rep3ShareVec64, 3> query_sh    = rss.ShareLocal(query);
        std::array<Rep3Share64, 3>    position_sh = rss.ShareLocal(position);
        for (size_t p = 0; p < ringoa::sharing::kThreeParties; ++p) {
            Logger::DebugLog(LOC, "Party " + ToString(p) + " rank share: " + db_sh[p].ToStringMatrix());
            Logger::DebugLog(LOC, "Party " + ToString(p) + " query share: " + query_sh[p].ToString());
            Logger::DebugLog(LOC, "Party " + ToString(p) + " position share: " + position_sh[p].ToString());
        }

        // Save data
        file_io.WriteBinary(MakePath("db", d, k), database);
        file_io.WriteBinary(MakePath("query", d, k), query);
        file_io.WriteBinary(MakePath("position", d, k), position);
        Logger::DebugLog(LOC, "Saved database to file: " + MakePath("db", d, k));
        Logger::DebugLog(LOC, "Saved query to file: " + MakePath("query", d, k));
        Logger::DebugLog(LOC, "Saved position to file: " + MakePath("position", d, k));

        for (size_t p = 0; p < ringoa::sharing::kThreeParties; ++p) {
            Rep3ShareIo::SaveShare(MakePath("db", d, k) + "_" + ToString(p), db_sh[p]);
            Rep3ShareIo::SaveShare(MakePath("query", d, k) + "_" + ToString(p), query_sh[p]);
            Rep3ShareIo::SaveShare(MakePath("position", d, k) + "_" + ToString(p), position_sh[p]);
            Logger::DebugLog(LOC, "Saved shared database to file: " + MakePath("db", d, k) + "_" + ToString(p));
            Logger::DebugLog(LOC, "Saved shared query to file: " + MakePath("query", d, k) + "_" + ToString(p));
            Logger::DebugLog(LOC, "Saved shared position to file: " + MakePath("position", d, k) + "_" + ToString(p));
        }
    }
    Logger::DebugLog(LOC, "OblivRank_DataGen_Test - Passed");
}

void OblivRank_Preprocess_Test(const osuCrypto::CLP &cmd) {
    Logger::DebugLog(LOC, "OblivRank_Preprocess_Test...");

    const auto cases = BuildTestCases();

    for (const auto &tc : cases) {
        OblivRankParameters params(tc.cfg, tc.share);
        params.PrintParametersDebug();

        uint64_t d = params.GetDbIndexBits();
        uint64_t k = params.GetShareBitsize();

        // Define the task for each party
        auto MakeTask = [&](int party_id) {
            return [=](osuCrypto::Channel &chl_next, osuCrypto::Channel &chl_prev) {
                ProtocolContext3P ctx(tc.share);
                OblivRank         orank(params, ctx);
                Channels          chls(party_id, chl_prev, chl_next);

                // Generate preprocess data
                ctx.StartCommStats(chls);
                size_t count        = 3;
                auto   rep3_data    = ringoa::sharing::PreprocessRep3PrfKeys(chls);
                auto   preproc_data = orank.Preprocess(chls, count);
                Logger::DebugLog(LOC, "[P" + ToString(party_id) + "] sent: " + ToString(ctx.StopCommStats(chls)) + " bytes");

                // Save preprocess data
                ringoa::sharing::SaveRep3PreprocessDataToFile(MakePath("rep3_prf_keys", d, k) + "_" + ToString(party_id), rep3_data);
                Logger::DebugLog(LOC, "Saved Rep3 PRF preprocess data to file: " + MakePath("rep3_prf_keys", d, k) + "_" + ToString(party_id));
                ProtocolIo::SaveToFile(MakePath("obliv_rank_preproc", d, k) + "_" + ToString(party_id), preproc_data);
                Logger::DebugLog(LOC, "Saved OblivRank preprocess data to file: " + MakePath("obliv_rank_preproc", d, k) + "_" + ToString(party_id));

                // Load preprocess data (verify)
                ringoa::sharing::Rep3PreprocessData loaded_rep3_data;
                ringoa::sharing::LoadRep3PreprocessDataFromFile(MakePath("rep3_prf_keys", d, k) + "_" + ToString(party_id), loaded_rep3_data);
                Logger::DebugLog(LOC, "Loaded Rep3 PRF preprocess data from file: " + MakePath("rep3_prf_keys", d, k) + "_" + ToString(party_id));
                OblivRankPreprocessData loaded_data;
                ProtocolIo::LoadFromFile(MakePath("obliv_rank_preproc", d, k) + "_" + ToString(party_id), loaded_data, params);
                Logger::DebugLog(LOC, "Loaded OblivRank preprocess data from file: " + MakePath("obliv_rank_preproc", d, k) + "_" + ToString(party_id));
                orank.ConsumePreprocessData(loaded_data);
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
    Logger::DebugLog(LOC, "OblivRank_Preprocess_Test - Passed");
}

void OblivRank_Online_Test(const osuCrypto::CLP &cmd) {
    Logger::DebugLog(LOC, "OblivRank_Online_Test...");

    const auto cases = BuildTestCases();

    for (const auto &tc : cases) {
        OblivRankParameters params(tc.cfg, tc.share);
        params.PrintParametersDebug();

        uint64_t d  = params.GetDbIndexBits();
        uint64_t k  = params.GetShareBitsize();
        uint64_t nu = params.GetParameters().GetParameters().GetTerminateBitsize();

        FileIo file_io;

        uint64_t              result{0};
        std::string           database;
        std::vector<uint64_t> query;
        uint64_t              position;
        file_io.ReadBinary(MakePath("db", d, k), database);
        file_io.ReadBinary(MakePath("query", d, k), query);
        file_io.ReadBinary(MakePath("position", d, k), position);
        Logger::DebugLog(LOC, "Loaded database from file: " + MakePath("db", d, k));
        Logger::DebugLog(LOC, "Loaded query from file: " + MakePath("query", d, k));
        Logger::DebugLog(LOC, "Loaded position from file: " + MakePath("position", d, k));

        auto MakeTask = [&](int party_id) {
            return [=, &result](osuCrypto::Channel &chl_next, osuCrypto::Channel &chl_prev) {
                ProtocolContext3P    ctx(tc.share);
                ReplicatedSharing3P &rss = ctx.Rss();
                OblivRank            orank(params, ctx);
                Channels             chls(party_id, chl_prev, chl_next);

                // Load this party's shares of the database, query, and position
                Rep3ShareMat64 db_sh;
                Rep3ShareVec64 query_sh;
                Rep3Share64    position_sh;
                Rep3ShareIo::LoadShare(MakePath("db", d, k) + "_" + ToString(party_id), db_sh);
                Rep3ShareIo::LoadShare(MakePath("query", d, k) + "_" + ToString(party_id), query_sh);
                Rep3ShareIo::LoadShare(MakePath("position", d, k) + "_" + ToString(party_id), position_sh);
                Logger::DebugLog(LOC, "Loaded shared database from file: " + MakePath("db", d, k) + "_" + ToString(party_id));
                Logger::DebugLog(LOC, "Loaded shared query from file: " + MakePath("query", d, k) + "_" + ToString(party_id));
                Logger::DebugLog(LOC, "Loaded shared position from file: " + MakePath("position", d, k) + "_" + ToString(party_id));

                // Load preprocess data
                ringoa::sharing::Rep3PreprocessData loaded_rep3_data;
                ringoa::sharing::LoadRep3PreprocessDataFromFile(MakePath("rep3_prf_keys", d, k) + "_" + ToString(party_id), loaded_rep3_data);
                Logger::DebugLog(LOC, "Loaded Rep3 PRF preprocess data from file: " + MakePath("rep3_prf_keys", d, k) + "_" + ToString(party_id));
                OblivRankPreprocessData preproc_data;
                ProtocolIo::LoadFromFile(MakePath("obliv_rank_preproc", d, k) + "_" + ToString(party_id), preproc_data, params);
                Logger::DebugLog(LOC, "Loaded OblivRank preprocess data from file: " + MakePath("obliv_rank_preproc", d, k) + "_" + ToString(party_id));

                // Set PRF keys and OblivRank keys
                ctx.Rss().SetPrfKeys(loaded_rep3_data.prf_key_self, loaded_rep3_data.prf_key_next);
                OblivRankKeys keys = std::move(preproc_data).ExtractKeys();

                // Consume preprocess data
                orank.ConsumePreprocessData(preproc_data);

                // Evaluate the rank operation
                ctx.StartCommStats(chls);
                Rep3Share64    result_sh;
                Rep3ShareVec64 position_vec_sh(2), result_vec_sh(2);
                position_vec_sh.Set(0, position_sh);
                position_vec_sh.Set(1, position_sh);
                std::vector<ringoa::block> uv_prev(1U << nu), uv_next(1U << nu);

                orank.EvaluateRankCF(chls, keys.GetView(0), uv_prev, uv_next, db_sh, Rep3ShareView64(query_sh), position_sh, result_sh);
                orank.EvaluateRankCFPair(chls, keys.GetView(1), keys.GetView(2), uv_prev, uv_next, db_sh, Rep3ShareView64(query_sh), position_vec_sh, result_vec_sh);
                Logger::DebugLog(LOC, "[P" + ToString(party_id) + "] sent: " + ToString(ctx.StopCommStats(chls)) + " bytes");

                // Open the resulting share to recover the final value
                uint64_t              local_res = 0;
                std::vector<uint64_t> local_res_vec(2);

                rss.Open(chls, result_sh, local_res);
                rss.Open(chls, result_vec_sh, local_res_vec);
                Logger::DebugLog(LOC, "result_vec_sh: " + ToString(local_res_vec));
                result = local_res;
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
        FMIndex  fmi(database);
        uint64_t expected_result = fmi.GetWaveletMatrix().RankCF(2, position);
        if (result != expected_result) {
            throw osuCrypto::UnitTestFail(
                "OblivRank_Online_Test failed: result = " + ToString(result) +
                ", expected = " + ToString(expected_result));
        }
    }

    Logger::DebugLog(LOC, "OblivRank_Online_Test - Passed");
}

void OblivRank_Fsc_DataGen_Test() {
    Logger::DebugLog(LOC, "OblivRank_Fsc_DataGen_Test...");

    const auto cases = BuildTestCases();

    for (const auto &tc : cases) {
        OblivRankParameters params(tc.cfg, tc.share);
        params.PrintParametersDebug();

        uint64_t d  = params.GetDbIndexBits();
        uint64_t ds = params.GetDatabaseSize();
        uint64_t k  = params.GetShareBitsize();

        ProtocolContext3P    ctx(tc.share);
        ReplicatedSharing3P &rss = ctx.Rss();
        OblivRank            orank(params, ctx);

        FileIo file_io;

        // Generate the database and index
        std::string           database = GenerateRandomString(ds - 2);
        FMIndex               fm(database);
        std::vector<uint64_t> query    = {0, 1, 0};
        uint64_t              position = Mod2N(GlobalRng::Rand<uint64_t>(), d);
        Logger::DebugLog(LOC, "Database: " + database);
        Logger::DebugLog(LOC, "Query   : " + ToString(query));
        Logger::DebugLog(LOC, "Position: " + ToString(position));

        // Generate replicated shares for the database, query, and position
        std::array<bool, 3>           v_sign;
        std::array<Rep3ShareMat64, 3> db_sh;
        std::array<Rep3ShareVec64, 3> aux_sh;
        orank.GenerateDatabaseU64Share(fm, db_sh, aux_sh, v_sign);
        std::array<Rep3ShareVec64, 3> query_sh    = rss.ShareLocal(query);
        std::array<Rep3Share64, 3>    position_sh = rss.ShareLocal(position);
        for (size_t p = 0; p < ringoa::sharing::kThreeParties; ++p) {
            Logger::DebugLog(LOC, "Party " + ToString(p) + " rank share: " + db_sh[p].ToStringMatrix());
            Logger::DebugLog(LOC, "Party " + ToString(p) + " auxiliary share: " + aux_sh[p].ToString());
            Logger::DebugLog(LOC, "Party " + ToString(p) + " query share: " + query_sh[p].ToString());
            Logger::DebugLog(LOC, "Party " + ToString(p) + " position share: " + position_sh[p].ToString());
        }

        // Save data
        file_io.WriteBinary(MakePath("db_fsc", d, k), database);
        file_io.WriteBinary(MakePath("query_fsc", d, k), query);
        file_io.WriteBinary(MakePath("position_fsc", d, k), position);
        Logger::DebugLog(LOC, "Saved database to file: " + MakePath("db_fsc", d, k));
        Logger::DebugLog(LOC, "Saved query to file: " + MakePath("query_fsc", d, k));
        Logger::DebugLog(LOC, "Saved position to file: " + MakePath("position_fsc", d, k));

        for (size_t p = 0; p < ringoa::sharing::kThreeParties; ++p) {
            file_io.WriteBinary(MakePath("v_sign", d, k) + "_" + ToString(p), v_sign[p]);
            Rep3ShareIo::SaveShare(MakePath("db_fsc", d, k) + "_" + ToString(p), db_sh[p]);
            Rep3ShareIo::SaveShare(MakePath("aux_fsc", d, k) + "_" + ToString(p), aux_sh[p]);
            Rep3ShareIo::SaveShare(MakePath("query_fsc", d, k) + "_" + ToString(p), query_sh[p]);
            Rep3ShareIo::SaveShare(MakePath("position_fsc", d, k) + "_" + ToString(p), position_sh[p]);
            Logger::DebugLog(LOC, "Saved shared database to file: " + MakePath("db_fsc", d, k) + "_" + ToString(p));
            Logger::DebugLog(LOC, "Saved shared auxiliary to file: " + MakePath("aux_fsc", d, k) + "_" + ToString(p));
            Logger::DebugLog(LOC, "Saved shared query to file: " + MakePath("query_fsc", d, k) + "_" + ToString(p));
            Logger::DebugLog(LOC, "Saved shared position to file: " + MakePath("position_fsc", d, k) + "_" + ToString(p));
        }
    }
    Logger::DebugLog(LOC, "OblivRank_Fsc_DataGen_Test - Passed");
}

void OblivRank_Fsc_Preprocess_Test(const osuCrypto::CLP &cmd) {
    Logger::DebugLog(LOC, "OblivRank_Fsc_Preprocess_Test...");

    const auto cases = BuildTestCases();

    for (const auto &tc : cases) {
        OblivRankParameters params(tc.cfg, tc.share);
        params.PrintParametersDebug();

        uint64_t d = params.GetDbIndexBits();
        uint64_t k = params.GetShareBitsize();

        // Define the task for each party
        auto MakeTask = [&](int party_id) {
            return [=](osuCrypto::Channel &chl_next, osuCrypto::Channel &chl_prev) {
                ProtocolContext3P ctx(tc.share);
                OblivRank         orank(params, ctx);
                Channels          chls(party_id, chl_prev, chl_next);

                FileIo file_io;
                // Load v_sign
                bool v_sign;
                file_io.ReadBinary(MakePath("v_sign", d, k) + "_" + ToString(party_id), v_sign);

                // Generate preprocess data
                ctx.StartCommStats(chls);
                size_t count        = 3;
                auto   rep3_data    = ringoa::sharing::PreprocessRep3PrfKeys(chls);
                auto   preproc_data = orank.PreprocessFsc(chls, count, v_sign);
                Logger::DebugLog(LOC, "[P" + ToString(party_id) + "] sent: " + ToString(ctx.StopCommStats(chls)) + " bytes");

                // Save preprocess data
                ringoa::sharing::SaveRep3PreprocessDataToFile(MakePath("rep3_prf_keys", d, k) + "_" + ToString(party_id), rep3_data);
                Logger::DebugLog(LOC, "Saved Rep3 PRF preprocess data to file: " + MakePath("rep3_prf_keys", d, k) + "_" + ToString(party_id));
                ProtocolIo::SaveToFile(MakePath("obliv_rank_fsc_preproc", d, k) + "_" + ToString(party_id), preproc_data);
                Logger::DebugLog(LOC, "Saved OblivRank FSC preprocess data to file: " + MakePath("obliv_rank_fsc_preproc", d, k) + "_" + ToString(party_id));

                // Load preprocess data (verify)
                ringoa::sharing::Rep3PreprocessData loaded_rep3_data;
                ringoa::sharing::LoadRep3PreprocessDataFromFile(MakePath("rep3_prf_keys", d, k) + "_" + ToString(party_id), loaded_rep3_data);
                Logger::DebugLog(LOC, "Loaded Rep3 PRF preprocess data from file: " + MakePath("rep3_prf_keys", d, k) + "_" + ToString(party_id));
                OblivRankFscPreprocessData loaded_data;
                ProtocolIo::LoadFromFile(MakePath("obliv_rank_fsc_preproc", d, k) + "_" + ToString(party_id), loaded_data, params);
                Logger::DebugLog(LOC, "Loaded OblivRank FSC preprocess data from file: " + MakePath("obliv_rank_fsc_preproc", d, k) + "_" + ToString(party_id));
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
    Logger::DebugLog(LOC, "OblivRank_Fsc_Preprocess_Test - Passed");
}

void OblivRank_Fsc_Online_Test(const osuCrypto::CLP &cmd) {
    Logger::DebugLog(LOC, "OblivRank_Fsc_Online_Test...");

    const auto cases = BuildTestCases();

    for (const auto &tc : cases) {
        OblivRankParameters params(tc.cfg, tc.share);
        params.PrintParametersDebug();

        uint64_t d  = params.GetDbIndexBits();
        uint64_t k  = params.GetShareBitsize();
        uint64_t nu = params.GetParameters().GetParameters().GetTerminateBitsize();

        FileIo file_io;

        uint64_t result{0};

        std::string           database;
        std::vector<uint64_t> query;
        uint64_t              position;
        file_io.ReadBinary(MakePath("db_fsc", d, k), database);
        file_io.ReadBinary(MakePath("query_fsc", d, k), query);
        file_io.ReadBinary(MakePath("position_fsc", d, k), position);
        Logger::DebugLog(LOC, "Loaded database from file: " + MakePath("db_fsc", d, k));
        Logger::DebugLog(LOC, "Loaded query from file: " + MakePath("query_fsc", d, k));
        Logger::DebugLog(LOC, "Loaded position from file: " + MakePath("position_fsc", d, k));

        auto MakeTask = [&](int party_id) {
            return [=, &result](osuCrypto::Channel &chl_next, osuCrypto::Channel &chl_prev) {
                ProtocolContext3P    ctx(tc.share);
                ReplicatedSharing3P &rss = ctx.Rss();
                OblivRank            orank(params, ctx);
                Channels             chls(party_id, chl_prev, chl_next);

                // Load this party's shares of the database, query, and position
                Rep3ShareMat64 db_sh;
                Rep3ShareVec64 aux_sh;
                Rep3ShareVec64 query_sh;
                Rep3Share64    position_sh;
                Rep3ShareIo::LoadShare(MakePath("db_fsc", d, k) + "_" + ToString(party_id), db_sh);
                Rep3ShareIo::LoadShare(MakePath("aux_fsc", d, k) + "_" + ToString(party_id), aux_sh);
                Rep3ShareIo::LoadShare(MakePath("query_fsc", d, k) + "_" + ToString(party_id), query_sh);
                Rep3ShareIo::LoadShare(MakePath("position_fsc", d, k) + "_" + ToString(party_id), position_sh);
                Logger::DebugLog(LOC, "Loaded shared database from file: " + MakePath("db_fsc", d, k) + "_" + ToString(party_id));
                Logger::DebugLog(LOC, "Loaded shared auxiliary from file: " + MakePath("aux_fsc", d, k) + "_" + ToString(party_id));
                Logger::DebugLog(LOC, "Loaded shared query from file: " + MakePath("query_fsc", d, k) + "_" + ToString(party_id));
                Logger::DebugLog(LOC, "Loaded shared position from file: " + MakePath("position_fsc", d, k) + "_" + ToString(party_id));

                // Load preprocess data
                ringoa::sharing::Rep3PreprocessData loaded_rep3_data;
                ringoa::sharing::LoadRep3PreprocessDataFromFile(MakePath("rep3_prf_keys", d, k) + "_" + ToString(party_id), loaded_rep3_data);
                Logger::DebugLog(LOC, "Loaded Rep3 PRF preprocess data from file: " + MakePath("rep3_prf_keys", d, k) + "_" + ToString(party_id));
                OblivRankFscPreprocessData preproc_data;
                ProtocolIo::LoadFromFile(MakePath("obliv_rank_fsc_preproc", d, k) + "_" + ToString(party_id), preproc_data, params);
                Logger::DebugLog(LOC, "Loaded OblivRank FSC preprocess data from file: " + MakePath("obliv_rank_fsc_preproc", d, k) + "_" + ToString(party_id));

                // Set PRF keys and OblivRank keys
                ctx.Rss().SetPrfKeys(loaded_rep3_data.prf_key_self, loaded_rep3_data.prf_key_next);
                OblivRankFscKeys keys = std::move(preproc_data).ExtractKeys();

                // Evaluate the rank operation
                ctx.StartCommStats(chls);
                Rep3Share64    result_sh;
                Rep3ShareVec64 position_vec_sh(2), result_vec_sh(2);
                position_vec_sh.Set(0, position_sh);
                position_vec_sh.Set(1, position_sh);
                std::vector<ringoa::block> uv_prev(1U << nu), uv_next(1U << nu);

                orank.EvaluateRankCFFsc(chls, keys.GetView(0), uv_prev, uv_next, db_sh, Rep3ShareView64(aux_sh), Rep3ShareView64(query_sh), position_sh, result_sh);
                orank.EvaluateRankCFFscPair(chls, keys.GetView(1), keys.GetView(2), uv_prev, uv_next, db_sh, Rep3ShareView64(aux_sh), Rep3ShareView64(query_sh), position_vec_sh, result_vec_sh);
                Logger::DebugLog(LOC, "[P" + ToString(party_id) + "] sent: " + ToString(ctx.StopCommStats(chls)) + " bytes");

                // Open the resulting share to recover the final value
                uint64_t              local_res = 0;
                std::vector<uint64_t> local_res_vec(2);

                rss.Open(chls, result_sh, local_res);
                rss.Open(chls, result_vec_sh, local_res_vec);
                Logger::DebugLog(LOC, "result_vec_sh: " + ToString(local_res_vec));
                result = local_res;
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
        FMIndex  fmi(database);
        uint64_t expected_result = fmi.GetWaveletMatrix().RankCF(2, position);
        if (result != expected_result) {
            throw osuCrypto::UnitTestFail(
                "OblivRank_Fsc_Online_Test failed: result = " + ToString(result) +
                ", expected = " + ToString(expected_result));
        }
    }

    Logger::DebugLog(LOC, "OblivRank_Fsc_Online_Test - Passed");
}

}    // namespace test_ringoa
