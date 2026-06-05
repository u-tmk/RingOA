#ifndef UTILS_TIMER_H_
#define UTILS_TIMER_H_

#include <chrono>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace ringoa {

/**
 * @brief Enumeration of time units.
 */
enum TimeUnit
{
    NANOSECONDS,  /**< Nanoseconds */
    MICROSECONDS, /**< Microseconds */
    MILLISECONDS, /**< Milliseconds */
    SECONDS       /**< Seconds */
};

using Clock     = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

/**
 * TimerManager provides measurement utilities.
 *
 *   - int32_t CreateTimer(name)
 *   - void Start(id)
 *   - void Stop(id, msg)
 *   - void Print(id)
 *   - void PrintAll()
 *   - ScopedTimer Scope(id, msg)  // RAII
 *
 * Notes:
 *   - Not thread-safe.
 *   - Start/Stop calls should be balanced per timer.
 *   - Results accumulate until TimerManager is destroyed.
 */
class TimerManager {
public:
    TimerManager() = default;

    int32_t CreateTimer(const std::string &name = "");
    int32_t GetOrCreateTimer(const std::string &name);

    void Start(int32_t timer_id);
    void Stop(int32_t timer_id, const std::string &msg = "");

    void Print(int32_t            timer_id,
               const std::string &msg  = "",
               TimeUnit           unit = MILLISECONDS) const;

    void PrintAll(const std::string &msg  = "",
                  TimeUnit           unit = MILLISECONDS) const;

    void Reset(int32_t timer_id);
    void ResetAll();
    bool ResetByName(const std::string &name);

    void SetVerbose(bool verbose) {
        verbose_ = verbose;
    }
    bool verbose() const {
        return verbose_;
    }

    // RAII helper
    class ScopedTimer {
    public:
        ScopedTimer(TimerManager &tm, int32_t timer_id, std::string msg = "");
        ~ScopedTimer();

        ScopedTimer(const ScopedTimer &)            = delete;
        ScopedTimer &operator=(const ScopedTimer &) = delete;

    private:
        TimerManager &tm_;
        int32_t       id_;
        std::string   msg_;
    };

    [[nodiscard]] ScopedTimer Scope(int32_t timer_id, const std::string &msg = "");

private:
    struct Timer {
        std::string              name;
        std::vector<TimePoint>   start_times;
        std::vector<TimePoint>   end_times;
        std::vector<uint64_t>    elapsed_times;    // base unit: nanoseconds
        std::vector<std::string> messages;
        bool                     running = false;
    };

    std::map<int32_t, Timer> timers_;
    int32_t                  timer_count_ = 0;
    bool                     verbose_     = false;

    std::unordered_map<std::string, int32_t> name_to_id_;

    Timer       *GetTimerOrLogError(int32_t timer_id);
    const Timer *GetTimerOrLogErrorConst(int32_t timer_id) const;

    static uint64_t    GetElapsedTimeNs(const TimePoint &start, const TimePoint &end);
    static double      ConvertElapsedTime(uint64_t time, TimeUnit from, TimeUnit to);
    static std::string GetUnitString(TimeUnit unit);

    void PrintOne(const Timer       &timer,
                  int32_t            timer_id,
                  const std::string &msg,
                  TimeUnit           unit) const;
};

}    // namespace ringoa

#endif    // UTILS_TIMER_H_
