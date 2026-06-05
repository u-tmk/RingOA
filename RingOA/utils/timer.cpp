#include "timer.h"

#include <cmath>

#include "logger.h"

namespace ringoa {

int32_t TimerManager::CreateTimer(const std::string &name) {
    const int32_t timer_id = timer_count_++;
    timers_[timer_id]      = Timer{name, {}, {}, {}, {}, false};
    return timer_id;
}

int32_t TimerManager::GetOrCreateTimer(const std::string &name) {
    if (name.empty()) {
        return CreateTimer(name);
    }
    auto it = name_to_id_.find(name);
    if (it != name_to_id_.end()) {
        return it->second;
    }
    const int32_t id = CreateTimer(name);
    name_to_id_.emplace(name, id);
    return id;
}

TimerManager::Timer *TimerManager::GetTimerOrLogError(int32_t timer_id) {
    auto it = timers_.find(timer_id);
    if (it == timers_.end()) {
        Logger::ErrorLog(LOC, "Invalid timer ID.");
        return nullptr;
    }
    return &it->second;
}

const TimerManager::Timer *TimerManager::GetTimerOrLogErrorConst(int32_t timer_id) const {
    auto it = timers_.find(timer_id);
    if (it == timers_.end()) {
        Logger::ErrorLog(LOC, "Invalid timer ID.");
        return nullptr;
    }
    return &it->second;
}

void TimerManager::Start(int32_t timer_id) {
    Timer *timer = GetTimerOrLogError(timer_id);
    if (!timer) {
        return;
    }
    if (timer->running) {
        Logger::ErrorLog(LOC, "Timer already running. Stop() must be called before Start().");
        return;
    }
    timer->running = true;
    timer->start_times.push_back(Clock::now());
}

void TimerManager::Stop(int32_t timer_id, const std::string &msg) {
    Timer *timer = GetTimerOrLogError(timer_id);
    if (!timer) {
        return;
    }
    if (!timer->running) {
        Logger::ErrorLog(LOC, "Timer is not running. Start() must be called before Stop().");
        return;
    }
    if (timer->start_times.empty()) {
        Logger::ErrorLog(LOC, "Timer has no start time.");
        timer->running = false;
        return;
    }

    const auto stop_time = Clock::now();
    timer->end_times.push_back(stop_time);

    const double elapsed_ns = GetElapsedTimeNs(timer->start_times.back(), stop_time);
    timer->elapsed_times.push_back(elapsed_ns);
    timer->messages.push_back(msg);

    timer->running = false;
}

void TimerManager::Print(int32_t            timer_id,
                         const std::string &msg,
                         TimeUnit           unit) const {
    const Timer *timer = GetTimerOrLogErrorConst(timer_id);
    if (!timer) {
        return;
    }
    PrintOne(*timer, timer_id, msg, unit);
}

void TimerManager::PrintAll(const std::string &msg,
                            TimeUnit           unit) const {
    for (const auto &kv : timers_) {
        PrintOne(kv.second, kv.first, msg, unit);
    }
}

void TimerManager::Reset(int32_t timer_id) {
    Timer *timer = GetTimerOrLogError(timer_id);
    if (!timer) {
        return;
    }

    // If it is currently running, discard the in-flight measurement.
    timer->running = false;

    timer->start_times.clear();
    timer->end_times.clear();
    timer->elapsed_times.clear();
    timer->messages.clear();
}

void TimerManager::ResetAll() {
    for (auto &kv : timers_) {
        Timer &timer  = kv.second;
        timer.running = false;
        timer.start_times.clear();
        timer.end_times.clear();
        timer.elapsed_times.clear();
        timer.messages.clear();
    }
}

bool TimerManager::ResetByName(const std::string &name) {
    auto it = name_to_id_.find(name);
    if (it == name_to_id_.end()) {
        if (verbose_) {
            Logger::WarnLog(LOC, "No timer found with name \"" + name + "\". ResetByName() did nothing.");
        }
        return false;
    }
    Reset(it->second);
    return true;
}

// ---- RAII ----

TimerManager::ScopedTimer::ScopedTimer(TimerManager &tm, int32_t timer_id, std::string msg)
    : tm_(tm), id_(timer_id), msg_(std::move(msg)) {
    tm_.Start(id_);
}

TimerManager::ScopedTimer::~ScopedTimer() {
    tm_.Stop(id_, msg_);
}

TimerManager::ScopedTimer TimerManager::Scope(int32_t timer_id, const std::string &msg) {
    return ScopedTimer(*this, timer_id, msg);
}

// ---- helpers ----

uint64_t TimerManager::GetElapsedTimeNs(const TimePoint &start, const TimePoint &end) {
    using namespace std::chrono;
    return duration_cast<nanoseconds>(end - start).count();
}

double TimerManager::ConvertElapsedTime(uint64_t time, TimeUnit from, TimeUnit to) {
    static const double k      = 1000.0;
    static const double conv[] = {1.0, k, k * k, k * k * k};    // ns, us, ms, s
    return time * (conv[from] / conv[to]);
}

std::string TimerManager::GetUnitString(TimeUnit unit) {
    switch (unit) {
        case NANOSECONDS:
            return "ns";
        case MICROSECONDS:
            return "\xC2\xB5s";
        case MILLISECONDS:
            return "ms";
        case SECONDS:
            return "s";
        default:
            return "ms";
    }
}

void TimerManager::PrintOne(const Timer       &timer,
                            int32_t            timer_id,
                            const std::string &msg,
                            TimeUnit           unit) const {
    const std::string unit_str = GetUnitString(unit);

    if (verbose_) {
        std::ostringstream header;
        header << "[TimerID=" << timer_id << "]"
               << " TimerName=\"" << timer.name << "\""
               << " Unit=" << unit_str
               << " Count=" << timer.elapsed_times.size();
        Logger::InfoLog("", header.str());
    }

    if (timer.elapsed_times.empty()) {
        Logger::InfoLog("", "[Summary] Name=\"" + timer.name + "\" Unit=" + unit_str +
                                " Message=\"" + msg + "\" No entries.");
        return;
    }

    double total = 0.0;
    double max_v = std::numeric_limits<double>::lowest();
    double min_v = std::numeric_limits<double>::max();

    for (size_t i = 0; i < timer.elapsed_times.size(); ++i) {
        const double elapsed = ConvertElapsedTime(timer.elapsed_times[i], NANOSECONDS, unit);
        total += elapsed;
        max_v = std::max(max_v, elapsed);
        min_v = std::min(min_v, elapsed);

        if (verbose_) {
            std::ostringstream line;
            line << "TimerName=\"" << timer.name << "\""
                 << " Message=\"" << timer.messages[i] << "\""
                 << " Elapsed=" << std::fixed << std::setprecision(3) << elapsed;
            Logger::InfoLog("", line.str());
        }
    }

    const double avg = total / static_cast<double>(timer.elapsed_times.size());

    double var = 0.0;
    if (timer.elapsed_times.size() > 1) {
        for (size_t i = 0; i < timer.elapsed_times.size(); ++i) {
            const double v = ConvertElapsedTime(timer.elapsed_times[i], NANOSECONDS, unit);
            const double d = v - avg;
            var += d * d;
        }
        var /= static_cast<double>(timer.elapsed_times.size() - 1);
    }
    const double stddev = (var > 0.0) ? std::sqrt(var) : 0.0;

    std::ostringstream summary_prefix;
    summary_prefix << "[Summary] Name=\"" << timer.name << "\""
                   << " Unit=" << unit_str
                   << " Message=\"" << msg << "\" ";

    {
        std::ostringstream s;
        s << std::fixed << std::setprecision(3)
          << "Avg=" << avg
          << " ± " << stddev << " (Total=" << total << ")";
        Logger::InfoLog("", summary_prefix.str() + s.str());
    }
    // CSV output (machine friendly)
    {
        static bool header_printed = false;

        if (!header_printed) {
            Logger::InfoLog("", "[CSV],timer_id,name,unit,msg,count,total,avg,stddev,min,max");
            header_printed = true;
        }

        std::ostringstream csv;
        csv << std::fixed << std::setprecision(3);

        const std::string csv_unit =
            (unit == MICROSECONDS) ? "us" : GetUnitString(unit);

        csv << "[CSV],"
            << timer_id << ","
            << "\"" << timer.name << "\","
            << csv_unit << ","
            << "\"" << msg << "\","
            << timer.elapsed_times.size() << ","
            << total << ","
            << avg << ","
            << stddev << ","
            << min_v << ","
            << max_v;

        Logger::InfoLog("", csv.str());
    }
}

}    // namespace ringoa
