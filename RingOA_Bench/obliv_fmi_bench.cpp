#include "obliv_fmi_bench.h"

#include <cryptoTools/Common/TestCollection.h>

#include "RingOA/obliv_fmi/obliv_fmi.h"
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
    return bench_ringoa::kBenchOblivFmiPath + name + "_d" + ringoa::ToString(d) + "_qs" + ringoa::ToString(qs) + "_k" + ringoa::ToString(k);
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
using ringoa::fmi::OblivFMI;
using ringoa::fmi::OblivFMIConfig;
using ringoa::fmi::OblivFMIFscKeys;
using ringoa::fmi::OblivFMIFscPreprocessData;
using ringoa::fmi::OblivFMIKeys;
using ringoa::fmi::OblivFMIParameters;
using ringoa::fmi::OblivFMIPreprocessData;
using ringoa::proto::ProtocolContext2P;
using ringoa::proto::ProtocolContext3P;
using ringoa::proto::ProtocolIo;
using ringoa::sharing::Rep3Share64;
using ringoa::sharing::Rep3ShareIo;
using ringoa::sharing::Rep3ShareMat64;
using ringoa::sharing::Rep3ShareVec64;
using ringoa::sharing::Rep3ShareView64;
using ringoa::sharing::ShareConfig;
using ringoa::wm::FMIndex;

