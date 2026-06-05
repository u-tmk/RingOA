#include "ddcf.h"

#include <cstring>

#include "RingOA/utils/bytes.h"
#include "RingOA/utils/logger.h"
#include "RingOA/utils/rng.h"
#include "RingOA/utils/to_string.h"
#include "RingOA/utils/utils.h"

namespace ringoa {
namespace proto {

void DdcfParameters::PrintParametersDebug(bool with_header, int key_width) const {
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    if (with_header) {
        Logger::DebugLog(LOC, Logger::MakeSeparatorLine());
        Logger::DebugLog(LOC, "[DDCF Params]");
        Logger::DebugLog(LOC, Logger::MakeSeparatorLine());
    }

    params_.PrintParametersDebug(false, key_width);

    if (with_header) {
        Logger::DebugLog(LOC, Logger::MakeSeparatorLine());
    }
#endif
}

DdcfKey::DdcfKey(const uint64_t id, const DdcfParameters &params)
    : dcf_key(id, params.GetParameters()),
      mask(0) {
}

size_t DdcfKey::CalculateSerializedSize() const {
    size_t size = 0;

    size += dcf_key.CalculateSerializedSize();
    size += sizeof(mask);
    return size;
}

void DdcfKey::Serialize(std::vector<uint8_t> &buffer) const {
    const size_t start = buffer.size();

    // Serialize the DDCF key
    dcf_key.Serialize(buffer);

    // Serialize the mask
    append_pod(buffer, mask);

    // Check size
    const size_t written  = buffer.size() - start;
    const size_t expected = CalculateSerializedSize();
    if (written != expected) {
        throw std::runtime_error(LOC + " DdcfKey::Serialize: serialized size mismatch: " +
                                 ToString(written) + " != " + ToString(expected));
    }
}

void DdcfKey::Deserialize(const std::vector<uint8_t> &buffer) {
    size_t offset = 0;
    Deserialize(buffer, offset);
}

void DdcfKey::Deserialize(const std::vector<uint8_t> &buffer, size_t &offset) {
    const size_t start = offset;

    // Deserialize the DDCF key
    dcf_key.Deserialize(buffer, offset);

    // Deserialize the mask
    read_pod(buffer, offset, mask);

    // Check size
    const size_t read     = offset - start;
    const size_t expected = CalculateSerializedSize();
    if (read != expected) {
        throw std::runtime_error(LOC + " DdcfKey::Deserialize: deserialized size mismatch: " +
                                 ToString(read) + " != " + ToString(expected));
    }
}

std::string DdcfKey::GetKeyInfo() const {
    return dcf_key.GetKeyInfo() +
           ", mask = " + ToString(mask);
}

void DdcfKey::PrintKey() const {
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "[DDCF Key] (P" + ToString(dcf_key.party_id) + ")");
    Logger::DebugLog(LOC, GetKeyInfo());
#endif
}

Ddcf::Ddcf(const DdcfParameters &params)
    : params_(params), gen_(params.GetParameters()), eval_(params.GetParameters()) {
}

std::pair<DdcfKey, DdcfKey> Ddcf::GenerateKeys(uint64_t alpha, uint64_t beta_1, uint64_t beta_2) const {
    uint64_t e = params_.GetOutputBitsize();

    std::array<DdcfKey, 2>      keys     = {DdcfKey(0, params_), DdcfKey(1, params_)};
    std::pair<DdcfKey, DdcfKey> key_pair = std::make_pair(std::move(keys[0]), std::move(keys[1]));

    // Calculate beta for DCF
    uint64_t beta = Mod2N(beta_1 - beta_2, e);

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "Generate DDCF keys: (n, e)=(" +
                              ToString(params_.GetInputBitsize()) + ", " +
                              ToString(params_.GetOutputBitsize()) + ")");
    Logger::DebugLog(LOC, "alpha: " + ToString(alpha));
    Logger::DebugLog(LOC, "(beta_1, beta_2): (" + ToString(beta_1) + ", " + ToString(beta_2) + ")");
    Logger::DebugLog(LOC, "beta (= beta_1 - beta_2): " + ToString(beta));
#endif

    // Calculate beta
    std::pair<fss::dcf::DcfKey, fss::dcf::DcfKey> dcf_keys = gen_.GenerateKeys(alpha, beta);

    // Set DDCF keys for each party
    key_pair.first.dcf_key  = std::move(dcf_keys.first);
    key_pair.second.dcf_key = std::move(dcf_keys.second);

    // Generate share of beta_2
    key_pair.first.mask  = Mod2N(GlobalRng::Rand<uint64_t>(), e);
    key_pair.second.mask = Mod2N(beta_2 - key_pair.first.mask, e);

    // Return the generated keys as a pair.
    return key_pair;
}

uint64_t Ddcf::EvaluateAt(const DdcfKey &key, uint64_t x) const {
    uint64_t party_id = key.dcf_key.party_id;

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "Evaluate DDCF key");
    std::string party_str = "[P" + ToString(party_id) + "] ";
    Logger::DebugLog(LOC, party_str + " x: " + ToString(x));
#endif

    uint64_t output = eval_.EvaluateAt(key.dcf_key, x);
    output          = Mod2N(output + key.mask, params_.GetOutputBitsize());
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, party_str + " output: " + ToString(output));
#endif

    return output;
}

void Ddcf::EvaluateAt(const std::vector<DdcfKey> &keys, const std::vector<uint64_t> &x, std::vector<uint64_t> &outputs) const {
    if (keys.size() != x.size()) {
        throw std::invalid_argument(
            "DcfEvaluator::EvaluateAt: keys.size() != x.size() (" +
            ToString(keys.size()) + " vs " + ToString(x.size()) + ")");
    }

    if (outputs.size() != x.size()) {
        outputs.resize(x.size());
    }

    for (std::size_t i = 0; i < keys.size(); ++i) {
        outputs[i] = EvaluateAt(keys[i], x[i]);    // may throw; let it propagate
    }
}

}    // namespace proto
}    // namespace ringoa
