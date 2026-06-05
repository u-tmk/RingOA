#include "equality.h"

#include <cstring>

#include "RingOA/utils/bytes.h"
#include "RingOA/utils/logger.h"
#include "RingOA/utils/rng.h"
#include "RingOA/utils/to_string.h"
#include "RingOA/utils/utils.h"

namespace ringoa {
namespace proto {

void EqualityParameters::PrintParametersDebug(bool with_header, int key_width) const {
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    if (with_header) {
        Logger::DebugLog(LOC, Logger::MakeSeparatorLine());
        Logger::DebugLog(LOC, "[Equality Params]");
        Logger::DebugLog(LOC, Logger::MakeSeparatorLine());
    }

    Logger::DebugLog(LOC, Logger::FormatKeyValue("InputDomainBits", ToString(input_domain_bits_), key_width));
    Logger::DebugLog(LOC, Logger::FormatKeyValue("DpfOutputBits", ToString(dpf_out_bits_), key_width));

    params_.PrintParametersDebug(false, key_width);

    if (with_header) {
        Logger::DebugLog(LOC, Logger::MakeSeparatorLine());
    }
#endif
}

EqualityKeys::EqualityKeys(
    uint64_t                  party_id,
    const EqualityParameters &params,
    size_t                    count)
    : dpf_key(),
      shr1_in(count, 0),
      shr2_in(count, 0) {
    dpf_key.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        dpf_key.emplace_back(party_id, params.GetParameters());
    }
}

size_t EqualityKeys::CalculateSerializedSize() const {
    size_t size = 0;

    size += sizeof(uint64_t);
    const size_t num = Size();
    for (size_t i = 0; i < num; ++i) {
        size += dpf_key[i].CalculateSerializedSize();
    }
    size += num * sizeof(uint64_t);
    size += num * sizeof(uint64_t);
    return size;
}

void EqualityKeys::Serialize(std::vector<uint8_t> &buffer) const {
    const size_t start = buffer.size();

    // Number of keys
    const uint64_t num = Size();
    append_pod(buffer, num);

    // Serialize each DPF key
    for (const auto &key : dpf_key) {
        key.Serialize(buffer);
    }

    // Serialize shared random values
    append_array(buffer, shr1_in.data(), static_cast<size_t>(num));
    append_array(buffer, shr2_in.data(), static_cast<size_t>(num));

    // Check size
    const size_t written  = buffer.size() - start;
    const size_t expected = CalculateSerializedSize();
    if (written != expected) {
        throw std::runtime_error(LOC + " EqualityKeys::Serialize: serialized size mismatch: " +
                                 ToString(written) + " != " + ToString(expected));
    }
}

void EqualityKeys::Deserialize(const std::vector<uint8_t> &buffer,
                               const EqualityParameters   &params) {
    size_t offset = 0;
    Deserialize(buffer, params, offset);
}

void EqualityKeys::Deserialize(const std::vector<uint8_t> &buffer,
                               const EqualityParameters   &params,
                               size_t                     &offset) {
    const size_t start = offset;

    // Number of keys
    uint64_t num = 0;
    read_pod(buffer, offset, num);

    // Reset containers
    dpf_key.clear();
    shr1_in.clear();
    shr2_in.clear();

    dpf_key.reserve(num);
    shr1_in.resize(num);
    shr2_in.resize(num);

    // DPF keys: construct with params then deserialize in-place
    for (size_t i = 0; i < static_cast<size_t>(num); ++i) {
        dpf_key.emplace_back(/*party_id=*/0, params.GetParameters());
        dpf_key.back().Deserialize(buffer, offset);
    }

    // Shared random values (bulk)
    read_array(buffer, offset, shr1_in.data(), static_cast<size_t>(num));
    read_array(buffer, offset, shr2_in.data(), static_cast<size_t>(num));

    // Check size
    const size_t read     = offset - start;
    const size_t expected = CalculateSerializedSize();
    if (read != expected) {
        throw std::runtime_error(LOC + " EqualityKeys::Deserialize: deserialized size mismatch: " +
                                 ToString(read) + " != " + ToString(expected));
    }
}

Equality::Equality(
    const EqualityParameters &params,
    ProtocolContext2P        &ctx)
    : params_(params),
      gen_(params.GetParameters()),
      eval_(params.GetParameters()),
      ctx_(ctx) {
}

std::pair<EqualityKeys, EqualityKeys> Equality::GenerateKeys(const size_t count) const {
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "Generating Equality keys, count = " + ToString(count));
#endif

