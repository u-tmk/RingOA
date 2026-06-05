#ifndef UTILS_BYTES_H_
#define UTILS_BYTES_H_

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace ringoa {

inline void append_raw(std::vector<uint8_t> &buffer,
                       const void           *data,
                       size_t                size) {
    const auto *b = static_cast<const uint8_t *>(data);
    buffer.insert(buffer.end(), b, b + size);
}

template <typename T>
inline void append_pod(std::vector<uint8_t> &buffer,
                       const T              &value) {
    static_assert(std::is_trivially_copyable_v<T>);
    append_raw(buffer, &value, sizeof(T));
}

template <typename T>
inline void append_array(std::vector<uint8_t> &buffer,
                         const T              *data,
                         size_t                count) {
    static_assert(std::is_trivially_copyable_v<T>);
    append_raw(buffer, data, sizeof(T) * count);
}

inline void require(const std::vector<uint8_t> &buffer,
                    size_t                      offset,
                    size_t                      n) {
    if (offset + n > buffer.size()) {
        throw std::runtime_error("Buffer underflow in deserialization");
    }
}

inline void read_raw(const std::vector<uint8_t> &buffer,
                     size_t                     &offset,
                     void                       *dst,
                     size_t                      n) {
    require(buffer, offset, n);
    std::memcpy(dst, buffer.data() + offset, n);
    offset += n;
}

template <typename T>
inline void read_pod(const std::vector<uint8_t> &buffer,
                     size_t                     &offset,
                     T                          &value) {
    static_assert(std::is_trivially_copyable_v<T>);
    read_raw(buffer, offset, &value, sizeof(T));
}

template <typename T>
inline void read_array(const std::vector<uint8_t> &buffer,
                       size_t                     &offset,
                       T                          *data,
                       size_t                      count) {
    static_assert(std::is_trivially_copyable_v<T>);
    read_raw(buffer, offset, data, sizeof(T) * count);
}

}    // namespace ringoa

#endif    // UTILS_BYTES_H_
