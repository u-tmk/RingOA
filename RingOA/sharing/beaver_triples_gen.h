#ifndef SHARING_BEAVER_TRIPLES_GEN_H_
#define SHARING_BEAVER_TRIPLES_GEN_H_

#include "beaver_triples.h"
#include "share_config.h"

namespace ringoa {
namespace sharing {

// Arithmetic (Z_{2^k}) Beaver triples: c = a * b mod 2^k
BeaverTriples                           GenerateBeaverTriples(const size_t num_triples, const ShareConfig &config);
std::pair<BeaverTriples, BeaverTriples> ShareBeaverTriples(const BeaverTriples &triples, const ShareConfig &config);
std::pair<BeaverTriples, BeaverTriples> GenerateAndShareBeaverTriples(const size_t num_triples, const ShareConfig &config);

// Binary (GF(2) or bitwise) Beaver triples: c = a AND b  (shared with XOR)
BeaverTriples                           GenerateBinaryBeaverTriples(const size_t num_triples, const ShareConfig &config);
std::pair<BeaverTriples, BeaverTriples> ShareBinaryBeaverTriples(const BeaverTriples &triples, const ShareConfig &config);
std::pair<BeaverTriples, BeaverTriples> GenerateAndShareBinaryBeaverTriples(const size_t num_triples, const ShareConfig &config);

}    // namespace sharing
}    // namespace ringoa

#endif    // SHARING_BEAVER_TRIPLES_GEN_H_
