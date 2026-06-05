#ifndef SHARING_BEAVER_TRIPLES_H_
#define SHARING_BEAVER_TRIPLES_H_

#include <sstream>

#include "RingOA/utils/bytes.h"

namespace ringoa {
namespace sharing {

/**
 * @brief A struct to store a Beaver triple.
 */
struct BeaverTriple {
    uint64_t a;
    uint64_t b;
    uint64_t c;    // Opened values satisfy c = a*b (mod 2^k)

    BeaverTriple()
        : a(0), b(0), c(0) {
    }
    BeaverTriple(uint64_t a_, uint64_t b_, uint64_t c_)
        : a(a_), b(b_), c(c_) {
    }

    bool operator==(const BeaverTriple &rhs) const {
        return a == rhs.a && b == rhs.b && c == rhs.c;
    }

    bool operator!=(const BeaverTriple &rhs) const {
        return !(*this == rhs);
    }
};

struct BeaverTriples {
    std::vector<uint64_t> a;
    std::vector<uint64_t> b;
    std::vector<uint64_t> c;

    BeaverTriples() = default;
    explicit BeaverTriples(size_t n) : a(n), b(n), c(n) {
    }

    BeaverTriples(const BeaverTriples &)            = delete;
    BeaverTriples &operator=(const BeaverTriples &) = delete;

    BeaverTriples(BeaverTriples &&) noexcept            = default;
    BeaverTriples &operator=(BeaverTriples &&) noexcept = default;

    size_t size() const noexcept {
        return a.size();
    }

    bool empty() const noexcept {
        return a.empty() && b.empty() && c.empty();
    }

    bool IsValid() const noexcept {
        return a.size() == b.size() && a.size() == c.size();
    }

    void Resize(size_t n) {
        a.resize(n);
        b.resize(n);
        c.resize(n);
    }

    BeaverTriple Get(size_t i) const {
        return BeaverTriple(a[i], b[i], c[i]);
    }

    void Set(size_t i, const BeaverTriple &t) {
        a[i] = t.a;
        b[i] = t.b;
        c[i] = t.c;
    }

    void Append(BeaverTriples t) {
        if (!t.IsValid()) {
            throw std::invalid_argument("BeaverTriples::Append: invalid triples");
        }
        if (t.empty()) {
            return;
        }
        if (!empty() && !IsValid()) {
            throw std::logic_error("BeaverTriples::Append: this triples invalid");
        }

        const size_t old_size = a.size();
        const size_t add_size = t.size();

        a.reserve(old_size + add_size);
        b.reserve(old_size + add_size);
        c.reserve(old_size + add_size);

        a.insert(a.end(),
                 std::make_move_iterator(t.a.begin()),
                 std::make_move_iterator(t.a.end()));

        b.insert(b.end(),
                 std::make_move_iterator(t.b.begin()),
                 std::make_move_iterator(t.b.end()));

        c.insert(c.end(),
                 std::make_move_iterator(t.c.begin()),
                 std::make_move_iterator(t.c.end()));
    }

    bool operator==(const BeaverTriples &rhs) const {
        return a == rhs.a && b == rhs.b && c == rhs.c;
    }
    bool operator!=(const BeaverTriples &rhs) const {
        return !(*this == rhs);
    }

    std::string ToString(size_t limit = 0, const std::string &delimiter = ", ") const {
        if (!IsValid()) {
            return "<invalid BeaverTriples>";
        }

        const size_t n = size();
        if (limit == 0 || limit > n) {
            limit = n;
        }

        std::ostringstream oss;
        oss << "[";
        for (size_t i = 0; i < limit; ++i) {
            oss << "(" << a[i] << "," << b[i] << "," << c[i] << ")";
            if (i + 1 < limit) {
                oss << delimiter;
            }
        }
        if (limit < n) {
            oss << "...";
        }
        oss << "]";
        return oss.str();
    }

    size_t CalculateSerializedSize() const {
        if (!IsValid()) {
            throw std::runtime_error("BeaverTriples::CalculateSerializedSize: invalid sizes");
        }
        const uint64_t n = static_cast<uint64_t>(size());
        return sizeof(uint64_t) + static_cast<size_t>(n) * 3 * sizeof(uint64_t);
    }

    void Serialize(std::vector<uint8_t> &buffer) const {
        if (!IsValid()) {
            throw std::runtime_error("BeaverTriples::Serialize: invalid sizes");
        }

        const size_t start = buffer.size();

        const uint64_t n = static_cast<uint64_t>(size());
        append_pod(buffer, n);

        append_array(buffer, a.data(), static_cast<size_t>(n));
        append_array(buffer, b.data(), static_cast<size_t>(n));
        append_array(buffer, c.data(), static_cast<size_t>(n));

        const size_t written  = buffer.size() - start;
        const size_t expected = CalculateSerializedSize();
        if (written != expected) {
            throw std::runtime_error("BeaverTriples::Serialize: serialized size mismatch: " +
                                     ToString(written) + " != " + ToString(expected));
        }
    }

    void Deserialize(const std::vector<uint8_t> &buffer) {
        size_t offset = 0;
        Deserialize(buffer, offset);

        if (offset != buffer.size()) {
            throw std::runtime_error("BeaverTriples::Deserialize: trailing bytes: " +
                                     ToString(buffer.size() - offset));
        }
    }

    void Deserialize(const std::vector<uint8_t> &buffer, size_t &offset) {
        const size_t start = offset;

        uint64_t n = 0;
        read_pod(buffer, offset, n);

        const size_t nn = static_cast<size_t>(n);
        Resize(nn);

        read_array(buffer, offset, a.data(), nn);
        read_array(buffer, offset, b.data(), nn);
        read_array(buffer, offset, c.data(), nn);

        if (!IsValid()) {
            throw std::runtime_error("BeaverTriples::Deserialize: invalid sizes after deserialize");
        }

        const size_t read     = offset - start;
        const size_t expected = CalculateSerializedSize();
        if (read != expected) {
            throw std::runtime_error("BeaverTriples::Deserialize: deserialized size mismatch: " +
                                     ToString(read) + " != " + ToString(expected));
        }
    }
};

}    // namespace sharing
}    // namespace ringoa

#endif    // SHARING_BEAVER_TRIPLES_H_
