#ifndef BENCH_BENCH_COMMON_H_
#define BENCH_BENCH_COMMON_H_

#include <cryptoTools/Common/CLP.h>

#include "RingOA/utils/logger.h"
#include "RingOA/utils/to_string.h"
#include "RingOA/utils/utils.h"

namespace bench_ringoa {

constexpr uint64_t kRepeatDefault = 10;
constexpr uint64_t kWarmupDefault = 5;

inline const std::vector<std::string> &HelpTags() {
    static const std::vector<std::string> tags{"h", "help"};
    return tags;
}

inline const std::vector<std::string> &DbBitsTags() {
    static const std::vector<std::string> tags{"d", "db_bits"};
    return tags;
}

struct BenchOptions {
    uint64_t              repeat;
    uint64_t              warmup;
    int                   party_id;
    std::string           network;
    std::string           log_dir;
    bool                  enable_log_timestamp = true;
    std::vector<uint64_t> db_bits_list;
};

inline bool IsHelpRequested(const osuCrypto::CLP &cmd) {
    return cmd.isSet(HelpTags());
}

inline std::vector<std::string> SplitString(const std::string &input, char delimiter) {
    std::vector<std::string> result;
    std::stringstream        ss(input);
    std::string              item;

    while (std::getline(ss, item, delimiter)) {
        if (item.empty()) {
            throw std::runtime_error("Empty item in option value: " + input);
        }

        result.push_back(item);
    }

    return result;
}

inline uint64_t ParseUint64(const std::string &input) {
    size_t   parsed = 0;
    uint64_t value  = std::stoull(input, &parsed);

    if (parsed != input.size()) {
        throw std::runtime_error("Invalid integer value: " + input);
    }

    return value;
}

inline std::vector<uint64_t> ParseDbBitsSpec(const std::string &spec) {
    std::vector<uint64_t> result;

    for (const auto &item : SplitString(spec, ',')) {
        const auto range = SplitString(item, ':');

        if (range.size() == 1) {
            result.push_back(ParseUint64(range[0]));
        } else if (range.size() == 2 || range.size() == 3) {
            const uint64_t begin = ParseUint64(range[0]);
            const uint64_t end   = ParseUint64(range[1]);
            const uint64_t step  = range.size() == 3 ? ParseUint64(range[2]) : 1;

            if (step == 0) {
                throw std::runtime_error("DB bits range step must be greater than zero.");
            }

            if (begin > end) {
                throw std::runtime_error("DB bits range begin must be less than or equal to end.");
            }

            for (uint64_t x = begin; x <= end; x += step) {
                result.push_back(x);

                if (end - x < step) {
                    break;
                }
            }
        } else {
            throw std::runtime_error("Invalid DB bits specification: " + item);
        }
    }

    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());

    if (result.empty()) {
        throw std::runtime_error("DB bits specification produced no values.");
    }

    return result;
}

inline std::vector<uint64_t> SelectDbBits(const osuCrypto::CLP &cmd) {
    if (!cmd.hasValue(DbBitsTags())) {
        throw std::runtime_error("Missing required option: -db_bits. Use -bench <Index> -help for usage.");
    }

    const std::string spec = cmd.get<std::string>(DbBitsTags());

    ringoa::Logger::InfoLog(LOC, "DB bits spec: " + spec);

    auto result = ParseDbBitsSpec(spec);

    std::string resolved = "Resolved DB bits:";
    for (uint64_t x : result) {
        resolved += " " + ringoa::ToString(x);
    }

    ringoa::Logger::InfoLog(LOC, resolved);

    return result;
}

inline uint64_t SelectRepeat(const osuCrypto::CLP &cmd) {
    const int64_t repeat = cmd.getOr<int64_t>("repeat", static_cast<int64_t>(kRepeatDefault));

    if (repeat < 1) {
        throw std::runtime_error("repeat must be >= 1.");
    }

    return static_cast<uint64_t>(repeat);
}

inline uint64_t SelectWarmup(const osuCrypto::CLP &cmd) {
    const int64_t warmup = cmd.getOr<int64_t>("warmup", static_cast<int64_t>(kWarmupDefault));

    if (warmup < 0) {
        throw std::runtime_error("warmup must be >= 0.");
    }

    return static_cast<uint64_t>(warmup);
}

inline int SelectPartyId(const osuCrypto::CLP &cmd) {
    return cmd.isSet("party") ? cmd.get<int>("party") : -1;
}

inline std::string SelectNetworkName(const osuCrypto::CLP &cmd) {
    return cmd.isSet("network") ? cmd.get<std::string>("network") : "";
}

inline bool SelectEnableTimestamp(const osuCrypto::CLP &cmd) {
    return !cmd.isSet("no_log_timestamp");
}

inline std::string JoinPath(const std::string &dir, const std::string &name) {
    if (dir.empty()) {
        return name;
    }

    if (dir.back() == '/') {
        return dir + name;
    }

    return dir + "/" + name;
}

inline std::string MakeLogBasePath(
    const BenchOptions &opts,
    const std::string  &default_dir,
    const std::string  &benchmark_name,
    int                 party_id) {
    const std::string dir = opts.log_dir.empty() ? default_dir : opts.log_dir;

    std::string name = benchmark_name;
    if (party_id >= 0) {
        name += "_p" + ringoa::ToString(party_id);
    }

    if (!opts.network.empty()) {
        name += "_" + opts.network;
    }

    return JoinPath(dir, name);
}

