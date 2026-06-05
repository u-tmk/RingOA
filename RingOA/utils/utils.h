#ifndef UTILS_UTILS_H_
#define UTILS_UTILS_H_

#include <bit>           // std::bit_width
#include <chrono>        // std::chrono::system_clock
#include <cstdint>       // uint64_t, int64_t
#include <filesystem>    // std::filesystem::current_path
#include <numeric>       // std::iota

namespace ringoa {

// #############################
// ######## Calculation ########
// #############################

// Integer power via exponentiation by squaring.
inline constexpr uint64_t Pow(uint64_t base, uint64_t exponent) noexcept {
    uint64_t result = 1;
    while (exponent) {
        if (exponent & 1ULL)
            result *= base;
        exponent >>= 1ULL;
        if (exponent)
            base *= base;
    }
    return result;
}

// Return value mod 2^bitsize (i.e., keep the lower bitsize bits).
inline uint64_t Mod2N(uint64_t value, uint64_t bitsize) noexcept {
    return value & ((1UL << bitsize) - 1UL);
}

// Return -1 if b is true, else +1.
inline int64_t Sign(bool b) noexcept {
    return b ? -1 : 1;
}

// floor(log2(x)); returns -1 if x == 0.
inline int Log2Floor(uint64_t x) noexcept {
    return (x == 0) ? -1 : static_cast<int>(std::bit_width(x) - 1);
}

// Least significant bit.
inline uint64_t GetLSB(uint64_t value) noexcept {
    return value & 1ULL;
}

// Most significant bit of an n-bit view (bit at position n-1). Returns 0 if n == 0.
inline uint64_t GetMSB(uint64_t value, uint64_t n) noexcept {
    if (n == 0)
        return 0ULL;
    if (n >= 64)
        return (value >> 63) & 1ULL;
    return (value >> (n - 1)) & 1ULL;
}

// Keep lower n bits (safe for n in [0, 64]).
inline uint64_t GetLowerNBits(uint64_t value, uint64_t n) noexcept {
    if (n >= 64) {
        return value;
    }
    return value & ((1ULL << n) - 1ULL);
}

// Interpret the lower n bits of an unsigned integer as signed (two's complement).
// n in [0, 64]. If n == 0, returns 0. If n >= 64, returns value as int64_t.
inline int64_t UnsignedToSignedNBits(uint64_t value, uint64_t n) noexcept {
    if (n == 0)
        return 0;
    if (n >= 64)
        return static_cast<int64_t>(value);
    const uint64_t mask = (1ULL << n) - 1ULL;
    value &= mask;
    const uint64_t sign_bit = 1ULL << (n - 1);
    if (value & sign_bit) {
        // Negative: sign-extend
        const uint64_t extend = ~mask;
        return static_cast<int64_t>(value | extend);
    }
    return static_cast<int64_t>(value);
}

// Current datetime as "YYYYMMDD_HHMMSS" (local time).
inline std::string GetCurrentDateTimeAsString() {
    const auto        now = std::chrono::system_clock::now();
    const std::time_t t   = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&t), "%Y%m%d_%H%M%S");
    return ss.str();
}

// Current working directory as string.
inline std::string GetCurrentDirectory() {
    return std::filesystem::current_path().string();
}

// Create [start, end) sequence. If end < start, returns empty vector.
inline std::vector<uint64_t> CreateSequence(const uint64_t start, const uint64_t end) {
    if (end <= start)
        return {};
    std::vector<uint64_t> v(static_cast<size_t>(end - start));
    std::iota(v.begin(), v.end(), start);
    return v;
}

// Check if a file exists at the given path.
inline bool FileExists(const std::string &path, const std::string &ext = ".bin") {
    std::error_code   ec;
    const std::string full_path = path + ext;
    return std::filesystem::exists(full_path, ec);
}

// Check if a directory exists at the given path.
inline void EnsureParentDirExists(const std::string &file_path) {
    namespace fs = std::filesystem;

    const fs::path path_obj(file_path);
    const fs::path parent_dir = path_obj.parent_path();
    if (parent_dir.empty()) {
        return;    // No parent directory to create
    }

    std::error_code ec;
    fs::create_directories(parent_dir, ec);
    if (ec) {
        throw std::runtime_error(
            "Failed to create directory: " + parent_dir.string() + " Error: " + ec.message());
    }
}

}    // namespace ringoa

#endif    // UTILS_UTILS_H_
