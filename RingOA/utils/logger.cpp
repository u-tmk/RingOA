#include "logger.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

#include "utils.h"

namespace {

const std::map<std::string, int> &GetColorMap() {
    static const std::map<std::string, int> color_map = {
        {"red", 31}, {"green", 32}, {"yellow", 33}, {"blue", 34}, {"magenta", 35}, {"cyan", 36}, {"white", 37}, {"black", 30}, {"bright_red", 91}, {"bright_green", 92}, {"bright_yellow", 93}, {"bright_blue", 94}, {"bright_magenta", 95}, {"bright_cyan", 96}, {"bright_white", 97}};
    return color_map;
}

void PrintLogMessage(const std::string &log_level, const std::string &location, const std::string &msg_body, int color) {
    // ANSI escape codes for colored and bold text
    std::cout << "\033[" << color << "m" << log_level << "\033[0m";                 // Colorized log level
    std::cout << "\033[1m[" << location << "]\033[0m " << msg_body << std::endl;    // Bold location and normal message
}

void PrintFatalMessage(const std::string &location, const std::string &msg_body) {
    PrintLogMessage("[FATAL]", location, msg_body, GetColorMap().at("red"));
}
void PrintErrorMessage(const std::string &location, const std::string &msg_body) {
    PrintLogMessage("[ERROR]", location, msg_body, GetColorMap().at("bright_red"));
}
void PrintWarnMessage(const std::string &location, const std::string &msg_body) {
    PrintLogMessage("[WARNING]", location, msg_body, GetColorMap().at("bright_yellow"));
}
void PrintInfoMessage(const std::string &location, const std::string &msg_body) {
    PrintLogMessage("[INFO]", location, msg_body, GetColorMap().at("green"));
}
void PrintDebugMessage(const std::string &location, const std::string &msg_body) {
    PrintLogMessage("[DEBUG]", location, msg_body, GetColorMap().at("bright_blue"));
}
void PrintTraceMessage(const std::string &location, const std::string &msg_body) {
    PrintLogMessage("[TRACE]", location, msg_body, GetColorMap().at("bright_magenta"));
}

}    // namespace

namespace ringoa {

LogFormat                Logger::log_format_;
std::mutex               Logger::log_mutex_;
std::vector<std::string> Logger::log_list_;
bool                     Logger::print_log_ = true;

std::string LogFormat::Format(const std::string del) {
    return log_level + del + time_stamp + del + func_name + del + message;
}

void Logger::FatalLog(const std::string &location, const std::string &message) {
    std::lock_guard<std::mutex> lock(log_mutex_);
#if LOG_LEVEL >= LOG_LEVEL_FATAL
    PrintFatalMessage(location, message);
#endif
    SetLogFormat("[FATAL]", location, message);
}

void Logger::ErrorLog(const std::string &location, const std::string &message) {
    std::lock_guard<std::mutex> lock(log_mutex_);
#if LOG_LEVEL >= LOG_LEVEL_ERROR
    PrintErrorMessage(location, message);
#endif
    SetLogFormat("[ERROR]", location, message);
}

void Logger::WarnLog(const std::string &location, const std::string &message) {
    std::lock_guard<std::mutex> lock(log_mutex_);
#if LOG_LEVEL >= LOG_LEVEL_WARN
    if (print_log_) {
        PrintWarnMessage(location, message);
    }
#endif
    SetLogFormat("[WARNING]", location, message);
}

void Logger::InfoLog(const std::string &location, const std::string &message) {
    std::lock_guard<std::mutex> lock(log_mutex_);
#if LOG_LEVEL >= LOG_LEVEL_INFO
    if (print_log_) {
        PrintInfoMessage(location, message);
    }
#endif
    SetLogFormat("[INFO]", location, message);
}

void Logger::DebugLog(const std::string &location, const std::string &message) {
    std::lock_guard<std::mutex> lock(log_mutex_);
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    if (print_log_) {
        PrintDebugMessage(location, message);
    }
#endif
    SetLogFormat("[DEBUG]", location, message);
}

void Logger::TraceLog(const std::string &location, const std::string &message) {
    std::lock_guard<std::mutex> lock(log_mutex_);
#if LOG_LEVEL >= LOG_LEVEL_TRACE
    if (print_log_) {
        PrintTraceMessage(location, message);
    }
#endif
    SetLogFormat("[TRACE]", location, message);
}

std::string Logger::StrWithSep(const std::string &message, char separator, int width) {
    int messageWidth  = message.length();
    int totalSepWidth = width - messageWidth - 2;    // Subtract 2 for the spaces around the message

    if (totalSepWidth <= 0) {
        // If the width is less than or equal to the length of the message + 2, return the message without separators
        return message;
    }

    int leftSepWidth  = totalSepWidth / 2;
    int rightSepWidth = totalSepWidth - leftSepWidth;    // Adjust for any rounding issues

    std::string leftSeparator(leftSepWidth, separator);
    std::string rightSeparator(rightSepWidth, separator);

    return leftSeparator + " " + message + " " + rightSeparator;
}

std::string Logger::FormatKeyValue(const std::string &key, const std::string &value, int key_width, char separator) {
    std::ostringstream oss;
    oss << std::left << std::setw(key_width) << key << " " << separator << " " << value;
    return oss.str();
}

std::string Logger::MakeSeparatorLine(std::size_t len, char separator) {
    return std::string(len, separator);
}

void Logger::SetPrintLog(bool print_log) {
    print_log_ = print_log;
}

bool Logger::GetPrintLog() {
    return print_log_;
}

const std::vector<std::string> &Logger::GetLogList() {
    std::lock_guard<std::mutex> lock(log_mutex_);
    return log_list_;
}

void Logger::ClearLogList() {
    std::lock_guard<std::mutex> lock(log_mutex_);
    log_list_.clear();
}

void Logger::ExportLogList(const std::string &file_path, const bool use_timestamp) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    if (log_list_.empty()) {
        return;    // No logs to export
    }

    std::string out_path = file_path;
    if (use_timestamp) {
        out_path = file_path + "_" + GetCurrentDateTimeAsString();
    }
    out_path += ".log";
    EnsureParentDirExists(out_path);

    std::ofstream file(out_path, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open log file: " + out_path);
    }

    file << log_list_.size() << "\n";
    for (const auto &line : log_list_) {
        file << line << "\n";
    }
}

void Logger::ExportLogListAndClear(const std::string &file_path, const bool use_timestamp) {
    ExportLogList(file_path, use_timestamp);
    ClearLogList();
}

void Logger::SetLogFormat(const std::string &log_level, const std::string &func_name, const std::string &message) {
    log_format_.log_level = log_level;
    std::time_t now       = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    char        buffer[80];
    std::strftime(buffer, sizeof(buffer), "%Y/%m/%d %H:%M:%S", std::localtime(&now));
    log_format_.time_stamp = buffer;
    log_format_.func_name  = func_name;
    log_format_.message    = message;
    log_list_.push_back(log_format_.Format());
}

}    // namespace ringoa
