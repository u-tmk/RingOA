#include "timer_test.h"

#include <cryptoTools/Common/TestCollection.h>

#include "RingOA/utils/logger.h"
#include "RingOA/utils/timer.h"
#include "RingOA/utils/to_string.h"
#include "RingOA/utils/utils.h"

#include <thread>

namespace test_ringoa {

using ringoa::Logger;
using ringoa::TimerManager;
using ringoa::ToString;

void Timer_Test() {
    Logger::DebugLog(LOC, "Timer_Test ...");

    TimerManager timer_mgr;

    const int32_t id_a  = timer_mgr.CreateTimer("Process A");
    const int32_t id_b  = timer_mgr.CreateTimer("Process B Total");
    const int32_t id_b1 = timer_mgr.CreateTimer("Process B - 1");
    const int32_t id_b2 = timer_mgr.CreateTimer("Process B - 2");
    const int32_t id_b3 = timer_mgr.CreateTimer("Process B - 3");

    // Repeat process A 10 times
    for (int i = 0; i < 10; ++i) {
        {
            auto scope = timer_mgr.Scope(id_a, "i=" + ToString(i));
            // =========================
            // Process A
            std::this_thread::sleep_for(std::chrono::milliseconds(10 + i * 20));
            // =========================
        }
    }

    // Process B: total and sub-steps
    {
        auto scope_total = timer_mgr.Scope(id_b, "Process B finished");

        {
            auto scope = timer_mgr.Scope(id_b1, "Process B - 1");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        {
            auto scope = timer_mgr.Scope(id_b2, "Process B - 2");
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        {
            auto scope = timer_mgr.Scope(id_b3, "Process B - 3");
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Print results (human-friendly + CSV line)
    timer_mgr.Print(id_a, "Process A", ringoa::MILLISECONDS);
    timer_mgr.Print(id_b, "Process B", ringoa::MILLISECONDS);
    timer_mgr.Print(id_b1, "Process B - 1", ringoa::MILLISECONDS);
    timer_mgr.Print(id_b2, "Process B - 2", ringoa::MILLISECONDS);
    timer_mgr.Print(id_b3, "Process B - 3", ringoa::MILLISECONDS);

    // Or print everything at once
    // timer_mgr.PrintAll("Timer_Test", ringoa::MILLISECONDS);

    Logger::DebugLog(LOC, "Timer_Test - Passed");
}

}    // namespace test_ringoa
