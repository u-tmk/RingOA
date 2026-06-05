#include "rep3_sharing_ring.h"

#include <cryptoTools/Crypto/PRNG.h>

#include "RingOA/utils/file_io.h"
#include "RingOA/utils/logger.h"
#include "RingOA/utils/rng.h"
#include "RingOA/utils/to_string.h"
#include "RingOA/utils/utils.h"

namespace ringoa {
namespace sharing {

ReplicatedSharing3P::ReplicatedSharing3P(const ShareConfig &config)
    : bitsize_(config.arith_bits) {
}

std::array<Rep3Share32, kThreeParties> ReplicatedSharing3P::ShareLocal(const uint32_t &x) const {
    uint32_t x0 = Mod2N(GlobalRng::Rand<uint32_t>(), bitsize_);
    uint32_t x1 = Mod2N(GlobalRng::Rand<uint32_t>(), bitsize_);
    uint32_t x2 = Mod2N(x - x0 - x1, bitsize_);

    std::array<Rep3Share32, kThreeParties> all_shares;
    all_shares[0].data = {x0, x2};
    all_shares[1].data = {x1, x0};
    all_shares[2].data = {x2, x1};

    return all_shares;
}

std::array<Rep3ShareVec32, kThreeParties> ReplicatedSharing3P::ShareLocal(const std::vector<uint32_t> &x_vec) const {
    const size_t n = x_vec.size();

    std::vector<uint32_t> p0_0(n), p0_1(n);
    std::vector<uint32_t> p1_0(n), p1_1(n);
    std::vector<uint32_t> p2_0(n), p2_1(n);

    for (size_t i = 0; i < n; ++i) {
        uint32_t x  = x_vec[i];
        uint32_t r0 = Mod2N(GlobalRng::Rand<uint32_t>(), bitsize_);
        uint32_t r1 = Mod2N(GlobalRng::Rand<uint32_t>(), bitsize_);
        uint32_t r2 = Mod2N(x - r0 - r1, bitsize_);

        // P0: (r0, r2)
        p0_0[i] = r0;
        p0_1[i] = r2;
        // P1: (r1, r0)
        p1_0[i] = r1;
        p1_1[i] = r0;
        // P2: (r2, r1)
        p2_0[i] = r2;
        p2_1[i] = r1;
    }

    return {
        Rep3ShareVec32(std::move(p0_0), std::move(p0_1)),
        Rep3ShareVec32(std::move(p1_0), std::move(p1_1)),
        Rep3ShareVec32(std::move(p2_0), std::move(p2_1))};
}

std::array<Rep3ShareMat32, kThreeParties> ReplicatedSharing3P::ShareLocal(const std::vector<uint32_t> &x_flat, size_t rows, size_t cols) const {
    const size_t          n = rows * cols;
    std::vector<uint32_t> p0_0(n), p0_1(n);
    std::vector<uint32_t> p1_0(n), p1_1(n);
    std::vector<uint32_t> p2_0(n), p2_1(n);

    for (size_t i = 0; i < n; ++i) {
        uint32_t x  = x_flat[i];
        uint32_t r0 = Mod2N(GlobalRng::Rand<uint32_t>(), bitsize_);
        uint32_t r1 = Mod2N(GlobalRng::Rand<uint32_t>(), bitsize_);
        uint32_t r2 = Mod2N(x - r0 - r1, bitsize_);

        // P0: (r0, r2)
        p0_0[i] = r0;
        p0_1[i] = r2;
        // P1: (r1, r0)
        p1_0[i] = r1;
        p1_1[i] = r0;
        // P2: (r2, r1)
        p2_0[i] = r2;
        p2_1[i] = r1;
    }

    return {
        Rep3ShareMat32(rows, cols, std::move(p0_0), std::move(p0_1)),
        Rep3ShareMat32(rows, cols, std::move(p1_0), std::move(p1_1)),
        Rep3ShareMat32(rows, cols, std::move(p2_0), std::move(p2_1))};
}

std::array<Rep3Share64, kThreeParties> ReplicatedSharing3P::ShareLocal(const uint64_t &x) const {
    uint64_t x0 = Mod2N(GlobalRng::Rand<uint64_t>(), bitsize_);
    uint64_t x1 = Mod2N(GlobalRng::Rand<uint64_t>(), bitsize_);
    uint64_t x2 = Mod2N(x - x0 - x1, bitsize_);

    std::array<Rep3Share64, kThreeParties> all_shares;
    all_shares[0].data = {x0, x2};
    all_shares[1].data = {x1, x0};
    all_shares[2].data = {x2, x1};

    return all_shares;
}

std::array<Rep3ShareVec64, kThreeParties> ReplicatedSharing3P::ShareLocal(const std::vector<uint64_t> &x_vec) const {
    const size_t n = x_vec.size();

    std::vector<uint64_t> p0_0(n), p0_1(n);
    std::vector<uint64_t> p1_0(n), p1_1(n);
    std::vector<uint64_t> p2_0(n), p2_1(n);

    for (size_t i = 0; i < n; ++i) {
        uint64_t x  = x_vec[i];
        uint64_t r0 = Mod2N(GlobalRng::Rand<uint64_t>(), bitsize_);
        uint64_t r1 = Mod2N(GlobalRng::Rand<uint64_t>(), bitsize_);
        uint64_t r2 = Mod2N(x - r0 - r1, bitsize_);

        // P0: (r0, r2)
        p0_0[i] = r0;
        p0_1[i] = r2;
        // P1: (r1, r0)
        p1_0[i] = r1;
        p1_1[i] = r0;
        // P2: (r2, r1)
        p2_0[i] = r2;
        p2_1[i] = r1;
    }

    return {
        Rep3ShareVec64(std::move(p0_0), std::move(p0_1)),
        Rep3ShareVec64(std::move(p1_0), std::move(p1_1)),
        Rep3ShareVec64(std::move(p2_0), std::move(p2_1))};
}

std::array<Rep3ShareMat64, kThreeParties> ReplicatedSharing3P::ShareLocal(const std::vector<uint64_t> &x_flat, size_t rows, size_t cols) const {
    const size_t          n = rows * cols;
    std::vector<uint64_t> p0_0(n), p0_1(n);
    std::vector<uint64_t> p1_0(n), p1_1(n);
    std::vector<uint64_t> p2_0(n), p2_1(n);

    for (size_t i = 0; i < n; ++i) {
        uint64_t x  = x_flat[i];
        uint64_t r0 = Mod2N(GlobalRng::Rand<uint64_t>(), bitsize_);
        uint64_t r1 = Mod2N(GlobalRng::Rand<uint64_t>(), bitsize_);
        uint64_t r2 = Mod2N(x - r0 - r1, bitsize_);

        // P0: (r0, r2)
        p0_0[i] = r0;
        p0_1[i] = r2;
        // P1: (r1, r0)
        p1_0[i] = r1;
        p1_1[i] = r0;
        // P2: (r2, r1)
        p2_0[i] = r2;
        p2_1[i] = r1;
    }

    return {
        Rep3ShareMat64(rows, cols, std::move(p0_0), std::move(p0_1)),
        Rep3ShareMat64(rows, cols, std::move(p1_0), std::move(p1_1)),
        Rep3ShareMat64(rows, cols, std::move(p2_0), std::move(p2_1))};
}

void ReplicatedSharing3P::Open(Channels &chls, const Rep3Share64 &x_sh, uint64_t &open_x) const {
    // Send the first share to the previous party
    chls.prev.send(x_sh[0]);

    // Receive the share from the next party
    uint64_t x_next;
    chls.next.recv(x_next);

    // Sum the shares and compute the open value
    open_x = Mod2N(x_sh[0] + x_sh[1] + x_next, bitsize_);

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "[P" + ToString(chls.party_id) + "] Sent first share to the previous party: " + ToString(x_sh[0]));
    Logger::DebugLog(LOC, "[P" + ToString(chls.party_id) + "] Received share from the next party: " + ToString(x_next));
    Logger::DebugLog(LOC, "[P" + ToString(chls.party_id) + "] (x_0, x_1, x_2): (" + ToString(x_sh[0]) + ", " + ToString(x_sh[1]) + ", " + ToString(x_next) + ")");
#endif
}

void ReplicatedSharing3P::Open(Channels &chls, const Rep3ShareVec64 &x_vec_sh, std::vector<uint64_t> &open_x_vec) const {
    // Send the first share to the previous party
    chls.prev.send(x_vec_sh[0]);

    // Receive the share from the next party
    std::vector<uint64_t> x_vec_next;
    chls.next.recv(x_vec_next);

    // Sum the shares and compute the open values
    if (open_x_vec.size() != x_vec_sh.num_shares) {
        open_x_vec.resize(x_vec_sh.num_shares);
    }
    for (uint64_t i = 0; i < open_x_vec.size(); ++i) {
        open_x_vec[i] = Mod2N(x_vec_sh[0][i] + x_vec_sh[1][i] + x_vec_next[i], bitsize_);
    }

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "[P" + ToString(chls.party_id) + "] Sent first share to the previous party: " + ToString(x_vec_sh[0]));
    Logger::DebugLog(LOC, "[P" + ToString(chls.party_id) + "] Received share from the next party: " + ToString(x_vec_next));
    Logger::DebugLog(LOC, "[P" + ToString(chls.party_id) + "] x_0: " + ToString(x_vec_sh[0]) + ", x_1: " + ToString(x_vec_sh[1]) + ", x_2: " + ToString(x_vec_next));
#endif
}

