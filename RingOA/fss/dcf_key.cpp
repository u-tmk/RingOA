#include "dcf_key.h"

#include <cstring>

#include "RingOA/utils/bytes.h"
#include "RingOA/utils/logger.h"
#include "RingOA/utils/to_string.h"

namespace ringoa {
namespace fss {
namespace dcf {

DcfParameters::DcfParameters(const uint64_t n, const uint64_t e)
    : input_bitsize_(n), element_bitsize_(e) {
    ValidateParameters();
}

void DcfParameters::ValidateParameters() const {
    if (input_bitsize_ == 0 || element_bitsize_ == 0) {
        throw std::invalid_argument(LOC + " DcfParameters: input_bitsize and element_bitsize must be > 0");
    }

    if (input_bitsize_ > 32) {
        throw std::invalid_argument(LOC + " DcfParameters: input_bitsize must be <= 32 (got " + ToString(input_bitsize_) + ")");
    }
}

void DcfParameters::PrintParametersDebug(bool with_header, int key_width) const {
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    if (with_header) {
        Logger::DebugLog(LOC, Logger::MakeSeparatorLine());
        Logger::DebugLog(LOC, "[DCF Params]");
        Logger::DebugLog(LOC, Logger::MakeSeparatorLine());
    }

    Logger::DebugLog(LOC, Logger::FormatKeyValue("InputBits", ToString(input_bitsize_), key_width));
    Logger::DebugLog(LOC, Logger::FormatKeyValue("OutputBits", ToString(element_bitsize_), key_width));

    if (with_header) {
        Logger::DebugLog(LOC, Logger::MakeSeparatorLine());
    }
#endif
}

DcfKey::DcfKey(const uint64_t id, const DcfParameters &params)
    : party_id(id),
      init_seed(zero_block),
      cw_length(params.GetInputBitsize()),
      cw_seed(std::make_unique<block[]>(cw_length)),
      cw_control_left(std::make_unique<bool[]>(cw_length)),
      cw_control_right(std::make_unique<bool[]>(cw_length)),
      cw_value(std::make_unique<uint64_t[]>(cw_length)),
      output(0) {
    std::fill(cw_seed.get(), cw_seed.get() + cw_length, zero_block);
    std::fill(cw_control_left.get(), cw_control_left.get() + cw_length, false);
    std::fill(cw_control_right.get(), cw_control_right.get() + cw_length, false);
    std::fill(cw_value.get(), cw_value.get() + cw_length, 0);
}

bool DcfKey::operator==(const DcfKey &rhs) const {
    if (party_id != rhs.party_id)
        return false;
    if (init_seed != rhs.init_seed)
        return false;
    if (cw_length != rhs.cw_length)
        return false;
    if (output != rhs.output)
        return false;

    for (uint64_t i = 0; i < cw_length; ++i) {
        if (cw_seed[i] != rhs.cw_seed[i])
            return false;
        if (cw_control_left[i] != rhs.cw_control_left[i])
            return false;
        if (cw_control_right[i] != rhs.cw_control_right[i])
            return false;
        if (cw_value[i] != rhs.cw_value[i])
            return false;
    }
    return true;
}

size_t DcfKey::CalculateSerializedSize() const {
    size_t size = 0;

    size += sizeof(party_id);
    size += sizeof(init_seed);
    size += sizeof(cw_length);
    size += sizeof(block) * cw_length;
    size += sizeof(uint8_t) * cw_length * 2;
    size += sizeof(uint64_t) * cw_length;
    size += sizeof(output);

    return size;
}

void DcfKey::Serialize(std::vector<uint8_t> &buffer) const {
    const size_t start = buffer.size();

    // Party ID and Initial seed
    append_pod(buffer, party_id);
    append_pod(buffer, init_seed);

    // Correction words
    append_pod(buffer, cw_length);
    append_array(buffer, cw_seed.get(), static_cast<size_t>(cw_length));

    // Store bool arrays as uint8_t (0/1)
    for (size_t i = 0; i < static_cast<size_t>(cw_length); ++i) {
        const uint8_t v = cw_control_left[i] ? 1 : 0;
        append_pod(buffer, v);
    }
    for (size_t i = 0; i < static_cast<size_t>(cw_length); ++i) {
        const uint8_t v = cw_control_right[i] ? 1 : 0;
        append_pod(buffer, v);
    }
    append_array(buffer, cw_value.get(), static_cast<size_t>(cw_length));

    // Output
    append_pod(buffer, output);

    // Check size
    const size_t written  = buffer.size() - start;
    const size_t expected = CalculateSerializedSize();
    if (written != expected) {
        throw std::runtime_error(LOC + " DcfKey::Serialize: serialized size mismatch: " +
                                 ToString(written) + " != " + ToString(expected));
    }
}

void DcfKey::Deserialize(const std::vector<uint8_t> &buffer) {
    size_t offset = 0;
    Deserialize(buffer, offset);
}

void DcfKey::Deserialize(const std::vector<uint8_t> &buffer, size_t &offset) {
    const size_t start = offset;

    // Party ID and Initial seed
    read_pod(buffer, offset, party_id);
    read_pod(buffer, offset, init_seed);
    read_pod(buffer, offset, cw_length);

    const size_t len = static_cast<size_t>(cw_length);

    // Allocate
    cw_seed          = std::make_unique<block[]>(len);
    cw_control_left  = std::make_unique<bool[]>(len);
    cw_control_right = std::make_unique<bool[]>(len);
    cw_value         = std::make_unique<uint64_t[]>(len);

    // Seeds
    read_array(buffer, offset, cw_seed.get(), len);

    // bool arrays stored as uint8_t
    for (size_t i = 0; i < len; ++i) {
        uint8_t v = 0;
        read_pod(buffer, offset, v);
        cw_control_left[i] = (v != 0);
    }
    for (size_t i = 0; i < len; ++i) {
        uint8_t v = 0;
        read_pod(buffer, offset, v);
        cw_control_right[i] = (v != 0);
    }

    read_array(buffer, offset, cw_value.get(), len);

    // Output
    read_pod(buffer, offset, output);

    // Check size
    const size_t read     = offset - start;
    const size_t expected = CalculateSerializedSize();
    if (read != expected) {
        throw std::runtime_error(LOC + " DcfKey::Deserialize: deserialized size mismatch: " +
                                 ToString(read) + " != " + ToString(expected));
    }
}

std::string DcfKey::GetKeyInfo() const {
    std::ostringstream oss;
    oss << "[DCF Key] P" << party_id << " - CW Length: " << cw_length;
    return oss.str();
}

void DcfKey::PrintKeyTrace() const {
#if LOG_LEVEL >= LOG_LEVEL_TRACE
    Logger::TraceLog(LOC, Logger::StrWithSep("DCF Key"));
    Logger::TraceLog(LOC, "Party ID: " + ToString(this->party_id));
    Logger::TraceLog(LOC, "Initial seed: " + Format(init_seed));
    Logger::TraceLog(LOC, Logger::StrWithSep("Correction words"));
    for (uint64_t i = 0; i < this->cw_length; ++i) {
        Logger::TraceLog(LOC, "Level(" + ToString(i) + ") Seed: " + Format(this->cw_seed[i]));
        Logger::TraceLog(LOC, "Level(" + ToString(i) + ") Control bit (L, R): " + ToString(this->cw_control_left[i]) + ", " + ToString(this->cw_control_right[i]));
        Logger::TraceLog(LOC, "Level(" + ToString(i) + ") Value: " + ToString(this->cw_value[i]));
    }
    Logger::TraceLog(LOC, "Output: " + ToString(output));
    Logger::TraceLog(LOC, Logger::MakeSeparatorLine());
#endif
}

}    // namespace dcf
}    // namespace fss
}    // namespace ringoa
