#include "obliv_select_test.h"

#include <cryptoTools/Common/TestCollection.h>

#include "RingOA/protocol/obliv_select.h"
#include "RingOA/protocol/protocol_io.h"
#include "RingOA/sharing/rep3_preprocess.h"
#include "RingOA/sharing/rep3_share_io.h"
#include "RingOA/sharing/rep3_sharing_binary.h"
#include "RingOA/sharing/rep3_sharing_ring.h"
#include "RingOA/sharing/sharing_2p_binary.h"
#include "RingOA/sharing/sharing_2p_ring.h"
#include "RingOA/utils/logger.h"
#include "RingOA/utils/network.h"
#include "RingOA/utils/rng.h"
#include "RingOA/utils/to_string.h"
#include "RingOA/utils/utils.h"

namespace {

const std::string kCurrentPath = ringoa::GetCurrentDirectory();
const std::string kTestOSPath  = kCurrentPath + "/data/test/protocol/obliv_select/";

struct TestCase {
    ringoa::sharing::ShareConfig     share;
    ringoa::proto::OblivSelectConfig cfg;

    TestCase(ringoa::sharing::ShareConfig     s,
             ringoa::proto::OblivSelectConfig e)
        : share(std::move(s)), cfg(std::move(e)) {
    }
};

std::vector<TestCase> BuildTestCases() {
    using ringoa::proto::OblivSelectConfig;
    using ringoa::sharing::ShareConfig;

    std::vector<TestCase> cases;

    // 1) small arithmetic ring (arith_bits=10), db size = 10
    {
        OblivSelectConfig cfg{10};
        cases.emplace_back(ShareConfig::Custom(10), cfg);
    }

    // 2) arithmetic ring 32, db size = 10
    {
        OblivSelectConfig cfg{10};
        cases.emplace_back(ShareConfig::Arith32(), cfg);
    }

    // 3) small arithmetic ring (arith_bits=10), db size = 10, single bit mask output
    {
        OblivSelectConfig cfg{10};
        cfg.output_mode = ringoa::fss::OutputType::kSingleBitMask;
        cases.emplace_back(ShareConfig::Custom(10), cfg);
    }

    // 4) arithmetic ring 32, db size = 10, single bit mask output
    {
        OblivSelectConfig cfg{10};
        cfg.output_mode = ringoa::fss::OutputType::kSingleBitMask;
        cases.emplace_back(ShareConfig::Arith32(), cfg);
    }

    return cases;
}

inline std::string MakePath(const std::string &name, uint64_t d, uint64_t k) {
    return kTestOSPath + name + "_d" + ringoa::ToString(d) + "_k" + ringoa::ToString(k);
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
using ringoa::proto::OblivSelect;
using ringoa::proto::OblivSelectKeys;
using ringoa::proto::OblivSelectParameters;
using ringoa::proto::OblivSelectPreprocessData;
using ringoa::proto::ProtocolContext3PBinary;
using ringoa::proto::ProtocolIo;
using ringoa::sharing::BinaryReplicatedSharing3P;
using ringoa::sharing::BinarySharing2P;
using ringoa::sharing::Rep3Share64;
using ringoa::sharing::Rep3ShareBlock;
using ringoa::sharing::Rep3ShareIo;
using ringoa::sharing::Rep3ShareVec64;
using ringoa::sharing::Rep3ShareVecBlock;
using ringoa::sharing::Rep3ShareView64;
using ringoa::sharing::Rep3ShareViewBlock;

void OblivSelect_DataGen_Test() {
    Logger::DebugLog(LOC, "OblivSelect_DataGen_Test...");

    const auto cases = BuildTestCases();

    for (const auto &tc : cases) {
        OblivSelectParameters params(tc.cfg, tc.share);
        params.PrintParametersDebug();

        uint64_t d = params.GetDbIndexBits();
        uint64_t k = params.GetShareBitsize();

        ProtocolContext3PBinary    ctx(tc.share);
        BinaryReplicatedSharing3P &brss = ctx.Brss();

        FileIo file_io;

        if (params.GetParameters().GetOutputType() == ringoa::fss::OutputType::kSingleBitMask) {
            // Generate the database and index
            std::vector<ringoa::block> database(1U << d);
            for (size_t i = 0; i < database.size(); ++i) {
                database[i] = ringoa::MakeBlock(0, i);
            }
            Logger::DebugLog(LOC, "Database: " + Format(database));

            std::array<Rep3ShareVecBlock, 3> database_sh = brss.ShareLocal(database);
            for (size_t p = 0; p < ringoa::sharing::kThreeParties; ++p) {
                Logger::DebugLog(LOC, "Party " + ToString(p) + " shares: " + database_sh[p].ToString());
            }

            // Save data
            file_io.WriteBinary(MakePath("dbSBM", d, k), database);
            Logger::DebugLog(LOC, "Saved database to file: " + MakePath("dbSBM", d, k));

            for (size_t p = 0; p < ringoa::sharing::kThreeParties; ++p) {
                Rep3ShareIo::SaveShare(MakePath("dbSBM", d, k) + "_" + ToString(p), database_sh[p]);
                Logger::DebugLog(LOC, "Saved shared database to file: " + MakePath("dbSBM", d, k) + "_" + ToString(p));
            }
        } else {
            // Generate the database and index
            std::vector<uint64_t> database(1U << d);
            for (size_t i = 0; i < database.size(); ++i) {
                database[i] = i;
            }
            Logger::DebugLog(LOC, "Database: " + ToString(database));

            std::array<Rep3ShareVec64, 3> database_sh = brss.ShareLocal(database);
            for (size_t p = 0; p < ringoa::sharing::kThreeParties; ++p) {
                Logger::DebugLog(LOC, "Party " + ToString(p) + " db: " + database_sh[p].ToString());
            }

            // Save data
            file_io.WriteBinary(MakePath("dbSA", d, k), database);
            Logger::DebugLog(LOC, "Saved database to file: " + MakePath("dbSA", d, k));

            for (size_t p = 0; p < ringoa::sharing::kThreeParties; ++p) {
                Rep3ShareIo::SaveShare(MakePath("dbSA", d, k) + "_" + ToString(p), database_sh[p]);
                Logger::DebugLog(LOC, "Saved shared database to file: " + MakePath("dbSA", d, k) + "_" + ToString(p));
            }
        }

        // Generate a random index
        uint64_t index = Mod2N(GlobalRng::Rand<uint64_t>(), d);
        Logger::DebugLog(LOC, "Index: " + ToString(index));
        std::array<Rep3Share64, 3> index_sh = brss.ShareLocal(index);
        for (size_t p = 0; p < ringoa::sharing::kThreeParties; ++p) {
            Logger::DebugLog(LOC, "Party " + ToString(p) + " index_sh: " + index_sh[p].ToString());
        }

        file_io.WriteBinary(MakePath("idx", d, k), index);
        Logger::DebugLog(LOC, "Saved index to file: " + MakePath("idx", d, k));
        for (size_t p = 0; p < ringoa::sharing::kThreeParties; ++p) {
            Rep3ShareIo::SaveShare(MakePath("idx", d, k) + "_" + ToString(p), index_sh[p]);
            Logger::DebugLog(LOC, "Saved shared index to file: " + MakePath("idx", d, k) + "_" + ToString(p));
        }
    }
    Logger::DebugLog(LOC, "OblivSelect_DataGen_Test - Passed");
}

void OblivSelect_Preprocess_Test(const osuCrypto::CLP &cmd) {
    Logger::DebugLog(LOC, "OblivSelect_Preprocess_Test...");

    const auto cases = BuildTestCases();

    for (const auto &tc : cases) {
        OblivSelectParameters params(tc.cfg, tc.share);
        params.PrintParametersDebug();

        uint64_t d = params.GetDbIndexBits();
        uint64_t k = params.GetShareBitsize();

        // Define the task for each party
        auto MakeTask = [&](int party_id) {
            return [=](osuCrypto::Channel &chl_next, osuCrypto::Channel &chl_prev) {
                ProtocolContext3PBinary ctx(tc.share);
                OblivSelect             os(params, ctx);
                Channels                chls(party_id, chl_prev, chl_next);

                // Generate preprocess data
                size_t count = 3;
                ctx.StartCommStats(chls);
                auto rep3_data    = ringoa::sharing::PreprocessRep3PrfKeys(chls);
                auto preproc_data = os.Preprocess(chls, count);
                Logger::DebugLog(LOC, "[P" + ToString(party_id) + "] sent: " + ToString(ctx.StopCommStats(chls)) + " bytes");

                const bool is_sbm =
                    (params.GetParameters().GetOutputType() == ringoa::fss::OutputType::kSingleBitMask);

                const std::string rep3_path =
                    MakePath("rep3_prf_keys", d, k) + "_" + ToString(party_id);

                const std::string preproc_tag = is_sbm ? "obliv_select_preprocSBM" : "obliv_select_preprocSA";
                const std::string preproc_path =
                    MakePath(preproc_tag, d, k) + "_" + ToString(party_id);

                // Save preprocess data
                ringoa::sharing::SaveRep3PreprocessDataToFile(rep3_path, rep3_data);
                Logger::DebugLog(LOC, "Saved Rep3 PRF preprocess data to file: " + rep3_path);

                ProtocolIo::SaveToFile(preproc_path, preproc_data);
                Logger::DebugLog(LOC, "Saved OblivSelect preprocess data to file: " + preproc_path);

                // Load preprocess data (verify)
                ringoa::sharing::Rep3PreprocessData loaded_rep3_data;
                ringoa::sharing::LoadRep3PreprocessDataFromFile(rep3_path, loaded_rep3_data);
                Logger::DebugLog(LOC, "Loaded Rep3 PRF preprocess data from file: " + rep3_path);

                OblivSelectPreprocessData loaded_data;
                ProtocolIo::LoadFromFile(preproc_path, loaded_data, params);
                Logger::DebugLog(LOC, "Loaded OblivSelect preprocess data from file: " + preproc_path);
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
    Logger::DebugLog(LOC, "OblivSelect_Preprocess_Test - Passed");
}

void OblivSelect_SingleBitMask_Online_Test(const osuCrypto::CLP &cmd) {
    Logger::DebugLog(LOC, "OblivSelect_SingleBitMask_Online_Test...");

    const auto cases = BuildTestCases();

    for (const auto &tc : cases) {
        if (tc.cfg.output_mode != ringoa::fss::OutputType::kSingleBitMask) {
            continue;
        }
        OblivSelectParameters params(tc.cfg, tc.share);
        params.PrintParametersDebug();

        uint64_t d = params.GetDbIndexBits();
        uint64_t k = params.GetShareBitsize();

        FileIo file_io;

        // Variable for the opened result (all parties will write into this)
        ringoa::block result = ringoa::MakeBlock(0, 0);

        std::vector<ringoa::block> database;
        uint64_t                   index;
        file_io.ReadBinary(MakePath("dbSBM", d, k), database);
        Logger::DebugLog(LOC, "Loaded database from file: " + MakePath("dbSBM", d, k));
        file_io.ReadBinary(MakePath("idx", d, k), index);
        Logger::DebugLog(LOC, "Loaded index from file: " + MakePath("idx", d, k));

        auto MakeTask = [&](int party_id) {
            return [=, &result](osuCrypto::Channel &chl_next, osuCrypto::Channel &chl_prev) {
                ProtocolContext3PBinary    ctx(tc.share);
                BinaryReplicatedSharing3P &brss = ctx.Brss();
                OblivSelect                os(params, ctx);
                Channels                   chls(party_id, chl_prev, chl_next);

                // Load database and index shares
                Rep3ShareVecBlock database_sh;
                Rep3Share64       index_sh;
                Rep3ShareIo::LoadShare(MakePath("dbSBM", d, k) + "_" + ToString(party_id), database_sh);
                Rep3ShareIo::LoadShare(MakePath("idx", d, k) + "_" + ToString(party_id), index_sh);
                Logger::DebugLog(LOC, "Loaded shared database from file: " + MakePath("dbSBM", d, k) + "_" + ToString(party_id));
                Logger::DebugLog(LOC, "Loaded shared index from file: " + MakePath("idx", d, k) + "_" + ToString(party_id));

                // Load preprocess data
                ringoa::sharing::Rep3PreprocessData loaded_rep3_data;
                ringoa::sharing::LoadRep3PreprocessDataFromFile(MakePath("rep3_prf_keys", d, k) + "_" + ToString(party_id), loaded_rep3_data);
                Logger::DebugLog(LOC, "Loaded Rep3 PRF preprocess data from file: " + MakePath("rep3_prf_keys", d, k) + "_" + ToString(party_id));
                OblivSelectPreprocessData preproc_data;
                ProtocolIo::LoadFromFile(MakePath("obliv_select_preprocSBM", d, k) + "_" + ToString(party_id), preproc_data, params);
                Logger::DebugLog(LOC, "Loaded OblivSelect preprocess data from file: " + MakePath("obliv_select_preprocSBM", d, k) + "_" + ToString(party_id));

                ctx.Brss().SetPrfKeys(loaded_rep3_data.prf_key_self, loaded_rep3_data.prf_key_next);
                OblivSelectKeys &key = preproc_data.keys;

                // Evaluate
                ctx.StartCommStats(chls);
                Rep3ShareBlock result_sh;
                os.ObliviousAccess(chls, key.GetView(0), Rep3ShareViewBlock(database_sh), index_sh, result_sh);
                Logger::DebugLog(LOC, "[P" + ToString(party_id) + "] sent: " + ToString(ctx.StopCommStats(chls)) + " bytes");

                // Open the result
                brss.Open(chls, result_sh, result);
            };
        };

        // Create tasks for parties 0, 1, and 2
        auto task_p0 = MakeTask(0);
        auto task_p1 = MakeTask(1);
        auto task_p2 = MakeTask(2);

        ThreePartyNetworkManager net_mgr;
        // Configure network based on party ID and wait for completion
        int party_id = cmd.isSet("party") ? cmd.get<int>("party") : -1;
        net_mgr.AutoConfigure(party_id, task_p0, task_p1, task_p2);
        net_mgr.WaitForCompletion();

        Logger::DebugLog(LOC, "Result: " + Format(result));

        if (result != database[index]) {
            throw osuCrypto::UnitTestFail(
                "OblivSelect_SingleBitMask_Online_Test failed: result = " + Format(result) +
                ", expected = " + Format(database[index]));
        }
    }

    Logger::DebugLog(LOC, "OblivSelect_SingleBitMask_Online_Test - Passed");
}

void OblivSelect_ShiftedAdditive_Online_Test(const osuCrypto::CLP &cmd) {
    Logger::DebugLog(LOC, "OblivSelect_ShiftedAdditive_Online_Test...");

    const auto cases = BuildTestCases();

    for (const auto &tc : cases) {
        if (tc.cfg.output_mode != ringoa::fss::OutputType::kShiftedAdditive) {
            continue;
        }
        OblivSelectParameters params(tc.cfg, tc.share);
        params.PrintParametersDebug();

        uint64_t d  = params.GetDbIndexBits();
        uint64_t k  = params.GetShareBitsize();
        uint64_t nu = params.GetParameters().GetTerminateBitsize();

        FileIo file_io;

        uint64_t              result{0};
        std::vector<uint64_t> database;
        uint64_t              index;
        file_io.ReadBinary(MakePath("dbSA", d, k), database);
        file_io.ReadBinary(MakePath("idx", d, k), index);

        // Define the task for each party
        auto MakeTask = [&](int party_id) {
            return [=, &result](osuCrypto::Channel &chl_next, osuCrypto::Channel &chl_prev) {
                ProtocolContext3PBinary    ctx(tc.share);
                BinaryReplicatedSharing3P &brss = ctx.Brss();
                OblivSelect                os(params, ctx);
                Channels                   chls(party_id, chl_prev, chl_next);

                // Load database and index shares
                Rep3ShareVec64 database_sh;
                Rep3Share64    index_sh;
                Rep3ShareIo::LoadShare(MakePath("dbSA", d, k) + "_" + ToString(party_id), database_sh);
                Rep3ShareIo::LoadShare(MakePath("idx", d, k) + "_" + ToString(party_id), index_sh);
                Logger::DebugLog(LOC, "Loaded shared database from file: " + MakePath("dbSA", d, k) + "_" + ToString(party_id));
                Logger::DebugLog(LOC, "Loaded shared index from file: " + MakePath("idx", d, k) + "_" + ToString(party_id));

                // Load preprocess data
                ringoa::sharing::Rep3PreprocessData loaded_rep3_data;
                ringoa::sharing::LoadRep3PreprocessDataFromFile(MakePath("rep3_prf_keys", d, k) + "_" + ToString(party_id), loaded_rep3_data);
                Logger::DebugLog(LOC, "Loaded Rep3 PRF preprocess data from file: " + MakePath("rep3_prf_keys", d, k) + "_" + ToString(party_id));
                OblivSelectPreprocessData preproc_data;
                ProtocolIo::LoadFromFile(MakePath("obliv_select_preprocSA", d, k) + "_" + ToString(party_id), preproc_data, params);
                Logger::DebugLog(LOC, "Loaded OblivSelect preprocess data from file: " + MakePath("obliv_select_preprocSA", d, k) + "_" + ToString(party_id));

                ctx.Brss().SetPrfKeys(loaded_rep3_data.prf_key_self, loaded_rep3_data.prf_key_next);
                OblivSelectKeys &key = preproc_data.keys;

                std::vector<ringoa::block> uv_prev(1U << nu), uv_next(1U << nu);

                // Evaluate
                ctx.StartCommStats(chls);
                Rep3Share64 result_sh;
                os.ObliviousAccess(chls, key.GetView(0), uv_prev, uv_next, Rep3ShareView64(database_sh), index_sh, result_sh);

                Rep3ShareVec64 index_vec_sh(2), result_vec_sh(2);
                index_vec_sh.Set(0, index_sh);
                index_vec_sh.Set(1, index_sh);
                os.ObliviousAccessPair(chls, key.GetView(1), key.GetView(2), uv_prev, uv_next, Rep3ShareView64(database_sh), index_vec_sh, result_vec_sh);
                Logger::DebugLog(LOC, "[P" + ToString(party_id) + "] sent: " + ToString(ctx.StopCommStats(chls)) + " bytes");

                // Open the result
                uint64_t              local_res = 0;
                std::vector<uint64_t> local_res_vec(2);

                brss.Open(chls, result_sh, local_res);
                brss.Open(chls, result_vec_sh, local_res_vec);
                Logger::DebugLog(LOC, "result_vec_sh: " + ToString(local_res_vec));
                result = local_res;

                // ctx.Timers().PrintAll(LOC + std::string(" [P") + ToString(party_id) + "]");
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
            throw osuCrypto::UnitTestFail("OblivSelect_ShiftedAdditive_Online_Test failed: result = " + ToString(result) +
                                          ", expected = " + ToString(database[index]));
    }
    Logger::DebugLog(LOC, "OblivSelect_ShiftedAdditive_Online_Test - Passed");
}

}    // namespace test_ringoa
