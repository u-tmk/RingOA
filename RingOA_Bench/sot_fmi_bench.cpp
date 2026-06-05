#include "sot_fmi_bench.h"

#include <cryptoTools/Common/TestCollection.h>

#include "RingOA/obliv_fmi/sot_fmi.h"
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

inline std::string MakePath(const std::string &name, uint64_t d, uint64_t qs, uint64_t k) {
    return bench_ringoa::kBenchSotfmiPath + name + "_d" + ringoa::ToString(d) + "_qs" + ringoa::ToString(qs) + "_k" + ringoa::ToString(k);
}

}    // namespace

namespace bench_ringoa {

using ringoa::Channels;
using ringoa::CreateSequence;
using ringoa::FileIo;
using ringoa::GlobalRng;
using ringoa::Logger;
using ringoa::Mod2N;
using ringoa::ThreePartyNetworkManager;
using ringoa::TimerManager;
using ringoa::ToString;
using ringoa::fmi::SotFMI;
using ringoa::fmi::SotFMIConfig;
using ringoa::fmi::SotFMIKeys;
using ringoa::fmi::SotFMIParameters;
using ringoa::fmi::SotFMIPreprocessData;
using ringoa::proto::ProtocolContext2P;
using ringoa::proto::ProtocolContext3P;
using ringoa::proto::ProtocolIo;
using ringoa::sharing::Rep3Share64;
using ringoa::sharing::Rep3ShareIo;
using ringoa::sharing::Rep3ShareMat64;
using ringoa::sharing::Rep3ShareVec64;
using ringoa::sharing::ReplicatedSharing3P;
using ringoa::sharing::ShareConfig;
using ringoa::wm::FMIndex;

void SotFMI_Preprocess_Bench(const osuCrypto::CLP &cmd) {
    if (IsHelpRequested(cmd)) {
        PrintBenchmarkHelp("SotFMI Benchmark");
        return;
    }
    const auto            opts          = SelectBenchOptions(cmd);
    std::vector<uint64_t> text_bitsizes = opts.db_bits_list;
    std::vector<uint64_t> query_sizes   = SelectQueryBitsize(cmd);

    Logger::InfoLog(LOC, "SotFMI Preprocess Benchmark started (repeat=" + ToString(opts.repeat) + ")");

    // Define the task for each party
    auto MakeTask = [&](int p) {
        const std::string ptag = "(P" + ToString(p) + ")";
        return [=](osuCrypto::Channel &chl_next, osuCrypto::Channel &chl_prev) {
            for (auto text_bitsize : text_bitsizes) {
                for (auto query_size : query_sizes) {
                    ShareConfig       share = ShareConfig::Arith32();
                    ProtocolContext2P ctx2p(share);
                    ProtocolContext3P ctx3p(share);
                    SotFMIConfig      cfg{text_bitsize, query_size};
                    cfg.eval_type = ringoa::fss::EvalType::kHybridBatchedFullDepth;
                    SotFMIParameters params(cfg, share);

                    uint64_t d  = params.GetDbIndexBits();
                    uint64_t k  = params.GetShareBitsize();
                    uint64_t qs = params.GetQuerySize();

                    int32_t timer_prep = ctx3p.Timers().CreateTimer("SotFMI::Preprocess" + ptag);

                    SotFMI   sotfmi(params, ctx2p, ctx3p);
                    Channels chls(p, chl_prev, chl_next);

                    // Preprocess
                    ringoa::sharing::Rep3PreprocessData rep3_data;
                    SotFMIPreprocessData                preproc_data;

                    for (uint64_t i = 0; i < opts.warmup; ++i) {
                        ringoa::sharing::PreprocessRep3PrfKeys(chls);
                        sotfmi.Preprocess(chls);
                    }

                    for (uint64_t i = 0; i < opts.repeat; ++i) {
                        ctx3p.Timers().Start(timer_prep);
                        ctx3p.StartCommStats(chls);
                        rep3_data    = ringoa::sharing::PreprocessRep3PrfKeys(chls);
                        preproc_data = sotfmi.Preprocess(chls);
                        ctx3p.Timers().Stop(timer_prep, "d=" + ToString(d) + ",qs=" + ToString(qs) + ",iter=" + ToString(i));
                        if (i == 0) {
                            Logger::InfoLog(LOC, "(P" + ToString(p) + "),bytes,d=" + ToString(d) + ",qs=" + ToString(qs) + ",sent=" + ToString(ctx3p.StopCommStats(chls)));
                        }
                    }
                    // Print communication cost and timer results
                    ctx3p.Timers().Print(timer_prep, "d=" + ToString(d) + ",qs=" + ToString(qs), ringoa::TimeUnit::MICROSECONDS);

                    // Save preprocess data
                    ringoa::sharing::SaveRep3PreprocessDataToFile(kBenchSotfmiPath + "rep3_prf_keys_" + ToString(p), rep3_data);
                    ProtocolIo::SaveToFile(MakePath("preproc", d, qs, k) + "_" + ToString(p), preproc_data);
                }
            }
        };
    };

    auto task_p0 = MakeTask(0);
    auto task_p1 = MakeTask(1);
    auto task_p2 = MakeTask(2);

    ThreePartyNetworkManager net_mgr;
    net_mgr.AutoConfigure(opts.party_id, task_p0, task_p1, task_p2);
    net_mgr.WaitForCompletion();

    Logger::InfoLog(LOC, "SotFMI Preprocess Benchmark completed");
    Logger::ExportLogListAndClear(
        MakeLogBasePath(opts, kLogSotfmiPath, "sotfmi_preproc", opts.party_id),
        opts.enable_log_timestamp);
}

void SotFMI_Online_Bench(const osuCrypto::CLP &cmd) {
    if (IsHelpRequested(cmd)) {
        PrintBenchmarkHelp("SotFMI Benchmark");
        return;
    }
    const auto            opts          = SelectBenchOptions(cmd);
    std::vector<uint64_t> text_bitsizes = opts.db_bits_list;
    std::vector<uint64_t> query_sizes   = SelectQueryBitsize(cmd);

    Logger::InfoLog(LOC, "SotFMI Online Benchmark started (repeat=" + ToString(opts.repeat) + ", party=" + ToString(opts.party_id) + ")");

    auto MakeTask = [&](int p) {
        const std::string ptag = "(P" + ToString(p) + ")";
        return [=](osuCrypto::Channel &chl_next, osuCrypto::Channel &chl_prev) {
            for (auto text_bitsize : text_bitsizes) {
                for (auto query_size : query_sizes) {
                    ShareConfig       share = ShareConfig::Arith32();
                    ProtocolContext2P ctx2p(share);
                    ProtocolContext3P ctx3p(share);
                    SotFMIConfig      cfg{text_bitsize, query_size};
                    cfg.eval_type = ringoa::fss::EvalType::kHybridBatchedFullDepth;
                    SotFMIParameters params(cfg, share);

                    uint64_t d  = params.GetDbIndexBits();
                    uint64_t k  = params.GetShareBitsize();
                    uint64_t qs = params.GetQuerySize();

                    int32_t timer_setup = ctx3p.Timers().CreateTimer("SotFMI::OnlineSetUp" + ptag);
                    int32_t timer_eval  = ctx3p.Timers().CreateTimer("SotFMI::Eval" + ptag);

                    ctx3p.Timers().Start(timer_setup);

                    SotFMI   sotfmi(params, ctx2p, ctx3p);
                    Channels chls(p, chl_prev, chl_next);

                    std::vector<uint64_t> uv_prev(1ULL << d), uv_next(1ULL << d);

                    Rep3ShareMat64 db_sh;
                    Rep3ShareMat64 query_sh;

                    Rep3ShareIo::LoadShare(MakeDatasetPath("fmi_db", d, qs, k) + "_" + ToString(p), db_sh);
                    Rep3ShareIo::LoadShare(MakeDatasetPath("fmi_query", d, qs, k) + "_" + ToString(p), query_sh);

                    // Load preprocess data
                    ringoa::sharing::Rep3PreprocessData loaded_rep3_data;
                    SotFMIPreprocessData                preproc_data;
                    ringoa::sharing::LoadRep3PreprocessDataFromFile(kBenchSotfmiPath + "rep3_prf_keys_" + ToString(p), loaded_rep3_data);
                    ProtocolIo::LoadFromFile(MakePath("preproc", d, qs, k) + "_" + ToString(p), preproc_data, params);

                    ctx3p.Rss().SetPrfKeys(loaded_rep3_data.prf_key_self, loaded_rep3_data.prf_key_next);
                    SotFMIKeys key = std::move(preproc_data).ExtractKeys();

                    ctx3p.Timers().Stop(timer_setup, "d=" + ToString(d) + ",qs=" + ToString(qs) + ",iter=0");
                    ctx3p.Timers().Print(timer_setup, "d=" + ToString(d) + ",qs=" + ToString(qs), ringoa::TimeUnit::MICROSECONDS);

                    for (uint64_t i = 0; i < opts.repeat; ++i) {
                        ctx3p.Timers().Start(timer_eval);
                        ctx3p.StartCommStats(chls);
                        Rep3ShareVec64 result_sh(qs);
                        sotfmi.EvaluateLPMPair(chls, key, uv_prev, uv_next, db_sh, query_sh, result_sh);
                        ctx3p.Timers().Stop(timer_eval, "d=" + ToString(d) + ",qs=" + ToString(qs) + ",iter=" + ToString(i));
                        if (i == 0) {
                            Logger::InfoLog(LOC, "(P" + ToString(p) + "),bytes,d=" + ToString(d) + ",qs=" + ToString(qs) + ",sent=" + ToString(ctx3p.StopCommStats(chls)));
                        }
                        ctx3p.Timers().Print(ctx3p.Timers().GetOrCreateTimer("SharedOT::LocalEval"), "d=" + ToString(d) + ",qs=" + ToString(qs) + ",iter=" + ToString(i), ringoa::TimeUnit::MICROSECONDS);
                        ctx3p.Timers().ResetByName("SharedOT::LocalEval");
                    }
                    ctx3p.Timers().Print(timer_eval, "d=" + ToString(d) + ",qs=" + ToString(qs), ringoa::TimeUnit::MICROSECONDS);
                }
            }
        };
    };

    ThreePartyNetworkManager net_mgr;
    net_mgr.AutoConfigure(opts.party_id, MakeTask(0), MakeTask(1), MakeTask(2));
    net_mgr.WaitForCompletion();

    Logger::InfoLog(LOC, "SotFMI Online Benchmark completed");
    Logger::ExportLogListAndClear(
        MakeLogBasePath(opts, kLogSotfmiPath, "sotfmi_online", opts.party_id),
        opts.enable_log_timestamp);
}

}    // namespace bench_ringoa
