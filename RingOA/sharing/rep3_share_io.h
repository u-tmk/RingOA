#ifndef SHARING_REP3_SHARE_IO_H_
#define SHARING_REP3_SHARE_IO_H_

#include <fstream>

#include "RingOA/utils/utils.h"
#include "rep3_share_types.h"

namespace ringoa {
namespace sharing {

// ------------------------------
// Low level helpers
// ------------------------------
inline void WriteBytes(std::ofstream &ofs, const void *p, size_t n) {
    ofs.write(reinterpret_cast<const char *>(p), static_cast<std::streamsize>(n));
}

inline void ReadBytes(std::ifstream &ifs, void *p, size_t n) {
    ifs.read(reinterpret_cast<char *>(p), static_cast<std::streamsize>(n));
}

inline void WriteU64(std::ofstream &ofs, uint64_t x) {
    WriteBytes(ofs, &x, sizeof(x));
}

inline uint64_t ReadU64(std::ifstream &ifs) {
    uint64_t x = 0;
    ReadBytes(ifs, &x, sizeof(x));
    return x;
}

// ------------------------------
// Element IO traits
// ------------------------------
template <typename T>
struct ElementIo;

// uint32_t
template <>
struct ElementIo<uint32_t> {
    static void Write(std::ofstream &ofs, const uint32_t &x) {
        WriteBytes(ofs, &x, sizeof(x));
    }
    static void Read(std::ifstream &ifs, uint32_t &x) {
        ReadBytes(ifs, &x, sizeof(x));
    }
    static void WriteArray(std::ofstream &ofs, const uint32_t *p, size_t n) {
        WriteBytes(ofs, p, sizeof(uint32_t) * n);
    }
    static void ReadArray(std::ifstream &ifs, uint32_t *p, size_t n) {
        ReadBytes(ifs, p, sizeof(uint32_t) * n);
    }
};

// uint64_t
template <>
struct ElementIo<uint64_t> {
    static void Write(std::ofstream &ofs, const uint64_t &x) {
        WriteBytes(ofs, &x, sizeof(x));
    }
    static void Read(std::ifstream &ifs, uint64_t &x) {
        ReadBytes(ifs, &x, sizeof(x));
    }
    static void WriteArray(std::ofstream &ofs, const uint64_t *p, size_t n) {
        WriteBytes(ofs, p, sizeof(uint64_t) * n);
    }
    static void ReadArray(std::ifstream &ifs, uint64_t *p, size_t n) {
        ReadBytes(ifs, p, sizeof(uint64_t) * n);
    }
};

// Canonical encoding: 16 raw bytes
template <>
struct ElementIo<block> {
    static constexpr size_t kSize = 16;

    static void Write(std::ofstream &ofs, const block &b) {
        static_assert(sizeof(block) >= kSize, "block must be at least 16 bytes");
        alignas(16) unsigned char buf[kSize];
        std::memcpy(buf, &b, kSize);
        WriteBytes(ofs, buf, kSize);
    }

    static void Read(std::ifstream &ifs, block &b) {
        static_assert(sizeof(block) >= kSize, "block must be at least 16 bytes");
        alignas(16) unsigned char buf[kSize];
        ReadBytes(ifs, buf, kSize);
        std::memcpy(&b, buf, kSize);
    }

    static void WriteArray(std::ofstream &ofs, const block *p, size_t n) {
        // write sequentially to avoid assumptions about padding, though sizeof(block) is typically 16
        for (size_t i = 0; i < n; ++i) {
            Write(ofs, p[i]);
        }
    }

