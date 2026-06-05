#include "integer_comparison.h"

#include <cstring>

#include "RingOA/utils/bytes.h"
#include "RingOA/utils/logger.h"
#include "RingOA/utils/rng.h"
#include "RingOA/utils/to_string.h"
#include "RingOA/utils/utils.h"

namespace ringoa {
namespace proto {

void IntegerComparisonParameters::PrintParametersDebug(bool with_header, int key_width) const {
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    if (with_header) {
        Logger::DebugLog(LOC, Logger::MakeSeparatorLine());
        Logger::DebugLog(LOC, "[IntegerComparison Params]");
        Logger::DebugLog(LOC, Logger::MakeSeparatorLine());
    }

    Logger::DebugLog(LOC, Logger::FormatKeyValue("InputDomainBits", ToString(input_domain_bits_), key_width));
    Logger::DebugLog(LOC, Logger::FormatKeyValue("DdcfOutputBits", ToString(ddcf_out_bits_), key_width));

    params_.PrintParametersDebug(false, key_width);

    if (with_header) {
        Logger::DebugLog(LOC, Logger::MakeSeparatorLine());
    }
#endif
}

IntegerComparisonKeys::IntegerComparisonKeys(
    uint64_t                           party_id,
    const IntegerComparisonParameters &params,
    size_t                             count)
    : ddcf_key(),
      shr1_in(count, 0),
      shr2_in(count, 0) {
    ddcf_key.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        ddcf_key.emplace_back(party_id, params.GetParameters());
    }
}

size_t IntegerComparisonKeys::CalculateSerializedSize() const {
    size_t size = 0;

    size += sizeof(uint64_t);
    const size_t num = Size();
    for (size_t i = 0; i < num; ++i) {
        size += ddcf_key[i].CalculateSerializedSize();
    }
    size += num * sizeof(uint64_t);
    size += num * sizeof(uint64_t);
    return size;
}

void IntegerComparisonKeys::Serialize(std::vector<uint8_t> &buffer) const {
    const size_t start = buffer.size();

    // Number of keys
    const uint64_t num = Size();
    append_pod(buffer, num);

    // Serialize each DPF key
    for (const auto &key : ddcf_key) {
        key.Serialize(buffer);
    }

    // Serialize shared random values
    append_array(buffer, shr1_in.data(), static_cast<size_t>(num));
    append_array(buffer, shr2_in.data(), static_cast<size_t>(num));

    // Check size
    const size_t written  = buffer.size() - start;
    const size_t expected = CalculateSerializedSize();
    if (written != expected) {
        throw std::runtime_error(LOC + " IntegerComparisonKeys::Serialize: serialized size mismatch: " +
                                 ToString(written) + " != " + ToString(expected));
    }
}

void IntegerComparisonKeys::Deserialize(const std::vector<uint8_t>        &buffer,
                                        const IntegerComparisonParameters &params) {
    size_t offset = 0;
    Deserialize(buffer, params, offset);
}

void IntegerComparisonKeys::Deserialize(const std::vector<uint8_t>        &buffer,
                                        const IntegerComparisonParameters &params,
                                        size_t                            &offset) {
    const size_t start = offset;

    // Number of keys
    uint64_t num = 0;
    read_pod(buffer, offset, num);

    // Reset containers
    ddcf_key.clear();
    shr1_in.clear();
    shr2_in.clear();

    ddcf_key.reserve(num);
    shr1_in.resize(num);
    shr2_in.resize(num);

    // DPF keys: construct with params then deserialize in-place
    for (size_t i = 0; i < static_cast<size_t>(num); ++i) {
        ddcf_key.emplace_back(/*party_id=*/0, params.GetParameters());
        ddcf_key.back().Deserialize(buffer, offset);
    }

    // Shared random values (bulk)
    read_array(buffer, offset, shr1_in.data(), static_cast<size_t>(num));
    read_array(buffer, offset, shr2_in.data(), static_cast<size_t>(num));

    // Check size
    const size_t read     = offset - start;
    const size_t expected = CalculateSerializedSize();
    if (read != expected) {
        throw std::runtime_error(LOC + " IntegerComparisonKeys::Deserialize: deserialized size mismatch: " +
                                 ToString(read) + " != " + ToString(expected));
    }
}

IntegerComparison::IntegerComparison(const IntegerComparisonParameters &params,
                                     ProtocolContext2P                 &ctx)
    : params_(params), ddcf_(params.GetParameters()), ctx_(ctx) {
}

std::pair<IntegerComparisonKeys, IntegerComparisonKeys> IntegerComparison::GenerateKeys(const size_t count) const {
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "Generating batch of IntegerComparison keys, count = " + ToString(count));
#endif

