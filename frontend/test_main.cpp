#include <cryptoTools/Common/CLP.h>
#include <cryptoTools/Common/TestCollection.h>
#include <random>
#include <tests_cryptoTools/UnitTests.h>

#include "RingOA/utils/logger.h"
#include "RingOA/utils/rng.h"
#include "RingOA/utils/to_string.h"
#include "unit_tests.h"

namespace test_ringoa {

}    // namespace test_ringoa

namespace {

std::vector<std::string>
    helpTags{"h", "help"},
    listTags{"l", "list"},
    testTags{"t", "test"},
    unitTags{"u", "unitTests"},
    suiteTags{"s", "suite"};

void PrintHelp(const char *prog) {
    std::cout << "Usage: " << prog << " [OPTIONS]\n";
    std::cout << "Options:\n";
    std::cout << "  -unit, -u            Run all unit tests.\n";
    std::cout << "  -list, -l            List all available tests.\n";
    std::cout << "  -test <Index>, -t    Run the specified test by its index.\n";
    std::cout << "  -suite <Name>, -s    Run the specified test suite.\n";
    std::cout << "  -help, -h            Display this help message.\n";
}

}    // namespace

int main(int argc, char **argv) {
    try {
        osuCrypto::CLP            cmd(argc, argv);
        osuCrypto::TestCollection tests;
        // tests += tests_cryptoTools::Tests;
        tests += test_ringoa::Tests;

        // Display help message
        if (cmd.isSet(helpTags)) {
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
        if (cmd.hasValue(testTags)) {
            auto testIdxs = cmd.getMany<osuCrypto::u64>(testTags);
            if (testIdxs.empty()) {
                std::cerr << "Error: No test index specified.\n";
                return 1;
            }

            auto result = tests.run(testIdxs, /*repeatCount=*/1, &cmd);
            return (result == osuCrypto::TestCollection::Result::passed) ? 0 : 1;
        }

        if (cmd.hasValue(suiteTags)) {
            auto prefix = cmd.get<std::string>(suiteTags);

            // search expects a list<string>
            std::list<std::string> queries = {prefix};

            // this will return all test indices whose name contains prefix
            auto testIdxs = tests.search(queries);

            if (testIdxs.empty()) {
                std::cerr << "No tests match suite string: " << prefix << "\n";
                return 1;
            }

            auto result = tests.run(testIdxs, /*repeatCount=*/1, &cmd);
            return (result == osuCrypto::TestCollection::Result::passed) ? 0 : 1;
        }

        // Unit test execution
        if (cmd.isSet(unitTags)) {
            ringoa::Logger::SetPrintLog(false);
            auto result = tests.runIf(cmd);
            if (result != osuCrypto::TestCollection::Result::passed) {
                return 1;    // Exit on failure
            }
            return 0;    // Success
        }

        // Invalid options
        std::cerr << "Error: No valid options specified.\n";
        PrintHelp(argv[0]);
        return 1;

    } catch (const std::exception &ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}
