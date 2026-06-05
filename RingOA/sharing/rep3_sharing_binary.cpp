#include "rep3_sharing_binary.h"

#include <cryptoTools/Crypto/PRNG.h>

#include "RingOA/utils/file_io.h"
#include "RingOA/utils/logger.h"
#include "RingOA/utils/rng.h"
#include "RingOA/utils/to_string.h"
#include "RingOA/utils/utils.h"

namespace ringoa {
namespace sharing {

BinaryReplicatedSharing3P::BinaryReplicatedSharing3P(const ShareConfig &config)
    : bitsize_(config.arith_bits) {
}

std::array<Rep3Share32, kThreeParties> BinaryReplicatedSharing3P::ShareLocal(const uint32_t &x) const {
    uint32_t x0 = GlobalRng::Rand<uint32_t>();
    uint32_t x1 = GlobalRng::Rand<uint32_t>();
    uint32_t x2 = x ^ x0 ^ x1;

    std::array<Rep3Share32, kThreeParties> all_shares;
    all_shares[0].data = {x0, x2};
    all_shares[1].data = {x1, x0};
    all_shares[2].data = {x2, x1};

    return all_shares;
}

std::array<Rep3ShareVec32, kThreeParties> BinaryReplicatedSharing3P::ShareLocal(const std::vector<uint32_t> &x_vec) const {
    const size_t n = x_vec.size();

    std::vector<uint32_t> p0_0(n), p0_1(n);
    std::vector<uint32_t> p1_0(n), p1_1(n);
    std::vector<uint32_t> p2_0(n), p2_1(n);

    for (size_t i = 0; i < n; ++i) {
        uint32_t x  = x_vec[i];
        uint32_t r0 = GlobalRng::Rand<uint32_t>();
        uint32_t r1 = GlobalRng::Rand<uint32_t>();
        uint32_t r2 = x ^ r0 ^ r1;

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

std::array<Rep3ShareMat32, kThreeParties> BinaryReplicatedSharing3P::ShareLocal(const std::vector<uint32_t> &x_flat, size_t rows, size_t cols) const {
    const size_t          n = rows * cols;
    std::vector<uint32_t> p0_0(n), p0_1(n);
    std::vector<uint32_t> p1_0(n), p1_1(n);
    std::vector<uint32_t> p2_0(n), p2_1(n);

    for (size_t i = 0; i < n; ++i) {
        uint32_t x  = x_flat[i];
        uint32_t r0 = GlobalRng::Rand<uint32_t>();
        uint32_t r1 = GlobalRng::Rand<uint32_t>();
        uint32_t r2 = x ^ r0 ^ r1;

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

std::array<Rep3Share64, kThreeParties> BinaryReplicatedSharing3P::ShareLocal(const uint64_t &x) const {
    uint64_t x0 = GlobalRng::Rand<uint64_t>();
    uint64_t x1 = GlobalRng::Rand<uint64_t>();
    uint64_t x2 = x ^ x0 ^ x1;

    std::array<Rep3Share64, kThreeParties> all_shares;
    all_shares[0].data = {x0, x2};
    all_shares[1].data = {x1, x0};
    all_shares[2].data = {x2, x1};

    return all_shares;
}

std::array<Rep3ShareVec64, kThreeParties> BinaryReplicatedSharing3P::ShareLocal(const std::vector<uint64_t> &x_vec) const {
    const size_t n = x_vec.size();

    std::vector<uint64_t> p0_0(n), p0_1(n);
    std::vector<uint64_t> p1_0(n), p1_1(n);
    std::vector<uint64_t> p2_0(n), p2_1(n);

    for (size_t i = 0; i < n; ++i) {
        uint64_t x  = x_vec[i];
        uint64_t r0 = GlobalRng::Rand<uint64_t>();
        uint64_t r1 = GlobalRng::Rand<uint64_t>();
        uint64_t r2 = x ^ r0 ^ r1;

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

std::array<Rep3ShareMat64, kThreeParties> BinaryReplicatedSharing3P::ShareLocal(const std::vector<uint64_t> &x_flat, size_t rows, size_t cols) const {
    const size_t          n = rows * cols;
    std::vector<uint64_t> p0_0(n), p0_1(n);
    std::vector<uint64_t> p1_0(n), p1_1(n);
    std::vector<uint64_t> p2_0(n), p2_1(n);

    for (size_t i = 0; i < n; ++i) {
        uint64_t x  = x_flat[i];
        uint64_t r0 = GlobalRng::Rand<uint64_t>();
        uint64_t r1 = GlobalRng::Rand<uint64_t>();
        uint64_t r2 = x ^ r0 ^ r1;

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

std::array<Rep3ShareBlock, kThreeParties> BinaryReplicatedSharing3P::ShareLocal(const block &x) const {
    block x0 = GlobalRng::Rand<block>();
    block x1 = GlobalRng::Rand<block>();
    block x2 = x ^ x0 ^ x1;

    std::array<Rep3ShareBlock, kThreeParties> all_shares;
    all_shares[0].data = {x0, x2};
    all_shares[1].data = {x1, x0};
    all_shares[2].data = {x2, x1};

    return all_shares;
}

std::array<Rep3ShareVecBlock, kThreeParties> BinaryReplicatedSharing3P::ShareLocal(const std::vector<block> &x_vec) const {
    const size_t n = x_vec.size();

    std::vector<block> p0_0(n), p0_1(n);
    std::vector<block> p1_0(n), p1_1(n);
    std::vector<block> p2_0(n), p2_1(n);

    for (size_t i = 0; i < n; ++i) {
        block x  = x_vec[i];
        block r0 = GlobalRng::Rand<block>();
        block r1 = GlobalRng::Rand<block>();
        block r2 = x ^ r0 ^ r1;

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
        Rep3ShareVecBlock(std::move(p0_0), std::move(p0_1)),
        Rep3ShareVecBlock(std::move(p1_0), std::move(p1_1)),
        Rep3ShareVecBlock(std::move(p2_0), std::move(p2_1))};
}

std::array<Rep3ShareMatBlock, kThreeParties> BinaryReplicatedSharing3P::ShareLocal(const std::vector<block> &x_flat, size_t rows, size_t cols) const {
    const size_t       n = rows * cols;
    std::vector<block> p0_0(n), p0_1(n);
    std::vector<block> p1_0(n), p1_1(n);
    std::vector<block> p2_0(n), p2_1(n);

    for (size_t i = 0; i < n; ++i) {
        block x  = x_flat[i];
        block r0 = GlobalRng::Rand<block>();
        block r1 = GlobalRng::Rand<block>();
        block r2 = x ^ r0 ^ r1;

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
        Rep3ShareMatBlock(rows, cols, std::move(p0_0), std::move(p0_1)),
        Rep3ShareMatBlock(rows, cols, std::move(p1_0), std::move(p1_1)),
        Rep3ShareMatBlock(rows, cols, std::move(p2_0), std::move(p2_1))};
}

void BinaryReplicatedSharing3P::Open(Channels &chls, const Rep3Share64 &x_sh, uint64_t &open_x) const {
    // Send the first share to the previous party
    chls.prev.send(x_sh[0]);

    // Receive the share from the next party
    uint64_t x_next;
    chls.next.recv(x_next);

    // Sum the shares and compute the open value
    open_x = x_sh[0] ^ x_sh[1] ^ x_next;

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "[P" + ToString(chls.party_id) + "] Sent first share to the previous party: " + ToString(x_sh[0]));
    Logger::DebugLog(LOC, "[P" + ToString(chls.party_id) + "] Received share from the next party: " + ToString(x_next));
    Logger::DebugLog(LOC, "[P" + ToString(chls.party_id) + "] (x_0, x_1, x_2): (" + ToString(x_sh[0]) + ", " + ToString(x_sh[1]) + ", " + ToString(x_next) + ")");
#endif
}

void BinaryReplicatedSharing3P::Open(Channels &chls, const Rep3ShareVec64 &x_vec_sh, std::vector<uint64_t> &open_x_vec) const {
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
        open_x_vec[i] = x_vec_sh[0][i] ^ x_vec_sh[1][i] ^ x_vec_next[i];
    }

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "[P" + ToString(chls.party_id) + "] Sent first share to the previous party: " + ToString(x_vec_sh[0]));
    Logger::DebugLog(LOC, "[P" + ToString(chls.party_id) + "] Received share from the next party: " + ToString(x_vec_next));
    Logger::DebugLog(LOC, "[P" + ToString(chls.party_id) + "] x_0: " + ToString(x_vec_sh[0]) + ", x_1: " + ToString(x_vec_sh[1]) + ", x_2: " + ToString(x_vec_next));
#endif
}

void BinaryReplicatedSharing3P::Open(Channels &chls, const Rep3ShareMat64 &x_mat_sh, std::vector<uint64_t> &open_x_flat) const {
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
        open_x_flat[i] = x_mat_sh[0][i] ^ x_mat_sh[1][i] ^ x_mat_next[i];
    }
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "[P" + ToString(chls.party_id) + "] Sent first share to the previous party: " + ToStringMatrix(x_mat_sh[0], rows, cols));
    Logger::DebugLog(LOC, "[P" + ToString(chls.party_id) + "] Received share from the next party: " + ToStringMatrix(x_mat_next, rows, cols));
    Logger::DebugLog(LOC, "[P" + ToString(chls.party_id) + "] x_0: " + ToStringMatrix(x_mat_sh[0], rows, cols) +
                              ", x_1: " + ToStringMatrix(x_mat_sh[1], rows, cols) + ", x_2: " + ToStringMatrix(x_mat_next, rows, cols));
#endif
}

void BinaryReplicatedSharing3P::Open(Channels &chls, const Rep3ShareBlock &x_sh, block &open_x) const {
    // Send the first share to the previous party
    chls.prev.send(x_sh[0]);

    // Receive the share from the next party
    block x_next;
    chls.next.recv(x_next);

    // Sum the shares and compute the open value
    open_x = x_sh[0] ^ x_sh[1] ^ x_next;

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "[P" + ToString(chls.party_id) + "] Sent first share to the previous party: " + Format(x_sh[0]));
    Logger::DebugLog(LOC, "[P" + ToString(chls.party_id) + "] Received share from the next party: " + Format(x_next));
    Logger::DebugLog(LOC, "[P" + ToString(chls.party_id) + "] (x_0, x_1, x_2): (" + Format(x_sh[0]) + ", " + Format(x_sh[1]) + ", " + Format(x_next) + ")");
#endif
}

void BinaryReplicatedSharing3P::Open(Channels &chls, const Rep3ShareVecBlock &x_vec_sh, std::vector<block> &open_x_vec) const {
    // Send the first share to the previous party
    chls.prev.send(x_vec_sh[0]);

    // Receive the share from the next party
    std::vector<block> x_vec_next;
    chls.next.recv(x_vec_next);

    // Sum the shares and compute the open values
    if (open_x_vec.size() != x_vec_sh.num_shares) {
        open_x_vec.resize(x_vec_sh.num_shares);
    }
    for (uint64_t i = 0; i < open_x_vec.size(); ++i) {
        open_x_vec[i] = x_vec_sh[0][i] ^ x_vec_sh[1][i] ^ x_vec_next[i];
    }

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "[P" + ToString(chls.party_id) + "] Sent first share to the previous party: " + Format(x_vec_sh[0]));
    Logger::DebugLog(LOC, "[P" + ToString(chls.party_id) + "] Received share from the next party: " + Format(x_vec_next));
    Logger::DebugLog(LOC, "[P" + ToString(chls.party_id) + "] x_0: " + Format(x_vec_sh[0]) + ", x_1: " + Format(x_vec_sh[1]) + ", x_2: " + Format(x_vec_next));
#endif
}

void BinaryReplicatedSharing3P::Open(Channels &chls, const Rep3ShareMatBlock &x_mat_sh, std::vector<block> &open_x_flat) const {
    const size_t rows = x_mat_sh.rows;
    const size_t cols = x_mat_sh.cols;
    const size_t n    = rows * cols;
    // Send the first share to the previous party
    chls.prev.send(x_mat_sh[0]);

    // Receive the shares from the next party
    std::vector<block> x_mat_next(n);
    chls.next.recv(x_mat_next);

    // Sum the shares and compute the open values
    if (open_x_flat.size() != n) {
        open_x_flat.resize(n);
    }
    for (size_t i = 0; i < n; ++i) {
        open_x_flat[i] = x_mat_sh[0][i] ^ x_mat_sh[1][i] ^ x_mat_next[i];
    }
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "[P" + ToString(chls.party_id) + "] Sent first share to the previous party: " + FormatMatrix(x_mat_sh[0], rows, cols));
    Logger::DebugLog(LOC, "[P" + ToString(chls.party_id) + "] Received share from the next party: " + FormatMatrix(x_mat_next, rows, cols));
    Logger::DebugLog(LOC, "[P" + ToString(chls.party_id) + "] x_0: " + FormatMatrix(x_mat_sh[0], rows, cols) +
                              ", x_1: " + FormatMatrix(x_mat_sh[1], rows, cols) + ", x_2: " + FormatMatrix(x_mat_next, rows, cols));
#endif
}

void BinaryReplicatedSharing3P::Rand(Rep3Share64 &x) {
    RequirePrf("BinaryReplicatedSharing3P::Rand(uint64)");
    constexpr std::size_t kNeed       = sizeof(uint64_t);
    const std::size_t     total_bytes = prf_buff_[0].size() * sizeof(block);
    if (prf_idx_ + kNeed > total_bytes) {
        RefillBuffer();
    }

    uint64_t    a = 0, b = 0;
    const auto *base0 = reinterpret_cast<const uint8_t *>(prf_buff_[0].data());
    const auto *base1 = reinterpret_cast<const uint8_t *>(prf_buff_[1].data());
    std::memcpy(&a, base0 + prf_idx_, kNeed);
    std::memcpy(&b, base1 + prf_idx_, kNeed);
    x.data[0] = a;
    x.data[1] = b;
    prf_idx_ += kNeed;
}

void BinaryReplicatedSharing3P::Rand(Rep3ShareBlock &x) {
    RequirePrf("BinaryReplicatedSharing3P::Rand(block)");
    constexpr std::size_t kNeed       = sizeof(block);
    const std::size_t     total_bytes = prf_buff_[0].size() * sizeof(block);
    if (prf_idx_ + kNeed > total_bytes) {
        RefillBuffer();
    }

    const auto *base0 = reinterpret_cast<const uint8_t *>(prf_buff_[0].data());
    const auto *base1 = reinterpret_cast<const uint8_t *>(prf_buff_[1].data());
    std::memcpy(&x.data[0], base0 + prf_idx_, kNeed);
    std::memcpy(&x.data[1], base1 + prf_idx_, kNeed);
    prf_idx_ += kNeed;
}

void BinaryReplicatedSharing3P::EvaluateXor(const Rep3Share64 &x_sh, const Rep3Share64 &y_sh, Rep3Share64 &z_sh) const {
    z_sh.data[0] = x_sh.data[0] ^ y_sh.data[0];
    z_sh.data[1] = x_sh.data[1] ^ y_sh.data[1];
}

void BinaryReplicatedSharing3P::EvaluateXor(const Rep3ShareVec64 &x_vec_sh, const Rep3ShareVec64 &y_vec_sh, Rep3ShareVec64 &z_vec_sh) const {
    if (x_vec_sh.num_shares != y_vec_sh.num_shares) {
        throw std::invalid_argument(LOC + " Size mismatch: x_vec_sh.num_shares != y_vec_sh.num_shares in EvaluateXor.");
    }

    if (z_vec_sh.num_shares != x_vec_sh.num_shares) {
        z_vec_sh.num_shares = x_vec_sh.num_shares;
        z_vec_sh.data[0].resize(x_vec_sh.num_shares);
        z_vec_sh.data[1].resize(x_vec_sh.num_shares);
    }

    for (uint64_t i = 0; i < x_vec_sh.num_shares; ++i) {
        z_vec_sh.data[0][i] = x_vec_sh.data[0][i] ^ y_vec_sh.data[0][i];
        z_vec_sh.data[1][i] = x_vec_sh.data[1][i] ^ y_vec_sh.data[1][i];
    }
}

void BinaryReplicatedSharing3P::EvaluateAnd(Channels &chls, const Rep3Share64 &x_sh, const Rep3Share64 &y_sh, Rep3Share64 &z_sh) {
    // (t_0, t_1, t_2) forms a (3, 3)-sharing of t = x & y
    uint64_t    t_sh = (x_sh.data[0] & y_sh.data[0]) ^ (x_sh.data[1] & y_sh.data[0]) ^ (x_sh.data[0] & y_sh.data[1]);
    Rep3Share64 r_sh;
    Rand(r_sh);
    z_sh.data[0] = t_sh ^ r_sh.data[0] ^ r_sh.data[1];
    chls.next.send(z_sh.data[0]);
    chls.prev.recv(z_sh.data[1]);
}

void BinaryReplicatedSharing3P::EvaluateAnd(Channels &chls, const Rep3ShareVec64 &x_vec_sh, const Rep3ShareVec64 &y_vec_sh, Rep3ShareVec64 &z_vec_sh) {
    if (x_vec_sh.num_shares != y_vec_sh.num_shares) {
        throw std::invalid_argument(LOC + " Size mismatch: x_vec_sh.num_shares != y_vec_sh.num_shares in EvaluateAnd.");
    }

    if (z_vec_sh.num_shares != x_vec_sh.num_shares) {
        z_vec_sh.num_shares = x_vec_sh.num_shares;
        z_vec_sh.data[0].resize(x_vec_sh.num_shares);
        z_vec_sh.data[1].resize(x_vec_sh.num_shares);
    }

    for (uint64_t i = 0; i < x_vec_sh.num_shares; ++i) {
        // (t_0, t_1, t_2) forms a (3, 3)-sharing of t = x & y
        uint64_t    t_sh = (x_vec_sh.data[0][i] & y_vec_sh.data[0][i]) ^ (x_vec_sh.data[1][i] & y_vec_sh.data[0][i]) ^ (x_vec_sh.data[0][i] & y_vec_sh.data[1][i]);
        Rep3Share64 r_sh;
        Rand(r_sh);
        z_vec_sh.data[0][i] = t_sh ^ r_sh.data[0] ^ r_sh.data[1];
    }

    chls.next.send(z_vec_sh.data[0]);
    chls.prev.recv(z_vec_sh.data[1]);
}

void BinaryReplicatedSharing3P::EvaluateSelect(Channels &chls, const Rep3Share64 &x_sh, const Rep3Share64 &y_sh, const Rep3Share64 &c_sh, Rep3Share64 &z_sh) {
    Rep3Share64 xy_sh;
    EvaluateXor(x_sh, y_sh, xy_sh);
    Rep3Share64 c_and_xy_sh;
    EvaluateAnd(chls, c_sh, xy_sh, c_and_xy_sh);
    EvaluateXor(x_sh, c_and_xy_sh, z_sh);
}

void BinaryReplicatedSharing3P::EvaluateSelect(Channels &chls, const Rep3ShareVec64 &x_vec_sh, const Rep3ShareVec64 &y_vec_sh, const Rep3Share64 &c_sh, Rep3ShareVec64 &z_vec_sh) {
    if (x_vec_sh.num_shares != y_vec_sh.num_shares) {
        throw std::invalid_argument(LOC + " Size mismatch: x_vec_sh.num_shares != y_vec_sh.num_shares in EvaluateSelect.");
    }

    if (z_vec_sh.num_shares != x_vec_sh.num_shares) {
        z_vec_sh.num_shares = x_vec_sh.num_shares;
        z_vec_sh.data[0].resize(x_vec_sh.num_shares);
        z_vec_sh.data[1].resize(x_vec_sh.num_shares);
    }

    const size_t   n = x_vec_sh.num_shares;
    Rep3ShareVec64 xy_sh(n);
    EvaluateXor(x_vec_sh, y_vec_sh, xy_sh);
    Rep3ShareVec64 c_and_xy_sh(n);

    for (uint64_t i = 0; i < n; ++i) {
        uint64_t    t_sh = (xy_sh.data[0][i] & c_sh.data[0]) ^ (xy_sh.data[1][i] & c_sh.data[0]) ^ (xy_sh.data[0][i] & c_sh.data[1]);
        Rep3Share64 r_sh;
        Rand(r_sh);
        c_and_xy_sh.data[0][i] = t_sh ^ r_sh.data[0] ^ r_sh.data[1];
    }

    chls.next.send(c_and_xy_sh.data[0]);
    chls.prev.recv(c_and_xy_sh.data[1]);

    EvaluateXor(x_vec_sh, c_and_xy_sh, z_vec_sh);
}

uint64_t BinaryReplicatedSharing3P::GetBitSize() const {
    return bitsize_;
}

void BinaryReplicatedSharing3P::SetPrfKeys(const block &prf_key_self,
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

void BinaryReplicatedSharing3P::ClearPrf() {
    prf_ready_     = false;
    prf_idx_       = 0;
    prf_counter_   = 0;
    prf_buff_size_ = 0;
    prf_buff_[0].clear();
    prf_buff_[1].clear();
    prf_key_[0] = zero_block;
    prf_key_[1] = zero_block;
}

bool BinaryReplicatedSharing3P::HasPrf() const {
    return prf_ready_;
}

void BinaryReplicatedSharing3P::RequirePrf(const std::string &func_name) const {
    if (!prf_ready_) {
        throw std::runtime_error(func_name + ": PRF is not initialized. Call SetPrfKeys() first.");
    }
}

void BinaryReplicatedSharing3P::RefillBuffer() {
    prf_[0].ecbEncCounterMode(prf_counter_, prf_buff_[0].size(), prf_buff_[0].data());
    prf_[1].ecbEncCounterMode(prf_counter_, prf_buff_[1].size(), prf_buff_[1].data());
    prf_counter_ += prf_buff_[0].size();
    prf_idx_ = 0;
}

}    // namespace sharing
}    // namespace ringoa
