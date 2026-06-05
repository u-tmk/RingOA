#ifndef UTILS_LOGGER_H_
#define UTILS_LOGGER_H_

#include <filesystem>
#include <mutex>
#include <vector>

// LOC macro: Returns the current file name, line number, and function name as a string
// Without function name: [LOG-INFO][filename.cpp:123]
#define LOC (std::filesystem::path(__FILE__).filename().string() + ":" + std::to_string(__LINE__))

// Absolute path: [LOG-INFO][/home/username/.../filename.cpp:123][function_name]
#define LOC_ABS (std::string(__FILE__) + ":" + std::to_string(__LINE__) + "][" + std::string(__func__))
// File name only: [LOG-INFO][filename.cpp:123][function_name]
#define LOC_FILE (std::filesystem::path(__FILE__).filename().string() + ":" + \
                  std::to_string(__LINE__) + "][" + std::string(__func__))

// PRETTY_LOC macro: Returns the current file name, line number, and pretty function name as a string
#define PRETTY_LOC (std::filesystem::path(__FILE__).filename().string() + ":" + \
                    std::to_string(__LINE__) + "][" + std::string(__PRETTY_FUNCTION__))

// Define log levels
#define LOG_LEVEL_NONE  0
#define LOG_LEVEL_FATAL 1
#define LOG_LEVEL_ERROR 2
#define LOG_LEVEL_WARN  3
#define LOG_LEVEL_INFO  4
#define LOG_LEVEL_DEBUG 5
#define LOG_LEVEL_TRACE 6

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_INFO    // Default log level
#endif

namespace ringoa {

constexpr int kKeyWidth     = 20;
constexpr int kMsgMaxLength = 70;

struct LogFormat {
    std::string log_level  = "";
    std::string time_stamp = "";
    std::string func_name  = "";
    std::string message    = "";

    std::string Format(const std::string del = ",");
};

class Logger {
public:
    Logger() = delete;

    static void FatalLog(const std::string &location, const std::string &message);
    static void ErrorLog(const std::string &location, const std::string &message);
    static void WarnLog(const std::string &location, const std::string &message);
    static void InfoLog(const std::string &location, const std::string &message);
    static void DebugLog(const std::string &location, const std::string &message);
    static void TraceLog(const std::string &location, const std::string &message);

    static std::string StrWithSep(const std::string &message, char separator = '-', int width = kMsgMaxLength);
    static std::string MakeSeparatorLine(std::size_t len = kMsgMaxLength, char separator = '=');
    static std::string FormatKeyValue(const std::string &key, const std::string &value, int key_width = kKeyWidth, char separator = ':');

    static void SetPrintLog(bool print_log);

    static bool                            GetPrintLog();
    const static std::vector<std::string> &GetLogList();

    static void ClearLogList();
    static void ExportLogList(const std::string &file_path, const bool use_timestamp = true);
    static void ExportLogListAndClear(const std::string &file_path, const bool use_timestamp = true);

private:
    static LogFormat                log_format_; /**< A struct to store log information. */
    static std::mutex               log_mutex_;  /**< A mutex to protect the log list. */
    static std::vector<std::string> log_list_;   /**< A list to store log entries as strings. */
    static bool                     print_log_;  /**< A flag to indicate whether to print the log message. */

    static void SetLogFormat(const std::string &log_level, const std::string &func_name, const std::string &message);
};

}    // namespace ringoa

#endif    // UTILS_LOGGER_H_
