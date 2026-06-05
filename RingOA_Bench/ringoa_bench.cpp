#include "ringoa_bench.h"

#include <cryptoTools/Common/TestCollection.h>

#include "RingOA/protocol/protocol_io.h"
#include "RingOA/protocol/ringoa.h"
#include "RingOA/protocol/ringoa_fsc.h"
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
    return bench_ringoa::kBenchRingOAPath + name + "_d" + ringoa::ToString(d) + "_k" + ringoa::ToString(k);
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
using ringoa::proto::RingOa;
using ringoa::proto::RingOaConfig;
using ringoa::proto::RingOaFsc;
using ringoa::proto::RingOaFscKeys;
using ringoa::proto::RingOaFscPreprocessData;
using ringoa::proto::RingOaKeys;
using ringoa::proto::RingOaParameters;
using ringoa::proto::RingOaPreprocessData;
using ringoa::sharing::Rep3Share32;
using ringoa::sharing::Rep3Share64;
using ringoa::sharing::Rep3ShareIo;
using ringoa::sharing::Rep3ShareVec32;
using ringoa::sharing::Rep3ShareVec64;
using ringoa::sharing::Rep3ShareView32;
using ringoa::sharing::Rep3ShareView64;
using ringoa::sharing::ReplicatedSharing3P;
using ringoa::sharing::ShareConfig;

