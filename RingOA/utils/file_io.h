#ifndef UTILS_FILE_IO_H_
#define UTILS_FILE_IO_H_

#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include "block.h"
#include "logger.h"
#include "utils.h"

namespace ringoa {

//=========================================================
// Type traits to detect which types are binary‐writable
//=========================================================
// 1) std::vector<arithmetic>
template <typename T>
struct is_vector_arith : std::false_type {};
template <typename U>
struct is_vector_arith<std::vector<U>> : std::bool_constant<std::is_arithmetic_v<U>> {};

// 2) std::vector<block>
template <typename T>
struct is_vector_block : std::false_type {};
template <>
struct is_vector_block<std::vector<block>> : std::true_type {};

// 3) std::array<arithmetic, N>
template <typename T>
struct is_array_arith : std::false_type {};
template <typename U, std::size_t N>
struct is_array_arith<std::array<U, N>> : std::bool_constant<std::is_arithmetic_v<U>> {};

// 4) std::array<block, N>
template <typename T>
struct is_array_block : std::false_type {};
template <std::size_t N>
struct is_array_block<std::array<block, N>> : std::true_type {};

/**
 * @brief A class for file I/O operations (binary/text).
 *        - Supports:
 *          ・Arithmetic types (int, float, etc.)
 *          ・block
 *          ・std::string
 *          ・std::vector<arithmetic> / std::vector<block>
 *          ・std::array<arithmetic, N> / std::array<block, N>
 *          ・std::vector<std::string> (In text format)
 */
class FileIo {
public:
    explicit FileIo(const std::string ext = ".dat")
        : ext_(ext) {
    }

    const std::string &GetExtension() const {
        return ext_;
    }
    std::string GetFullPath(const std::string &file_path) const {
        return AddExtension(file_path);
    }

    /**
     * @brief Writes data to a file in binary mode.
     * @param T Enabled only if is_binary_writable<T>::value == true
     * @param file_path The file name (without extension).
     * @param data The data to be written.
     * @param append If true, appends data to the file. Otherwise, overwrites it. Defaults to false.
     */
    template <typename T>
    void WriteBinary(const std::string &file_path, const T &data, bool append = false) {
        std::string full_path = AddExtension(file_path);
        EnsureParentDirExists(full_path);

        // Open in binary mode
        std::ios_base::openmode mode = std::ios::out | std::ios::binary;
        if (append) {
            mode |= std::ios::app;
        }
        std::ofstream file(full_path, mode);
        if (!file.is_open()) {
            throw std::runtime_error("Could not open file for writing: " + full_path);
        }

        // Enable exceptions on write failures (badbit / failbit)
        file.exceptions(std::ios::badbit | std::ios::failbit);

        // If T is a single arithmetic value
        if constexpr (std::is_arithmetic_v<T>) {
            const uint64_t count = 1;
            WriteU64(file, count);
            file.write(reinterpret_cast<const char *>(&data), sizeof(T));
        }
        // If T is a single block
        else if constexpr (std::is_same_v<T, block>) {
            const uint64_t count = 1;
            WriteU64(file, count);
            WriteBlock16(file, data);
        }
        // If T is a std::string
        else if constexpr (std::is_same_v<T, std::string>) {
            const uint64_t length = static_cast<uint64_t>(data.size());
            WriteU64(file, length);
            if (length > 0) {
                file.write(data.data(), static_cast<std::streamsize>(length));
            }
        }
        // If T is a std::vector<arithmetic>
        else if constexpr (is_vector_arith<T>::value) {
            using ElemT          = typename T::value_type;
            const uint64_t count = static_cast<uint64_t>(data.size());
            WriteU64(file, count);
            if (count > 0) {
                const size_t bytes = static_cast<size_t>(count) * sizeof(ElemT);
                file.write(reinterpret_cast<const char *>(data.data()),
                           static_cast<std::streamsize>(bytes));
            }
        }
        // If T is an std::vector<block>
        else if constexpr (is_vector_block<T>::value) {
            const uint64_t count = static_cast<uint64_t>(data.size());
            WriteU64(file, count);

            if (count > 0) {
                for (size_t i = 0; i < data.size(); ++i) {
                    WriteBlock16(file, data[i]);
                }
            }
        }
        // If T is an std::array<arithmetic, N>
        else if constexpr (is_array_arith<T>::value) {
            constexpr size_t N   = std::tuple_size<T>::value;
            using ElemT          = typename T::value_type;
            const uint64_t count = static_cast<uint64_t>(N);
            WriteU64(file, count);
            if constexpr (N > 0) {
                file.write(reinterpret_cast<const char *>(data.data()),
                           static_cast<std::streamsize>(N * sizeof(ElemT)));
            }
        }
        // If T is an std::array<block, N>
        else if constexpr (is_array_block<T>::value) {
            constexpr size_t N     = std::tuple_size<T>::value;
            const uint64_t   count = static_cast<uint64_t>(N);
            WriteU64(file, count);
            for (size_t i = 0; i < N; ++i) {
                WriteBlock16(file, data[i]);
            }
        } else {
            static_assert(sizeof(T) == 0, "Unsupported data type for binary writing.");
        }
        // ofstream is closed automatically when it goes out of scope
    }

