#include "beaver_triples_gen.h"

#include "RingOA/utils/rng.h"
#include "RingOA/utils/to_string.h"
#include "RingOA/utils/utils.h"

namespace ringoa {
namespace sharing {

namespace {

inline uint64_t BitsizeFromConfig(const ShareConfig &config) {
    const uint64_t bits = config.arith_bits;
    if (bits == 0 || bits > 64) {
        throw std::invalid_argument("GenerateBeaverTriples: invalid arith_bits in ShareConfig");
    }
    return bits;
}

}    // namespace

BeaverTriples GenerateBeaverTriples(const size_t num_triples, const ShareConfig &config) {
    if (num_triples == 0) {
        throw std::invalid_argument("GenerateBeaverTriples: num_triples must be > 0");
    }

    const uint64_t bitsize = BitsizeFromConfig(config);

    BeaverTriples triples(num_triples);

    for (size_t i = 0; i < num_triples; ++i) {
        const uint64_t a = Mod2N(GlobalRng::Rand<uint64_t>(), bitsize);
        const uint64_t b = Mod2N(GlobalRng::Rand<uint64_t>(), bitsize);
        const uint64_t c = Mod2N(a * b, bitsize);

        triples.a[i] = a;
        triples.b[i] = b;
        triples.c[i] = c;
    }

    if (!triples.IsValid()) {
        throw std::runtime_error("GenerateBeaverTriples: generated triples are invalid");
    }
    return triples;
}

std::pair<BeaverTriples, BeaverTriples> ShareBeaverTriples(const BeaverTriples &triples, const ShareConfig &config) {
    if (!triples.IsValid()) {
        throw std::invalid_argument("ShareBeaverTriples: invalid BeaverTriples");
    }

    const uint64_t bitsize = BitsizeFromConfig(config);
    const size_t   n       = triples.size();

    BeaverTriples bt_0(n), bt_1(n);

    for (size_t i = 0; i < n; ++i) {
        // Additive shares mod 2^bitsize: x = x0 + x1 mod 2^bitsize
        const uint64_t a0 = Mod2N(GlobalRng::Rand<uint64_t>(), bitsize);
        const uint64_t b0 = Mod2N(GlobalRng::Rand<uint64_t>(), bitsize);
        const uint64_t c0 = Mod2N(GlobalRng::Rand<uint64_t>(), bitsize);

        const uint64_t a1 = Mod2N(triples.a[i] - a0, bitsize);
        const uint64_t b1 = Mod2N(triples.b[i] - b0, bitsize);
        const uint64_t c1 = Mod2N(triples.c[i] - c0, bitsize);

        bt_0.a[i] = a0;
        bt_1.a[i] = a1;
        bt_0.b[i] = b0;
        bt_1.b[i] = b1;
        bt_0.c[i] = c0;
        bt_1.c[i] = c1;
    }

    if (!bt_0.IsValid() || !bt_1.IsValid()) {
        throw std::runtime_error("ShareBeaverTriples: shared triples are invalid");
    }
    return {std::move(bt_0), std::move(bt_1)};
}

std::pair<BeaverTriples, BeaverTriples> GenerateAndShareBeaverTriples(const size_t num_triples, const ShareConfig &config) {
    BeaverTriples triples = GenerateBeaverTriples(num_triples, config);
    return ShareBeaverTriples(triples, config);
}

BeaverTriples GenerateBinaryBeaverTriples(const size_t num_triples, const ShareConfig &config) {
    if (num_triples == 0) {
        throw std::invalid_argument("GenerateBinaryBeaverTriples: num_triples must be > 0");
    }

    const uint64_t bitsize = BitsizeFromConfig(config);

    BeaverTriples triples(num_triples);

    for (size_t i = 0; i < num_triples; ++i) {
        const uint64_t a = Mod2N(GlobalRng::Rand<uint64_t>(), bitsize);
        const uint64_t b = Mod2N(GlobalRng::Rand<uint64_t>(), bitsize);
        const uint64_t c = Mod2N(a & b, bitsize);

        triples.a[i] = a;
        triples.b[i] = b;
        triples.c[i] = c;
    }

    if (!triples.IsValid()) {
        throw std::runtime_error("GenerateBinaryBeaverTriples: generated triples are invalid");
    }
    return triples;
}

std::pair<BeaverTriples, BeaverTriples> ShareBinaryBeaverTriples(const BeaverTriples &triples, const ShareConfig &config) {
    if (!triples.IsValid()) {
        throw std::invalid_argument("ShareBinaryBeaverTriples: invalid BeaverTriples");
    }

    const uint64_t bitsize = BitsizeFromConfig(config);
    const size_t   n       = triples.size();

    BeaverTriples bt_0(n), bt_1(n);

    for (size_t i = 0; i < n; ++i) {
        // Additive shares mod 2^bitsize: x = x0 + x1 mod 2^bitsize
        const uint64_t a0 = Mod2N(GlobalRng::Rand<uint64_t>(), bitsize);
        const uint64_t b0 = Mod2N(GlobalRng::Rand<uint64_t>(), bitsize);
        const uint64_t c0 = Mod2N(GlobalRng::Rand<uint64_t>(), bitsize);

        const uint64_t a1 = triples.a[i] ^ a0;
        const uint64_t b1 = triples.b[i] ^ b0;
        const uint64_t c1 = triples.c[i] ^ c0;

        bt_0.a[i] = a0;
        bt_1.a[i] = a1;
        bt_0.b[i] = b0;
        bt_1.b[i] = b1;
        bt_0.c[i] = c0;
        bt_1.c[i] = c1;
    }

    if (!bt_0.IsValid() || !bt_1.IsValid()) {
        throw std::runtime_error("ShareBinaryBeaverTriples: shared triples are invalid");
    }
    return {std::move(bt_0), std::move(bt_1)};
}

std::pair<BeaverTriples, BeaverTriples> GenerateAndShareBinaryBeaverTriples(const size_t num_triples, const ShareConfig &config) {
    BeaverTriples triples = GenerateBinaryBeaverTriples(num_triples, config);
    return ShareBinaryBeaverTriples(triples, config);
}

}    // namespace sharing
}    // namespace ringoa
