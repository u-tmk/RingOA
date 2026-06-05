#include "beaver_triples_io.h"

#include "RingOA/utils/file_io.h"
#include "RingOA/utils/to_string.h"

namespace ringoa {
namespace sharing {

void SaveTriplesToFile(const std::string   &path,
                       const BeaverTriples &triples) {
    std::vector<uint8_t> buffer;
    triples.Serialize(buffer);

    FileIo io(".bt.bin");
    io.WriteBinary(path, buffer);
}

void LoadTriplesFromFile(const std::string &path,
                         BeaverTriples     &triples) {
    std::vector<uint8_t> buffer;

    FileIo io(".bt.bin");
    io.ReadBinary(path, buffer);

    size_t off = 0;
    triples.Deserialize(buffer, off);

    if (off != buffer.size()) {
        throw std::runtime_error("LoadTriplesFromFile: trailing bytes in " + path + io.GetExtension());
    }
}

void Save2PTriplesShares(const std::string   &base_path,
                         const BeaverTriples &share0,
                         const BeaverTriples &share1) {
    SaveTriplesToFile(base_path + "_0", share0);
    SaveTriplesToFile(base_path + "_1", share1);
}

BeaverTriples Load2PTriplesShare(const std::string &base_path,
                                 uint64_t           party_id) {
    BeaverTriples triples;
    LoadTriplesFromFile(base_path + "_" + ToString(party_id), triples);
    return triples;
}

}    // namespace sharing
}    // namespace ringoa
