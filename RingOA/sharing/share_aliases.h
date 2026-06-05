#ifndef SHARING_SHARE_ALIASES_H_
#define SHARING_SHARE_ALIASES_H_

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "rep3_share_types.h"

namespace ringoa {
namespace sharing {

constexpr size_t kTwoParties   = 2;
constexpr size_t kThreeParties = 3;

using Rep3Share32        = Rep3Share<uint32_t>;
using Rep3ShareVec32     = Rep3ShareVec<uint32_t>;
using Rep3ShareMat32     = Rep3ShareMat<uint32_t>;
using Rep3ShareView32    = Rep3ShareView<uint32_t>;
using Rep3Share64        = Rep3Share<uint64_t>;
using Rep3ShareVec64     = Rep3ShareVec<uint64_t>;
using Rep3ShareMat64     = Rep3ShareMat<uint64_t>;
using Rep3ShareView64    = Rep3ShareView<uint64_t>;
using Rep3ShareBlock     = Rep3Share<block>;
using Rep3ShareVecBlock  = Rep3ShareVec<block>;
using Rep3ShareMatBlock  = Rep3ShareMat<block>;
using Rep3ShareViewBlock = Rep3ShareView<block>;

}    // namespace sharing
}    // namespace ringoa

#endif    // SHARING_SHARE_ALIASES_H_