    uint64_t     n = params_.GetInputDomainBits();
    EqualityKeys key_0(0, params_, count);
    EqualityKeys key_1(1, params_, count);

    for (size_t i = 0; i < count; ++i) {
        // Generate random inputs for the keys
        uint64_t r1_in = Mod2N(GlobalRng::Rand<uint64_t>(), n);
        uint64_t r2_in = Mod2N(GlobalRng::Rand<uint64_t>(), n);
        uint64_t alpha = Mod2N(r1_in - r2_in, n);

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
        Logger::DebugLog(LOC, "Key " + ToString(i) + ": (r1_in, r2_in)=(" + ToString(r1_in) + ", " + ToString(r2_in) + ")");
        Logger::DebugLog(LOC, "Key " + ToString(i) + ": alpha (= r1_in - r2_in): " + ToString(alpha));
#endif

        // Generate DPF keys
        auto dpf_keys = gen_.GenerateKeys(alpha, 1);

        key_0.dpf_key[i] = std::move(dpf_keys.first);
        key_1.dpf_key[i] = std::move(dpf_keys.second);

        // Generate shared random values
        std::pair<uint64_t, uint64_t> r1_in_sh = ctx_.Arith().Share(r1_in);
        std::pair<uint64_t, uint64_t> r2_in_sh = ctx_.Arith().Share(r2_in);

        key_0.shr1_in[i] = r1_in_sh.first;
        key_1.shr1_in[i] = r1_in_sh.second;
        key_0.shr2_in[i] = r2_in_sh.first;
        key_1.shr2_in[i] = r2_in_sh.second;
    }

    return std::make_pair(std::move(key_0), std::move(key_1));
}

uint64_t Equality::EvaluateSharedInput(osuCrypto::Channel &chl, const EqualityKeyView &key, const uint64_t x1, const uint64_t x2) const {
    uint64_t party_id = key.dpf_key.party_id;
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "Evaluating Equality protocol with shared inputs");
    std::string party_str = (party_id == 0) ? "[P0]" : "[P1]";
    Logger::DebugLog(LOC, party_str + " (x1_sh, x2_sh): (" + ToString(x1) + ", " + ToString(x2) + ")");
#endif

    if (party_id == 0) {
        // Reconstruct masked inputs
        std::array<uint64_t, 2> masked_x_0, masked_x_1, masked_x;
        ctx_.Arith().EvaluateAdd({x1, x2}, {key.shr1_in, key.shr2_in}, masked_x_0);
        ctx_.Arith().Reconst(party_id, chl, masked_x_0, masked_x_1, masked_x);

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
        Logger::DebugLog(LOC, party_str + " (masked_x1_0, masked_x2_0): (" + ToString(masked_x_0[0]) + ", " + ToString(masked_x_0[1]) + ")");
        Logger::DebugLog(LOC, party_str + " (masked_x1, masked_x2): (" + ToString(masked_x[0]) + ", " + ToString(masked_x[1]) + ")");
#endif
        return EvaluateMaskedInput(key, masked_x[0], masked_x[1]);
    } else {
        // Reconstruct masked inputs
        std::array<uint64_t, 2> masked_x_0, masked_x_1, masked_x;
        ctx_.Arith().EvaluateAdd({x1, x2}, {key.shr1_in, key.shr2_in}, masked_x_1);
        ctx_.Arith().Reconst(party_id, chl, masked_x_0, masked_x_1, masked_x);

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
        Logger::DebugLog(LOC, party_str + " (masked_x1_1, masked_x2_1): (" + ToString(masked_x_1[0]) + ", " + ToString(masked_x_1[1]) + ")");
        Logger::DebugLog(LOC, party_str + " (masked_x1, masked_x2): (" + ToString(masked_x[0]) + ", " + ToString(masked_x[1]) + ")");
#endif
        return EvaluateMaskedInput(key, masked_x[0], masked_x[1]);
    }
}