void RingOA_Preprocess_Bench(const osuCrypto::CLP &cmd) {
    if (IsHelpRequested(cmd)) {
        PrintBenchmarkHelp("RingOA Benchmark");
        return;
    }
    const auto opts = SelectBenchOptions(cmd);

    Logger::InfoLog(LOC, "RingOA Preprocess Benchmark started (repeat=" + ToString(opts.repeat) + ")");

    // Define the task for each party
    auto MakeTask = [&](int p) {
        const std::string ptag = "(P" + ToString(p) + ")";
        return [=](osuCrypto::Channel &chl_next, osuCrypto::Channel &chl_prev) {
            for (auto db_bitsize : opts.db_bits_list) {
                ShareConfig       share = ShareConfig::Arith32();
                ProtocolContext3P ctx(share);
                RingOaConfig      cfg{db_bitsize};
                RingOaParameters  params(cfg, share);

                uint64_t d = params.GetDbIndexBits();
                uint64_t k = params.GetShareBitsize();

                const std::string timer_prep_name = "RingOA::Preprocess" + ptag;
                int32_t           timer_prep      = ctx.Timers().CreateTimer(timer_prep_name);

                RingOa   ringoa(params, ctx);
                Channels chls(p, chl_prev, chl_next);

                // Preprocess
                ringoa::sharing::Rep3PreprocessData rep3_data;
                RingOaPreprocessData                preproc_data;

                for (uint64_t i = 0; i < opts.warmup; ++i) {
                    ringoa::sharing::PreprocessRep3PrfKeys(chls);
                    ringoa.Preprocess(chls, 1);
                }

                for (uint64_t i = 0; i < opts.repeat; ++i) {
                    ctx.Timers().Start(timer_prep);
                    ctx.StartCommStats(chls);
                    rep3_data    = ringoa::sharing::PreprocessRep3PrfKeys(chls);
                    preproc_data = ringoa.Preprocess(chls, 1);
                    ctx.Timers().Stop(timer_prep, "d=" + ToString(d) + ",iter=" + ToString(i));
                    if (i == 0) {
                        Logger::InfoLog(LOC, "(P" + ToString(p) + "),bytes,d=" + ToString(d) + ",sent=" + ToString(ctx.StopCommStats(chls)));
                    }
                }
                // Print communication cost and timer results
                ctx.Timers().Print(timer_prep, "d=" + ToString(d), ringoa::TimeUnit::MICROSECONDS);

                // Save preprocess data
                ringoa::sharing::SaveRep3PreprocessDataToFile(kBenchRingOAPath + "rep3_prf_keys_" + ToString(p), rep3_data);
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

    Logger::InfoLog(LOC, "RingOA Preprocess Benchmark completed");
    Logger::ExportLogListAndClear(
        MakeLogBasePath(opts, kLogRingOaPath, "ringoa_preproc", opts.party_id),
        opts.enable_log_timestamp);
}

void RingOA_Online_Bench(const osuCrypto::CLP &cmd) {
    if (IsHelpRequested(cmd)) {
        PrintBenchmarkHelp("RingOA Benchmark");
        return;
    }
    const auto opts = SelectBenchOptions(cmd);

    Logger::InfoLog(LOC, "RingOA Online Benchmark started (repeat=" + ToString(opts.repeat) +
                             ", party=" + ToString(opts.party_id) + ")");

    auto MakeTask = [&](int p) {
        const std::string ptag = "(P" + ToString(p) + ")";
        return [=](osuCrypto::Channel &chl_next, osuCrypto::Channel &chl_prev) {
            for (auto db_bitsize : opts.db_bits_list) {
                ShareConfig       share = ShareConfig::Arith32();
                ProtocolContext3P ctx(share);
                RingOaConfig      cfg{db_bitsize};
                RingOaParameters  params(cfg, share);

                uint64_t d  = params.GetDbIndexBits();
                uint64_t k  = params.GetShareBitsize();
                uint64_t nu = params.GetParameters().GetTerminateBitsize();

                // Timers
                const std::string timer_setup_name = "RingOA::OnlineSetUp" + ptag;
                const std::string timer_eval_name  = "RingOA::Eval" + ptag;
                int32_t           timer_setup      = ctx.Timers().CreateTimer(timer_setup_name);
                int32_t           timer_eval       = ctx.Timers().CreateTimer(timer_eval_name);

                // --- OnlineSetUp timing ---
                ctx.Timers().Start(timer_setup);
                RingOa   ringoa(params, ctx);
                Channels chls(p, chl_prev, chl_next);

                // Load shares (database and index)
                Rep3ShareVec64 database_sh;
                Rep3Share64    index_sh;

                Rep3ShareIo::LoadShare(MakeDatasetPath("oa_db", d, k) + "_" + ToString(p), database_sh);
                Rep3ShareIo::LoadShare(MakeDatasetPath("oa_idx", d, k) + "_" + ToString(p), index_sh);

                // Load preprocess data
                ringoa::sharing::Rep3PreprocessData loaded_rep3_data;
                RingOaPreprocessData                preproc_data;
                ringoa::sharing::LoadRep3PreprocessDataFromFile(kBenchRingOAPath + "rep3_prf_keys_" + ToString(p), loaded_rep3_data);
                ProtocolIo::LoadFromFile(MakePath("preproc", d, k) + "_" + ToString(p), preproc_data, params);

                ctx.Rss().SetPrfKeys(loaded_rep3_data.prf_key_self, loaded_rep3_data.prf_key_next);
                RingOaKeys &key = preproc_data.keys;

                // Consume preprocess data
                ringoa.ConsumePreprocessData(preproc_data);

                // Buffers sized by terminate bitsize
                std::vector<ringoa::block> uv_prev(1ULL << nu), uv_next(1ULL << nu);

                ctx.Timers().Stop(timer_setup, "d=" + ToString(d) + ",iter=0");

                // --- Eval timing ---
                Rep3Share64 result_sh;
                for (uint64_t i = 0; i < opts.warmup; ++i) {
                    ringoa.ObliviousAccess(chls, key.GetView(0), uv_prev, uv_next,
                                           Rep3ShareView64(database_sh), index_sh, result_sh);
                    ctx.AssWithNext().ResetTripleIndex();
                    ctx.AssWithPrev().ResetTripleIndex();
                }
                ctx.Timers().ResetByName("RingOA::FullDomainEval");
                ctx.Timers().ResetByName("RingOA::DotProduct");

                for (uint64_t i = 0; i < opts.repeat; ++i) {
                    ctx.Timers().Start(timer_eval);
                    ctx.StartCommStats(chls);
                    ringoa.ObliviousAccess(chls, key.GetView(0), uv_prev, uv_next,
                                           Rep3ShareView64(database_sh), index_sh, result_sh);
                    ctx.Timers().Stop(timer_eval, "d=" + ToString(d) + ",iter=" + ToString(i));
                    if (i == 0) {
                        Logger::InfoLog(LOC, "(P" + ToString(p) + "),bytes,d=" + ToString(d) + ",sent=" + ToString(ctx.StopCommStats(chls)));
                    }
                    ctx.AssWithNext().ResetTripleIndex();
                    ctx.AssWithPrev().ResetTripleIndex();
                }
                ctx.Timers().PrintAll("d=" + ToString(d), ringoa::TimeUnit::MICROSECONDS);
            }
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

    Logger::InfoLog(LOC, "RingOA Online Benchmark completed");
    Logger::ExportLogListAndClear(
        MakeLogBasePath(opts, kLogRingOaPath, "ringoa_online", opts.party_id),
        opts.enable_log_timestamp);
}

void RingOA_Fsc_Preprocess_Bench(const osuCrypto::CLP &cmd) {
    if (IsHelpRequested(cmd)) {
        PrintBenchmarkHelp("RingOA (FSC) Preprocess Benchmark");
        return;
    }
    const auto opts = SelectBenchOptions(cmd);

    Logger::InfoLog(LOC, "RingOA (FSC) Preprocess Benchmark started (repeat=" + ToString(opts.repeat) + ")");

    // Define the task for each party
    auto MakeTask = [&](int p) {
        const std::string ptag = "(P" + ToString(p) + ")";
        return [=](osuCrypto::Channel &chl_next, osuCrypto::Channel &chl_prev) {
            for (auto db_bitsize : opts.db_bits_list) {
                ShareConfig       share = ShareConfig::Arith32();
                ProtocolContext3P ctx(share);
                RingOaConfig      cfg{db_bitsize};
                RingOaParameters  params(cfg, share);

                uint64_t d = params.GetDbIndexBits();
                uint64_t k = params.GetShareBitsize();

                const std::string timer_prep_name = "RingOAFSC::Preprocess" + ptag;
                int32_t           timer_prep      = ctx.Timers().CreateTimer(timer_prep_name);

                RingOaFsc ringoa(params, ctx);
                Channels  chls(p, chl_prev, chl_next);

                FileIo file_io;
                // Load v_sign
                bool v_sign;
                file_io.ReadBinary(MakeDatasetPath("oa_v_sign", d, k) + "_" + ToString(p), v_sign);

                // Preprocess
                ringoa::sharing::Rep3PreprocessData rep3_data;
                RingOaFscPreprocessData             preproc_data;

                for (uint64_t i = 0; i < opts.warmup; ++i) {
                    ringoa::sharing::PreprocessRep3PrfKeys(chls);
                    ringoa.Preprocess(chls, v_sign, 1);
                }

                for (uint64_t i = 0; i < opts.repeat; ++i) {
                    ctx.Timers().Start(timer_prep);
                    ctx.StartCommStats(chls);
                    rep3_data    = ringoa::sharing::PreprocessRep3PrfKeys(chls);
                    preproc_data = ringoa.Preprocess(chls, v_sign, 1);
                    ctx.Timers().Stop(timer_prep, "d=" + ToString(d) + ",iter=" + ToString(i));
                    if (i == 0) {
                        Logger::InfoLog(LOC, "(P" + ToString(p) + "),bytes,d=" + ToString(d) + ",sent=" + ToString(ctx.StopCommStats(chls)));
                    }
                }
                // Print communication cost and timer results
                ctx.Timers().Print(timer_prep, "d=" + ToString(d), ringoa::TimeUnit::MICROSECONDS);

                // Save preprocess data
                ringoa::sharing::SaveRep3PreprocessDataToFile(kBenchRingOAPath + "rep3_prf_keys_" + ToString(p), rep3_data);
                ProtocolIo::SaveToFile(MakePath("preproc_fsc", d, k) + "_" + ToString(p), preproc_data);
            }
        };
    };

    auto task_p0 = MakeTask(0);
    auto task_p1 = MakeTask(1);
    auto task_p2 = MakeTask(2);

    ThreePartyNetworkManager net_mgr;
    net_mgr.AutoConfigure(opts.party_id, task_p0, task_p1, task_p2);
    net_mgr.WaitForCompletion();

    Logger::InfoLog(LOC, "RingOA_Fsc_Preprocess_Bench - Finished");
    Logger::ExportLogListAndClear(
        MakeLogBasePath(opts, kLogRingOaPath, "ringoa_fsc_preproc", opts.party_id),
        opts.enable_log_timestamp);
}

void RingOA_Fsc_Online_Bench(const osuCrypto::CLP &cmd) {
    if (IsHelpRequested(cmd)) {
        PrintBenchmarkHelp("RingOA (FSC) Online Benchmark");
        return;
    }
    const auto opts = SelectBenchOptions(cmd);

    Logger::InfoLog(LOC, "RingOA (FSC) Online Benchmark started (repeat=" + ToString(opts.repeat) +
                             ", party=" + ToString(opts.party_id) + ")");

    auto MakeTask = [&](int p) {
        const std::string ptag = "(P" + ToString(p) + ")";
        return [=](osuCrypto::Channel &chl_next, osuCrypto::Channel &chl_prev) {
            for (auto db_bitsize : opts.db_bits_list) {
                ShareConfig       share = ShareConfig::Arith32();
                ProtocolContext3P ctx(share);
                RingOaConfig      cfg{db_bitsize};
                RingOaParameters  params(cfg, share);

                uint64_t d  = params.GetDbIndexBits();
                uint64_t k  = params.GetShareBitsize();
                uint64_t nu = params.GetParameters().GetTerminateBitsize();

                // Timers
                const std::string timer_setup_name = "RingOAFSC::OnlineSetUp" + ptag;
                const std::string timer_eval_name  = "RingOAFSC::Eval" + ptag;
                int32_t           timer_setup      = ctx.Timers().CreateTimer(timer_setup_name);
                int32_t           timer_eval       = ctx.Timers().CreateTimer(timer_eval_name);

                // --- OnlineSetUp timing ---
                ctx.Timers().Start(timer_setup);

                RingOaFsc ringoa(params, ctx);
                Channels  chls(p, chl_prev, chl_next);

                // Load shares (database and index)
                Rep3ShareVec64 database_sh;
                Rep3Share64    index_sh;

                Rep3ShareIo::LoadShare(MakeDatasetPath("oa_db_fsc", d, k) + "_" + ToString(p), database_sh);
                Rep3ShareIo::LoadShare(MakeDatasetPath("oa_idx", d, k) + "_" + ToString(p), index_sh);

                // Load preprocess data
                ringoa::sharing::Rep3PreprocessData loaded_rep3_data;
                RingOaFscPreprocessData             preproc_data;
                ringoa::sharing::LoadRep3PreprocessDataFromFile(kBenchRingOAPath + "rep3_prf_keys_" + ToString(p), loaded_rep3_data);
                ProtocolIo::LoadFromFile(MakePath("preproc_fsc", d, k) + "_" + ToString(p), preproc_data, params);

                ctx.Rss().SetPrfKeys(loaded_rep3_data.prf_key_self, loaded_rep3_data.prf_key_next);
                RingOaFscKeys &key = preproc_data.keys;

                std::vector<ringoa::block> uv_prev(1ULL << nu), uv_next(1ULL << nu);

                ctx.Timers().Stop(timer_setup, "d=" + ToString(d) + ",iter=0");

                Rep3Share64 result_sh;
                for (uint64_t i = 0; i < opts.warmup; ++i) {
                    ringoa.ObliviousAccess(chls, key.GetView(0), uv_prev, uv_next,
                                           Rep3ShareView64(database_sh), index_sh, result_sh);
                }
                ctx.Timers().ResetByName("RingOAFSC::FullDomainEval");
                ctx.Timers().ResetByName("RingOAFSC::DotProduct");

                for (uint64_t i = 0; i < opts.repeat; ++i) {
                    ctx.Timers().Start(timer_eval);
                    ctx.StartCommStats(chls);
                    ringoa.ObliviousAccess(chls, key.GetView(0),
                                           uv_prev, uv_next,
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

    auto task_p0 = MakeTask(0);
    auto task_p1 = MakeTask(1);
    auto task_p2 = MakeTask(2);

    ThreePartyNetworkManager net_mgr;
    net_mgr.AutoConfigure(opts.party_id, task_p0, task_p1, task_p2);
    net_mgr.WaitForCompletion();

    Logger::InfoLog(LOC, "RingOA (FSC) Online Benchmark completed");
    Logger::ExportLogListAndClear(
        MakeLogBasePath(opts, kLogRingOaPath, "ringoa_fsc_online", opts.party_id),
        opts.enable_log_timestamp);
}

void RingOA_FullDomainThenDotProduct_Bench(const osuCrypto::CLP &cmd) {
    if (IsHelpRequested(cmd)) {
        PrintBenchmarkHelp("RingOA FullDomainThenDotProduct Benchmark");
        return;
    }
    const auto opts = SelectBenchOptions(cmd);

    Logger::InfoLog(LOC, "RingOA FullDomainThenDotProduct Benchmark started (repeat=" + ToString(opts.repeat) + ")");

    for (auto db_bitsize : opts.db_bits_list) {
        ShareConfig      share = ShareConfig::Arith32();
        RingOaConfig     cfg{db_bitsize};
        RingOaParameters params(cfg, share);

        ProtocolContext3P    ctx(share);
        ReplicatedSharing3P &rss = ctx.Rss();
        RingOa               ringoa(params, ctx);

        uint64_t d  = params.GetDbIndexBits();
        uint64_t nu = params.GetParameters().GetTerminateBitsize();

        std::vector<uint64_t> database(1ULL << d);
        for (size_t i = 0; i < database.size(); i++)
            database[i] = i;
        auto db_shares = rss.ShareLocal(database);

        Rep3ShareView64 db_view(db_shares[0]);

        auto keys = ringoa.GenerateKeys(1);

        std::vector<block> uv_prev(1ULL << nu);
        std::vector<block> uv_next(1ULL << nu);
        uint64_t           pr_prev = Mod2N(GlobalRng::Rand<uint64_t>(), d);
        uint64_t           pr_next = Mod2N(GlobalRng::Rand<uint64_t>(), d);

        TimerManager tm;
        int          timer_id = tm.CreateTimer("FullDomainThenDotProduct");

        for (uint64_t r = 0; r < opts.repeat; r++) {
            tm.Start(timer_id);
            auto [dp_prev, dp_next] =
                ringoa.EvaluateFullDomainThenDotProduct(
                    0,    // party_id
                    keys[0].key_from_prev[0],
                    keys[0].key_from_next[0],
                    uv_prev,
                    uv_next,
                    db_view,
                    pr_prev,
                    pr_next);
            Logger::DebugLog(LOC, "dp_prev: " + ToString(dp_prev) + ", dp_next: " + ToString(dp_next));
            tm.Stop(timer_id, "iter=" + ToString(r));
        }

        tm.Print(timer_id, "FullDomainThenDotProduct d=" + ToString(d), ringoa::TimeUnit::MICROSECONDS);
    }
    Logger::InfoLog(LOC, "RingOA_FullDomainThenDotProduct_Bench Completed");
    Logger::ExportLogListAndClear(
        MakeLogBasePath(opts, kLogRingOaPath, "ringoa_fsc_online", opts.party_id),
        opts.enable_log_timestamp);
}

void RingOA_FullDomainThenDotProduct_UINT32_Bench(const osuCrypto::CLP &cmd) {
    if (IsHelpRequested(cmd)) {
        PrintBenchmarkHelp("RingOA FullDomainThenDotProduct UINT32 Benchmark");
        return;
    }
    const auto opts = SelectBenchOptions(cmd);

    Logger::InfoLog(LOC, "RingOA FullDomainThenDotProduct UINT32 Benchmark started (repeat=" + ToString(opts.repeat) + ")");

    for (auto db_bitsize : opts.db_bits_list) {
        ShareConfig      share = ShareConfig::Arith32();
        RingOaConfig     cfg{db_bitsize};
        RingOaParameters params(cfg, share);

        ProtocolContext3P    ctx(share);
        ReplicatedSharing3P &rss = ctx.Rss();
        RingOa               ringoa(params, ctx);

        uint64_t d  = params.GetDbIndexBits();
        uint64_t nu = params.GetParameters().GetTerminateBitsize();

        std::vector<uint32_t> database(1ULL << d);
        for (size_t i = 0; i < database.size(); i++)
            database[i] = i;
        auto db_shares = rss.ShareLocal(database);

        Rep3ShareView32 db_view(db_shares[0]);

        auto keys = ringoa.GenerateKeys(1);

        std::vector<block> uv_prev(1ULL << nu);
        std::vector<block> uv_next(1ULL << nu);
        uint64_t           pr_prev = Mod2N(GlobalRng::Rand<uint64_t>(), d);
        uint64_t           pr_next = Mod2N(GlobalRng::Rand<uint64_t>(), d);

        TimerManager tm;
        int          timer_id = tm.CreateTimer("FullDomainThenDotProductUINT32");

        for (uint64_t r = 0; r < opts.repeat; r++) {
            tm.Start(timer_id);
            auto [dp_prev, dp_next] =
                ringoa.EvaluateFullDomainThenDotProduct(
                    0,    // party_id
                    keys[0].key_from_prev[0],
                    keys[0].key_from_next[0],
                    uv_prev,
                    uv_next,
                    db_view,
                    pr_prev,
                    pr_next);
            Logger::DebugLog(LOC, "dp_prev: " + ToString(dp_prev) + ", dp_next: " + ToString(dp_next));
            tm.Stop(timer_id, "iter=" + ToString(r));
        }

        tm.Print(timer_id, "FullDomainThenDotProductUINT32 d=" + ToString(d), ringoa::TimeUnit::MICROSECONDS);
    }
    Logger::InfoLog(LOC, "RingOA_FullDomainThenDotProduct_UINT32_Bench Completed");
    Logger::ExportLogListAndClear(
        MakeLogBasePath(opts, kLogRingOaPath, "ringoa_fde_then_dp_bench_uint32", opts.party_id),
        opts.enable_log_timestamp);
}

}    // namespace bench_ringoa