void ReplicatedSharing3P::Open(Channels &chls, const Rep3ShareMat64 &x_mat_sh, std::vector<uint64_t> &open_x_flat) const {
    const size_t rows = x_mat_sh.rows;
    const size_t cols = x_mat_sh.cols;
    const size_t n    = rows * cols;
    // Send the first share to the previous party
    chls.prev.send(x_mat_sh[0]);

    // Receive the shares from the next party
    std::vector<uint64_t> x_mat_next(n);
    chls.next.recv(x_mat_next);

    // Sum the shares and compute the open values
    if (open_x_flat.size() != n) {
        open_x_flat.resize(n);
    }
    for (size_t i = 0; i < n; ++i) {
        open_x_flat[i] = Mod2N(x_mat_sh[0][i] + x_mat_sh[1][i] + x_mat_next[i], bitsize_);
    }
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "[P" + ToString(chls.party_id) + "] Sent first share to the previous party: " + ToStringMatrix(x_mat_sh[0], rows, cols));
    Logger::DebugLog(LOC, "[P" + ToString(chls.party_id) + "] Received share from the next party: " + ToStringMatrix(x_mat_next, rows, cols));
    Logger::DebugLog(LOC, "[P" + ToString(chls.party_id) + "] x_0: " + ToStringMatrix(x_mat_sh[0], rows, cols) +
                              ", x_1: " + ToStringMatrix(x_mat_sh[1], rows, cols) + ", x_2: " + ToStringMatrix(x_mat_next, rows, cols));