uint64_t Equality::EvaluateMaskedInput(const EqualityKeyView &key, const uint64_t x1, const uint64_t x2) const {
    uint64_t party_id = key.dpf_key.party_id;
    uint64_t n        = params_.GetInputDomainBits();

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "Evaluating Equality protocol with masked inputs");
    std::string party_str = (party_id == 0) ? "[P0]" : "[P1]";
    Logger::DebugLog(LOC, party_str + " (masked_x1, masked_x2): (" + ToString(x1) + ", " + ToString(x2) + ")");
#endif

    uint64_t alpha  = Mod2N(x1 - x2, n);
    uint64_t output = eval_.EvaluateAt(key.dpf_key, alpha);

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, party_str + " alpha: " + ToString(alpha) + ", output: " + ToString(output));
#endif

    return output;
}

void Equality::EvaluateSharedInputBatch(osuCrypto::Channel          &chl,
                                        const EqualityKeys          &keys,
                                        const std::vector<uint64_t> &x1,
                                        const std::vector<uint64_t> &x2,
                                        std::vector<uint64_t>       &out) const {
    const size_t size = keys.Size();
    if (x1.size() != size || x2.size() != size) {
        throw std::runtime_error("EvaluateSharedInputBatch: input size mismatch");
    }

    const uint64_t party_id = keys.dpf_key.at(0).party_id;

    // masked = [masked_x1 || masked_x2] (public)
    std::vector<uint64_t> masked(2 * size, 0);

    // local shares for reconstruction
    std::vector<uint64_t> masked_0(2 * size, 0);
    std::vector<uint64_t> masked_1(2 * size, 0);

    // Compute this party's masked shares: (x1 + r1), (x2 + r2)
    std::vector<uint64_t> tmp1(size, 0);
    std::vector<uint64_t> tmp2(size, 0);
    ctx_.Arith().EvaluateAdd(x1, keys.shr1_in, tmp1);
    ctx_.Arith().EvaluateAdd(x2, keys.shr2_in, tmp2);

    if (party_id == 0) {
        std::copy(tmp1.begin(), tmp1.end(), masked_0.begin());
        std::copy(tmp2.begin(), tmp2.end(),
                  masked_0.begin() + static_cast<std::ptrdiff_t>(size));
    } else {
        std::copy(tmp1.begin(), tmp1.end(), masked_1.begin());
        std::copy(tmp2.begin(), tmp2.end(),
                  masked_1.begin() + static_cast<std::ptrdiff_t>(size));
    }

    // Open masked inputs in one communication round
    ctx_.Arith().Reconst(party_id, chl, masked_0, masked_1, masked);

    // Split public masked vectors and evaluate
    const uint64_t *masked_x1 = masked.data();
    const uint64_t *masked_x2 = masked.data() + size;

    std::vector<uint64_t> mx1(size, 0);
    std::vector<uint64_t> mx2(size, 0);
    std::copy(masked_x1, masked_x1 + size, mx1.begin());
    std::copy(masked_x2, masked_x2 + size, mx2.begin());

    EvaluateMaskedInputBatch(keys, mx1, mx2, out);
}

void Equality::EvaluateMaskedInputBatch(const EqualityKeys          &keys,
                                        const std::vector<uint64_t> &x1,
                                        const std::vector<uint64_t> &x2,
                                        std::vector<uint64_t>       &out) const {
    const size_t size = keys.Size();
    if (x1.size() != size || x2.size() != size) {
        throw std::runtime_error("EvaluateMaskedInputBatch: input size mismatch");
    }
    if (keys.dpf_key.size() != size) {
        throw std::runtime_error("EvaluateMaskedInputBatch: keys size mismatch");
    }

    const uint64_t n = params_.GetInputDomainBits();

    // alpha[i] = (x1[i] - x2[i]) mod 2^n
    std::vector<uint64_t> alpha(size, 0);
    for (size_t i = 0; i < size; ++i) {
        alpha[i] = Mod2N(x1[i] - x2[i], n);
    }

    eval_.EvaluateAt(keys.dpf_key, alpha, out);
}

}    // namespace proto
}    // namespace ringoa
