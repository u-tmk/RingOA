#ifndef UTILS_TO_STRING_H_
#define UTILS_TO_STRING_H_

#include <bitset>     // std::bitset
#include <iomanip>    // std::setw, std::setfill
#include <ranges>     // std::ranges::*
#include <span>       // std::span
#include <sstream>    // std::ostringstream
#include <string>     // std::string, std::string_view
#include <vector>     // std::vector

#include "block.h"
#include "format_types.h"

namespace ringoa {

// ――――――――――――――――――――――
//  scalar overloads
// ――――――――――――――――――――――
inline std::string ToString(std::string_view sv) {
    return std::string{sv};
}

template <std::integral I>
std::string ToString(I v) {
    return std::to_string(v);
}

template <std::floating_point F>
std::string ToString(F v) {
    return std::to_string(v);
}

// ――――――――――――――――――――――――
//  span overloads
//  (T: string or integral or floating-point)
//  (contiguous range supports: std::span, std::vector, std::array, C-style arrays)
// ――――――――――――――――――――――――
template <typename T>
std::string ToString(std::span<const T> data,
                     std::string_view   delim    = " ",
                     size_t             max_size = kSizeMax) {
    size_t      n = std::min(data.size(), max_size);
    std::string out;
    out.reserve(n * 8);
    for (size_t i = 0; i < n; ++i) {
        out += ToString(data[i]);
        if (i + 1 < n)
            out += delim;
    }
    if (data.size() > max_size)
        out += delim, out += "...";
    return "[" + out + "]";
}

template <std::ranges::contiguous_range R>
    requires std::ranges::sized_range<R>
std::string ToString(const R         &r,
                     std::string_view delim    = " ",
                     size_t           max_size = kSizeMax) {
    return ToString(std::span{std::ranges::data(r), std::ranges::size(r)},
                    delim, max_size);
}

// ――――――――――――――――――――――――――――――
//  Matrix overloads
//  (T: string or integral or floating-point)
//  (contiguous range supports: std::span, std::vector, std::array, C-style arrays)
// ――――――――――――――――――――――――――――――
template <typename T>
std::string ToStringMatrix(
    std::span<const T> data,
    size_t             rows,
    size_t             cols,
    std::string_view   row_pref = "[",
    std::string_view   row_suff = "]",
    std::string_view   col_del  = " ",
    std::string_view   row_del  = ",",
    size_t             max_size = kSizeMax) {
    if (data.size() != rows * cols)
        throw std::invalid_argument("ToStringMatrix: size mismatch");

    std::ostringstream oss;
    size_t             printed = 0;
    for (size_t i = 0; i < rows && printed < max_size; ++i) {
        oss << row_pref;
        for (size_t j = 0; j < cols && printed < max_size; ++j, ++printed) {
            oss << ToString(data[i * cols + j]);
            if (j + 1 < cols && printed + 1 < max_size)
                oss << col_del;
        }
        if (printed < rows * cols && printed >= max_size) {
            oss << "...";
        }
        oss << row_suff;
        if (i + 1 < rows && printed < max_size)
            oss << row_del;
    }
    return oss.str();
}

template <std::ranges::contiguous_range R>
    requires std::ranges::sized_range<R>
std::string ToStringMatrix(
    const R         &flat,
    size_t           rows,
    size_t           cols,
    std::string_view row_pref = "[",
    std::string_view row_suff = "]",
    std::string_view col_del  = " ",
    std::string_view row_del  = ",",
    size_t           max_size = kSizeMax) {
    return ToStringMatrix(
        std::span{std::ranges::data(flat), std::ranges::size(flat)},
        rows, cols, row_pref, row_suff, col_del, row_del, max_size);
}

// ―-―――――――――――――――――――
//  block overloads
//  (osuCrypto::block)
//  (contiguous range supports: std::span, std::vector, std::array, C-style arrays)
// ―-―――――――――――――――――――
inline std::string Format(const block &blk, FormatType format = FormatType::kHex) {
    // Retrieve block data as two 64-bit values
    auto     arr  = blk.get<uint64_t>();
    uint64_t low  = arr[0];
    uint64_t high = arr[1];

    std::ostringstream oss;
    switch (format) {
        case FormatType::kBin:    // Binary (32-bit groups)
            oss << std::bitset<64>(high).to_string().substr(0, 32) << " "
                << std::bitset<64>(high).to_string().substr(32, 32) << " "
                << std::bitset<64>(low).to_string().substr(0, 32) << " "
                << std::bitset<64>(low).to_string().substr(32, 32);
            break;
        case FormatType::kHex:    // Hexadecimal (64-bit groups)
            oss << std::hex << std::setw(16) << std::setfill('0') << high << " "
                << std::setw(16) << std::setfill('0') << low;
            break;
        case FormatType::kDec:    // Decimal (64-bit groups)
            if (high > 0) {
                oss << std::dec << high << " " << low;
            } else {
                oss << std::dec << low;
            }
            break;
    }
    return oss.str();
}

inline std::string Format(std::span<const block> data,
                          FormatType             fmt      = FormatType::kHex,
                          std::string_view       delim    = ",",
                          size_t                 max_size = kSizeMax) {
    size_t             n = std::min(data.size(), max_size);
    std::ostringstream oss;

    for (size_t i = 0; i < n; ++i) {
        oss << Format(data[i], fmt);
        if (i + 1 < n)
            oss << delim;
    }
    if (data.size() > max_size)
        oss << delim << "...";

    return "[" + oss.str() + "]";
}

template <std::ranges::contiguous_range R>
    requires std::same_as<std::ranges::range_value_t<R>, block>
std::string Format(const R         &r,
                   FormatType       fmt      = FormatType::kHex,
                   std::string_view delim    = " ",
                   size_t           max_size = kSizeMax) {
    return Format(std::span<const block>{std::ranges::data(r), std::ranges::size(r)},
                  fmt, delim, max_size);
}

// ―――――――――――――――――――――――――――――――
// Matrix formatting for block elements
// ―――――――――――――――――――――――――――――――
inline std::string FormatMatrix(std::span<const block> data,
                                size_t                 rows,
                                size_t                 cols,
                                FormatType             fmt      = FormatType::kHex,
                                std::string_view       row_pref = "[",
                                std::string_view       row_suff = "]",
                                std::string_view       col_del  = ",",
                                std::string_view       row_del  = ",",
                                size_t                 max_size = kSizeMax) {
    if (data.size() != rows * cols) {
        throw std::invalid_argument("FormatMatrix: size mismatch");
    }

    std::ostringstream oss;
    size_t             printed = 0;

    for (size_t i = 0; i < rows && printed < max_size; ++i) {
        oss << row_pref;
        for (size_t j = 0; j < cols && printed < max_size; ++j, ++printed) {
            // block → Format(block, fmt)
            oss << Format(data[i * cols + j], fmt);

            if (j + 1 < cols && printed + 1 < max_size) {
                oss << col_del;
            }
        }
        if (printed < rows * cols && printed >= max_size) {
            oss << "...";
        }
        oss << row_suff;
        if (i + 1 < rows && printed < max_size) {
            oss << row_del;
        }
    }
    return oss.str();
}

// contiguous_range overload
template <std::ranges::contiguous_range R>
    requires std::same_as<std::ranges::range_value_t<R>, block> &&
             std::ranges::sized_range<R>
std::string FormatMatrix(const R         &flat,
                         size_t           rows,
                         size_t           cols,
                         FormatType       fmt      = FormatType::kHex,
                         std::string_view row_pref = "[",
                         std::string_view row_suff = "]",
                         std::string_view col_del  = " ",
                         std::string_view row_del  = ",",
                         size_t           max_size = kSizeMax) {
    return FormatMatrix(
        std::span{std::ranges::data(flat), std::ranges::size(flat)},
        rows, cols, fmt, row_pref, row_suff, col_del, row_del, max_size);
}

// ――――――――――――――――――――――――
//  Overloads for std::vector<bool>
// ――――――――――――――――――――――――
inline std::string ToString(const std::vector<bool> &bv) {
    std::string s;
    s.reserve(bv.size());
    for (bool b : bv) {
        s.push_back(b ? '1' : '0');
    }
    return s;
}

}    // namespace ringoa

#endif    // UTILS_TO_STRING_H_
