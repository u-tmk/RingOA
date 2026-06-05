#include "obliv_range_bench.h"

#include <cryptoTools/Common/TestCollection.h>

#include "RingOA/obliv_range/obliv_range.h"
#include "RingOA/protocol/protocol_io.h"
#include "RingOA/sharing/rep3_preprocess.h"
#include "RingOA/sharing/rep3_share_io.h"
#include "RingOA/sharing/rep3_sharing_ring.h"
#include "RingOA/sharing/sharing_2p_ring.h"
#include "RingOA/utils/logger.h"
#include "RingOA/utils/network.h"
#include "RingOA/utils/rng.h"
#include "RingOA/utils/timer.h"
#include "RingOA/utils/utils.h"
#include "RingOA/wm/plain_wm.h"
#include "bench_common.h"

namespace {

inline std::string MakePath(const std::string &name, uint64_t d, uint64_t k) {
    return bench_ringoa::kBenchOblivRangePath + name + "_d" + ringoa::ToString(d) + "_k" + ringoa::ToString(k);
}

}    // namespace

namespace bench_ringoa {

using ringoa::block;
using ringoa::Channels;
using ringoa::FileIo;
using ringoa::GlobalRng;
using ringoa::Logger;
using ringoa::Mod2N;
using ringoa::ThreePartyNetworkManager;
using ringoa::TimerManager;
using ringoa::ToString, ringoa::Format;
using ringoa::proto::ProtocolContext2P;
using ringoa::proto::ProtocolContext3P;
using ringoa::proto::ProtocolIo;
using ringoa::range::OblivRange;
using ringoa::range::OblivRangeConfig;
using ringoa::range::OblivRangeFscKeys;
using ringoa::range::OblivRangeFscPreprocessData;
using ringoa::range::OblivRangeKeys;
using ringoa::range::OblivRangeParameters;
using ringoa::range::OblivRangePreprocessData;
using ringoa::sharing::Rep3Share64, ringoa::sharing::Rep3ShareVec64;
using ringoa::sharing::Rep3ShareIo;
using ringoa::sharing::Rep3ShareMat64, ringoa::sharing::Rep3ShareView64;
using ringoa::sharing::ShareConfig;
using ringoa::wm::WaveletMatrix;

void OblivRange_VAF_Preprocess_Bench(const osuCrypto::CLP &cmd) {
    if (IsHelpRequested(cmd)) {
        PrintBenchmarkHelp("OblivRange (VAF) Benchmark");
        return;
    }
    const auto opts = SelectBenchOptions(cmd);

    Logger::InfoLog(LOC, "OblivRange VAF Preprocess Benchmark started (repeat=" + ToString(opts.repeat) + ")");

    // Define the task for each party
    auto MakeTask = [&](int p) {
        const std::string ptag = "(P" + ToString(p) + ")";
        return [=](osuCrypto::Channel &chl_next, osuCrypto::Channel &chl_prev) {
            // VAF database size is approximately 23 million entries < 2^25
            // sigma for VAF values is 7 (0-100)
            ShareConfig          share = ShareConfig::Arith32();
            ProtocolContext2P    ctx2p(share);
            ProtocolContext3P    ctx3p(share);
            OblivRangeConfig     cfg{25, 7};
            OblivRangeParameters params(cfg, share);

            const uint64_t d = params.GetDbIndexBits();
            const uint64_t k = params.GetShareBitsize();

            OblivRange orange(params, ctx2p, ctx3p);
            Channels   chls(p, chl_prev, chl_next);

            int32_t timer_prep = ctx3p.Timers().CreateTimer("OblivRange::VAF::Preprocess" + ptag);

            // Preprocess
            ringoa::sharing::Rep3PreprocessData rep3_data;
            OblivRangePreprocessData            preproc_data;

            for (uint64_t i = 0; i < opts.warmup; ++i) {
                ringoa::sharing::PreprocessRep3PrfKeys(chls);
                orange.Preprocess(chls);
            }

            for (uint64_t i = 0; i < opts.repeat; ++i) {
                ctx3p.Timers().Start(timer_prep);
                ctx3p.StartCommStats(chls);
                rep3_data    = ringoa::sharing::PreprocessRep3PrfKeys(chls);
                preproc_data = orange.Preprocess(chls);
                ctx3p.Timers().Stop(timer_prep, "d=" + ToString(d) + ",iter=" + ToString(i));
                if (i == 0) {
                    Logger::InfoLog(LOC, "(P" + ToString(p) + "),bytes,d=" + ToString(d) + ",sent=" + ToString(ctx3p.StopCommStats(chls)));
                }
            }
            // Print communication cost and timer results
            ctx3p.Timers().Print(timer_prep, "d=" + ToString(d), ringoa::TimeUnit::MICROSECONDS);

            // Save preprocess data
            ringoa::sharing::SaveRep3PreprocessDataToFile(kBenchOblivRangePath + "rep3_prf_keys_" + ToString(p), rep3_data);
            ProtocolIo::SaveToFile(MakePath("preproc_vaf", d, k) + "_" + ToString(p), preproc_data);
        };
    };

    auto task_p0 = MakeTask(0);
    auto task_p1 = MakeTask(1);
    auto task_p2 = MakeTask(2);

    ThreePartyNetworkManager net_mgr;
    net_mgr.AutoConfigure(opts.party_id, task_p0, task_p1, task_p2);
    net_mgr.WaitForCompletion();

    Logger::InfoLog(LOC, "OblivRange VAF Preprocess Benchmark completed");
    Logger::ExportLogListAndClear(
        MakeLogBasePath(opts, kLogOblivRangePath, "orange_vaf_preproc", opts.party_id),
        opts.enable_log_timestamp);
}

void OblivRange_VAF_Online_Bench(const osuCrypto::CLP &cmd) {
    if (IsHelpRequested(cmd)) {
        PrintBenchmarkHelp("OblivRange (VAF) Benchmark");
        return;
    }
    const auto opts = SelectBenchOptions(cmd);

    Logger::InfoLog(LOC, "OblivRange VAF Online Benchmark started (repeat=" + ToString(opts.repeat) +
                             ", party=" + ToString(opts.party_id) + ")");

    // Helper that returns a task lambda for a given party p
    auto MakeTask = [&](int p) {
        const std::string ptag = "(P" + ToString(p) + ")";
        return [=](osuCrypto::Channel &chl_next, osuCrypto::Channel &chl_prev) {
            ShareConfig          share = ShareConfig::Arith32();
            ProtocolContext2P    ctx2p(share);
            ProtocolContext3P    ctx3p(share);
            OblivRangeConfig     cfg{25, 7};
            OblivRangeParameters params(cfg, share);

            const uint64_t d  = params.GetDbIndexBits();
            const uint64_t k  = params.GetShareBitsize();
            const uint64_t nu = params.GetOaParameters().GetParameters().GetTerminateBitsize();

            // ----- Timers -----
            const std::string timer_setup_name = "OblivRange::VAF::OnlineSetUp" + ptag;
            const std::string timer_eval_name  = "OblivRange::VAF::Eval" + ptag;
            const int32_t     timer_setup      = ctx3p.Timers().CreateTimer(timer_setup_name);
            const int32_t     timer_eval       = ctx3p.Timers().CreateTimer(timer_eval_name);

            // ================================
            // OnlineSetUp timing
            // ================================
            ctx3p.Timers().Start(timer_setup);

            OblivRange orange(params, ctx2p, ctx3p);
            Channels   chls(p, chl_prev, chl_next);

            // Load shares (database and query = [left,right,k])
            Rep3ShareMat64 db_sh;
            Rep3ShareVec64 query_sh;

            Rep3ShareIo::LoadShare(MakeDatasetPath("vaf_db", d, k) + "_" + ToString(p), db_sh);
            Rep3ShareIo::LoadShare(MakeDatasetPath("vaf_query_brca", d, k) + "_" + ToString(p), query_sh);

            // Extract query components
            Rep3Share64 left_sh  = query_sh.At(0);
            Rep3Share64 right_sh = query_sh.At(1);
            Rep3Share64 k_sh     = query_sh.At(2);

            // Load preprocess data
            ringoa::sharing::Rep3PreprocessData loaded_rep3_data;
            OblivRangePreprocessData            preproc_data;
            ringoa::sharing::LoadRep3PreprocessDataFromFile(kBenchOblivRangePath + "rep3_prf_keys_" + ToString(p), loaded_rep3_data);
            ProtocolIo::LoadFromFile(MakePath("preproc_vaf", d, k) + "_" + ToString(p), preproc_data, params);

            ctx3p.Rss().SetPrfKeys(loaded_rep3_data.prf_key_self, loaded_rep3_data.prf_key_next);
            OblivRangeKeys key = std::move(preproc_data).ExtractKeys();

            // Consume preprocess data
            orange.ConsumePreprocessData(preproc_data);

            // Buffers sized by terminate bitsize
            std::vector<ringoa::block> uv_prev(1ULL << nu), uv_next(1ULL << nu);

            ctx3p.Timers().Stop(timer_setup, "d=" + ToString(d) + ",iter=0");
            ctx3p.Timers().Print(timer_setup, "d=" + ToString(d), ringoa::TimeUnit::MICROSECONDS);

            // ================================
            // Eval timing
            // ================================
            Rep3Share64 result_sh;
            for (uint64_t i = 0; i < opts.repeat; ++i) {
                ctx3p.Timers().Start(timer_eval);
                ctx3p.StartCommStats(chls);
                orange.EvaluateRangePair(
                    chls, key, uv_prev, uv_next,
                    db_sh, left_sh, right_sh, k_sh, result_sh);
                ctx3p.Timers().Stop(timer_eval, "d=" + ToString(d) + ",iter=" + ToString(i));
                if (i == 0) {
                    Logger::InfoLog(LOC, "(P" + ToString(p) + "),bytes,d=" + ToString(d) + ",sent=" + ToString(ctx3p.StopCommStats(chls)));
                }
                ctx3p.AssWithPrev().ResetTripleIndex();
                ctx3p.AssWithNext().ResetTripleIndex();
                ctx3p.Timers().Print(ctx3p.Timers().GetOrCreateTimer("RingOA::LocalEval"), "d=" + ToString(d) + ",iter=" + ToString(i), ringoa::TimeUnit::MICROSECONDS);
                ctx3p.Timers().ResetByName("RingOA::LocalEval");
            }
            ctx3p.Timers().Print(timer_eval, "d=" + ToString(d), ringoa::TimeUnit::MICROSECONDS);
        };
    };

    // Create tasks for parties 0, 1, and 2
    auto task_p0 = MakeTask(0);
    auto task_p1 = MakeTask(1);
    auto task_p2 = MakeTask(2);

    // Configure network and run
    ThreePartyNetworkManager net_mgr;
    net_mgr.AutoConfigure(opts.party_id, task_p0, task_p1, task_p2);
    net_mgr.WaitForCompletion();

    Logger::InfoLog(LOC, "OblivRange VAF Online Benchmark completed");
    Logger::ExportLogListAndClear(
        MakeLogBasePath(opts, kLogOblivRangePath, "orange_vaf_online", opts.party_id),
        opts.enable_log_timestamp);
}

void OblivRange_Fsc_VAF_Preprocess_Bench(const osuCrypto::CLP &cmd) {
    if (IsHelpRequested(cmd)) {
        PrintBenchmarkHelp("OblivRange (FSC) VAF Benchmark");
        return;
    }
    const auto opts = SelectBenchOptions(cmd);

    Logger::InfoLog(LOC, "OblivRange (FSC) VAF Preprocess Benchmark started (repeat=" + ToString(opts.repeat) + ")");

    // Define the task for each party
    auto MakeTask = [&](int p) {
        const std::string ptag = "(P" + ToString(p) + ")";
        return [=](osuCrypto::Channel &chl_next, osuCrypto::Channel &chl_prev) {
            // VAF database size is approximately 23 million entries < 2^25
            // sigma for VAF values is 7 (0-100)
            ShareConfig          share = ShareConfig::Arith32();
            ProtocolContext2P    ctx2p(share);
            ProtocolContext3P    ctx3p(share);
            OblivRangeConfig     cfg{25, 7};
            OblivRangeParameters params(cfg, share);

            const uint64_t d = params.GetDbIndexBits();
            const uint64_t k = params.GetShareBitsize();

            OblivRange orange(params, ctx2p, ctx3p);
            Channels   chls(p, chl_prev, chl_next);

            const std::string timer_prep_name = "OblivRangeFSC::VAF::Preprocess" + ptag;
            int32_t           timer_prep      = ctx3p.Timers().CreateTimer(timer_prep_name);

            FileIo file_io;
            // Load v_sign
            bool v_sign;
            file_io.ReadBinary(MakeDatasetPath("vaf_v_sign", d, k) + "_" + ToString(p), v_sign);

            // Preprocess
            ringoa::sharing::Rep3PreprocessData rep3_data;
            OblivRangeFscPreprocessData         preproc_data;

            for (uint64_t i = 0; i < opts.warmup; ++i) {
                ringoa::sharing::PreprocessRep3PrfKeys(chls);
                orange.Preprocess(chls);
            }

            for (uint64_t i = 0; i < opts.repeat; ++i) {
                ctx3p.Timers().Start(timer_prep);
                ctx3p.StartCommStats(chls);
                rep3_data    = ringoa::sharing::PreprocessRep3PrfKeys(chls);
                preproc_data = orange.PreprocessFsc(chls, v_sign);
                ctx3p.Timers().Stop(timer_prep, "d=" + ToString(d) + ",iter=" + ToString(i));
                if (i == 0) {
                    Logger::InfoLog(LOC, "(P" + ToString(p) + "),bytes,d=" + ToString(d) + ",sent=" + ToString(ctx3p.StopCommStats(chls)));
                }
            }
            // Print communication cost and timer results
            ctx3p.Timers().Print(timer_prep, "d=" + ToString(d), ringoa::TimeUnit::MICROSECONDS);

            // Save preprocess data
            ringoa::sharing::SaveRep3PreprocessDataToFile(kBenchOblivRangePath + "rep3_prf_keys_" + ToString(p), rep3_data);
            ProtocolIo::SaveToFile(MakePath("preproc_fsc_vaf", d, k) + "_" + ToString(p), preproc_data);
        };
    };

    auto task_p0 = MakeTask(0);
    auto task_p1 = MakeTask(1);
    auto task_p2 = MakeTask(2);

    ThreePartyNetworkManager net_mgr;
    net_mgr.AutoConfigure(opts.party_id, task_p0, task_p1, task_p2);
    net_mgr.WaitForCompletion();

    Logger::InfoLog(LOC, "OblivRange (FSC) VAF Preprocess Benchmark completed");
    Logger::ExportLogListAndClear(
        MakeLogBasePath(opts, kLogOblivRangePath, "orange_fsc_vaf_preproc", opts.party_id),
        opts.enable_log_timestamp);
}

void OblivRange_Fsc_VAF_Online_Bench(const osuCrypto::CLP &cmd) {
    if (IsHelpRequested(cmd)) {
        PrintBenchmarkHelp("OblivRange (FSC) VAF Benchmark");
        return;
    }
    const auto opts = SelectBenchOptions(cmd);

    Logger::InfoLog(LOC, "OblivRange (FSC) VAF Online Benchmark started (repeat=" + ToString(opts.repeat) +
                             ", party=" + ToString(opts.party_id) + ")");

    // Helper that returns a task lambda for a given party p
    auto MakeTask = [&](int p) {
        const std::string ptag = "(P" + ToString(p) + ")";
        return [=](osuCrypto::Channel &chl_next, osuCrypto::Channel &chl_prev) {
            ShareConfig          share = ShareConfig::Arith32();
            ProtocolContext2P    ctx2p(share);
            ProtocolContext3P    ctx3p(share);
            OblivRangeConfig     cfg{25, 7};
            OblivRangeParameters params(cfg, share);

            const uint64_t d  = params.GetDbIndexBits();
            const uint64_t k  = params.GetShareBitsize();
            const uint64_t nu = params.GetOaParameters().GetParameters().GetTerminateBitsize();

            // ----- Timers -----
            const std::string timer_setup_name = "OblivRangeFSC::VAF::OnlineSetUp" + ptag;
            const std::string timer_eval_name  = "OblivRangeFSC::VAF::Eval" + ptag;
            const int32_t     timer_setup      = ctx3p.Timers().CreateTimer(timer_setup_name);
            const int32_t     timer_eval       = ctx3p.Timers().CreateTimer(timer_eval_name);

            // ================================
            // OnlineSetUp timing
            // ================================
            ctx3p.Timers().Start(timer_setup);

            OblivRange orange(params, ctx2p, ctx3p);
            Channels   chls(p, chl_prev, chl_next);

            // Load shares (database and query = [left,right,k])
            Rep3ShareMat64 db_sh;
            Rep3ShareVec64 aux_sh;
            Rep3ShareVec64 query_sh;

            Rep3ShareIo::LoadShare(MakeDatasetPath("vaf_db_fsc", d, k) + "_" + ToString(p), db_sh);
            Rep3ShareIo::LoadShare(MakeDatasetPath("vaf_aux_fsc", d, k) + "_" + ToString(p), aux_sh);
            Rep3ShareIo::LoadShare(MakeDatasetPath("vaf_query_brca", d, k) + "_" + ToString(p), query_sh);

            // Extract query components
            Rep3Share64 left_sh  = query_sh.At(0);
            Rep3Share64 right_sh = query_sh.At(1);
            Rep3Share64 k_sh     = query_sh.At(2);

            // Load preprocess data
            ringoa::sharing::Rep3PreprocessData loaded_rep3_data;
            OblivRangeFscPreprocessData         preproc_data;
            ringoa::sharing::LoadRep3PreprocessDataFromFile(kBenchOblivRangePath + "rep3_prf_keys_" + ToString(p), loaded_rep3_data);
            ProtocolIo::LoadFromFile(MakePath("preproc_fsc_vaf", d, k) + "_" + ToString(p), preproc_data, params);

            ctx3p.Rss().SetPrfKeys(loaded_rep3_data.prf_key_self, loaded_rep3_data.prf_key_next);
            OblivRangeFscKeys key = std::move(preproc_data).ExtractKeys();

            // Buffers sized by terminate bitsize
            std::vector<ringoa::block> uv_prev(1ULL << nu), uv_next(1ULL << nu);

            ctx3p.Timers().Stop(timer_setup, "d=" + ToString(d) + ",iter=0");
            ctx3p.Timers().Print(timer_setup, "d=" + ToString(d), ringoa::TimeUnit::MICROSECONDS);

            // ================================
            // Eval timing
            // ================================
            Rep3Share64 result_sh;
            for (uint64_t i = 0; i < opts.repeat; ++i) {
                ctx3p.Timers().Start(timer_eval);
                ctx3p.StartCommStats(chls);
                orange.EvaluateRangeFscPair(
                    chls, key, uv_prev, uv_next,
                    db_sh, Rep3ShareView64(aux_sh), left_sh, right_sh, k_sh, result_sh);
                ctx3p.Timers().Stop(timer_eval, "d=" + ToString(d) + ",iter=" + ToString(i));
                if (i == 0) {
                    Logger::InfoLog(LOC, "(P" + ToString(p) + "),bytes,d=" + ToString(d) + ",sent=" + ToString(ctx3p.StopCommStats(chls)));
                }
                ctx3p.Timers().Print(ctx3p.Timers().GetOrCreateTimer("RingOAFSC::LocalEval"), "d=" + ToString(d) + ",iter=" + ToString(i), ringoa::TimeUnit::MICROSECONDS);
                ctx3p.Timers().ResetByName("RingOAFSC::LocalEval");
            }
            ctx3p.Timers().Print(timer_eval, "d=" + ToString(d), ringoa::TimeUnit::MICROSECONDS);
        };
    };

    // Create tasks for parties 0, 1, and 2
    auto task_p0 = MakeTask(0);
    auto task_p1 = MakeTask(1);
    auto task_p2 = MakeTask(2);

    // Configure network and run
    ThreePartyNetworkManager net_mgr;
    net_mgr.AutoConfigure(opts.party_id, task_p0, task_p1, task_p2);
    net_mgr.WaitForCompletion();

    Logger::InfoLog(LOC, "OblivRange (FSC) VAF Online Benchmark completed");
    Logger::ExportLogListAndClear(
        MakeLogBasePath(opts, kLogOblivRangePath, "orange_fsc_vaf_online", opts.party_id),
        opts.enable_log_timestamp);
}

}    // namespace bench_ringoa