#endif
}

void ReplicatedSharing3P::Rand(Rep3Share64 &x) {
    RequirePrf("ReplicatedSharing3P::Rand");
    constexpr std::size_t kNeed       = sizeof(uint64_t);
    const std::size_t     total_bytes = prf_buff_[0].size() * sizeof(block);
    if (total_bytes < kNeed) {
        throw std::runtime_error("ReplicatedSharing3P::Rand: PRF buffer is too small");
    }
    if (prf_idx_ + kNeed > total_bytes) {
        RefillBuffer();
    }

    uint64_t    a = 0, b = 0;
    const auto *base0 = reinterpret_cast<const std::uint8_t *>(prf_buff_[0].data());
    const auto *base1 = reinterpret_cast<const std::uint8_t *>(prf_buff_[1].data());
    std::memcpy(&a, base0 + prf_idx_, kNeed);
    std::memcpy(&b, base1 + prf_idx_, kNeed);
    x.data[0] = Mod2N(a, bitsize_);
    x.data[1] = Mod2N(b, bitsize_);
    prf_idx_ += kNeed;
}

void ReplicatedSharing3P::EvaluateAdd(const Rep3Share64 &x_sh, const Rep3Share64 &y_sh, Rep3Share64 &z_sh) const {
    z_sh.data[0] = Mod2N(x_sh.data[0] + y_sh.data[0], bitsize_);
    z_sh.data[1] = Mod2N(x_sh.data[1] + y_sh.data[1], bitsize_);
}