    /**
     * @brief Reads data from a file in binary mode.
     * @tparam T Enabled only if is_binary_writable<T>::value == true
     * @param file_path The file name (without extension).
     * @param data The variable to store the read data.
     */
    template <typename T>
    void ReadBinary(const std::string &file_path, T &data) {
        // Compose full path with binary extension
        std::string full_path = AddExtension(file_path);

        // Open in binary mode
        std::ios_base::openmode mode = std::ios::in | std::ios::binary;
        std::ifstream           file(full_path, mode);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file for binary reading: " + full_path);
        }

        // Enable exceptions on read failures
        file.exceptions(std::ios::badbit | std::ios::failbit);

        // Read the number of elements
        const uint64_t count_u64 = ReadU64(file);

        // If T is a single arithmetic value
        if constexpr (std::is_arithmetic_v<T>) {
            if (count_u64 != 1) {
                throw std::runtime_error("Unexpected count for single value: " + full_path);
            }
            file.read(reinterpret_cast<char *>(&data), sizeof(T));
        }
        // If T is a single block
        else if constexpr (std::is_same_v<T, block>) {
            if (count_u64 != 1) {
                throw std::runtime_error("Unexpected count for single block: " + full_path);
            }
            ReadBlock16(file, data);
        }
        // If T is a std::string
        else if constexpr (std::is_same_v<T, std::string>) {
            const size_t len = CheckedU64ToSize(count_u64, full_path);
            data.resize(len);
            if (len > 0) {
                file.read(reinterpret_cast<char *>(data.data()),
                          static_cast<std::streamsize>(len));
            }
        }
        // If T is a std::vector<arithmetic>
        else if constexpr (is_vector_arith<T>::value) {
            using ElemT    = typename T::value_type;
            const size_t n = CheckedU64ToSize(count_u64, full_path);
            data.resize(n);
            if (n > 0) {
                file.read(reinterpret_cast<char *>(data.data()),
                          static_cast<std::streamsize>(n * sizeof(ElemT)));
            }
        }
        // If T is a std::vector<block>
        else if constexpr (is_vector_block<T>::value) {
            const size_t n = CheckedU64ToSize(count_u64, full_path);
            data.resize(n);
            for (size_t i = 0; i < n; ++i) {
                ReadBlock16(file, data[i]);
            }
        }
        // If T is an std::array<arithmetic, N>
        else if constexpr (is_array_arith<T>::value) {
            constexpr size_t N = std::tuple_size<T>::value;
            if (count_u64 != static_cast<uint64_t>(N)) {
                throw std::runtime_error("Size mismatch for std::array: " + full_path);
            }
            if constexpr (N > 0) {
                file.read(reinterpret_cast<char *>(data.data()),
                          static_cast<std::streamsize>(N * sizeof(typename T::value_type)));
            }
        }
        // If T is an std::array<block, N>
        else if constexpr (is_array_block<T>::value) {
            constexpr size_t N = std::tuple_size<T>::value;
            if (count_u64 != static_cast<uint64_t>(N)) {
                throw std::runtime_error("Size mismatch for std::array<block>: " + full_path);
            }
            for (size_t i = 0; i < N; ++i) {
                ReadBlock16(file, data[i]);
            }
        } else {
            static_assert(sizeof(T) == 0, "Unsupported data type for binary reading.");
        }
        // ofstream is closed automatically when it goes out of scope
    }

    void WriteTextToFile(const std::string &file_path, const std::vector<std::string> &data, bool append = false) {
        std::string full_path = AddExtension(file_path);
        EnsureParentDirExists(full_path);

        std::ofstream           file;
        std::ios_base::openmode mode = std::ios::out;
        if (append) {
            mode |= std::ios::app;
        }
        file.open(full_path, mode);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open log file: " + full_path);
        }

        file << data.size() << "\n";
        for (const auto &line : data) {
            file << line << "\n";
        }
    }

    void ClearFileContents(const std::string &file_path) {
        std::ofstream file(AddExtension(file_path), std::ios::trunc);    // Truncate mode clears the content
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file for clearing: " + AddExtension(file_path));
        }
    }

private:
    const std::string       ext_; /**< Default file extension. */
    static constexpr size_t kBlockBytes = 16;

    std::string AddExtension(const std::string &file_path) const {
        return file_path + ext_;
    }

    static void WriteU64(std::ofstream &file, uint64_t x) {
        file.write(reinterpret_cast<const char *>(&x), sizeof(x));
    }
    static uint64_t ReadU64(std::ifstream &file) {
        uint64_t x = 0;
        file.read(reinterpret_cast<char *>(&x), sizeof(x));
        return x;
    }

    static size_t CheckedU64ToSize(uint64_t x, const std::string &full_path) {
        if (x > static_cast<uint64_t>(std::numeric_limits<size_t>::max())) {
            throw std::runtime_error("Count/length too large for this platform: " + full_path);
        }
        return static_cast<size_t>(x);
    }

    static void WriteBlock16(std::ofstream &file, const block &b) {
        static_assert(sizeof(block) >= kBlockBytes, "block must be at least 16 bytes");
        alignas(16) unsigned char buf[kBlockBytes];
        std::memcpy(buf, &b, kBlockBytes);
        file.write(reinterpret_cast<const char *>(buf), kBlockBytes);
    }

    static void ReadBlock16(std::ifstream &file, block &b) {
        static_assert(sizeof(block) >= kBlockBytes, "block must be at least 16 bytes");
        alignas(16) unsigned char buf[kBlockBytes];
        file.read(reinterpret_cast<char *>(buf), kBlockBytes);
        std::memcpy(&b, buf, kBlockBytes);
    }
};

}    // namespace ringoa

#endif    // UTILS_FILE_IO_H_
