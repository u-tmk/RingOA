#include "gen_dataset.h"

#include <charconv>
#include <cryptoTools/Common/TestCollection.h>
#include <random>

#include "RingOA/obliv_fmi/obliv_fmi.h"
#include "RingOA/obliv_range/obliv_range.h"
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
#include "RingOA/wm/plain_wm.h"
#include "bench_common.h"
#include "seq_io.h"

namespace {

const uint64_t kFixedSeed = 6;

std::string GenerateRandomString(size_t length, const std::string &charset = "ATGC") {
    if (charset.empty() || length == 0)
        return "";
    static thread_local std::mt19937_64   rng(kFixedSeed);
    std::uniform_int_distribution<size_t> dist(0, charset.size() - 1);
    std::string                           result(length, '\0');
    for (size_t i = 0; i < length; ++i) {
        result[i] = charset[dist(rng)];
    }
    return result;
}

void SetRandomVAFValues(std::vector<uint64_t> &values) {
    static thread_local std::mt19937_64     rng(kFixedSeed);
    std::uniform_int_distribution<uint64_t> dist(0, 100);
    for (size_t i = 0; i < values.size(); ++i) {
        values[i] = dist(rng);
    }
}

void LoadVAFValues(const std::string &file_path, std::vector<uint64_t> &values) {
    std::ifstream ifs(file_path);
    if (!ifs.is_open()) {
        throw std::runtime_error("Failed to open file: " + file_path);
    }

    const size_t capacity = values.size();    // fixed capacity
    size_t       idx      = 0;

    std::string line;
    line.reserve(32);

    while (idx < capacity && std::getline(ifs, line)) {
        uint64_t v     = 0;
        auto     begin = line.data();
        auto     end   = line.data() + line.size();

        auto result = std::from_chars(begin, end, v);
        if (result.ec != std::errc()) {
            throw std::runtime_error("Invalid integer in line: " + line);
        }

        values[idx++] = v;
    }

    // Fill unused tail with 0
    while (idx < capacity) {
        values[idx++] = 0;
    }
}

}    // namespace