    uint64_t              n = params_.GetInputDomainBits();
    IntegerComparisonKeys key_0(0, params_, count);
    IntegerComparisonKeys key_1(1, params_, count);

    for (size_t i = 0; i < count; ++i) {
        // Generate random inputs for the keys
        uint64_t r1_in = Mod2N(GlobalRng::Rand<uint64_t>(), n);
        uint64_t r2_in = Mod2N(GlobalRng::Rand<uint64_t>(), n);

        // Compute the random value r and alpha
        // uint64_t r     = Mod2N(Pow(2, n) - (r1_in - r2_in), n);
        uint64_t r     = Mod2N(r1_in - r2_in, n);
        uint64_t alpha = GetLowerNBits(r, n - 1);
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
        Logger::DebugLog(LOC, "Key " + ToString(i) + ": (r1_in, r2_in)=(" + ToString(r1_in) + ", " + ToString(r2_in) + ")");
        Logger::DebugLog(LOC, "Key " + ToString(i) + ": r (= r1_in - r2_in): " + ToString(r) + ", alpha: " + ToString(alpha));
#endif

        // Generate DDCF keys
        uint64_t msb_r = GetMSB(r, n);
        // 不等号の向きを調整できる
        // x >= y のときに true を返すなら beta_1 = !msb_r, beta_2 = msb_r
        uint64_t beta_1 = !msb_r;
        uint64_t beta_2 = msb_r;
        // x < y のときに true を返すなら beta_1 = msb_r, beta_2 = !msb_r
        // uint64_t beta_1 = msb_r;
        // uint64_t beta_2 = !msb_r;
        auto ddcf_keys = ddcf_.GenerateKeys(alpha, beta_1, beta_2);
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
        Logger::DebugLog(LOC, "Key " + ToString(i) + ": (beta_1, beta_2)=(" + ToString(beta_1) + ", " + ToString(beta_2) + ")");
#endif

        // Set DCF keys for each party
        key_0.ddcf_key[i] = std::move(ddcf_keys.first);
        key_1.ddcf_key[i] = std::move(ddcf_keys.second);

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

uint64_t IntegerComparison::EvaluateSharedInput(osuCrypto::Channel &chl, const IntegerComparisonKeyView &key, const uint64_t x, const uint64_t y) const {
    uint64_t party_id = key.ddcf_key.dcf_key.party_id;
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "Evaluating Comparison protocol with shared inputs");
    std::string party_str = (party_id == 0) ? "[P0]" : "[P1]";
    Logger::DebugLog(LOC, party_str + " (x_sh, y_sh): (" + ToString(x) + ", " + ToString(y) + ")");
#endif

    if (party_id == 0) {
        // Reconstruct masked inputs
        std::array<uint64_t, 2> masked_x_0, masked_x_1, masked_x;
        ctx_.Arith().EvaluateAdd({x, y}, {key.shr1_in, key.shr2_in}, masked_x_0);
        ctx_.Arith().Reconst(party_id, chl, masked_x_0, masked_x_1, masked_x);

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
        Logger::DebugLog(LOC, party_str + " (masked_x1_0, masked_x2_0): (" + ToString(masked_x_0[0]) + ", " + ToString(masked_x_0[1]) + ")");
        Logger::DebugLog(LOC, party_str + " (masked_x1, masked_x2): (" + ToString(masked_x[0]) + ", " + ToString(masked_x[1]) + ")");
#endif
        return EvaluateMaskedInput(key, masked_x[0], masked_x[1]);
    } else {
        // Reconstruct masked inputs
        std::array<uint64_t, 2> masked_x_0, masked_x_1, masked_x;
        ctx_.Arith().EvaluateAdd({x, y}, {key.shr1_in, key.shr2_in}, masked_x_1);
        ctx_.Arith().Reconst(party_id, chl, masked_x_0, masked_x_1, masked_x);

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
        Logger::DebugLog(LOC, party_str + " (masked_x1_1, masked_x2_1): (" + ToString(masked_x_1[0]) + ", " + ToString(masked_x_1[1]) + ")");
        Logger::DebugLog(LOC, party_str + " (masked_x1, masked_x2): (" + ToString(masked_x[0]) + ", " + ToString(masked_x[1]) + ")");
#endif
        return EvaluateMaskedInput(key, masked_x[0], masked_x[1]);
    }
}

uint64_t IntegerComparison::EvaluateMaskedInput(const IntegerComparisonKeyView &key, const uint64_t x, const uint64_t y) const {
    uint64_t party_id = key.ddcf_key.dcf_key.party_id;
    uint64_t n        = params_.GetInputDomainBits();
    uint64_t e        = params_.GetDdcfOutputBitsize();

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "Evaluating Comparison protocol with masked inputs");
    std::string party_str = (party_id == 0) ? "[P0]" : "[P1]";
    Logger::DebugLog(LOC, party_str + " (masked_x, masked_y): (" + ToString(x) + ", " + ToString(y) + ")");
#endif

