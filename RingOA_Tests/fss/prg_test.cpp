#include "prg_test.h"

#include "RingOA/fss/fss.h"
#include "RingOA/fss/prg.h"
#include "RingOA/utils/logger.h"
#include "RingOA/utils/timer.h"
#include "RingOA/utils/to_string.h"
#include "RingOA/utils/utils.h"

namespace test_ringoa {

using ringoa::block;
using ringoa::Logger;
using ringoa::ToString, ringoa::Format;
using ringoa::fss::prg::Side;

void Prg_Test() {
    Logger::DebugLog(LOC, "Prg_Test...");

    ringoa::fss::prg::PseudoRandomGenerator prg = ringoa::fss::prg::PseudoRandomGenerator::GetInstance();
    Logger::DebugLog(LOC, "PseudoRandomGenerator created successfully");

    block                seed_in = ringoa::MakeBlock(0x1234567890abcdef, 0x1234567890abcdef);
    std::array<block, 2> seed_out;
    prg.DoubleExpand(seed_in, seed_out);

    Logger::DebugLog(LOC, "seed_in: " + Format(seed_in));
    Logger::DebugLog(LOC, "seed_out[0]: " + Format(seed_out[0]));
    Logger::DebugLog(LOC, "seed_out[1]: " + Format(seed_out[1]));

    block expanded_seed;
    prg.Expand(seed_in, expanded_seed, Side::kLeft);

    Logger::DebugLog(LOC, "expanded_seed: " + Format(expanded_seed));
    Logger::DebugLog(LOC, "Equal(seed_out[0], expanded_seed): " + ToString(seed_out[0] == expanded_seed));

    prg.Expand(seed_in, expanded_seed, Side::kRight);

    Logger::DebugLog(LOC, "expanded_seed: " + Format(expanded_seed));
    Logger::DebugLog(LOC, "Equal(seed_out[1], expanded_seed): " + ToString(seed_out[1] == expanded_seed));

    Logger::DebugLog(LOC, "Prg_Test - Passed");
}

}    // namespace test_ringoa