void ReplicatedSharing3P::EvaluateAdd(const Rep3ShareVec64 &x_vec_sh, const Rep3ShareVec64 &y_vec_sh, Rep3ShareVec64 &z_vec_sh) const {
    if (x_vec_sh.num_shares != y_vec_sh.num_shares) {
        throw std::invalid_argument(LOC + " Size mismatch: x_vec_sh.num_shares != y_vec_sh.num_shares in EvaluateAdd.");
    }

    if (z_vec_sh.num_shares != x_vec_sh.num_shares) {
        z_vec_sh.num_shares = x_vec_sh.num_shares;
        z_vec_sh.data[0].resize(x_vec_sh.num_shares);
        z_vec_sh.data[1].resize(x_vec_sh.num_shares);
    }

    for (uint64_t i = 0; i < x_vec_sh.num_shares; ++i) {
        z_vec_sh.data[0][i] = Mod2N(x_vec_sh.data[0][i] + y_vec_sh.data[0][i], bitsize_);
        z_vec_sh.data[1][i] = Mod2N(x_vec_sh.data[1][i] + y_vec_sh.data[1][i], bitsize_);
    }
}

void ReplicatedSharing3P::EvaluateSub(const Rep3Share64 &x_sh, const Rep3Share64 &y_sh, Rep3Share64 &z_sh) const {
    z_sh.data[0] = Mod2N(x_sh.data[0] - y_sh.data[0], bitsize_);
    z_sh.data[1] = Mod2N(x_sh.data[1] - y_sh.data[1], bitsize_);
}

void ReplicatedSharing3P::EvaluateSub(const Rep3ShareVec64 &x_vec_sh, const Rep3ShareVec64 &y_vec_sh, Rep3ShareVec64 &z_vec_sh) const {
    if (x_vec_sh.num_shares != y_vec_sh.num_shares) {
        throw std::invalid_argument(LOC + " Size mismatch: x_vec_sh.num_shares != y_vec_sh.num_shares in EvaluateSub.");
    }

    if (z_vec_sh.num_shares != x_vec_sh.num_shares) {
        z_vec_sh.num_shares = x_vec_sh.num_shares;
        z_vec_sh.data[0].resize(x_vec_sh.num_shares);
        z_vec_sh.data[1].resize(x_vec_sh.num_shares);
    }

    for (uint64_t i = 0; i < x_vec_sh.num_shares; ++i) {
        z_vec_sh.data[0][i] = Mod2N(x_vec_sh.data[0][i] - y_vec_sh.data[0][i], bitsize_);
        z_vec_sh.data[1][i] = Mod2N(x_vec_sh.data[1][i] - y_vec_sh.data[1][i], bitsize_);
    }
}

void ReplicatedSharing3P::EvaluateMult(Channels &chls, const Rep3Share64 &x_sh, const Rep3Share64 &y_sh, Rep3Share64 &z_sh) {
    // (t_0, t_1, t_2) forms a (3, 3)-sharing of t = x * y
    uint64_t    t_sh = Mod2N(x_sh.data[0] * y_sh.data[0] + x_sh.data[1] * y_sh.data[0] + x_sh.data[0] * y_sh.data[1], bitsize_);
    Rep3Share64 r_sh;
    Rand(r_sh);
    z_sh.data[0] = Mod2N(t_sh + r_sh.data[0] - r_sh.data[1], bitsize_);
    chls.next.send(z_sh.data[0]);
    chls.prev.recv(z_sh.data[1]);
}

