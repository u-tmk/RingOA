#include "utils_test.h"

#include <cryptoTools/Common/TestCollection.h>

#include "RingOA/utils/logger.h"
#include "RingOA/utils/to_string.h"
#include "RingOA/utils/utils.h"

namespace {

const std::string kCurrentPath    = ringoa::GetCurrentDirectory();
const std::string kTestFileIoPath = kCurrentPath + "/data/test/utils/";

}    // namespace

namespace test_ringoa {

using ringoa::Format, ringoa::FormatMatrix;
using ringoa::Logger;
using ringoa::ToString, ringoa::ToStringMatrix;

void Utils_Test() {
    Logger::InfoLog(LOC, "Utils_Test...");

    Logger::DebugLog(LOC, "0 = " + ToString(0));
    Logger::DebugLog(LOC, "12345 = " + ToString(12345));
    Logger::DebugLog(LOC, "-100 = " + ToString(-100));
    Logger::DebugLog(LOC, "3.14159 = " + ToString(3.14159));
    Logger::DebugLog(LOC, "hello = " + ToString(std::string_view("hello")));

    // vector<bool> tests
    {
        std::vector<bool> bv = {true, false, true, false};
        Logger::DebugLog(LOC, "1010 = " + ToString(bv));
    }

    // span-based decimal tests
    {
        std::array<int, 5> arr = {1, 2, 3, 4, 5};
        // default delimiter=" ", max_size=100
        Logger::DebugLog(LOC, "1 2 3 4 5 = " + ToString(std::span<const int>(arr), " ", /*max_size*/ 100));
        // custom delimiter=",", max_size=3
        Logger::DebugLog(LOC, "1,2,3,... = " + ToString(std::span<const int>(arr), ",", 3));
    }

    // contiguous_range tests
    {
        std::vector<int> vec = {10, 20, 30};
        Logger::DebugLog(LOC, "10 20 30 = " + ToString(vec));
    }

    // ToStringMatrix tests
    {
        std::vector<int> flat = {1, 2, 3, 4, 5, 6};
        Logger::DebugLog(LOC, "[1 2 3],[4 5 6] = " + ToStringMatrix(flat, 2, 3));
        Logger::DebugLog(LOC, "[1 2],[3 4],[5 6] = " +
                                  ToStringMatrix(flat, 3, 2));
    }

    // block tests
    {
        ringoa::block blk(0x1234567890abcdef, 0xfedcba0987654321);
        Logger::DebugLog(LOC, "Block Hex: " + Format(blk));
        Logger::DebugLog(LOC, "Block Bin: " + Format(blk, ringoa::FormatType::kBin));
    }

    // span-based block tests
    {
        std::vector<ringoa::block> blocks = {
            ringoa::block(0x1234567890abcdef, 0xfedcba0987654321),
            ringoa::block(0x1122334455667788, 0x8877665544332211)};
        Logger::DebugLog(LOC, "Blocks Hex: " + Format(std::span<const ringoa::block>(blocks), ringoa::FormatType::kHex));
        Logger::DebugLog(LOC, "Blocks Bin: " + Format(std::span<const ringoa::block>(blocks), ringoa::FormatType::kBin));
    }

    // contiguous_range block tests
    {
        std::vector<ringoa::block> blocks = {
            ringoa::block(0x1234567890abcdef, 0xfedcba0987654321),
            ringoa::block(0x1122334455667788, 0x8877665544332211)};
        Logger::DebugLog(LOC, "Blocks Hex: " +
                                  FormatMatrix(blocks, 2, 1, ringoa::FormatType::kHex));
        Logger::DebugLog(LOC, "Blocks Bin: " +
                                  FormatMatrix(blocks, 2, 1, ringoa::FormatType::kBin));
    }

    // FormatMatrix block tests
    {
        std::vector<ringoa::block> blocks = {
            ringoa::MakeBlock(0, 0), ringoa::MakeBlock(1, 1),
            ringoa::MakeBlock(2, 2), ringoa::MakeBlock(3, 3),
            ringoa::MakeBlock(4, 4), ringoa::MakeBlock(5, 5)};
        Logger::DebugLog(LOC, "Blocks Hex: " +
                                  FormatMatrix(std::span<const ringoa::block>(blocks), 3, 2, ringoa::FormatType::kHex));
        Logger::DebugLog(LOC, "Blocks Bin: " +
                                  FormatMatrix(std::span<const ringoa::block>(blocks), 2, 3, ringoa::FormatType::kBin));
    }

    Logger::DebugLog(LOC, "Utils_Test - Passed");
}

}    // namespace test_ringoa
