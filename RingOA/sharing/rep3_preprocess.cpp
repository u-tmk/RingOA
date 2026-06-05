#include "rep3_preprocess.h"

#include "RingOA/utils/bytes.h"
#include "RingOA/utils/file_io.h"
#include "RingOA/utils/logger.h"
#include "RingOA/utils/rng.h"
#include "RingOA/utils/to_string.h"

namespace ringoa {
namespace sharing {

size_t Rep3PreprocessMsg::CalculateSerializedSize() const {
    return sizeof(block);
}

void Rep3PreprocessMsg::Serialize(std::vector<uint8_t> &buffer) const {
    const size_t start = buffer.size();

    append_pod(buffer, prf_key);

    const size_t written  = buffer.size() - start;
    const size_t expected = CalculateSerializedSize();
    if (written != expected) {
        throw std::runtime_error(LOC + " Rep3PreprocessMsg::Serialize: serialized size mismatch: " +
                                 ToString(written) + " != " + ToString(expected));
    }
}

void Rep3PreprocessMsg::Deserialize(const std::vector<uint8_t> &buffer) {
    size_t offset = 0;
    Deserialize(buffer, offset);

    if (offset != buffer.size()) {
        throw std::runtime_error(LOC + " Rep3PreprocessMsg::Deserialize: trailing bytes: " +
                                 ToString(buffer.size() - offset));
    }
}

void Rep3PreprocessMsg::Deserialize(const std::vector<uint8_t> &buffer,
                                    size_t                     &offset) {
    const size_t start = offset;

    read_pod(buffer, offset, prf_key);

    const size_t read     = offset - start;
    const size_t expected = CalculateSerializedSize();
    if (read != expected) {
        throw std::runtime_error(LOC + " Rep3PreprocessMsg::Deserialize: serialized size mismatch: " +
                                 ToString(read) + " != " + ToString(expected));
    }
}

Rep3PreprocessData Rep3PreprocessData::FromMessage(
    Rep3PreprocessMsg &&from_next,
    const block        &prf_key_self_in) {
    return Rep3PreprocessData(prf_key_self_in, from_next.prf_key);
}

size_t Rep3PreprocessData::CalculateSerializedSize() const {
    return sizeof(block) + sizeof(block);
}

void Rep3PreprocessData::Serialize(std::vector<uint8_t> &buffer) const {
    const size_t start = buffer.size();

    append_pod(buffer, prf_key_self);
    append_pod(buffer, prf_key_next);

    const size_t written  = buffer.size() - start;
    const size_t expected = CalculateSerializedSize();
    if (written != expected) {
        throw std::runtime_error(LOC + " Rep3PreprocessData::Serialize: serialized size mismatch: " +
                                 ToString(written) + " != " + ToString(expected));
    }
}

void Rep3PreprocessData::Deserialize(const std::vector<uint8_t> &buffer) {
    size_t offset = 0;
    Deserialize(buffer, offset);

    if (offset != buffer.size()) {
        throw std::runtime_error(LOC + " Rep3PreprocessData::Deserialize: trailing bytes: " +
                                 ToString(buffer.size() - offset));
    }
}

void Rep3PreprocessData::Deserialize(const std::vector<uint8_t> &buffer,
                                     size_t                     &offset) {
    const size_t start = offset;

    read_pod(buffer, offset, prf_key_self);
    read_pod(buffer, offset, prf_key_next);

    const size_t read     = offset - start;
    const size_t expected = CalculateSerializedSize();
    if (read != expected) {
        throw std::runtime_error(LOC + " Rep3PreprocessData::Deserialize: serialized size mismatch: " +
                                 ToString(read) + " != " + ToString(expected));
    }
}

Rep3PrfOutgoing MakeRep3PrfOutgoing() {
    Rep3PrfOutgoing out;
    out.prf_key_self    = GlobalRng::Rand<block>();
    out.to_prev.prf_key = out.prf_key_self;
    return out;
}

Rep3PreprocessMsg ExchangeRep3PrfMsg(Channels &chls, const Rep3PreprocessMsg &to_prev) {
    std::vector<uint8_t> buf;
    to_prev.Serialize(buf);
    chls.prev.send(buf);

    std::vector<uint8_t> in;
    chls.next.recv(in);

    Rep3PreprocessMsg from_next;
    from_next.Deserialize(in);
    return from_next;
}

Rep3PreprocessData BuildRep3PrfData(block prf_key_self, Rep3PreprocessMsg &&from_next) {
    return Rep3PreprocessData::FromMessage(std::move(from_next), prf_key_self);
}

Rep3PreprocessData PreprocessRep3PrfKeys(Channels &chls) {
    auto out       = MakeRep3PrfOutgoing();
    auto from_next = ExchangeRep3PrfMsg(chls, out.to_prev);
    return BuildRep3PrfData(out.prf_key_self, std::move(from_next));
}

void SaveRep3PreprocessDataToFile(const std::string &path, const Rep3PreprocessData &data) {
    std::vector<uint8_t> buffer;
    data.Serialize(buffer);
    FileIo io(".key");
    io.WriteBinary(path, buffer);
}

void LoadRep3PreprocessDataFromFile(const std::string &path, Rep3PreprocessData &data) {
    FileIo               io(".key");
    std::vector<uint8_t> buffer;
    io.ReadBinary(path, buffer);
    data.Deserialize(buffer);
}

}    // namespace sharing
}    // namespace ringoa
