#include "shared_ot_bench.h"

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
#include "RingOA/utils/timer.h"
#include "RingOA/utils/utils.h"
#include "bench_common.h"

namespace {

inline std::string MakePath(const std::string &name, uint64_t d, uint64_t k) {
    return bench_ringoa::kBenchSotPath + name + "_d" + ringoa::ToString(d) + "_k" + ringoa::ToString(k);
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
using ringoa::proto::ProtocolContext3P;
using ringoa::proto::ProtocolIo;
using ringoa::proto::SharedOt;
using ringoa::proto::SharedOtConfig;
using ringoa::proto::SharedOtKeys;
using ringoa::proto::SharedOtParameters;
using ringoa::proto::SharedOtPreprocessData;
using ringoa::sharing::AdditiveSharing2P;
using ringoa::sharing::Rep3Share64;
using ringoa::sharing::Rep3ShareIo;
using ringoa::sharing::Rep3ShareVec64;
using ringoa::sharing::Rep3ShareView64;
using ringoa::sharing::ReplicatedSharing3P;
using ringoa::sharing::ShareConfig;

void SharedOT_Preprocess_Bench(const osuCrypto::CLP &cmd) {
    if (IsHelpRequested(cmd)) {
        PrintBenchmarkHelp("SharedOT Benchmark");
        return;
    }
    const auto opts = SelectBenchOptions(cmd);

    Logger::InfoLog(LOC, "SharedOT Preprocess Benchmark started (repeat=" + ToString(opts.repeat) + ")");

    // Define the task for each party
    auto MakeTask = [&](int p) {
        const std::string ptag = "(P" + ToString(p) + ")";
        return [=](osuCrypto::Channel &chl_next, osuCrypto::Channel &chl_prev) {
            for (auto db_bitsize : opts.db_bits_list) {
                ShareConfig       share = ShareConfig::Arith32();
                ProtocolContext3P ctx(share);
                SharedOtConfig    cfg{db_bitsize};
                cfg.eval_type = ringoa::fss::EvalType::kHybridBatchedFullDepth;
                SharedOtParameters params(cfg, share);

                uint64_t d = params.GetDbIndexBits();
                uint64_t k = params.GetShareBitsize();

                const std::string timer_prep_name = "SharedOT::Preprocess" + ptag;
                int32_t           timer_prep      = ctx.Timers().CreateTimer(timer_prep_name);

                SharedOt sharedot(params, ctx);
                Channels chls(p, chl_prev, chl_next);

                // Preprocess
                ringoa::sharing::Rep3PreprocessData rep3_data;
                SharedOtPreprocessData              preproc_data;

                for (uint64_t i = 0; i < opts.warmup; ++i) {
                    ringoa::sharing::PreprocessRep3PrfKeys(chls);
                    sharedot.Preprocess(chls, 1);
                }

                for (uint64_t i = 0; i < opts.repeat; ++i) {
                    ctx.Timers().Start(timer_prep);
                    ctx.StartCommStats(chls);
                    rep3_data    = ringoa::sharing::PreprocessRep3PrfKeys(chls);
                    preproc_data = sharedot.Preprocess(chls, 1);
                    ctx.Timers().Stop(timer_prep, "d=" + ToString(d) + ",iter=" + ToString(i));
                    if (i == 0) {
                        Logger::InfoLog(LOC, "(P" + ToString(p) + "),bytes,d=" + ToString(d) + ",sent=" + ToString(ctx.StopCommStats(chls)));
                    }
                }
                // Print communication cost and timer results
                ctx.Timers().Print(timer_prep, "d=" + ToString(d), ringoa::TimeUnit::MICROSECONDS);

                // Save preprocess data
                ringoa::sharing::SaveRep3PreprocessDataToFile(kBenchSotPath + "rep3_prf_keys_" + ToString(p), rep3_data);
                ProtocolIo::SaveToFile(MakePath("preproc", d, k) + "_" + ToString(p), preproc_data);
            }
        };
    };

    auto task_p0 = MakeTask(0);
    auto task_p1 = MakeTask(1);
    auto task_p2 = MakeTask(2);

    ThreePartyNetworkManager net_mgr;
    net_mgr.AutoConfigure(opts.party_id, task_p0, task_p1, task_p2);
    net_mgr.WaitForCompletion();

    Logger::InfoLog(LOC, "SharedOT Preprocess Benchmark completed");
    Logger::ExportLogListAndClear(
        MakeLogBasePath(opts, kLogSotPath, "sharedot_preproc", opts.party_id),
        opts.enable_log_timestamp);
}

void SharedOT_Online_Bench(const osuCrypto::CLP &cmd) {
    if (IsHelpRequested(cmd)) {
        PrintBenchmarkHelp("SharedOT Benchmark");
        return;
    }
    const auto opts = SelectBenchOptions(cmd);

    Logger::InfoLog(LOC, "SharedOT Online Benchmark started (repeat=" + ToString(opts.repeat) + ", party=" + ToString(opts.party_id) + ")");

    auto MakeTask = [&](int p) {
        const std::string ptag = "(P" + ToString(p) + ")";
        return [=](osuCrypto::Channel &chl_next, osuCrypto::Channel &chl_prev) {
            for (auto db_bitsize : opts.db_bits_list) {
                ShareConfig       share = ShareConfig::Arith32();
                ProtocolContext3P ctx(share);
                SharedOtConfig    cfg{db_bitsize};
                cfg.eval_type = ringoa::fss::EvalType::kHybridBatchedFullDepth;
                SharedOtParameters params(cfg, share);

                uint64_t d = params.GetDbIndexBits();
                uint64_t k = params.GetShareBitsize();

                // Timers
                const std::string timer_setup_name = "SharedOT::OnlineSetUp" + ptag;
                const std::string timer_eval_name  = "SharedOT::Eval" + ptag;
                int32_t           timer_setup      = ctx.Timers().CreateTimer(timer_setup_name);
                int32_t           timer_eval       = ctx.Timers().CreateTimer(timer_eval_name);

                // --- OnlineSetUp timing ---
                ctx.Timers().Start(timer_setup);

                SharedOt sharedot(params, ctx);
                Channels chls(p, chl_prev, chl_next);

                // Load shares (database and index)
                Rep3ShareVec64 database_sh;
                Rep3Share64    index_sh;

                Rep3ShareIo::LoadShare(MakeDatasetPath("oa_db", d, k) + "_" + ToString(p), database_sh);
                Rep3ShareIo::LoadShare(MakeDatasetPath("oa_idx", d, k) + "_" + ToString(p), index_sh);

                // Load preprocess data
                ringoa::sharing::Rep3PreprocessData loaded_rep3_data;
                SharedOtPreprocessData              preproc_data;
                ringoa::sharing::LoadRep3PreprocessDataFromFile(kBenchSotPath + "rep3_prf_keys_" + ToString(p), loaded_rep3_data);
                ProtocolIo::LoadFromFile(MakePath("preproc", d, k) + "_" + ToString(p), preproc_data, params);

                ctx.Rss().SetPrfKeys(loaded_rep3_data.prf_key_self, loaded_rep3_data.prf_key_next);
                SharedOtKeys &key = preproc_data.keys;

                // Buffers sized by terminate bitsize
                std::vector<uint64_t> uv_prev(1U << d), uv_next(1U << d);

                ctx.Timers().Stop(timer_setup, "d=" + ToString(d) + ",iter=0");

                // --- Eval timing ---
                Rep3Share64 result_sh;
                for (uint64_t i = 0; i < opts.warmup; ++i) {
                    sharedot.ObliviousAccess(chls, key.GetView(0), uv_prev, uv_next,
                                             Rep3ShareView64(database_sh), index_sh, result_sh);
                }
                ctx.Timers().ResetByName("SharedOT::FullDomainEval");
                ctx.Timers().ResetByName("SharedOT::DotProduct");

                for (uint64_t i = 0; i < opts.repeat; ++i) {
                    ctx.Timers().Start(timer_eval);
                    ctx.StartCommStats(chls);
                    sharedot.ObliviousAccess(chls, key.GetView(0), uv_prev, uv_next,
                                             Rep3ShareView64(database_sh), index_sh, result_sh);
                    ctx.Timers().Stop(timer_eval, "d=" + ToString(d) + ",iter=" + ToString(i));
                    if (i == 0) {
                        Logger::InfoLog(LOC, "(P" + ToString(p) + "),bytes,d=" + ToString(d) + ",sent=" + ToString(ctx.StopCommStats(chls)));
                    }
                }
                ctx.Timers().PrintAll("d=" + ToString(d), ringoa::TimeUnit::MICROSECONDS);
            }
        };
    };

    ThreePartyNetworkManager net_mgr;
    net_mgr.AutoConfigure(opts.party_id, MakeTask(0), MakeTask(1), MakeTask(2));
    net_mgr.WaitForCompletion();

    Logger::InfoLog(LOC, "SharedOT Online Benchmark completed");

    Logger::ExportLogListAndClear(
        MakeLogBasePath(opts, kLogSotPath, "sharedot_online", opts.party_id),
        opts.enable_log_timestamp);
}

}    // namespace bench_ringoa