    static void ReadArray(std::ifstream &ifs, block *p, size_t n) {
        for (size_t i = 0; i < n; ++i) {
            Read(ifs, p[i]);
        }
    }
};

// ------------------------------
// Serialize/Deserialize for Rep3Share<T>
// ------------------------------
template <typename T>
inline void SerializeToStream(std::ofstream &ofs, const Rep3Share<T> &s) {
    ElementIo<T>::Write(ofs, s.data[0]);
    ElementIo<T>::Write(ofs, s.data[1]);
}

template <typename T>
inline void DeserializeFromStream(std::ifstream &ifs, Rep3Share<T> &s) {
    ElementIo<T>::Read(ifs, s.data[0]);
    ElementIo<T>::Read(ifs, s.data[1]);
}

// ------------------------------
// Serialize/Deserialize for Rep3ShareVec<T>
// Format: [num_shares:u64] [share0 array] [share1 array]
// ------------------------------
template <typename T>
inline void SerializeToStream(std::ofstream &ofs, const Rep3ShareVec<T> &v) {
    v.RequireInvariant();

    WriteU64(ofs, static_cast<uint64_t>(v.num_shares));
    if (v.num_shares == 0)
        return;

    ElementIo<T>::WriteArray(ofs, v.data[0].data(), v.num_shares);
    ElementIo<T>::WriteArray(ofs, v.data[1].data(), v.num_shares);
}

template <typename T>
inline void DeserializeFromStream(std::ifstream &ifs, Rep3ShareVec<T> &v) {
    const uint64_t n64 = ReadU64(ifs);
    v.num_shares       = static_cast<size_t>(n64);
    v.data[0].resize(v.num_shares);
    v.data[1].resize(v.num_shares);

    if (v.num_shares == 0)
        return;

    ElementIo<T>::ReadArray(ifs, v.data[0].data(), v.num_shares);
    ElementIo<T>::ReadArray(ifs, v.data[1].data(), v.num_shares);
}

// ------------------------------
// Serialize/Deserialize for Rep3ShareMat<T>
// Format: [rows:u64][cols:u64] then Rep3ShareVec<T> of size rows*cols
// ------------------------------
template <typename T>
inline void SerializeToStream(std::ofstream &ofs, const Rep3ShareMat<T> &m) {
    WriteU64(ofs, static_cast<uint64_t>(m.rows));
    WriteU64(ofs, static_cast<uint64_t>(m.cols));
    SerializeToStream(ofs, m.shares);
}

template <typename T>
inline void DeserializeFromStream(std::ifstream &ifs, Rep3ShareMat<T> &m) {
    m.rows = static_cast<size_t>(ReadU64(ifs));
    m.cols = static_cast<size_t>(ReadU64(ifs));
    DeserializeFromStream(ifs, m.shares);

    const size_t expected = m.rows * m.cols;
    if (m.shares.num_shares != expected) {
        throw std::runtime_error("Rep3ShareMat size mismatch: rows*cols does not match serialized shares");
    }
}

// ------------------------------
// Rep3ShareIo wrapper
// ------------------------------
class Rep3ShareIo {
public:
    Rep3ShareIo() = default;

    template <typename ShareType>
    static void SaveShare(const std::string &file_path, const ShareType &share) {
        const std::string full_path = file_path + ".sh.bin";
        EnsureParentDirExists(full_path);

        std::ofstream ofs(full_path, std::ios::binary | std::ios::out);
        if (!ofs.is_open()) {
            throw std::runtime_error("Failed to open file: " + full_path);
        }

        ofs.exceptions(std::ios::badbit | std::ios::failbit);

        static thread_local std::vector<char> obuf(1 << 20);
        ofs.rdbuf()->pubsetbuf(obuf.data(), static_cast<std::streamsize>(obuf.size()));

        SerializeToStream(ofs, share);
    }

    template <typename ShareType>
    static void LoadShare(const std::string &file_path, ShareType &share) {
        const std::string full_path = file_path + ".sh.bin";

        std::ifstream ifs(full_path, std::ios::binary | std::ios::in);
        if (!ifs.is_open()) {
            throw std::runtime_error("Failed to open file: " + full_path);
        }

        ifs.exceptions(std::ios::badbit | std::ios::failbit);

        static thread_local std::vector<char> ibuf(1 << 20);
        ifs.rdbuf()->pubsetbuf(ibuf.data(), static_cast<std::streamsize>(ibuf.size()));

        DeserializeFromStream(ifs, share);
    }
};

}    // namespace sharing
}    // namespace ringoa

#endif    // SHARING_REP3_SHARE_IO_H_
