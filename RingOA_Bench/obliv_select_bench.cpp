#include "obliv_select_bench.h"

#include <cryptoTools/Common/TestCollection.h>

#include "RingOA/protocol/obliv_select.h"
#include "RingOA/protocol/protocol_io.h"
#include "RingOA/sharing/rep3_preprocess.h"
#include "RingOA/sharing/rep3_share_io.h"
#include "RingOA/sharing/rep3_sharing_binary.h"
#include "RingOA/sharing/sharing_2p_binary.h"
#include "RingOA/utils/logger.h"
#include "RingOA/utils/network.h"
#include "RingOA/utils/rng.h"
#include "RingOA/utils/timer.h"
#include "RingOA/utils/utils.h"
#include "bench_common.h"

namespace {

inline std::string MakePath(const std::string &name, uint64_t d, uint64_t k) {
    return bench_ringoa::kBenchOsPath + name + "_d" + ringoa::ToString(d) + "_k" + ringoa::ToString(k);
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
using ringoa::fss::dpf::DpfEvaluator;
using ringoa::fss::dpf::DpfKey;
using ringoa::fss::dpf::DpfKeyGenerator;
using ringoa::fss::dpf::DpfParameters;
using ringoa::proto::OblivSelect;
using ringoa::proto::OblivSelectConfig;
using ringoa::proto::OblivSelectKeys;
using ringoa::proto::OblivSelectParameters;
using ringoa::proto::OblivSelectPreprocessData;
using ringoa::proto::ProtocolContext3PBinary;
using ringoa::proto::ProtocolIo;
using ringoa::sharing::BinaryReplicatedSharing3P;
using ringoa::sharing::BinarySharing2P;
using ringoa::sharing::Rep3Share64, ringoa::sharing::Rep3ShareBlock;
using ringoa::sharing::Rep3ShareIo;
using ringoa::sharing::Rep3ShareVec64, ringoa::sharing::Rep3ShareVecBlock;
using ringoa::sharing::Rep3ShareView64, ringoa::sharing::Rep3ShareViewBlock;
using ringoa::sharing::ShareConfig;

void OblivSelect_ShiftedAdditive_Preprocess_Bench(const osuCrypto::CLP &cmd) {
    if (IsHelpRequested(cmd)) {
        PrintBenchmarkHelp("OblivSelect ShiftedAdditive Preprocess Benchmark");
        return;
    }
    const auto opts = SelectBenchOptions(cmd);

    Logger::InfoLog(LOC, "OblivSelect ShiftedAdditive Preprocess Benchmark started (repeat=" + ToString(opts.repeat) + ")");

    // Define the task for each party
    auto MakeTask = [&](int p) {
        const std::string ptag = "(P" + ToString(p) + ")";
        return [=](osuCrypto::Channel &chl_next, osuCrypto::Channel &chl_prev) {
            for (auto db_bitsize : opts.db_bits_list) {
                ShareConfig             share = ShareConfig::Arith32();
                ProtocolContext3PBinary ctx(share);
                OblivSelectConfig       cfg{db_bitsize};
                OblivSelectParameters   params(cfg, share);

                uint64_t d = params.GetDbIndexBits();
                uint64_t k = params.GetShareBitsize();

                const std::string timer_prep_name = "OblivSelect::SA::Preprocess" + ptag;
                int32_t           timer_prep      = ctx.Timers().CreateTimer(timer_prep_name);

                OblivSelect os(params, ctx);
                Channels    chls(p, chl_prev, chl_next);

                // Preprocess
                ringoa::sharing::Rep3PreprocessData rep3_data;
                OblivSelectPreprocessData           preproc_data;

                for (uint64_t i = 0; i < opts.warmup; ++i) {
                    ringoa::sharing::PreprocessRep3PrfKeys(chls);
                    os.Preprocess(chls, 1);
                }

                for (uint64_t i = 0; i < opts.repeat; ++i) {
                    ctx.Timers().Start(timer_prep);
                    ctx.StartCommStats(chls);
                    rep3_data    = ringoa::sharing::PreprocessRep3PrfKeys(chls);
                    preproc_data = os.Preprocess(chls, 1);
                    ctx.Timers().Stop(timer_prep, "d=" + ToString(d) + ",iter=" + ToString(i));
                    if (i == 0) {
                        Logger::InfoLog(LOC, "(P" + ToString(p) + "),bytes,d=" + ToString(d) + ",sent=" + ToString(ctx.StopCommStats(chls)));
                    }
                }
                // Print communication cost and timer results
                ctx.Timers().Print(timer_prep, "d=" + ToString(d), ringoa::TimeUnit::MICROSECONDS);

                // Save preprocess data
                ringoa::sharing::SaveRep3PreprocessDataToFile(kBenchOsPath + "rep3_prf_keys_" + ToString(p), rep3_data);
                ProtocolIo::SaveToFile(MakePath("preproc_sa", d, k) + "_" + ToString(p), preproc_data);
            }
        };
    };

    auto task_p0 = MakeTask(0);
    auto task_p1 = MakeTask(1);
    auto task_p2 = MakeTask(2);

    ThreePartyNetworkManager net_mgr;
    net_mgr.AutoConfigure(opts.party_id, task_p0, task_p1, task_p2);
    net_mgr.WaitForCompletion();

    Logger::InfoLog(LOC, "OblivSelect ShiftedAdditive Preprocess Benchmark completed");
    Logger::ExportLogListAndClear(
        MakeLogBasePath(opts, kLogOsPath, "os_sa_preproc", opts.party_id),
        opts.enable_log_timestamp);
}

void OblivSelect_ShiftedAdditive_Online_Bench(const osuCrypto::CLP &cmd) {
    if (IsHelpRequested(cmd)) {
        PrintBenchmarkHelp("OblivSelect ShiftedAdditive Preprocess Benchmark");
        return;
    }
    const auto opts = SelectBenchOptions(cmd);

    Logger::InfoLog(LOC, "OblivSelect ShiftedAdditive Online Benchmark started (repeat=" + ToString(opts.repeat) +
                             ", party=" + ToString(opts.party_id) + ")");

    auto MakeTask = [&](int p) {
        const std::string ptag = "(P" + ToString(p) + ")";
        return [=](osuCrypto::Channel &chl_next, osuCrypto::Channel &chl_prev) {
            for (auto db_bitsize : opts.db_bits_list) {
                ShareConfig             share = ShareConfig::Arith32();
                ProtocolContext3PBinary ctx(share);
                OblivSelectConfig       cfg{db_bitsize};
                OblivSelectParameters   params(cfg, share);

                uint64_t d  = params.GetDbIndexBits();
                uint64_t k  = params.GetShareBitsize();
                uint64_t nu = params.GetParameters().GetTerminateBitsize();

                const std::string timer_setup_name = "OblivSelect::SA::OnlineSetUp" + ptag;
                const std::string timer_eval_name  = "OblivSelect::SA::Eval" + ptag;
                int32_t           timer_setup      = ctx.Timers().CreateTimer(timer_setup_name);
                int32_t           timer_eval       = ctx.Timers().CreateTimer(timer_eval_name);

                // --- OnlineSetUp timing ---
                ctx.Timers().Start(timer_setup);

                OblivSelect os(params, ctx);
                Channels    chls(p, chl_prev, chl_next);

                Rep3ShareVec64 database_sh;
                Rep3Share64    index_sh;

                Rep3ShareIo::LoadShare(MakeDatasetPath("oa_db_bin", d, k) + "_" + ToString(p), database_sh);
                Rep3ShareIo::LoadShare(MakeDatasetPath("oa_idx_bin", d, k) + "_" + ToString(p), index_sh);

                // Load preprocess data
                ringoa::sharing::Rep3PreprocessData loaded_rep3_data;
                OblivSelectPreprocessData           preproc_data;
                ringoa::sharing::LoadRep3PreprocessDataFromFile(kBenchOsPath + "rep3_prf_keys_" + ToString(p), loaded_rep3_data);
                ProtocolIo::LoadFromFile(MakePath("preproc_sa", d, k) + "_" + ToString(p), preproc_data, params);

                ctx.Brss().SetPrfKeys(loaded_rep3_data.prf_key_self, loaded_rep3_data.prf_key_next);
                OblivSelectKeys &key = preproc_data.keys;

                std::vector<ringoa::block> uv_prev(1ULL << nu), uv_next(1ULL << nu);

                ctx.Timers().Stop(timer_setup, "d=" + ToString(d) + ",iter=0");

                Rep3Share64 result_sh;
                for (uint64_t i = 0; i < opts.warmup; ++i) {
                    os.ObliviousAccess(chls, key.GetView(0), uv_prev, uv_next,
                                       Rep3ShareView64(database_sh), index_sh, result_sh);
                }
                ctx.Timers().ResetByName("OblivSelect::FullDomainEval");
                ctx.Timers().ResetByName("OblivSelect::DotProduct");

                for (uint64_t i = 0; i < opts.repeat; ++i) {
                    ctx.Timers().Start(timer_eval);
                    ctx.StartCommStats(chls);
                    os.ObliviousAccess(chls, key.GetView(0), uv_prev, uv_next,
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

    Logger::InfoLog(LOC, "OblivSelect ShiftedAdditive Online Benchmark completed");
    Logger::ExportLogListAndClear(
        MakeLogBasePath(opts, kLogOsPath, "os_sa_online", opts.party_id),
        opts.enable_log_timestamp);
}

void OblivSelect_SingleBitMask_Preprocess_Bench(const osuCrypto::CLP &cmd) {
    if (IsHelpRequested(cmd)) {
        PrintBenchmarkHelp("OblivSelect SingleBitMask Preprocess Benchmark");
        return;
    }
    const auto opts = SelectBenchOptions(cmd);

    Logger::InfoLog(LOC, "OblivSelect SingleBitMask Preprocess Benchmark started (repeat=" + ToString(opts.repeat) + ")");

    // Define the task for each party
    auto MakeTask = [&](int p) {
        const std::string ptag = "(P" + ToString(p) + ")";
        return [=](osuCrypto::Channel &chl_next, osuCrypto::Channel &chl_prev) {
            for (auto db_bitsize : opts.db_bits_list) {
                ShareConfig             share = ShareConfig::Arith32();
                ProtocolContext3PBinary ctx(share);
                OblivSelectConfig       cfg{db_bitsize};
                cfg.output_mode = ringoa::fss::OutputType::kSingleBitMask;
                OblivSelectParameters params(cfg, share);

                uint64_t d = params.GetDbIndexBits();
                uint64_t k = params.GetShareBitsize();

                const std::string timer_prep_name = "OblivSelect::SBM::Preprocess" + ptag;
                int32_t           timer_prep      = ctx.Timers().CreateTimer(timer_prep_name);

                OblivSelect os(params, ctx);
                Channels    chls(p, chl_prev, chl_next);

                // Preprocess
                ringoa::sharing::Rep3PreprocessData rep3_data;
                OblivSelectPreprocessData           preproc_data;

                for (uint64_t i = 0; i < opts.warmup; ++i) {
                    ringoa::sharing::PreprocessRep3PrfKeys(chls);
                    os.Preprocess(chls, 1);
                }

                for (uint64_t i = 0; i < opts.repeat; ++i) {
                    ctx.Timers().Start(timer_prep);
                    ctx.StartCommStats(chls);
                    rep3_data    = ringoa::sharing::PreprocessRep3PrfKeys(chls);
                    preproc_data = os.Preprocess(chls, 1);
                    ctx.Timers().Stop(timer_prep, "d=" + ToString(d) + ",iter=" + ToString(i));
                    if (i == 0) {
                        Logger::InfoLog(LOC, "(P" + ToString(p) + "),bytes,d=" + ToString(d) + ",sent=" + ToString(ctx.StopCommStats(chls)));
                    }
                }
                // Print communication cost and timer results
                ctx.Timers().Print(timer_prep, "d=" + ToString(d), ringoa::TimeUnit::MICROSECONDS);

                // Save preprocess data
                ringoa::sharing::SaveRep3PreprocessDataToFile(kBenchOsPath + "rep3_prf_keys_" + ToString(p), rep3_data);
                ProtocolIo::SaveToFile(MakePath("preproc_sbm", d, k) + "_" + ToString(p), preproc_data);
            }
        };
    };

    auto task_p0 = MakeTask(0);
    auto task_p1 = MakeTask(1);
    auto task_p2 = MakeTask(2);

    ThreePartyNetworkManager net_mgr;
    net_mgr.AutoConfigure(opts.party_id, task_p0, task_p1, task_p2);
    net_mgr.WaitForCompletion();

    Logger::InfoLog(LOC, "OblivSelect SingleBitMask Preprocess Benchmark completed");
    Logger::ExportLogListAndClear(
        MakeLogBasePath(opts, kLogOsPath, "os_sbm_preproc", opts.party_id),
        opts.enable_log_timestamp);
}

void OblivSelect_SingleBitMask_Online_Bench(const osuCrypto::CLP &cmd) {
    if (IsHelpRequested(cmd)) {
        PrintBenchmarkHelp("OblivSelect SingleBitMask Online Benchmark");
        return;
    }
    const auto opts = SelectBenchOptions(cmd);

    Logger::InfoLog(LOC, "OblivSelect SingleBitMask Online Benchmark started (repeat=" + ToString(opts.repeat) + ", party=" + ToString(opts.party_id) + ")");

    auto MakeTask = [&](int p) {
        const std::string ptag = "(P" + ToString(p) + ")";
        return [=](osuCrypto::Channel &chl_next, osuCrypto::Channel &chl_prev) {
            for (auto db_bitsize : opts.db_bits_list) {
                ShareConfig             share = ShareConfig::Arith32();
                ProtocolContext3PBinary ctx(share);
                OblivSelectConfig       cfg{db_bitsize};
                cfg.output_mode = ringoa::fss::OutputType::kSingleBitMask;
                OblivSelectParameters params(cfg, share);

                uint64_t d = params.GetDbIndexBits();
                uint64_t k = params.GetShareBitsize();

                const std::string timer_setup_name = "OblivSelect::SBM::OnlineSetUp" + ptag;
                const std::string timer_eval_name  = "OblivSelect::SBM::Eval" + ptag;
                int32_t           timer_setup      = ctx.Timers().CreateTimer(timer_setup_name);
                int32_t           timer_eval       = ctx.Timers().CreateTimer(timer_eval_name);

                ctx.Timers().Start(timer_setup);

                OblivSelect os(params, ctx);
                Channels    chls(p, chl_prev, chl_next);

                Rep3ShareVecBlock database_sh;
                Rep3Share64       index_sh;

                Rep3ShareIo::LoadShare(MakePath("oa_db_block", d, k) + "_" + ToString(p), database_sh);
                Rep3ShareIo::LoadShare(MakePath("oa_idx_bin", d, k) + "_" + ToString(p), index_sh);

                // Load preprocess data
                ringoa::sharing::Rep3PreprocessData loaded_rep3_data;
                OblivSelectPreprocessData           preproc_data;
                ringoa::sharing::LoadRep3PreprocessDataFromFile(kBenchOsPath + "rep3_prf_keys_" + ToString(p), loaded_rep3_data);
                ProtocolIo::LoadFromFile(MakePath("preproc_sbm", d, k) + "_" + ToString(p), preproc_data, params);

                ctx.Brss().SetPrfKeys(loaded_rep3_data.prf_key_self, loaded_rep3_data.prf_key_next);
                OblivSelectKeys &key = preproc_data.keys;

                ctx.Timers().Stop(timer_setup, "d=" + ToString(d) + ",iter=0");
                ctx.Timers().Print(timer_setup, "d=" + ToString(d), ringoa::TimeUnit::MICROSECONDS);

                Rep3ShareBlock result_sh;
                for (uint64_t i = 0; i < opts.warmup; ++i) {
                    os.ObliviousAccess(chls, key.GetView(0), Rep3ShareViewBlock(database_sh), index_sh, result_sh);
                }
                ctx.Timers().ResetByName("OblivSelect::FullDomainEval");
                ctx.Timers().ResetByName("OblivSelect::DotProduct");

                for (uint64_t i = 0; i < opts.repeat; ++i) {
                    ctx.Timers().Start(timer_eval);
                    ctx.StartCommStats(chls);
                    os.ObliviousAccess(chls, key.GetView(0), Rep3ShareViewBlock(database_sh), index_sh, result_sh);
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

    Logger::InfoLog(LOC, "OblivSelect SingleBitMask Online Benchmark completed");
    Logger::ExportLogListAndClear(
        MakeLogBasePath(opts, kLogOsPath, "os_sbm_online", opts.party_id),
        opts.enable_log_timestamp);
}

void OblivSelect_ComputeDotProductBlockSIMD_Bench(const osuCrypto::CLP &cmd) {
    if (IsHelpRequested(cmd)) {
        PrintBenchmarkHelp("OblivSelect ComputeDotProductBlockSIMD Online Benchmark");
        return;
    }
    const auto opts = SelectBenchOptions(cmd);

    Logger::InfoLog(LOC, "OblivSelect ComputeDotProductBlockSIMD Benchmark started (repeat=" + ToString(opts.repeat) + ")");

    for (auto db_bitsize : opts.db_bits_list) {
        ShareConfig             share = ShareConfig::Arith32();
        ProtocolContext3PBinary ctx(share);
        OblivSelectConfig       cfg{db_bitsize};
        OblivSelectParameters   params(cfg, share);

        uint64_t d = params.GetDbIndexBits();

        DpfKeyGenerator gen(params.GetParameters());
        OblivSelect     os(params, ctx);

        uint64_t alpha = Mod2N(GlobalRng::Rand<uint64_t>(), d);
        uint64_t beta  = 1;

        Rep3ShareVecBlock database_sh(1U << d);
        for (size_t i = 0; i < database_sh.Size(); ++i) {
            database_sh[0][i] = ringoa::MakeBlock(0, i);
            database_sh[1][i] = ringoa::MakeBlock(0, i);
        }
        uint64_t pr_prev = Mod2N(GlobalRng::Rand<uint64_t>(), d);
        uint64_t pr_next = Mod2N(GlobalRng::Rand<uint64_t>(), d);

        // Generate keys (outside per-iteration timing)
        std::pair<DpfKey, DpfKey> keys_next = gen.GenerateKeys(alpha, beta);
        std::pair<DpfKey, DpfKey> keys_prev = gen.GenerateKeys(alpha, beta);

        const std::string timer_name = "OblivSelect::ComputeDotProductBlockSIMD";
        int32_t           timer_id   = ctx.Timers().CreateTimer(timer_name);

        for (uint64_t i = 0; i < opts.repeat; ++i) {
            ctx.Timers().Start(timer_id);
            block result_prev = os.ComputeDotProductBlockSIMD(keys_prev.first, database_sh[0], pr_prev);
            block result_next = os.ComputeDotProductBlockSIMD(keys_next.second, database_sh[1], pr_next);
            ctx.Timers().Stop(timer_id, "d=" + ToString(d) + ",iter=" + ToString(i));
            Logger::DebugLog(LOC, "Result Prev: " + Format(result_prev) + ", Result Next: " + Format(result_next));
        }

        const std::string summary_msg = "d=" + ToString(d);
        ctx.Timers().Print(timer_id, summary_msg, ringoa::TimeUnit::MICROSECONDS);
    }

    Logger::InfoLog(LOC, "OblivSelect ComputeDotProductBlockSIMD Benchmark completed");
    Logger::ExportLogListAndClear(
        MakeLogBasePath(opts, kLogOsPath, "cdpb_simd", opts.party_id),
        opts.enable_log_timestamp);
}

void OblivSelect_EvaluateFullDomainThenDotProduct_Bench(const osuCrypto::CLP &cmd) {
    if (IsHelpRequested(cmd)) {
        PrintBenchmarkHelp("OblivSelect EvaluateFullDomainThenDotProduct Online Benchmark");
        return;
    }
    const auto opts = SelectBenchOptions(cmd);

    Logger::InfoLog(LOC, "OblivSelect EvaluateFullDomainThenDotProduct Benchmark started (repeat=" + ToString(opts.repeat) + ")");

    for (auto db_bitsize : opts.db_bits_list) {
        ShareConfig             share = ShareConfig::Arith32();
        ProtocolContext3PBinary ctx(share);
        OblivSelectConfig       cfg{db_bitsize};
        OblivSelectParameters   params(cfg, share);

        uint64_t d  = params.GetDbIndexBits();
        uint64_t nu = params.GetParameters().GetTerminateBitsize();

        DpfKeyGenerator gen(params.GetParameters());
        OblivSelect     os(params, ctx);

        uint64_t alpha = Mod2N(GlobalRng::Rand<uint64_t>(), d);
        uint64_t beta  = 1;

        std::vector<ringoa::block> uv_prev(1ULL << nu), uv_next(1ULL << nu);
        Rep3ShareVec64             database_sh(1U << d);
        std::vector<uint64_t>      shuf_db_0(1U << d), shuf_db_1(1U << d);

        for (size_t i = 0; i < database_sh.Size(); ++i) {
            database_sh[0][i] = i;
            database_sh[1][i] = i;
        }

        uint64_t                  pr_prev   = Mod2N(GlobalRng::Rand<uint64_t>(), d);
        uint64_t                  pr_next   = Mod2N(GlobalRng::Rand<uint64_t>(), d);
        std::pair<DpfKey, DpfKey> keys_next = gen.GenerateKeys(alpha, beta);
        std::pair<DpfKey, DpfKey> keys_prev = gen.GenerateKeys(alpha, beta);

        const std::string timer_name = "OblivSelect::EvaluateFullDomainThenDotProduct";
        int32_t           timer_id   = ctx.Timers().CreateTimer(timer_name);

        for (uint64_t i = 0; i < opts.repeat; ++i) {
            ctx.Timers().Start(timer_id);
            os.EvaluateFullDomainThenDotProduct(0, keys_prev.first, keys_next.second, uv_prev, uv_next,
                                                Rep3ShareView64(database_sh), pr_prev, pr_next);
            ctx.Timers().Stop(timer_id, "d=" + ToString(d) + " nu=" + ToString(nu) + ",iter=" + ToString(i));
        }

        const std::string summary_msg = "d=" + ToString(d) + " nu=" + ToString(nu);
        ctx.Timers().Print(timer_id, summary_msg, ringoa::TimeUnit::MICROSECONDS);
    }

    Logger::InfoLog(LOC, "OblivSelect EvaluateFullDomainThenDotProduct Benchmark completed");
    Logger::ExportLogListAndClear(
        MakeLogBasePath(opts, kLogOsPath, "fde_dp", opts.party_id),
        opts.enable_log_timestamp);
}

}    // namespace bench_ringoa