void OblivFMI_Preprocess_Bench(const osuCrypto::CLP &cmd) {
    if (IsHelpRequested(cmd)) {
        PrintBenchmarkHelp("OblivFMI Benchmark");
        return;
    }
    const auto            opts          = SelectBenchOptions(cmd);
    std::vector<uint64_t> text_bitsizes = opts.db_bits_list;
    std::vector<uint64_t> query_sizes   = SelectQueryBitsize(cmd);

    Logger::InfoLog(LOC, "OblivFMI Preprocess Benchmark started (repeat=" + ToString(opts.repeat) + ")");

    // Define the task for each party
    auto MakeTask = [&](int p) {
        const std::string ptag = "(P" + ToString(p) + ")";
        return [=](osuCrypto::Channel &chl_next, osuCrypto::Channel &chl_prev) {
            for (auto text_bitsize : text_bitsizes) {
                for (auto query_size : query_sizes) {
                    ShareConfig        share = ShareConfig::Arith32();
                    ProtocolContext2P  ctx2p(share);
                    ProtocolContext3P  ctx3p(share);
                    OblivFMIConfig     cfg{text_bitsize, query_size};
                    OblivFMIParameters params(cfg, share);

                    uint64_t d  = params.GetDbIndexBits();
                    uint64_t k  = params.GetShareBitsize();
                    uint64_t qs = params.GetQuerySize();

                    const std::string timer_prep_name = "OblivFMI::Preprocess" + ptag;
                    int32_t           timer_prep      = ctx3p.Timers().CreateTimer(timer_prep_name);

                    OblivFMI ofmi(params, ctx2p, ctx3p);
                    Channels chls(p, chl_prev, chl_next);

                    // Preprocess
                    ringoa::sharing::Rep3PreprocessData rep3_data;
                    OblivFMIPreprocessData              preproc_data;

                    for (uint64_t i = 0; i < opts.warmup; ++i) {
                        ringoa::sharing::PreprocessRep3PrfKeys(chls);
                        ofmi.Preprocess(chls);
                    }

                    for (uint64_t i = 0; i < opts.repeat; ++i) {
                        ctx3p.Timers().Start(timer_prep);
                        ctx3p.StartCommStats(chls);
                        rep3_data    = ringoa::sharing::PreprocessRep3PrfKeys(chls);
                        preproc_data = ofmi.Preprocess(chls);
                        ctx3p.Timers().Stop(timer_prep, "d=" + ToString(d) + ",qs=" + ToString(qs) + ",iter=" + ToString(i));
                        if (i == 0) {
                            Logger::InfoLog(LOC, "(P" + ToString(p) + "),bytes,d=" + ToString(d) + ",qs=" + ToString(qs) + ",sent=" + ToString(ctx3p.StopCommStats(chls)));
                        }
                    }
                    // Print communication cost and timer results
                    ctx3p.Timers().Print(timer_prep, "d=" + ToString(d) + ",qs=" + ToString(qs), ringoa::TimeUnit::MICROSECONDS);

                    // Save preprocess data
                    ringoa::sharing::SaveRep3PreprocessDataToFile(kBenchOblivFmiPath + "rep3_prf_keys_" + ToString(p), rep3_data);
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

    Logger::InfoLog(LOC, "OblivFMI Preprocess Benchmark completed");
    Logger::ExportLogListAndClear(
        MakeLogBasePath(opts, kLogOblivFmiPath, "ofmi_preproc", opts.party_id),
        opts.enable_log_timestamp);
}

void OblivFMI_Online_Bench(const osuCrypto::CLP &cmd) {
    if (IsHelpRequested(cmd)) {
        PrintBenchmarkHelp("OblivFMI Benchmark");
        return;
    }
    const auto            opts          = SelectBenchOptions(cmd);
    std::vector<uint64_t> text_bitsizes = opts.db_bits_list;
    std::vector<uint64_t> query_sizes   = SelectQueryBitsize(cmd);

    Logger::InfoLog(LOC, "OblivFMI Online Benchmark started (repeat=" + ToString(opts.repeat) + ", party=" + ToString(opts.party_id) + ")");

    auto MakeTask = [&](int p) {
        const std::string ptag = "(P" + ToString(p) + ")";
        return [=](osuCrypto::Channel &chl_next, osuCrypto::Channel &chl_prev) {
            for (auto text_bitsize : text_bitsizes) {
                for (auto query_size : query_sizes) {
                    ShareConfig        share = ShareConfig::Arith32();
                    ProtocolContext2P  ctx2p(share);
                    ProtocolContext3P  ctx3p(share);
                    OblivFMIConfig     cfg{text_bitsize, query_size};
                    OblivFMIParameters params(cfg, share);

                    uint64_t d  = params.GetDbIndexBits();
                    uint64_t k  = params.GetShareBitsize();
                    uint64_t qs = params.GetQuerySize();
                    uint64_t nu = params.GetOblivRankParameters().GetParameters().GetParameters().GetTerminateBitsize();

                    int32_t timer_setup = ctx3p.Timers().CreateTimer("OblivFMI::OnlineSetUp" + ptag);
                    int32_t timer_eval  = ctx3p.Timers().CreateTimer("OblivFMI::Eval" + ptag);

                    ctx3p.Timers().Start(timer_setup);

                    OblivFMI ofmi(params, ctx2p, ctx3p);
                    Channels chls(p, chl_prev, chl_next);

                    std::vector<ringoa::block> uv_prev(1ULL << nu), uv_next(1ULL << nu);

                    Rep3ShareMat64 db_sh;
                    Rep3ShareMat64 query_sh;

                    Rep3ShareIo::LoadShare(MakeDatasetPath("fmi_db", d, qs, k) + "_" + ToString(p), db_sh);
                    Rep3ShareIo::LoadShare(MakeDatasetPath("fmi_query", d, qs, k) + "_" + ToString(p), query_sh);

                    // Load preprocess data
                    ringoa::sharing::Rep3PreprocessData loaded_rep3_data;
                    OblivFMIPreprocessData              preproc_data;
                    ringoa::sharing::LoadRep3PreprocessDataFromFile(kBenchOblivFmiPath + "rep3_prf_keys_" + ToString(p), loaded_rep3_data);
                    ProtocolIo::LoadFromFile(MakePath("preproc", d, qs, k) + "_" + ToString(p), preproc_data, params);

                    ctx3p.Rss().SetPrfKeys(loaded_rep3_data.prf_key_self, loaded_rep3_data.prf_key_next);
                    OblivFMIKeys key = std::move(preproc_data).ExtractKeys();

                    // Consume preprocess data
                    ofmi.ConsumePreprocessData(preproc_data);

                    ctx3p.Timers().Stop(timer_setup, "d=" + ToString(d) + ",qs=" + ToString(qs) + ",iter=0");
                    ctx3p.Timers().Print(timer_setup, "d=" + ToString(d) + ",qs=" + ToString(qs), ringoa::TimeUnit::MICROSECONDS);

                    for (uint64_t i = 0; i < opts.repeat; ++i) {
                        ctx3p.Timers().Start(timer_eval);
                        Rep3ShareVec64 result_sh(qs);
                        ctx3p.StartCommStats(chls);
                        ofmi.EvaluateLPMPair(chls, key, uv_prev, uv_next, db_sh, query_sh, result_sh);
                        ctx3p.Timers().Stop(timer_eval, "d=" + ToString(d) + ",qs=" + ToString(qs) + ",iter=" + ToString(i));
                        if (i == 0) {
                            Logger::InfoLog(LOC, "(P" + ToString(p) + "),bytes,d=" + ToString(d) + ",qs=" + ToString(qs) + ",sent=" + ToString(ctx3p.StopCommStats(chls)));
                        }
                        ctx3p.AssWithPrev().ResetTripleIndex();
                        ctx3p.AssWithNext().ResetTripleIndex();
                        ctx3p.Timers().Print(ctx3p.Timers().GetOrCreateTimer("RingOA::LocalEval"), "d=" + ToString(d) + ",qs=" + ToString(qs) + ",iter=" + ToString(i), ringoa::TimeUnit::MICROSECONDS);
                        ctx3p.Timers().ResetByName("RingOA::LocalEval");
                    }
                    ctx3p.Timers().Print(timer_eval, "d=" + ToString(d) + ",qs=" + ToString(qs), ringoa::TimeUnit::MICROSECONDS);
                }
            }
        };
    };

    ThreePartyNetworkManager net_mgr;
    net_mgr.AutoConfigure(opts.party_id, MakeTask(0), MakeTask(1), MakeTask(2));
    net_mgr.WaitForCompletion();

    Logger::InfoLog(LOC, "OblivFMI Online Benchmark completed");
    Logger::ExportLogListAndClear(
        MakeLogBasePath(opts, kLogOblivFmiPath, "ofmi_online", opts.party_id),
        opts.enable_log_timestamp);
}

void OblivFMI_Fsc_Preprocess_Bench(const osuCrypto::CLP &cmd) {
    if (IsHelpRequested(cmd)) {
        PrintBenchmarkHelp("OblivFMI (FSC) Benchmark");
        return;
    }
    const auto            opts          = SelectBenchOptions(cmd);
    std::vector<uint64_t> text_bitsizes = opts.db_bits_list;
    std::vector<uint64_t> query_sizes   = SelectQueryBitsize(cmd);

    Logger::InfoLog(LOC, "OblivFMI (FSC) Preprocess Benchmark started (repeat=" + ToString(opts.repeat) + ")");

    // Define the task for each party
    auto MakeTask = [&](int p) {
        const std::string ptag = "(P" + ToString(p) + ")";
        return [=](osuCrypto::Channel &chl_next, osuCrypto::Channel &chl_prev) {
            for (auto text_bitsize : text_bitsizes) {
                for (auto query_size : query_sizes) {
                    ShareConfig        share = ShareConfig::Arith32();
                    ProtocolContext2P  ctx2p(share);
                    ProtocolContext3P  ctx3p(share);
                    OblivFMIConfig     cfg{text_bitsize, query_size};
                    OblivFMIParameters params(cfg, share);

                    uint64_t d  = params.GetDbIndexBits();
                    uint64_t k  = params.GetShareBitsize();
                    uint64_t qs = params.GetQuerySize();

                    const std::string timer_prep_name = "OblivFMIFSC::Preprocess" + ptag;
                    int32_t           timer_prep      = ctx3p.Timers().CreateTimer(timer_prep_name);

                    OblivFMI ofmi(params, ctx2p, ctx3p);
                    Channels chls(p, chl_prev, chl_next);

                    FileIo file_io;
                    // Load v_sign
                    bool v_sign;
                    file_io.ReadBinary(MakeDatasetPath("fmi_v_sign", d, qs, k) + "_" + ToString(p), v_sign);

                    // Preprocess
                    ringoa::sharing::Rep3PreprocessData rep3_data;
                    OblivFMIFscPreprocessData           preproc_data;

                    for (uint64_t i = 0; i < opts.warmup; ++i) {
                        ringoa::sharing::PreprocessRep3PrfKeys(chls);
                        ofmi.PreprocessFsc(chls, v_sign);
                    }

                    for (uint64_t i = 0; i < opts.repeat; ++i) {
                        ctx3p.Timers().Start(timer_prep);
                        ctx3p.StartCommStats(chls);
                        rep3_data    = ringoa::sharing::PreprocessRep3PrfKeys(chls);
                        preproc_data = ofmi.PreprocessFsc(chls, v_sign);
                        ctx3p.Timers().Stop(timer_prep, "d=" + ToString(d) + ",qs=" + ToString(qs) + ",iter=" + ToString(i));
                        if (i == 0) {
                            Logger::InfoLog(LOC, "(P" + ToString(p) + "),bytes,d=" + ToString(d) + ",qs=" + ToString(qs) + ",sent=" + ToString(ctx3p.StopCommStats(chls)));
                        }
                    }
                    // Print communication cost and timer results
                    ctx3p.Timers().Print(timer_prep, "d=" + ToString(d) + ",qs=" + ToString(qs), ringoa::TimeUnit::MICROSECONDS);

                    // Save preprocess data
                    ringoa::sharing::SaveRep3PreprocessDataToFile(kBenchOblivFmiPath + "rep3_prf_keys_" + ToString(p), rep3_data);
                    ProtocolIo::SaveToFile(MakePath("preproc_fsc", d, qs, k) + "_" + ToString(p), preproc_data);
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

    Logger::InfoLog(LOC, "OblivFMI (FSC) Preprocess Benchmark completed");
    Logger::ExportLogListAndClear(
        MakeLogBasePath(opts, kLogOblivFmiPath, "ofmi_fsc_preproc", opts.party_id),
        opts.enable_log_timestamp);
}

void OblivFMI_Fsc_Online_Bench(const osuCrypto::CLP &cmd) {
    if (IsHelpRequested(cmd)) {
        PrintBenchmarkHelp("OblivFMI (FSC) Benchmark");
        return;
    }
    const auto            opts          = SelectBenchOptions(cmd);
    std::vector<uint64_t> text_bitsizes = opts.db_bits_list;
    std::vector<uint64_t> query_sizes   = SelectQueryBitsize(cmd);

    Logger::InfoLog(LOC, "OblivFMI (FSC) Online Benchmark started (repeat=" + ToString(opts.repeat) + ", party=" + ToString(opts.party_id) + ")");

    auto MakeTask = [&](int p) {
        const std::string ptag = "(P" + ToString(p) + ")";
        return [=](osuCrypto::Channel &chl_next, osuCrypto::Channel &chl_prev) {
            for (auto text_bitsize : text_bitsizes) {
                for (auto query_size : query_sizes) {
                    ShareConfig        share = ShareConfig::Arith32();
                    ProtocolContext2P  ctx2p(share);
                    ProtocolContext3P  ctx3p(share);
                    OblivFMIConfig     cfg{text_bitsize, query_size};
                    OblivFMIParameters params(cfg, share);

                    uint64_t d  = params.GetDbIndexBits();
                    uint64_t k  = params.GetShareBitsize();
                    uint64_t qs = params.GetQuerySize();
                    uint64_t nu = params.GetOblivRankParameters().GetParameters().GetParameters().GetTerminateBitsize();

                    int32_t timer_setup = ctx3p.Timers().CreateTimer("OblivFMIFSC::OnlineSetUp" + ptag);
                    int32_t timer_eval  = ctx3p.Timers().CreateTimer("OblivFMIFSC::Eval" + ptag);

                    ctx3p.Timers().Start(timer_setup);

                    OblivFMI ofmi(params, ctx2p, ctx3p);
                    Channels chls(p, chl_prev, chl_next);

                    std::vector<ringoa::block> uv_prev(1ULL << nu), uv_next(1ULL << nu);

                    Rep3ShareMat64 db_sh;
                    Rep3ShareVec64 aux_sh;
                    Rep3ShareMat64 query_sh;

                    Rep3ShareIo::LoadShare(MakeDatasetPath("fmi_db_fsc", d, qs, k) + "_" + ToString(p), db_sh);
                    Rep3ShareIo::LoadShare(MakeDatasetPath("fmi_aux_fsc", d, qs, k) + "_" + ToString(p), aux_sh);
                    Rep3ShareIo::LoadShare(MakeDatasetPath("fmi_query", d, qs, k) + "_" + ToString(p), query_sh);

                    // Load preprocess data
                    ringoa::sharing::Rep3PreprocessData loaded_rep3_data;
                    OblivFMIFscPreprocessData           preproc_data;
                    ringoa::sharing::LoadRep3PreprocessDataFromFile(kBenchOblivFmiPath + "rep3_prf_keys_" + ToString(p), loaded_rep3_data);
                    ProtocolIo::LoadFromFile(MakePath("preproc_fsc", d, qs, k) + "_" + ToString(p), preproc_data, params);

                    ctx3p.Rss().SetPrfKeys(loaded_rep3_data.prf_key_self, loaded_rep3_data.prf_key_next);
                    OblivFMIFscKeys key = std::move(preproc_data).ExtractKeys();

                    ctx3p.Timers().Stop(timer_setup, "d=" + ToString(d) + ",qs=" + ToString(qs) + ",iter=0");
                    ctx3p.Timers().Print(timer_setup, "d=" + ToString(d) + ",qs=" + ToString(qs), ringoa::TimeUnit::MICROSECONDS);

                    for (uint64_t i = 0; i < opts.repeat; ++i) {
                        ctx3p.Timers().Start(timer_eval);
                        Rep3ShareVec64 result_sh(qs);
                        ctx3p.StartCommStats(chls);
                        ofmi.EvaluateLPMFscPair(chls, key, uv_prev, uv_next, db_sh, Rep3ShareView64(aux_sh), query_sh, result_sh);
                        ctx3p.Timers().Stop(timer_eval, "d=" + ToString(d) + ",qs=" + ToString(qs) + ",iter=" + ToString(i));
                        if (i == 0) {
                            Logger::InfoLog(LOC, "(P" + ToString(p) + "),bytes,d=" + ToString(d) + ",qs=" + ToString(qs) + ",sent=" + ToString(ctx3p.StopCommStats(chls)));
                        }
                        ctx3p.Timers().Print(ctx3p.Timers().GetOrCreateTimer("RingOAFSC::LocalEval"), "d=" + ToString(d) + ",qs=" + ToString(qs) + ",iter=" + ToString(i), ringoa::TimeUnit::MICROSECONDS);
                        ctx3p.Timers().ResetByName("RingOAFSC::LocalEval");
                    }
                    ctx3p.Timers().Print(timer_eval, "d=" + ToString(d) + ",qs=" + ToString(qs), ringoa::TimeUnit::MICROSECONDS);
                }
            }
        };
    };

    ThreePartyNetworkManager net_mgr;
    net_mgr.AutoConfigure(opts.party_id, MakeTask(0), MakeTask(1), MakeTask(2));
    net_mgr.WaitForCompletion();

    Logger::InfoLog(LOC, "OblivFMI (FSC) Online Benchmark completed");
    Logger::ExportLogListAndClear(
        MakeLogBasePath(opts, kLogOblivFmiPath, "ofmi_fsc_online", opts.party_id),
        opts.enable_log_timestamp);
}

}    // namespace bench_ringoa
