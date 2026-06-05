#include <cryptoTools/Common/CLP.h>
#include <cryptoTools/Common/TestCollection.h>
#include <random>

#include "RingOA/utils/logger.h"
#include "RingOA/utils/rng.h"
#include "RingOA/utils/to_string.h"
#include "RingOA_Bench/gen_dataset.h"
#include "RingOA_Bench/obliv_fmi_bench.h"
#include "RingOA_Bench/obliv_range_bench.h"
#include "RingOA_Bench/obliv_select_bench.h"
#include "RingOA_Bench/ringoa_bench.h"
#include "RingOA_Bench/shared_ot_bench.h"
#include "RingOA_Bench/sot_fmi_bench.h"
#include "RingOA_Bench/sot_range_bench.h"

namespace bench_ringoa {

osuCrypto::TestCollection Tests([](osuCrypto::TestCollection &t) {
    t.add("Gen_ObliviousAccess_Dataset", Gen_ObliviousAccess_Dataset);
    t.add("Gen_FullTextSearch_Dataset", Gen_FullTextSearch_Dataset);
    t.add("Gen_RangeSearch_Dataset", Gen_RangeSearch_Dataset);

    t.add("RingOA_Preprocess_Bench", RingOA_Preprocess_Bench);
    t.add("RingOA_Online_Bench", RingOA_Online_Bench);
    t.add("RingOA_Fsc_Preprocess_Bench", RingOA_Fsc_Preprocess_Bench);
    t.add("RingOA_Fsc_Online_Bench", RingOA_Fsc_Online_Bench);
    t.add("SharedOT_Preprocess_Bench", SharedOT_Preprocess_Bench);
    t.add("SharedOT_Online_Bench", SharedOT_Online_Bench);
    t.add("OblivSelect_ShiftedAdditive_Preprocess_Bench", OblivSelect_ShiftedAdditive_Preprocess_Bench);
    t.add("OblivSelect_ShiftedAdditive_Online_Bench", OblivSelect_ShiftedAdditive_Online_Bench);

    t.add("OblivFMI_Preprocess_Bench", OblivFMI_Preprocess_Bench);
    t.add("OblivFMI_Online_Bench", OblivFMI_Online_Bench);
    t.add("OblivFMI_Fsc_Preprocess_Bench", OblivFMI_Fsc_Preprocess_Bench);
    t.add("OblivFMI_Fsc_Online_Bench", OblivFMI_Fsc_Online_Bench);
    t.add("SotFMI_Preprocess_Bench", SotFMI_Preprocess_Bench);
    t.add("SotFMI_Online_Bench", SotFMI_Online_Bench);

    t.add("OblivRange_VAF_Preprocess_Bench", OblivRange_VAF_Preprocess_Bench);
    t.add("OblivRange_VAF_Online_Bench", OblivRange_VAF_Online_Bench);
    t.add("OblivRange_Fsc_VAF_Preprocess_Bench", OblivRange_Fsc_VAF_Preprocess_Bench);
    t.add("OblivRange_Fsc_VAF_Online_Bench", OblivRange_Fsc_VAF_Online_Bench);
    t.add("SotRange_VAF_Preprocess_Bench", SotRange_VAF_Preprocess_Bench);
    t.add("SotRange_VAF_Online_Bench", SotRange_VAF_Online_Bench);
});

}    // namespace bench_ringoa

namespace {

std::vector<std::string>
    helpTags{"h", "help"},
    listTags{"l", "list"},
    benchTags{"b", "bench"};

void PrintHelp(const char *prog) {
    std::cout << "Usage: " << prog << " [OPTIONS]\n";
    std::cout << "Options:\n";
    std::cout << "  -list, -l            List all available benchmarks.\n";
    std::cout << "  -bench <Index>, -b   Run the specified test by its index.\n";
    std::cout << "  -help, -h            Display this help message.\n";
}

}    // namespace

int main(int argc, char **argv) {
    try {
        osuCrypto::CLP cmd(argc, argv);
        auto          &tests = bench_ringoa::Tests;

        // Display help message
        if (cmd.isSet(helpTags) && !cmd.hasValue(benchTags)) {
            PrintHelp(argv[0]);
            return 0;
        }

        // Display available tests
        if (cmd.isSet(listTags)) {
            tests.list();
            return 0;
        }

#if !USE_FIXED_RANDOM_SEED
        std::random_device rd;
        osuCrypto::block   seed = osuCrypto::toBlock(rd(), rd());
        ringoa::GlobalRng::Initialize(seed);
        ringoa::Logger::InfoLog(LOC, "RNG initialized with random seed " + ringoa::Format(seed) + ".");
#else
        ringoa::GlobalRng::Initialize();
        ringoa::Logger::InfoLog(LOC, "RNG initialized with fixed default seed.");
#endif

        // Handle test execution
        if (cmd.hasValue(benchTags)) {
            auto testIdxs = cmd.getMany<osuCrypto::u64>(benchTags);
            if (testIdxs.empty()) {
                std::cerr << "Error: No test index specified.\n";
                return 1;
            }

            auto result = tests.run(testIdxs, 1, &cmd);
            return (result == osuCrypto::TestCollection::Result::passed) ? 0 : 1;
        }

        // Invalid options
        std::cerr << "Error: No valid options specified.\n";
        std::cerr << "       Use -list to see available tests or -help for usage.\n";
        PrintHelp(argv[0]);
        return 1;

    } catch (const std::exception &ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}
