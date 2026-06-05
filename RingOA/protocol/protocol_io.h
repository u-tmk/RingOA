#ifndef PROTOCOL_KEY_IO_H_
#define PROTOCOL_KEY_IO_H_

#include <string>

#include "RingOA/utils/file_io.h"
#include "RingOA/utils/logger.h"

namespace ringoa {
namespace proto {

class KeyIo {
public:
    KeyIo() = default;

    template <typename KeyType>
    static void SaveKey(const std::string &file_path, const KeyType &key) {
        std::vector<uint8_t> buffer;
        key.Serialize(buffer);
        FileIo io(".key.bin");
        io.WriteBinary(file_path, buffer);
    }

    template <typename KeyType>
    static void LoadKey(const std::string &file_path, KeyType &key) {
        std::vector<uint8_t> buffer;
        FileIo               io(".key.bin");
        io.ReadBinary(file_path, buffer);
        key.Deserialize(buffer);
    }

    template <typename KeyType, typename Parameters>
    static void LoadKey(const std::string &file_path, KeyType &key, const Parameters &params) {
        std::vector<uint8_t> buffer;
        FileIo               io(".key.bin");
        io.ReadBinary(file_path, buffer);
        key.Deserialize(buffer, params);
    }
};

class ProtocolIo {
public:
    template <typename T>
    static void SaveToFile(const std::string &file_path, const T &obj) {
        std::vector<uint8_t> buffer;
        obj.Serialize(buffer);

        FileIo file_io;
        file_io.WriteBinary(file_path, buffer);
    }

    template <typename T, typename Parameters>
    static void LoadFromFile(
        const std::string &file_path,
        T                 &obj,
        const Parameters  &params) {

        FileIo               file_io;
        std::vector<uint8_t> buffer;
        file_io.ReadBinary(file_path, buffer);
        obj.Deserialize(buffer, params);
    }
};

}    // namespace proto
}    // namespace ringoa

#endif    // PROTOCOL_KEY_IO_H_
