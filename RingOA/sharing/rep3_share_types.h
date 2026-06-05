#ifndef SHARING_REP3_SHARE_TYPES_H_
#define SHARING_REP3_SHARE_TYPES_H_

#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <span>
#include <sstream>
#include <string>
#include <vector>

#include "RingOA/utils/block.h"
#include "RingOA/utils/to_string.h"

namespace ringoa {
namespace sharing {

// Generic share pair struct for uint64_t or block types
template <typename T>
struct Rep3Share {
    static_assert(
        std::is_same_v<T, uint32_t> || std::is_same_v<T, uint64_t> || std::is_same_v<T, block>,
        "Rep3Share<T> supports only uint32_t, uint64_t or block");

    std::array<T, 2> data;

    // Default, copy, and move constructors/assignments
    Rep3Share()                                 = default;
    Rep3Share(const Rep3Share &)                = default;
    Rep3Share(Rep3Share &&) noexcept            = default;
    Rep3Share &operator=(const Rep3Share &)     = default;
    Rep3Share &operator=(Rep3Share &&) noexcept = default;

    // Construct from two shares
    Rep3Share(T share0, T share1) {
        data[0] = share0;
        data[1] = share1;
    }

    // Construct from array
    Rep3Share(const std::array<T, 2> &other) : data(other) {
    }
    Rep3Share &operator=(const std::array<T, 2> &other) {
        data = other;
        return *this;
    }

    // Element access
    T &operator[](size_t idx) {
        return data[idx];
    }
    const T &operator[](size_t idx) const {
        return data[idx];
    }

    // Convert to string for debugging
    std::string ToString() const {
        std::ostringstream oss;
        if constexpr (std::is_same_v<T, block>) {
            oss << "(" << ringoa::Format(data[0]) << ", " << ringoa::Format(data[1]) << ")";
        } else {
            oss << "(" << data[0] << ", " << data[1] << ")";
        }
        return oss.str();
    }
};

// Generic replicated share vector that holds two vectors of type T
// T must be trivially copyable for serialization
template <typename T>
struct Rep3ShareVec {
    static_assert(
        std::is_same_v<T, uint32_t> || std::is_same_v<T, uint64_t> || std::is_same_v<T, block>,
        "Rep3ShareVec can only be used with uint32_t, uint64_t or block types");

    size_t                        num_shares = 0;
    std::array<std::vector<T>, 2> data;

    Rep3ShareVec()                                    = default;
    Rep3ShareVec(const Rep3ShareVec &)                = delete;
    Rep3ShareVec(Rep3ShareVec &&) noexcept            = delete;
    Rep3ShareVec &operator=(const Rep3ShareVec &)     = default;
    Rep3ShareVec &operator=(Rep3ShareVec &&) noexcept = default;

    explicit Rep3ShareVec(size_t n)
        : num_shares(n) {
        data[0].resize(n);
        data[1].resize(n);
    }

    Rep3ShareVec(std::vector<T> &&share_0, std::vector<T> &&share_1)
        : num_shares(share_0.size()) {
        if (share_0.size() != share_1.size()) {
            throw std::invalid_argument("Shares must have the same size");
        }
        data[0] = std::move(share_0);
        data[1] = std::move(share_1);
    }

    std::vector<T> &operator[](const size_t idx) {
        return data[idx];
    }
    const std::vector<T> &operator[](const size_t idx) const {
        return data[idx];
    }

    size_t Size() const {
        return num_shares;
    }

    Rep3Share<T> At(size_t idx) const {
        if (idx >= num_shares) {
            throw std::out_of_range("Rep3ShareVec::At index out of range");
        }
        return Rep3Share<T>(data[0][idx], data[1][idx]);
    }

    void Set(size_t idx, const Rep3Share<T> &share) {
        if (idx >= num_shares) {
            throw std::out_of_range("Rep3ShareVec::Set index out of range");
        }
        data[0][idx] = share[0];
        data[1][idx] = share[1];
    }

    void RequireInvariant() const {
        if (data[0].size() != num_shares || data[1].size() != num_shares) {
            throw std::runtime_error("Rep3ShareVec invariant violation: vector sizes do not match num_shares");
        }
    }

    std::string ToString(FormatType format = FormatType::kHex, std::string_view delim = " ", size_t max_size = kSizeMax) const {
        std::ostringstream oss;
        if constexpr (std::is_same_v<T, block>) {
            oss << "(" << ringoa::Format(data[0], format, delim, max_size) << ", " << ringoa::Format(data[1], format, delim, max_size) << ")";
        } else {
            oss << "(" << ringoa::ToString(data[0], delim, max_size) << ", " << ringoa::ToString(data[1], delim, max_size) << ")";
        }
        return oss.str();
    }
};

// Lightweight view for Rep3ShareVec<T>, non-owning, read-only
template <typename T>
struct Rep3ShareView {
    size_t             num_shares;
    std::span<const T> share0;
    std::span<const T> share1;

    explicit Rep3ShareView(const Rep3ShareVec<T> &rep_share)
        : num_shares(rep_share.num_shares),
          share0(rep_share.data[0]),
          share1(rep_share.data[1]) {
    }