void ReplicatedSharing3P::EvaluateMult(Channels &chls, const Rep3ShareVec64 &x_vec_sh, const Rep3ShareVec64 &y_vec_sh, Rep3ShareVec64 &z_vec_sh) {
    if (x_vec_sh.num_shares != y_vec_sh.num_shares) {
        throw std::invalid_argument(LOC + " Size mismatch: x_vec_sh.num_shares != y_vec_sh.num_shares in EvaluateMult.");
    }

    if (z_vec_sh.num_shares != x_vec_sh.num_shares) {
        z_vec_sh.num_shares = x_vec_sh.num_shares;
        z_vec_sh.data[0].resize(x_vec_sh.num_shares);
        z_vec_sh.data[1].resize(x_vec_sh.num_shares);
    }

    for (uint64_t i = 0; i < x_vec_sh.num_shares; ++i) {
        // (t_0, t_1, t_2) forms a (3, 3)-sharing of t = x * y
        uint64_t    t_sh = Mod2N(x_vec_sh.data[0][i] * y_vec_sh.data[0][i] + x_vec_sh.data[1][i] * y_vec_sh.data[0][i] + x_vec_sh.data[0][i] * y_vec_sh.data[1][i], bitsize_);
        Rep3Share64 r_sh;
        Rand(r_sh);
        z_vec_sh.data[0][i] = Mod2N(t_sh + r_sh.data[0] - r_sh.data[1], bitsize_);
    }

    chls.next.send(z_vec_sh.data[0]);
    chls.prev.recv(z_vec_sh.data[1]);
}

void ReplicatedSharing3P::EvaluateSelect(Channels &chls, const Rep3Share64 &x_sh, const Rep3Share64 &y_sh, const Rep3Share64 &c_sh, Rep3Share64 &z_sh) {
    // ----------------------------------------------------
    // 1) Compute y_sub_x = (y - x) mod bitsize
    // ----------------------------------------------------
    Rep3Share64 y_sub_x;
    EvaluateSub(y_sh, x_sh, y_sub_x);

    // ----------------------------------------------------
    // 2) Compute c_mul_y_sub_x = c * (y - x) using Beaver triple
    //    This is a secure multiplication: EvaluateMult()
    // ----------------------------------------------------
    Rep3Share64 c_mul_y_sub_x;
    EvaluateMult(chls, c_sh, y_sub_x, c_mul_y_sub_x);

    // ----------------------------------------------------
    // 3) Finally, z = x + c_mul_y_sub_x
    // ----------------------------------------------------
    EvaluateAdd(x_sh, c_mul_y_sub_x, z_sh);
}

void ReplicatedSharing3P::EvaluateSelect(Channels &chls, const Rep3ShareVec64 &x_vec_sh, const Rep3ShareVec64 &y_vec_sh, const Rep3Share64 &c_sh, Rep3ShareVec64 &z_vec_sh) {
    if (x_vec_sh.num_shares != y_vec_sh.num_shares) {
        throw std::invalid_argument(LOC + " Size mismatch: x_vec_sh.num_shares != y_vec_sh.num_shares in EvaluateSelect.");
    }

    if (z_vec_sh.num_shares != x_vec_sh.num_shares) {
        z_vec_sh.num_shares = x_vec_sh.num_shares;
        z_vec_sh.data[0].resize(x_vec_sh.num_shares);
        z_vec_sh.data[1].resize(x_vec_sh.num_shares);
    }

    const size_t n = x_vec_sh.num_shares;
    // ----------------------------------------------------
    // 1) Compute y_sub_x = (y - x) mod bitsize
    // ----------------------------------------------------
    Rep3ShareVec64 y_sub_x(n);
    EvaluateSub(y_vec_sh, x_vec_sh, y_sub_x);

    // ----------------------------------------------------
    // 2) Compute c_mul_y_sub_x = c * (y - x) using Beaver triple
    //    This is a secure multiplication: EvaluateMult()
    // ----------------------------------------------------
    Rep3ShareVec64 c_mul_y_sub_x(n);
    for (uint64_t i = 0; i < n; ++i) {
        uint64_t    t_sh = Mod2N(y_sub_x.data[0][i] * c_sh.data[0] + y_sub_x.data[1][i] * c_sh.data[0] + y_sub_x.data[0][i] * c_sh.data[1], bitsize_);
        Rep3Share64 r_sh;
        Rand(r_sh);
        c_mul_y_sub_x.data[0][i] = Mod2N(t_sh + r_sh.data[0] - r_sh.data[1], bitsize_);
    }
    chls.next.send(c_mul_y_sub_x.data[0]);
    chls.prev.recv(c_mul_y_sub_x.data[1]);
    // ----------------------------------------------------
    // 3) Finally, z = x + c_mul_y_sub_x
    // ----------------------------------------------------
    EvaluateAdd(x_vec_sh, c_mul_y_sub_x, z_vec_sh);
}

