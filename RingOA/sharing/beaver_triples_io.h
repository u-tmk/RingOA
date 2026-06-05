#ifndef SHARING_BEAVER_TRIPLES_IO_H_
#define SHARING_BEAVER_TRIPLES_IO_H_

#include "beaver_triples.h"

namespace ringoa {
namespace sharing {

// Low level: single BeaverTriples file
void SaveTriplesToFile(const std::string   &path,
                       const BeaverTriples &triples);

void LoadTriplesFromFile(const std::string &path,
                         BeaverTriples     &triples);

// 2P share helpers
void Save2PTriplesShares(const std::string   &base_path,
                         const BeaverTriples &share0,
                         const BeaverTriples &share1);

BeaverTriples Load2PTriplesShare(const std::string &base_path,
                                 uint64_t           party_id);

}    // namespace sharing
}    // namespace ringoa

#endif