    Rep3ShareView(size_t             count,
                  std::span<const T> s0,
                  std::span<const T> s1) noexcept
        : num_shares(count),
          share0(s0),
          share1(s1) {
    }

    size_t Size() const {
        return num_shares;
    }

    Rep3Share<T> At(size_t idx) const {
        if (idx >= num_shares) {
            throw std::out_of_range("Rep3ShareView::At index out of range");
        }
        return Rep3Share<T>(share0[idx], share1[idx]);
    }

    void RequireInvariant() const {
        if (share0.size() != num_shares || share1.size() != num_shares) {
            throw std::runtime_error("Rep3ShareView invariant violation: span sizes do not match num_shares");
        }
    }

    std::string ToString(FormatType format = FormatType::kHex, std::string_view delim = " ", size_t max_size = kSizeMax) const {
        std::ostringstream oss;
        if constexpr (std::is_same_v<T, block>) {
            oss << "(" << ringoa::Format(share0, format, delim, max_size) << ", " << ringoa::Format(share1, format, delim, max_size) << ")";
        } else {
            oss << "(" << ringoa::ToString(share0, delim, max_size) << ", " << ringoa::ToString(share1, delim, max_size) << ")";
        }
        return oss.str();
    }
};

template <typename T>
struct Rep3ShareMat {
    size_t          rows, cols;
    Rep3ShareVec<T> shares;    // Internally holds rows*cols × 2 shares

    static_assert(
        std::is_same_v<T, uint32_t> || std::is_same_v<T, uint64_t> || std::is_same_v<T, block>,
        "Rep3ShareMat<T> supports only uint32_t, uint64_t, or block");

    Rep3ShareMat(size_t rows_, size_t cols_)
        : rows(rows_), cols(cols_), shares(rows_ * cols_) {
    }

    Rep3ShareMat(size_t rows_, size_t cols_, std::vector<T> &&share_0, std::vector<T> &&share_1)
        : rows(rows_), cols(cols_), shares(0) {
        size_t n = rows_ * cols_;
        if (share_0.size() != n || share_1.size() != n) {
            throw std::invalid_argument("Rep3ShareMat: flat vector size mismatch");
        }
        // Move into internal shares
        shares.data[0]    = std::move(share_0);
        shares.data[1]    = std::move(share_1);
        shares.num_shares = n;
    }

    Rep3ShareMat()                                    = default;
    Rep3ShareMat(const Rep3ShareMat &)                = delete;
    Rep3ShareMat(Rep3ShareMat &&) noexcept            = delete;
    Rep3ShareMat &operator=(const Rep3ShareMat &)     = default;
    Rep3ShareMat &operator=(Rep3ShareMat &&) noexcept = default;

    std::vector<T> &operator[](const size_t idx) {
        return shares.data[idx];
    }
    const std::vector<T> &operator[](const size_t idx) const {
        return shares.data[idx];
    }

    Rep3ShareView<T> RowView(size_t i) const {
        if (i >= rows) {
            throw std::out_of_range("Row index out of range");
        }
        size_t             offset = i * cols;
        std::span<const T> s0(shares.data[0].data() + offset, cols);
        std::span<const T> s1(shares.data[1].data() + offset, cols);
        return Rep3ShareView<T>(cols, s0, s1);
    }

    Rep3Share<T> At(size_t i, size_t j) const {
        if (i >= rows || j >= cols) {
            throw std::out_of_range("Index out of range");
        }
        return shares.At(i * cols + j);
    }

    void Set(size_t i, size_t j, const Rep3Share<T> &share) {
        if (i >= rows || j >= cols) {
            throw std::out_of_range("Index out of range");
        }
        shares.Set(i * cols + j, share);
    }

    void RequireInvariant() const {
        shares.RequireInvariant();
        if (shares.num_shares != rows * cols) {
            throw std::runtime_error("Rep3ShareMat invariant violation: shares size does not match rows*cols");
        }
    }

    // String representation up to optional row/col limits
    std::string ToStringMatrix(FormatType       format   = FormatType::kHex,
                               std::string_view row_pref = "[",
                               std::string_view row_suff = "]",
                               std::string_view col_del  = " ",
                               std::string_view row_del  = ",",
                               size_t           max_size = kSizeMax) const {
        std::ostringstream oss;
        if constexpr (std::is_same_v<T, block>) {
            oss << "(" << ringoa::FormatMatrix(shares.data[0], rows, cols, format, row_pref, row_suff, col_del, row_del, max_size) << ", "
                << ringoa::FormatMatrix(shares.data[1], rows, cols, format, row_pref, row_suff, col_del, row_del, max_size) << ")";
            return oss.str();
        } else {
            oss << "(" << ringoa::ToStringMatrix(shares.data[0], rows, cols, row_pref, row_suff, col_del, row_del, max_size) << ", "
                << ringoa::ToStringMatrix(shares.data[1], rows, cols, row_pref, row_suff, col_del, row_del, max_size) << ")";
        }
        return oss.str();
    }
};

}    // namespace sharing
}    // namespace ringoa

#endif    // SHARING_REP3_SHARE_TYPES_H_