void ReplicatedSharing3P::EvaluateInnerProduct(Channels &chls, const Rep3ShareVec64 &x_vec_sh, const Rep3ShareVec64 &y_vec_sh, Rep3Share64 &z) {
    if (x_vec_sh.num_shares != y_vec_sh.num_shares) {
        throw std::invalid_argument(LOC + " Size mismatch: x_vec_sh.num_shares != y_vec_sh.num_shares in EvaluateInnerProduct.");
    }

    uint64_t s_sh = 0;
    for (uint64_t i = 0; i < x_vec_sh.num_shares; ++i) {
        s_sh = Mod2N(s_sh + x_vec_sh.data[0][i] * y_vec_sh.data[0][i] + x_vec_sh.data[1][i] * y_vec_sh.data[0][i] + x_vec_sh.data[0][i] * y_vec_sh.data[1][i], bitsize_);
    }
    Rep3Share64 r_sh;
    Rand(r_sh);
    z.data[0] = Mod2N(s_sh + r_sh.data[0] - r_sh.data[1], bitsize_);
    chls.next.send(z.data[0]);
    chls.prev.recv(z.data[1]);
}

uint64_t ReplicatedSharing3P::GetBitSize() const {
    return bitsize_;
}

void ReplicatedSharing3P::SetPrfKeys(const block &prf_key_self,
                                     const block &prf_key_from_next,
                                     uint64_t     buffer_size) {
    prf_ready_ = true;

    prf_key_[0] = prf_key_self;
    prf_key_[1] = prf_key_from_next;

    prf_[0].setKey(prf_key_[0]);
    prf_[1].setKey(prf_key_[1]);

    prf_buff_size_ = buffer_size;
    prf_buff_[0].resize(prf_buff_size_);
    prf_buff_[1].resize(prf_buff_size_);

    prf_idx_     = 0;
    prf_counter_ = 0;
    RefillBuffer();
}

void ReplicatedSharing3P::ClearPrf() {
    prf_ready_     = false;
    prf_idx_       = 0;
    prf_counter_   = 0;
    prf_buff_size_ = 0;
    prf_buff_[0].clear();
    prf_buff_[1].clear();
    prf_key_[0] = zero_block;
    prf_key_[1] = zero_block;
}

bool ReplicatedSharing3P::HasPrf() const {
    return prf_ready_;
}

void ReplicatedSharing3P::RequirePrf(const std::string &func_name) const {
    if (!prf_ready_) {
        throw std::runtime_error(func_name + ": PRF is not initialized. Call SetPrfKeys() first.");
    }
}

void ReplicatedSharing3P::RefillBuffer() {
    prf_[0].ecbEncCounterMode(prf_counter_, prf_buff_[0].size(), prf_buff_[0].data());
    prf_[1].ecbEncCounterMode(prf_counter_, prf_buff_[1].size(), prf_buff_[1].data());
    prf_counter_ += prf_buff_[0].size();
    prf_idx_ = 0;
}

}    // namespace sharing
}    // namespace ringoa