    uint64_t z     = Mod2N(x - y, n);
    uint64_t msb_z = GetMSB(z, n);
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, party_str + " z: " + ToString(z) + ", msb_z: " + ToString(msb_z));
#endif

    // Evaluate the DDCF key
    // uint64_t alpha  = Mod2N(Pow(2, n - 1) - GetLowerNBits(z, n - 1) - 1, n - 1);
    uint64_t alpha  = GetLowerNBits(z, n - 1);
    uint64_t output = ddcf_.EvaluateAt(key.ddcf_key, alpha);

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, party_str + " alpha: " + ToString(alpha));
    Logger::DebugLog(LOC, party_str + " output: " + ToString(output));
#endif

    output = Mod2N(party_id - ((party_id * msb_z) + output - (2 * msb_z * output)), e);

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, party_str + " Final output: " + ToString(output));
#endif

    return output;
}

void IntegerComparison::EvaluateSharedInputBatch(osuCrypto::Channel          &chl,
                                                 const IntegerComparisonKeys &keys,
                                                 const std::vector<uint64_t> &x1,
                                                 const std::vector<uint64_t> &x2,
                                                 std::vector<uint64_t>       &out) const {
    const size_t size = keys.Size();
    if (x1.size() != size || x2.size() != size) {
        throw std::runtime_error("EvaluateSharedInputBatch: input size mismatch");
    }

    const uint64_t party_id = keys.ddcf_key.at(0).dcf_key.party_id;

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

void IntegerComparison::EvaluateMaskedInputBatch(const IntegerComparisonKeys &keys,
                                                 const std::vector<uint64_t> &x1,
                                                 const std::vector<uint64_t> &x2,
                                                 std::vector<uint64_t>       &out) const {
    const size_t size = keys.Size();
    if (x1.size() != size || x2.size() != size) {
        throw std::runtime_error("EvaluateMaskedInputBatch: input size mismatch");
    }
    if (keys.ddcf_key.size() != size) {
        throw std::runtime_error("EvaluateMaskedInputBatch: keys size mismatch");
    }

    const uint64_t n        = params_.GetInputDomainBits();
    const uint64_t e        = params_.GetDdcfOutputBitsize();
    const uint64_t party_id = keys.ddcf_key.at(0).dcf_key.party_id;

    std::vector<uint64_t> msb_z(size, 0);
    std::vector<uint64_t> alpha(size, 0);
    for (size_t i = 0; i < size; ++i) {
        uint64_t z = Mod2N(x1[i] - x2[i], n);
        msb_z[i]   = GetMSB(z, n);
        // alpha[i]  = Mod2N(Pow(2, n - 1) - GetLowerNBits(z, n - 1) - 1, n - 1);
        alpha[i] = GetLowerNBits(z, n - 1);
    }

    ddcf_.EvaluateAt(keys.ddcf_key, alpha, out);

    for (size_t i = 0; i < size; ++i) {
        out[i] = Mod2N(party_id - ((party_id * msb_z[i]) + out[i] - (2 * msb_z[i] * out[i])), e);
    }
}

}    // namespace proto
}    // namespace ringoa
