#include "dpf_key.h"

#include <cstring>

#include "RingOA/utils/bytes.h"
#include "RingOA/utils/logger.h"
#include "RingOA/utils/to_string.h"

namespace ringoa {
namespace fss {
namespace dpf {

DpfParameters::DpfParameters(const uint64_t n, const uint64_t e, EvalType eval_type, OutputType output_mode)
    : input_bitsize_(n),
      element_bitsize_(e),
      enable_et_(true),
      fde_type_(eval_type),
      output_mode_(output_mode) {
    Resolve_();
    ValidateOrThrow_();
}

void DpfParameters::Resolve_() {
    // Small-n -> Naive
    if ((element_bitsize_ == 1 && input_bitsize_ < 10) ||
        (element_bitsize_ > 1 && input_bitsize_ <= 8)) {
        if (fde_type_ != EvalType::kBruteForce) {
            Logger::WarnLog(LOC, "Switching to naive evaluation: EvalType -> Naive");
        }
        fde_type_ = EvalType::kBruteForce;
    }

    // Disable ET for Naive / IterDepthFirst
    if (fde_type_ == EvalType::kBruteForce || fde_type_ == EvalType::kIterativeFullDepth || fde_type_ == EvalType::kHybridBatchedFullDepth) {
        if (enable_et_) {
            Logger::WarnLog(LOC, "Disabling early termination for non-ET strategy: ET OFF");
        }
        enable_et_   = false;
        output_mode_ = OutputType::kShiftedAdditive;    // keep only if truly required
    }

    // Compute terminate_bitsize_
    if (enable_et_) {
        int32_t nu = 0;
        if (element_bitsize_ == 1) {
            nu = static_cast<int32_t>(input_bitsize_) - 7;    // 2^7
        } else if (element_bitsize_ < 17) {
            nu = static_cast<int32_t>(input_bitsize_) - 3;    // 8 blocks
            if (output_mode_ == OutputType::kSingleBitMask) {
                Logger::WarnLog(LOC, "Switching output to Additive for e!=1: OutputType -> ShiftedAdditive");
                output_mode_ = OutputType::kShiftedAdditive;
            }
        } else if (element_bitsize_ < 33) {
            nu = static_cast<int32_t>(input_bitsize_) - 2;    // 4 blocks
            if (output_mode_ == OutputType::kSingleBitMask) {
                Logger::WarnLog(LOC, "Switching output to Additive for e!=1: OutputType -> ShiftedAdditive");
                output_mode_ = OutputType::kShiftedAdditive;
            }
        }
        terminate_bitsize_ = static_cast<uint64_t>(std::max(nu, 0));
    } else {
        terminate_bitsize_ = input_bitsize_;
    }
}

void DpfParameters::ValidateOrThrow_() const {
    if (input_bitsize_ == 0 || element_bitsize_ == 0) {
        throw std::invalid_argument("input_bitsize and element_bitsize must be > 0");
    }
    if (input_bitsize_ > 32) {
        throw std::invalid_argument("input_bitsize must be <= 32 (got " + ToString(input_bitsize_) + ")");
    }
    if (enable_et_) {
        if (terminate_bitsize_ > input_bitsize_) {
            throw std::invalid_argument("nu (" + ToString(terminate_bitsize_) + ") must be <= n (" + ToString(input_bitsize_) + ") when ET is enabled");
        }
    } else {
        if (terminate_bitsize_ != input_bitsize_) {
            throw std::invalid_argument("nu (" + ToString(terminate_bitsize_) + ") must equal n (" + ToString(input_bitsize_) + ") when ET is disabled");
        }
    }
    if (fde_type_ == EvalType::kBruteForce && enable_et_) {
        throw std::invalid_argument("EvalType::kBruteForce requires ET to be disabled");
    }
    if (element_bitsize_ != 1 && output_mode_ == OutputType::kSingleBitMask) {
        throw std::invalid_argument("OutputType::kSingleBitMask requires element_bitsize == 1");
    }
}

void DpfParameters::PrintParametersDebug(bool with_header, int key_width) const {
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    if (with_header) {
        Logger::DebugLog(LOC, Logger::MakeSeparatorLine());
        Logger::DebugLog(LOC, "[DPF Params]");
        Logger::DebugLog(LOC, Logger::MakeSeparatorLine());
    }

    Logger::DebugLog(LOC, Logger::FormatKeyValue("InputBits", ToString(input_bitsize_), key_width));
    Logger::DebugLog(LOC, Logger::FormatKeyValue("OutputBits", ToString(element_bitsize_), key_width));
    Logger::DebugLog(LOC, Logger::FormatKeyValue("TerminateBits", ToString(terminate_bitsize_), key_width));
    Logger::DebugLog(LOC, Logger::FormatKeyValue("EarlyTermination", enable_et_ ? "ON" : "OFF", key_width));
    Logger::DebugLog(LOC, Logger::FormatKeyValue("EvalType", GetEvalTypeString(fde_type_), key_width));
    Logger::DebugLog(LOC, Logger::FormatKeyValue("OutputType", GetOutputTypeString(output_mode_), key_width));

    if (with_header) {
        Logger::DebugLog(LOC, Logger::MakeSeparatorLine());
    }
#endif
}

DpfKey::DpfKey(const uint64_t id, const DpfParameters &params)
    : party_id(id),
      init_seed(zero_block),
      cw_length(params.GetTerminateBitsize()),
      cw_seed(std::make_unique<block[]>(cw_length)),
      cw_control_left(std::make_unique<bool[]>(cw_length)),
      cw_control_right(std::make_unique<bool[]>(cw_length)),
      output(zero_block) {
    std::fill(cw_seed.get(), cw_seed.get() + cw_length, zero_block);
    std::fill(cw_control_left.get(), cw_control_left.get() + cw_length, false);
    std::fill(cw_control_right.get(), cw_control_right.get() + cw_length, false);
}

bool DpfKey::operator==(const DpfKey &rhs) const {
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
    }
    return true;
}

size_t DpfKey::CalculateSerializedSize() const {
    size_t size =
        sizeof(party_id) +
        sizeof(init_seed) +
        sizeof(cw_length) +
        sizeof(block) * cw_length +
        sizeof(uint8_t) * cw_length +
        sizeof(uint8_t) * cw_length +
        sizeof(output);
    return size;
}

void DpfKey::Serialize(std::vector<uint8_t> &buffer) const {
    const size_t start = buffer.size();

    // Party ID and initial seed
    append_pod(buffer, party_id);
    append_pod(buffer, init_seed);

    // Correction words length + seeds
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

    // Output
    append_pod(buffer, output);

    // Check size
    const size_t written  = buffer.size() - start;
    const size_t expected = CalculateSerializedSize();
    if (written != expected) {
        throw std::runtime_error(LOC + " DpfKey::Serialize: serialized size mismatch: " +
                                 ToString(written) + " != " + ToString(expected));
    }
}

void DpfKey::Deserialize(const std::vector<uint8_t> &buffer) {
    size_t offset = 0;
    Deserialize(buffer, offset);
}

void DpfKey::Deserialize(const std::vector<uint8_t> &buffer, size_t &offset) {
    const size_t start = offset;

    // Party ID and initial seed
    read_pod(buffer, offset, party_id);
    read_pod(buffer, offset, init_seed);
    read_pod(buffer, offset, cw_length);

    const size_t len = static_cast<size_t>(cw_length);

    // Allocate
    cw_seed          = std::make_unique<block[]>(len);
    cw_control_left  = std::make_unique<bool[]>(len);
    cw_control_right = std::make_unique<bool[]>(len);

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

    // Output
    read_pod(buffer, offset, output);

    // Check size
    const size_t read     = offset - start;
    const size_t expected = CalculateSerializedSize();
    if (read != expected) {
        throw std::runtime_error(LOC + " DpfKey::Deserialize: deserialized size mismatch: " +
                                 ToString(read) + " != " + ToString(expected));
    }
}

std::string DpfKey::GetKeyInfo() const {
    std::ostringstream oss;
    oss << "[DPF Key] (P" << party_id << ") CW Length = " << cw_length;
    return oss.str();
}

void DpfKey::PrintKeyTrace() const {
#if LOG_LEVEL >= LOG_LEVEL_TRACE
    Logger::TraceLog(LOC, Logger::StrWithSep("DPF Key"));
    Logger::TraceLog(LOC, "Party ID: " + ToString(this->party_id));
    Logger::TraceLog(LOC, "Initial seed: " + Format(init_seed));
    Logger::TraceLog(LOC, Logger::StrWithSep("Correction words"));
    for (uint64_t i = 0; i < this->cw_length; ++i) {
        Logger::TraceLog(LOC, "Level(" + ToString(i) + ") Seed: " + Format(this->cw_seed[i]));
        Logger::TraceLog(LOC, "Level(" + ToString(i) + ") Control bit (L, R): " + ToString(this->cw_control_left[i]) + ", " + ToString(this->cw_control_right[i]));
    }
    Logger::TraceLog(LOC, "Output: " + Format(output));
    Logger::TraceLog(LOC, Logger::MakeSeparatorLine());
#endif
}

}    // namespace dpf
}    // namespace fss
}    // namespace ringoa
