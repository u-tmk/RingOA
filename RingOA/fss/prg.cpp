#include "prg.h"

namespace {
using ringoa::block;
using ringoa::MakeBlock;
using ringoa::fss::prg::Side;

// Fixed keys for the singleton instance.
const block kSeedLeft   = MakeBlock(0x00, 0x00);
const block kSeedRight  = MakeBlock(0x00, 0x01);
const block kValueLeft  = MakeBlock(0x01, 0x01);
const block kValueRight = MakeBlock(0x01, 0x00);

size_t idx(Side s) noexcept {
    return static_cast<size_t>(s);
}
}    // namespace

namespace ringoa {
namespace fss {
namespace prg {

PseudoRandomGenerator::PseudoRandomGenerator(block seedL, block seedR,
                                             block valueL, block valueR) {
    aes_seed_[0].setKey(seedL);
    aes_seed_[1].setKey(seedR);
    aes_value_[0].setKey(valueL);
    aes_value_[1].setKey(valueR);
}

void PseudoRandomGenerator::Expand(const block &in, block &out, Side side) noexcept {
    block tmp = in;
    aes_seed_[idx(side)].ecbEncBlock(tmp, tmp);
    out = in ^ tmp;
}

void PseudoRandomGenerator::ExpandValue(const block &in, block &out, Side side) noexcept {
    block tmp = in;
    aes_value_[idx(side)].ecbEncBlock(tmp, tmp);
    out = in ^ tmp;
}

template <size_t N>
void PseudoRandomGenerator::Expand(const std::array<block, N> &in,
                                   std::array<block, N>       &out,
                                   Side                        side) noexcept {
    std::array<block, N> tmp = in;
    // osuCrypto provides templated batch encrypt; fall back to loop if unavailable.
    aes_seed_[idx(side)].ecbEncBlocks<N>(tmp.data(), tmp.data());
    for (size_t i = 0; i < N; ++i)
        out[i] = in[i] ^ tmp[i];
}

// Explicit instantiations you actively use (keep or extend as needed):
template void PseudoRandomGenerator::Expand<8>(const std::array<block, 8> &,
                                               std::array<block, 8> &,
                                               Side) noexcept;

void PseudoRandomGenerator::DoubleExpand(const block &in, std::array<block, 2> &out) noexcept {
    block l = in, r = in;
    aes_seed_[0].ecbEncBlock(l, l);
    aes_seed_[1].ecbEncBlock(r, r);
    out[0] = in ^ l;    // Left
    out[1] = in ^ r;    // Right
}

void PseudoRandomGenerator::DoubleExpandValue(const block &in, std::array<block, 2> &out) noexcept {
    block l = in, r = in;
    aes_value_[0].ecbEncBlock(l, l);
    aes_value_[1].ecbEncBlock(r, r);
    out[0] = in ^ l;    // Left
    out[1] = in ^ r;    // Right
}

PseudoRandomGenerator &PseudoRandomGenerator::GetInstance() noexcept {
    static PseudoRandomGenerator instance(kSeedLeft, kSeedRight, kValueLeft, kValueRight);
    return instance;
}

}    // namespace prg
}    // namespace fss
}    // namespace ringoa