inline std::string SelectLogDir(const osuCrypto::CLP &cmd) {
    return cmd.isSet("log_dir") ? cmd.get<std::string>("log_dir") : "";
}

inline BenchOptions SelectBenchOptions(const osuCrypto::CLP &cmd) {
    BenchOptions opts;

    opts.repeat               = SelectRepeat(cmd);
    opts.warmup               = SelectWarmup(cmd);
    opts.party_id             = SelectPartyId(cmd);
    opts.network              = SelectNetworkName(cmd);
    opts.log_dir              = SelectLogDir(cmd);
    opts.enable_log_timestamp = SelectEnableTimestamp(cmd);
    opts.db_bits_list         = SelectDbBits(cmd);
    return opts;
}

inline void PrintCommonBenchOptionsHelp() {
    std::cout << "Common benchmark options:\n";
    std::cout << "  -db_bits <Spec>, -d        log2 DB sizes to benchmark.\n";
    std::cout << "                             Examples: 20, 20,24, 20:24, 20:26:2.\n";
    std::cout << "  -repeat <Count>            Number of measured repetitions inside each benchmark.\n";
    std::cout << "                             Default: " << kRepeatDefault << ".\n";
    std::cout << "  -warmup <Count>            Number of warmup repetitions before measurement.\n";
    std::cout << "                             Default: " << kWarmupDefault << ".\n";
    std::cout << "  -party <Id>                Party id for distributed execution.\n";
    std::cout << "                             If omitted, all parties are launched locally.\n";
    std::cout << "  -network <Name>            Network label used in output log filenames.\n";
    std::cout << "  -log_dir <Path>            Directory to save log files.\n";
    std::cout << "  -no_log_timestamp          Do not append timestamps to log filenames.\n";
}

inline void PrintBenchmarkHelp(const std::string &name, const std::string &specific_options = "") {
    std::cout << name << "\n";
    std::cout << "\n";

    PrintCommonBenchOptionsHelp();

    if (!specific_options.empty()) {
        std::cout << "\n";
        std::cout << "Benchmark-specific options:\n";
        std::cout << specific_options;
    }
}

inline std::vector<uint64_t> SelectQueryBitsize(const osuCrypto::CLP &cmd) {
    std::string size = cmd.getOr<std::string>("qsize", "default");

    if (size == "small") {
        return {16};
    } else if (size == "medium") {
        return {64};
    } else if (size == "large") {
        return {128};
    } else {
        return {16};
    }
}

inline const std::string kCurrentPath = ringoa::GetCurrentDirectory();

// dataset
inline const std::string kBenchDataPath = kCurrentPath + "/data/bench/dataset/";
// preprocess data
inline const std::string kBenchPirPath        = kCurrentPath + "/data/bench/pir/";
inline const std::string kBenchRingOAPath     = kCurrentPath + "/data/bench/ringoa/";
inline const std::string kBenchSotPath        = kCurrentPath + "/data/bench/sot/";
inline const std::string kBenchOsPath         = kCurrentPath + "/data/bench/obliv_select/";
inline const std::string kBenchOblivRangePath = kCurrentPath + "/data/bench/obliv_range/";
inline const std::string kBenchOblivFmiPath   = kCurrentPath + "/data/bench/obliv_fmi/";
inline const std::string kBenchSotfmiPath     = kCurrentPath + "/data/bench/sot_fmi/";
inline const std::string kBenchSotRangePath   = kCurrentPath + "/data/bench/sot_range/";
// logs
inline const std::string kLogDpfPath        = kCurrentPath + "/data/logs/dpf/";
inline const std::string kLogPirPath        = kCurrentPath + "/data/logs/pir/";
inline const std::string kLogRingOaPath     = kCurrentPath + "/data/logs/ringoa/";
inline const std::string kLogSotPath        = kCurrentPath + "/data/logs/sot/";
inline const std::string kLogOsPath         = kCurrentPath + "/data/logs/obliv_select/";
inline const std::string kLogOblivRangePath = kCurrentPath + "/data/logs/obliv_range/";
inline const std::string kLogOblivFmiPath   = kCurrentPath + "/data/logs/obliv_fmi/";
inline const std::string kLogSotfmiPath     = kCurrentPath + "/data/logs/sot_fmi/";
inline const std::string kLogSotRangePath   = kCurrentPath + "/data/logs/sot_range/";
inline const std::string kLogMiscPath       = kCurrentPath + "/data/logs/misc/";
// real data
inline const std::string kChromosomePath = kCurrentPath + "/data/bench/grch38/";
inline const std::string kVafDataPath    = kCurrentPath + "/data/bench/icgc/";

inline std::string MakeDatasetPath(const std::string &name, uint64_t d, uint64_t k) {
    return kBenchDataPath + name + "_d" + ringoa::ToString(d) + "_k" + ringoa::ToString(k);
}

inline std::string MakeDatasetPath(const std::string &name, uint64_t d, uint64_t qs, uint64_t k) {
    return kBenchDataPath + name + "_d" + ringoa::ToString(d) + "_qs" + ringoa::ToString(qs) + "_k" + ringoa::ToString(k);
}

}    // namespace bench_ringoa

#endif    // BENCH_BENCH_COMMON_H_
