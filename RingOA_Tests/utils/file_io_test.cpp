#include "file_io_test.h"

#include <cryptoTools/Common/TestCollection.h>

#include "RingOA/utils/file_io.h"
#include "RingOA/utils/logger.h"
#include "RingOA/utils/to_string.h"
#include "RingOA/utils/utils.h"

namespace {

const std::string kCurrentPath    = ringoa::GetCurrentDirectory();
const std::string kTestFileIoPath = kCurrentPath + "/data/test/utils/";

}    // namespace

namespace test_ringoa {

using ringoa::block;
using ringoa::Logger;
using ringoa::ToString, ringoa::Format;

void File_Io_Test() {
    Logger::InfoLog(LOC, "File_Io_Test...");

    ringoa::FileIo io;
    // Write data to files
    uint64_t                val     = 123456;
    std::vector<uint64_t>   vec     = {1, 2, 3, 4, 5};
    std::array<uint64_t, 3> arr     = {1, 2, 3};
    block                   blk     = ringoa::MakeBlock(0x1234567890abcdef, 0xfedcba0987654321);
    std::vector<block>      blk_vec = {
        ringoa::MakeBlock(0x1111111111111111, 0x2222222222222222),
        ringoa::MakeBlock(0x3333333333333333, 0x4444444444444444),
        ringoa::MakeBlock(0x5555555555555555, 0x6666666666666666)};
    std::array<block, 2> blk_arr = {
        ringoa::MakeBlock(0x5555555555555555, 0x6666666666666666),
        ringoa::MakeBlock(0x7777777777777777, 0x8888888888888888)};
    std::vector<std::string> str_vec = {"a", "b", "c"};

    Logger::DebugLog(LOC, "val: " + ToString(val));
    Logger::DebugLog(LOC, "vec: " + ToString(vec));
    Logger::DebugLog(LOC, "arr: " + ToString(arr));
    Logger::DebugLog(LOC, "blk: " + Format(blk));
    Logger::DebugLog(LOC, "blk_vec: " + Format(blk_vec));
    Logger::DebugLog(LOC, "blk_arr: " + Format(blk_arr));
    Logger::DebugLog(LOC, "str_vec: " + ToString(str_vec));

    io.WriteBinary(kTestFileIoPath + "val", val);
    io.WriteBinary(kTestFileIoPath + "vec", vec);
    io.WriteBinary(kTestFileIoPath + "arr", arr);
    io.WriteBinary(kTestFileIoPath + "blk", blk);
    io.WriteBinary(kTestFileIoPath + "blk_vec", blk_vec);
    io.WriteBinary(kTestFileIoPath + "blk_arr", blk_arr);
    io.WriteTextToFile(kTestFileIoPath + "str_vec", str_vec);

    // Read data from files
    uint64_t                val_read;
    std::vector<uint64_t>   vec_read;
    std::array<uint64_t, 3> arr_read;
    block                   blk_read;
    std::vector<block>      blk_vec_read;
    std::array<block, 2>    blk_arr_read;

    io.ReadBinary(kTestFileIoPath + "val", val_read);
    io.ReadBinary(kTestFileIoPath + "vec", vec_read);
    io.ReadBinary(kTestFileIoPath + "arr", arr_read);
    io.ReadBinary(kTestFileIoPath + "blk", blk_read);
    io.ReadBinary(kTestFileIoPath + "blk_vec", blk_vec_read);
    io.ReadBinary(kTestFileIoPath + "blk_arr", blk_arr_read);

    Logger::DebugLog(LOC, "val_read: " + ToString(val_read));
    Logger::DebugLog(LOC, "vec_read: " + ToString(vec_read));
    Logger::DebugLog(LOC, "arr_read: " + ToString(arr_read));
    Logger::DebugLog(LOC, "blk_read: " + Format(blk_read));
    Logger::DebugLog(LOC, "blk_vec_read: " + Format(blk_vec_read));
    Logger::DebugLog(LOC, "blk_arr_read: " + Format(blk_arr_read));

    // Check if the data was read correctly
    if (val != val_read)
        throw osuCrypto::UnitTestFail("Failed to read val correctly.");
    if (vec != vec_read)
        throw osuCrypto::UnitTestFail("Failed to read vec correctly.");
    if (arr != arr_read)
        throw osuCrypto::UnitTestFail("Failed to read arr correctly.");
    if (blk != blk_read)
        throw osuCrypto::UnitTestFail("Failed to read blk correctly.");
    if (blk_vec != blk_vec_read)
        throw osuCrypto::UnitTestFail("Failed to read blk_vec correctly.");
    if (blk_arr != blk_arr_read)
        throw osuCrypto::UnitTestFail("Failed to read blk_arr correctly.");

    Logger::DebugLog(LOC, "File_Io_Test - Passed");
}

}    // namespace test_ringoa