namespace bench_ringoa {

using ringoa::block;
using ringoa::FileIo;
using ringoa::GlobalRng;
using ringoa::Logger;
using ringoa::Mod2N;
using ringoa::TimerManager;
using ringoa::ToString, ringoa::Format;
using ringoa::fmi::OblivFMI;
using ringoa::fmi::OblivFMIConfig;
using ringoa::fmi::OblivFMIParameters;
using ringoa::proto::ProtocolContext2P;
using ringoa::proto::ProtocolContext3P;
using ringoa::proto::ProtocolContext3PBinary;
using ringoa::proto::ProtocolIo;
using ringoa::proto::RingOa;
using ringoa::proto::RingOaConfig;
using ringoa::proto::RingOaFsc;
using ringoa::proto::RingOaParameters;
using ringoa::range::OblivRange;
using ringoa::range::OblivRangeConfig;
using ringoa::range::OblivRangeParameters;
using ringoa::sharing::AdditiveSharing2P;
using ringoa::sharing::BinaryReplicatedSharing3P;
using ringoa::sharing::Rep3Share64, ringoa::sharing::Rep3ShareBlock;
using ringoa::sharing::Rep3ShareIo;
using ringoa::sharing::Rep3ShareMat64;
using ringoa::sharing::Rep3ShareVec64, ringoa::sharing::Rep3ShareVecBlock;
using ringoa::sharing::Rep3ShareView64, ringoa::sharing::Rep3ShareViewBlock;
using ringoa::sharing::ReplicatedSharing3P;
using ringoa::sharing::ShareConfig;
using ringoa::wm::FMIndex;
using ringoa::wm::WaveletMatrix;

void Gen_ObliviousAccess_Dataset(const osuCrypto::CLP &cmd) {
    if (IsHelpRequested(cmd)) {
        PrintBenchmarkHelp("Generate ObliviousAccess Dataset");
        return;
    }
    const auto opts = SelectBenchOptions(cmd);

    Logger::InfoLog(LOC, "Generating ObliviousAccess dataset started");
    TimerManager timer_mgr;
    FileIo       file_io;
    int32_t      timer_id = timer_mgr.CreateTimer("ObliviousAccess DataGen");

    timer_mgr.Start(timer_id);
    for (auto db_bitsize : opts.db_bits_list) {
        ShareConfig             share = ShareConfig::Arith32();
        ProtocolContext3P       ctx(share);
        ProtocolContext3PBinary ctx_binary(share);
        RingOaConfig            cfg{db_bitsize};
        RingOaParameters        params(cfg, share);

        uint64_t d = params.GetDbIndexBits();
        uint64_t k = params.GetShareBitsize();

        RingOaFsc                  ringoa_fsc(params, ctx);
        ReplicatedSharing3P       &rss  = ctx.Rss();
        BinaryReplicatedSharing3P &brss = ctx_binary.Brss();

        // Build database of size 2^d with values 0..(2^d-1)
        std::vector<uint64_t> database(1ULL << d);
        for (size_t i = 0; i < database.size(); ++i) {
            database[i] = static_cast<uint64_t>(i);
        }
        // std::vector<block> database_block(1ULL << d);
        // for (size_t i = 0; i < database_block.size(); ++i) {
        //     database_block[i] = ringoa::MakeBlock(0, i);
        // }

        // Random query index over Z_{2^d}
        uint64_t index = Mod2N(GlobalRng::Rand<uint64_t>(), d);

        // 3-party replicated shares (local)
        std::array<Rep3ShareVec64, 3> database_sh = rss.ShareLocal(database);
        std::array<Rep3Share64, 3>    index_sh    = rss.ShareLocal(index);
        // Database for RingOA FSC (with sign bits)
        std::array<bool, 3>           v_sign{};
        std::array<Rep3ShareVec64, 3> database_sh_fsc;
        ringoa_fsc.GenerateDatabaseShare(database, database_sh_fsc, v_sign);
        // Binary shares for OblivSelect
        // std::array<Rep3ShareVecBlock, 3> database_block_sh = brss.ShareLocal(database_block);
        std::array<Rep3ShareVec64, 3> database_bin_sh = brss.ShareLocal(database);
        std::array<Rep3Share64, 3>    index_bin_sh    = brss.ShareLocal(index);

        // Save per-party shares
        for (size_t p = 0; p < ringoa::sharing::kThreeParties; ++p) {
            // Database and index for RingOA and Shared OT
            Rep3ShareIo::SaveShare(MakeDatasetPath("oa_db", d, k) + "_" + ToString(p), database_sh[p]);
            Rep3ShareIo::SaveShare(MakeDatasetPath("oa_idx", d, k) + "_" + ToString(p), index_sh[p]);
            // Database for RingOA FSC (with sign bits)
            file_io.WriteBinary(MakeDatasetPath("oa_v_sign", d, k) + "_" + ToString(p), v_sign[p]);
            Rep3ShareIo::SaveShare(MakeDatasetPath("oa_db_fsc", d, k) + "_" + ToString(p), database_sh_fsc[p]);
            // Database and index for OblivSelect (binary shares)
            // Rep3ShareIo::SaveShare(MakeDatasetPath("oa_db_block", d, k) + "_" + ToString(p), database_block_sh[p]);
            Rep3ShareIo::SaveShare(MakeDatasetPath("oa_db_bin", d, k) + "_" + ToString(p), database_bin_sh[p]);
            Rep3ShareIo::SaveShare(MakeDatasetPath("oa_idx_bin", d, k) + "_" + ToString(p), index_bin_sh[p]);
        }

        Logger::InfoLog(LOC, "Dataset generated for db_bitsize=" + ToString(db_bitsize) + " (db_size=" + ToString(database.size()) +
                                 ", index=" + ToString(index) + ")");
    }
    timer_mgr.Stop(timer_id, "ObliviousAccess DataGen completed");
    timer_mgr.Print(timer_id, "ObliviousAccess DataGen", ringoa::TimeUnit::SECONDS);

    Logger::InfoLog(LOC, "ObliviousAccess DataGen completed");
    Logger::ExportLogListAndClear(
        MakeLogBasePath(opts, kLogMiscPath, "oa_datagen", opts.party_id),
        opts.enable_log_timestamp);
}

void Gen_FullTextSearch_Dataset(const osuCrypto::CLP &cmd) {
    if (IsHelpRequested(cmd)) {
        PrintBenchmarkHelp(
            "Generate FM-Index Dataset",
            "  -real                      Use real-world dataset instead of random dataset.\n");
        return;
    }
    const auto            opts          = SelectBenchOptions(cmd);
    bool                  use_real_data = cmd.isSet("real");
    std::vector<uint64_t> text_bitsizes = opts.db_bits_list;
    std::vector<uint64_t> query_sizes   = SelectQueryBitsize(cmd);

    Logger::InfoLog(LOC, "Generating FullTextSearch dataset started");
    TimerManager timer_mgr;
    FileIo       file_io;
    int32_t      timer_id = timer_mgr.CreateTimer("FullTextSearch DataGen");

    timer_mgr.Start(timer_id);

    std::unique_ptr<ringoa::ChromosomeLoader> chr_loader;
    if (use_real_data) {
        std::vector<std::string> fasta_paths;
        for (int i = 1; i <= 6; ++i) {
            std::string path = kChromosomePath + "chr" + ToString(i) + "_clean.fa";
            if (std::filesystem::exists(path))
                fasta_paths.push_back(path);
        }
        if (fasta_paths.empty())
            throw std::runtime_error("No FASTA files found in " + kChromosomePath);
        chr_loader = std::make_unique<ringoa::ChromosomeLoader>(std::move(fasta_paths));
    }

    for (auto text_bitsize : text_bitsizes) {
        for (auto query_size : query_sizes) {
            ShareConfig        share = ShareConfig::Arith32();
            ProtocolContext2P  ctx2p(share);
            ProtocolContext3P  ctx3p(share);
            OblivFMIConfig     cfg{text_bitsize, query_size};
            OblivFMIParameters params(cfg, share);

            uint64_t d  = params.GetDbIndexBits();
            uint64_t k  = params.GetShareBitsize();
            uint64_t ds = params.GetDatabaseSize();
            uint64_t qs = params.GetQuerySize();

            OblivFMI ofmi(params, ctx2p, ctx3p);

            std::string database, query;

            if (use_real_data) {
                database = chr_loader->EnsurePrefix(ds - 2);
                // Choose a random start position so that query fits entirely
                uint64_t max_start = database.size() - qs;
                uint64_t start_pos = Mod2N(GlobalRng::Rand<uint64_t>(), d) % (max_start + 1);    // uses osuCrypto's RNG seed
                query              = database.substr(start_pos, qs);

                // Logging with explicit file names (robust even if some chrs missing)
                auto used = chr_loader->loaded_count();
                Logger::InfoLog(LOC, "Query start position: " + ToString(start_pos));
                Logger::InfoLog(LOC, "Genome sequence prepared (" +
                                         ToString(database.size()) + " bp), files consumed=" + ToString(used));
                Logger::InfoLog(LOC, "Database sample: " + database.substr(0, std::min<size_t>(50, database.size())) + "...");
                Logger::InfoLog(LOC, "Query sample: " + query.substr(0, std::min<size_t>(50, query.size())) + "...");
            } else {
                database = GenerateRandomString(ds - 2);
                query    = GenerateRandomString(qs);
            }

            FMIndex fm(database);

            {
                // 3-party replicated shares (local)
                auto db_sh    = ofmi.GenerateDatabaseU64Share(fm);
                auto query_sh = ofmi.GenerateQueryU64Share(fm, query);

                for (size_t p = 0; p < ringoa::sharing::kThreeParties; ++p) {
                    Rep3ShareIo::SaveShare(MakeDatasetPath("fmi_db", d, qs, k) + "_" + ToString(p), db_sh[p]);
                    Rep3ShareIo::SaveShare(MakeDatasetPath("fmi_query", d, qs, k) + "_" + ToString(p), query_sh[p]);
                }
            }

            {
                // Database for OblivFMI FSC (with sign bits)
                std::array<bool, 3>           v_sign = {};
                std::array<Rep3ShareMat64, 3> db_sh_fsc;
                std::array<Rep3ShareVec64, 3> aux_sh;
                ofmi.GenerateDatabaseU64Share(fm, db_sh_fsc, aux_sh, v_sign);

                for (size_t p = 0; p < ringoa::sharing::kThreeParties; ++p) {
                    file_io.WriteBinary(MakeDatasetPath("fmi_v_sign", d, qs, k) + "_" + ToString(p), v_sign[p]);
                    Rep3ShareIo::SaveShare(MakeDatasetPath("fmi_db_fsc", d, qs, k) + "_" + ToString(p), db_sh_fsc[p]);
                    Rep3ShareIo::SaveShare(MakeDatasetPath("fmi_aux_fsc", d, qs, k) + "_" + ToString(p), aux_sh[p]);
                }
            }

            Logger::InfoLog(LOC, "Dataset generated for text_bitsize=" + ToString(text_bitsize) + " (text_size=" + ToString(database.size()) +
                                     "), query_size=" + ToString(query_size));
        }
    }
    timer_mgr.Stop(timer_id, "FullTextSearch DataGen completed");
    timer_mgr.Print(timer_id, "FullTextSearch DataGen", ringoa::TimeUnit::SECONDS);

    Logger::InfoLog(LOC, "FullTextSearch DataGen completed");
    if (use_real_data) {
        Logger::ExportLogListAndClear(
            MakeLogBasePath(opts, kLogMiscPath, "fts_datagen_real", opts.party_id),
            opts.enable_log_timestamp);
    } else {
        Logger::ExportLogListAndClear(
            MakeLogBasePath(opts, kLogMiscPath, "fts_datagen", opts.party_id),
            opts.enable_log_timestamp);
    }
}

void Gen_RangeSearch_Dataset(const osuCrypto::CLP &cmd) {
    if (IsHelpRequested(cmd)) {
        PrintBenchmarkHelp("Generate RangeSearch Dataset",
                           "  -real                      Use real-world dataset instead of random dataset.\n");
        return;
    }
    const auto opts          = SelectBenchOptions(cmd);
    bool       use_real_data = cmd.isSet("real");

    Logger::InfoLog(LOC, "Generating RangeSearch dataset started");
    TimerManager timer_mgr;
    FileIo       file_io;
    int32_t      timer_id = timer_mgr.CreateTimer("RangeSearch DataGen");

    timer_mgr.Start(timer_id);

    // VAF database size is approximately 23 million entries < 2^25
    // sigma for VAF values is 7 (0-100)
    ShareConfig          share = ShareConfig::Arith32();
    ProtocolContext2P    ctx2p(share);
    ProtocolContext3P    ctx3p(share);
    OblivRangeConfig     cfg{25, 7};
    OblivRangeParameters params(cfg, share);

    uint64_t d  = params.GetDbIndexBits();
    uint64_t k  = params.GetShareBitsize();
    uint64_t ds = params.GetDatabaseSize();

    OblivRange           orange(params, ctx2p, ctx3p);
    ReplicatedSharing3P &rss = ctx3p.Rss();

    // Build database of size ds over [0, 100]
    std::vector<uint64_t> database(ds - 1);
    if (use_real_data) {
        LoadVAFValues(kVafDataPath + "vaf_values.txt", database);
        Logger::InfoLog(LOC, "Loaded VAF data from file: " + ToString(database));
    } else {
        SetRandomVAFValues(database);
        Logger::InfoLog(LOC, "Generated random VAF data: " + ToString(database));
    }

    // Build Wavelet Matrix once
    WaveletMatrix wm(database, params.GetSigma());

    // ======================================================
    // Secret-share the database
    // ======================================================
    std::array<Rep3ShareMat64, 3> db_sh = orange.GenerateDatabaseU64Share(wm);
    std::array<bool, 3>           v_sign;
    std::array<Rep3ShareMat64, 3> db_sh_fsc;
    std::array<Rep3ShareVec64, 3> aux_sh;
    orange.GenerateDatabaseU64Share(wm, db_sh_fsc, aux_sh, v_sign);

    // Save database shares (common for both queries)
    for (size_t p = 0; p < ringoa::sharing::kThreeParties; ++p) {
        Rep3ShareIo::SaveShare(MakeDatasetPath("vaf_db", d, k) + "_" + ToString(p), db_sh[p]);
        // Database for OblivRange FSC (with sign bits)
        file_io.WriteBinary(MakeDatasetPath("vaf_v_sign", d, k) + "_" + ToString(p), v_sign[p]);
        Rep3ShareIo::SaveShare(MakeDatasetPath("vaf_db_fsc", d, k) + "_" + ToString(p), db_sh_fsc[p]);
        Rep3ShareIo::SaveShare(MakeDatasetPath("vaf_aux_fsc", d, k) + "_" + ToString(p), aux_sh[p]);
    }

    // ======================================================
    // BRCA Query
    // ======================================================
    uint64_t left  = 19613831;
    uint64_t right = 19614213;
    uint64_t count = right - left - 1;    // max/rank-"last"

    std::vector<uint64_t> q = {left, right, count};

    // Secret-share BRCA query
    std::array<Rep3ShareVec64, 3> q_sh = rss.ShareLocal(q);

    // Save BRCA query shares
    for (size_t p = 0; p < ringoa::sharing::kThreeParties; ++p) {
        Rep3ShareIo::SaveShare(MakeDatasetPath("vaf_query_brca", d, k) + "_" + ToString(p), q_sh[p]);
    }

    Logger::InfoLog(LOC, "Dataset generated for RangeSearch (VAF) with db_size=" + ToString(database.size()) +
                             ", query=(left=" + ToString(left) + ", right=" + ToString(right) + ", count=" + ToString(count) + ")");

    timer_mgr.Stop(timer_id, "RangeSearch DataGen completed");
    timer_mgr.Print(timer_id, "RangeSearch DataGen", ringoa::TimeUnit::SECONDS);

    Logger::InfoLog(LOC, "RangeSearch DataGen completed");
    Logger::ExportLogListAndClear(
        MakeLogBasePath(opts, kLogMiscPath, "range_datagen", opts.party_id),
        opts.enable_log_timestamp);
}

}    // namespace bench_ringoa
