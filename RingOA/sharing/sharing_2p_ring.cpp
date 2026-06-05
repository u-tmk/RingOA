#include "sharing_2p_ring.h"

#include <cryptoTools/Network/Channel.h>

#include "RingOA/utils/file_io.h"
#include "RingOA/utils/logger.h"
#include "RingOA/utils/rng.h"
#include "RingOA/utils/to_string.h"
#include "RingOA/utils/utils.h"

namespace ringoa {
namespace sharing {

AdditiveSharing2P::AdditiveSharing2P(const ShareConfig &config)
    : bitsize_(config.arith_bits), triples_(0), triple_index_(0) {
}

std::pair<uint64_t, uint64_t> AdditiveSharing2P::Share(const uint64_t x) const {
    const uint64_t r   = Mod2N(GlobalRng::Rand<uint64_t>(), bitsize_);
    const uint64_t x_0 = r;
    const uint64_t x_1 = Mod2N(x - r, bitsize_);
    return {x_0, x_1};
}

std::pair<std::vector<uint64_t>, std::vector<uint64_t>>
AdditiveSharing2P::Share(const std::vector<uint64_t> &x) const {
    std::vector<uint64_t> x_0(x.size()), x_1(x.size());
    ShareCore(std::span<const uint64_t>(x),
              std::span<uint64_t>(x_0),
              std::span<uint64_t>(x_1));
    return {std::move(x_0), std::move(x_1)};
}

void AdditiveSharing2P::ReconstLocal(const uint64_t x_0, const uint64_t x_1, uint64_t &x) const {
    std::span<const uint64_t> s0(&x_0, 1);
    std::span<const uint64_t> s1(&x_1, 1);
    std::span<uint64_t>       so(&x, 1);
    ReconstLocalCore(s0, s1, so);
}

void AdditiveSharing2P::ReconstLocal(const std::vector<uint64_t> &x_0, const std::vector<uint64_t> &x_1, std::vector<uint64_t> &x) const {
    if (x.size() != x_0.size()) {
        x.resize(x_0.size());    // wrapper layer may resize for convenience
    }
    ReconstLocalCore(std::span<const uint64_t>(x_0),
                     std::span<const uint64_t>(x_1),
                     std::span<uint64_t>(x));
}

void AdditiveSharing2P::ReconstLocal(const BeaverTriples &triples_0,
                                     const BeaverTriples &triples_1,
                                     BeaverTriples       &triples) const {
    if (!triples_0.IsValid() || !triples_1.IsValid()) {
        throw std::invalid_argument(LOC + " Invalid BeaverTriples input in ReconstLocal.");
    }

    const size_t n0 = triples_0.size();
    const size_t n1 = triples_1.size();

    if (n0 != n1) {
        throw std::invalid_argument(LOC + " Size mismatch in ReconstLocal BeaverTriples: triples_0(" +
                                    ToString(n0) + "), triples_1(" + ToString(n1) + ").");
    }

    if (triples.size() != n0) {
        triples.Resize(n0);
    }

    for (size_t i = 0; i < n0; ++i) {
        triples.a[i] = Mod2N(triples_0.a[i] + triples_1.a[i], bitsize_);
        triples.b[i] = Mod2N(triples_0.b[i] + triples_1.b[i], bitsize_);
        triples.c[i] = Mod2N(triples_0.c[i] + triples_1.c[i], bitsize_);
    }
}

void AdditiveSharing2P::Reconst(const uint64_t      party_id,
                                osuCrypto::Channel &chl,
                                uint64_t           &x_0,
                                uint64_t           &x_1,
                                uint64_t           &x) const {
    std::span<uint64_t> s0(&x_0, 1);
    std::span<uint64_t> s1(&x_1, 1);
    std::span<uint64_t> so(&x, 1);
    ReconstCore(party_id, chl, s0, s1, so);
}

void AdditiveSharing2P::Reconst(const uint64_t party_id, osuCrypto::Channel &chl, std::vector<uint64_t> &x_0, std::vector<uint64_t> &x_1, std::vector<uint64_t> &x) const {
    const size_t n_local = (party_id == 0) ? x_0.size() : x_1.size();
    if (n_local == 0) {
        throw std::invalid_argument(LOC + " Input share size is zero in Reconst.");
    }

    // Ensure the receive buffer has the correct size
    if (party_id == 0) {
        if (x_1.size() != n_local)
            x_1.resize(n_local);
    } else {
        if (x_0.size() != n_local)
            x_0.resize(n_local);
    }

    if (x.size() != n_local)
        x.resize(n_local);

    ReconstCore(party_id, chl,
                std::span<uint64_t>(x_0),
                std::span<uint64_t>(x_1),
                std::span<uint64_t>(x));
}

void AdditiveSharing2P::Reconst(const uint64_t party_id, osuCrypto::Channel &chl, std::array<std::vector<uint64_t>, 2> &x_0, std::array<std::vector<uint64_t>, 2> &x_1, std::array<std::vector<uint64_t>, 2> &x) const {
    if (party_id == 0) {
        chl.send(x_0[0]);
        chl.send(x_0[1]);
        chl.recv(x_1[0]);
        chl.recv(x_1[1]);
    } else {
        chl.recv(x_0[0]);
        chl.recv(x_0[1]);
        chl.send(x_1[0]);
        chl.send(x_1[1]);
    }
    if (x_0[0].size() != x_1[0].size() || x_0[1].size() != x_1[1].size()) {
        throw std::invalid_argument(LOC + " Size mismatch in Reconst for array of vectors.");
    }
    // Resize x if necessary
    if (x[0].size() != x_0[0].size()) {
        x[0].resize(x_0[0].size());
        x[1].resize(x_0[1].size());
    }
    for (size_t i = 0; i < x[0].size(); ++i) {
        x[0][i] = Mod2N(x_0[0][i] + x_1[0][i], bitsize_);
        x[1][i] = Mod2N(x_0[1][i] + x_1[1][i], bitsize_);
    }
}

void AdditiveSharing2P::EvaluateAdd(const uint64_t x, const uint64_t y, uint64_t &z) const {
    std::span<const uint64_t> sx(&x, 1);
    std::span<const uint64_t> sy(&y, 1);
    std::span<uint64_t>       sz(&z, 1);
    EvaluateAddCore(sx, sy, sz);
}

void AdditiveSharing2P::EvaluateAdd(const std::vector<uint64_t> &x,
                                    const std::vector<uint64_t> &y,
                                    std::vector<uint64_t>       &z) const {
    if (x.size() != y.size()) {
        throw std::invalid_argument(LOC + " Size mismatch in EvaluateAdd(vector): x(" +
                                    ToString(x.size()) + "), y(" + ToString(y.size()) + ").");
    }
    if (z.size() != x.size()) {
        z.resize(x.size());
    }

    EvaluateAddCore(std::span<const uint64_t>(x),
                    std::span<const uint64_t>(y),
                    std::span<uint64_t>(z));
}

void AdditiveSharing2P::EvaluateSub(const uint64_t x, const uint64_t y, uint64_t &z) const {
    std::span<const uint64_t> sx(&x, 1);
    std::span<const uint64_t> sy(&y, 1);
    std::span<uint64_t>       sz(&z, 1);
    EvaluateSubCore(sx, sy, sz);
}

void AdditiveSharing2P::EvaluateSub(const std::vector<uint64_t> &x,
                                    const std::vector<uint64_t> &y,
                                    std::vector<uint64_t>       &z) const {
    if (x.size() != y.size()) {
        throw std::invalid_argument(LOC + " Size mismatch in EvaluateSub(vector): x(" +
                                    ToString(x.size()) + "), y(" + ToString(y.size()) + ").");
    }
    if (z.size() != x.size()) {
        z.resize(x.size());
    }

    EvaluateSubCore(std::span<const uint64_t>(x),
                    std::span<const uint64_t>(y),
                    std::span<uint64_t>(z));
}

void AdditiveSharing2P::EvaluatePubMult(const uint64_t x, const uint64_t c, uint64_t &z) const {
    std::span<const uint64_t> sx(&x, 1);
    std::span<const uint64_t> sc(&c, 1);
    std::span<uint64_t>       sz(&z, 1);
    EvaluatePubMultCore(sx, sc, sz);
}

void AdditiveSharing2P::EvaluatePubMult(const std::vector<uint64_t> &x,
                                        const std::vector<uint64_t> &c,
                                        std::vector<uint64_t>       &z) const {
    if (x.size() != c.size()) {
        throw std::invalid_argument(LOC + " Size mismatch in EvaluatePubMult(vector): x(" + ToString(x.size()) +
                                    "), c(" + ToString(c.size()) + ").");
    }
    if (z.size() != x.size()) {
        z.resize(x.size());
    }

    EvaluatePubMultCore(std::span<const uint64_t>(x),
                        std::span<const uint64_t>(c),
                        std::span<uint64_t>(z));
}

void AdditiveSharing2P::EvaluateMult(const uint64_t      party_id,
                                     osuCrypto::Channel &chl,
                                     uint64_t            x,
                                     uint64_t            y,
                                     uint64_t           &z) {
    std::span<const uint64_t> sx(&x, 1);
    std::span<const uint64_t> sy(&y, 1);
    std::span<uint64_t>       sz(&z, 1);
    EvaluateMultCore(party_id, chl, sx, sy, sz);
}

void AdditiveSharing2P::EvaluateMult(const uint64_t               party_id,
                                     osuCrypto::Channel          &chl,
                                     const std::vector<uint64_t> &x,
                                     const std::vector<uint64_t> &y,
                                     std::vector<uint64_t>       &z) {
    if (x.size() != y.size()) {
        throw std::invalid_argument(LOC + " Size mismatch in EvaluateMult(vector): x(" + ToString(x.size()) +
                                    "), y(" + ToString(y.size()) + ").");
    }
    if (z.size() != x.size()) {
        z.resize(x.size());    // wrapper may resize
    }

    EvaluateMultCore(party_id, chl,
                     std::span<const uint64_t>(x),
                     std::span<const uint64_t>(y),
                     std::span<uint64_t>(z));
}

void AdditiveSharing2P::EvaluateSelect(const uint64_t      party_id,
                                       osuCrypto::Channel &chl,
                                       uint64_t            x,
                                       uint64_t            y,
                                       uint64_t            c,
                                       uint64_t           &z) {
    std::span<const uint64_t> sx(&x, 1);
    std::span<const uint64_t> sy(&y, 1);
    std::span<const uint64_t> sc(&c, 1);
    std::span<uint64_t>       sz(&z, 1);
    EvaluateSelectCore(party_id, chl, sx, sy, sc, sz);
}

void AdditiveSharing2P::EvaluateSelect(const uint64_t               party_id,
                                       osuCrypto::Channel          &chl,
                                       const std::vector<uint64_t> &x,
                                       const std::vector<uint64_t> &y,
                                       const std::vector<uint64_t> &c,
                                       std::vector<uint64_t>       &z) {
    if (x.size() != y.size() || x.size() != c.size()) {
        throw std::invalid_argument(LOC + " Size mismatch in EvaluateSelect(vector): x(" + ToString(x.size()) +
                                    "), y(" + ToString(y.size()) +
                                    "), c(" + ToString(c.size()) + ").");
    }
    if (z.size() != x.size()) {
        z.resize(x.size());
    }

    EvaluateSelectCore(party_id, chl,
                       std::span<const uint64_t>(x),
                       std::span<const uint64_t>(y),
                       std::span<const uint64_t>(c),
                       std::span<uint64_t>(z));
}

uint64_t AdditiveSharing2P::GetBitSize() const {
    return bitsize_;
}

void AdditiveSharing2P::SetTriples(BeaverTriples triples) {
    triples_      = std::move(triples);
    triple_index_ = 0;
}

void AdditiveSharing2P::ClearTriples() {
    triples_.Resize(0);
    triple_index_ = 0;
}

bool AdditiveSharing2P::HasTriples() const {
    return !triples_.empty();
}

void AdditiveSharing2P::RequireTriples(const std::string &where) const {
    if (!HasTriples()) {
        throw std::runtime_error(LOC + " In " + where +
                                 ", no Beaver triples are set. Call SetTriples() first.");
    }
}

uint64_t AdditiveSharing2P::GetNumTriples() const {
    return static_cast<uint64_t>(triples_.size());
}

uint64_t AdditiveSharing2P::GetRemainingTripleCount() const {
    const uint64_t total = static_cast<uint64_t>(triples_.size());
    return (triple_index_ <= total) ? (total - triple_index_) : 0;
}

void AdditiveSharing2P::ResetTripleIndex() {
    triple_index_ = 0;
}

void AdditiveSharing2P::PrintTriples(const size_t limit) const {
    Logger::DebugLog(LOC, "Beaver triples: " + triples_.ToString(limit));
}

// ----------------------------------------------------
// Internal functions
// ----------------------------------------------------

void AdditiveSharing2P::ShareCore(std::span<const uint64_t> x,
                                  std::span<uint64_t>       x_0,
                                  std::span<uint64_t>       x_1) const {
    if (x_0.size() != x.size() || x_1.size() != x.size()) {
        throw std::invalid_argument(LOC + " Size mismatch in Share(span): x(" + ToString(x.size()) +
                                    "), x_0(" + ToString(x_0.size()) +
                                    "), x_1(" + ToString(x_1.size()) + ").");
    }

    const size_t n = x.size();
    for (size_t i = 0; i < n; ++i) {
        const uint64_t r = Mod2N(GlobalRng::Rand<uint64_t>(), bitsize_);
        x_0[i]           = r;
        x_1[i]           = Mod2N(x[i] - r, bitsize_);
    }
}

void AdditiveSharing2P::ReconstLocalCore(std::span<const uint64_t> x_0,
                                         std::span<const uint64_t> x_1,
                                         std::span<uint64_t>       x) const {
    if (x_0.size() != x_1.size() || x.size() != x_0.size()) {
        throw std::invalid_argument(LOC + " Size mismatch in ReconstLocal: x_0(" + ToString(x_0.size()) +
                                    "), x_1(" + ToString(x_1.size()) +
                                    "), x(" + ToString(x.size()) + ").");
    }

    for (size_t i = 0; i < x.size(); ++i) {
        x[i] = Mod2N(x_0[i] + x_1[i], bitsize_);
    }
}

void AdditiveSharing2P::ReconstCore(const uint64_t      party_id,
                                    osuCrypto::Channel &chl,
                                    std::span<uint64_t> x_0,
                                    std::span<uint64_t> x_1,
                                    std::span<uint64_t> x) const {
    if (x_0.size() != x_1.size() || x.size() != x_0.size()) {
        throw std::invalid_argument(LOC + " Size mismatch in Reconst: x_0(" + ToString(x_0.size()) +
                                    "), x_1(" + ToString(x_1.size()) +
                                    "), x(" + ToString(x.size()) + ").");
    }

    const uint64_t n = static_cast<uint64_t>(x.size());

    if (party_id == 0) {
        // Party 0 sends its share x_0, receives other share into x_1
        chl.send(x_0.data(), n);
        chl.recv(x_1.data(), n);
    } else {
        // Party 1 receives other share into x_0, sends its share x_1
        chl.recv(x_0.data(), n);
        chl.send(x_1.data(), n);
    }

    for (size_t i = 0; i < x.size(); ++i) {
        x[i] = Mod2N(x_0[i] + x_1[i], bitsize_);
    }
}

void AdditiveSharing2P::EvaluateAddCore(std::span<const uint64_t> x,
                                        std::span<const uint64_t> y,
                                        std::span<uint64_t>       z) const {
    if (x.size() != y.size() || z.size() != x.size()) {
        throw std::invalid_argument(LOC + " Size mismatch in EvaluateAdd(span): x(" + ToString(x.size()) +
                                    "), y(" + ToString(y.size()) +
                                    "), z(" + ToString(z.size()) + ").");
    }

    for (size_t i = 0; i < x.size(); ++i) {
        z[i] = Mod2N(x[i] + y[i], bitsize_);
    }
}

void AdditiveSharing2P::EvaluateSubCore(std::span<const uint64_t> x,
                                        std::span<const uint64_t> y,
                                        std::span<uint64_t>       z) const {
    if (x.size() != y.size() || z.size() != x.size()) {
        throw std::invalid_argument(LOC + " Size mismatch in EvaluateSub(span): x(" + ToString(x.size()) +
                                    "), y(" + ToString(y.size()) +
                                    "), z(" + ToString(z.size()) + ").");
    }

    for (size_t i = 0; i < x.size(); ++i) {
        z[i] = Mod2N(x[i] - y[i], bitsize_);
    }
}

void AdditiveSharing2P::EvaluatePubMultCore(std::span<const uint64_t> x,
                                            std::span<const uint64_t> c,
                                            std::span<uint64_t>       z) const {
    if (x.size() != c.size() || z.size() != x.size()) {
        throw std::invalid_argument(LOC + " Size mismatch in EvaluatePubMult(span): x(" + ToString(x.size()) +
                                    "), c(" + ToString(c.size()) +
                                    "), z(" + ToString(z.size()) + ").");
    }

    for (size_t i = 0; i < x.size(); ++i) {
        z[i] = Mod2N(x[i] * c[i], bitsize_);
    }
}

void AdditiveSharing2P::EvaluateMultCore(const uint64_t            party_id,
                                         osuCrypto::Channel       &chl,
                                         std::span<const uint64_t> x,
                                         std::span<const uint64_t> y,
                                         std::span<uint64_t>       z) {
    if (x.size() != y.size() || z.size() != x.size()) {
        throw std::invalid_argument(LOC + " Size mismatch in EvaluateMult(span): x(" + ToString(x.size()) +
                                    "), y(" + ToString(y.size()) +
                                    "), z(" + ToString(z.size()) + ").");
    }

    const size_t n = x.size();
    if (n == 0) {
        return;
    }

    // Check beaver triples availability
    RequireTriples("EvaluateMultCore");
    EnsureTriples(n, "EvaluateMultCore");

    // Prepare local d,e values packed as [d0,e0,d1,e1,...].
    std::vector<uint64_t> de0(2 * n);
    std::vector<uint64_t> de1(2 * n);
    std::vector<uint64_t> de(2 * n);

    for (size_t i = 0; i < n; ++i) {
        const size_t idx = triple_index_ + i;

        const uint64_t a = triples_.a[idx];
        const uint64_t b = triples_.b[idx];

        const uint64_t di = Mod2N(x[i] - a, bitsize_);
        const uint64_t ei = Mod2N(y[i] - b, bitsize_);

        if (party_id == 0) {
            de0[2 * i + 0] = di;
            de0[2 * i + 1] = ei;
        } else {
            de1[2 * i + 0] = di;
            de1[2 * i + 1] = ei;
        }
    }

    // Open all d,e in one shot.
    ReconstCore(party_id, chl,
                std::span<uint64_t>(de0),
                std::span<uint64_t>(de1),
                std::span<uint64_t>(de));

    // Compute output shares.
    for (size_t i = 0; i < n; ++i) {
        const size_t idx = triple_index_ + i;

        // SoA layout
        const uint64_t a = triples_.a[idx];
        const uint64_t b = triples_.b[idx];
        const uint64_t c = triples_.c[idx];

        const uint64_t d = de[2 * i + 0];
        const uint64_t e = de[2 * i + 1];

        uint64_t val = 0;
        val          = Mod2N(val + (e * a), bitsize_);
        val          = Mod2N(val + (d * b), bitsize_);
        val          = Mod2N(val + c, bitsize_);
        if (party_id == 0) {
            val = Mod2N(val + (d * e), bitsize_);
        }
        z[i] = val;
    }

    triple_index_ += n;
}

void AdditiveSharing2P::EvaluateSelectCore(const uint64_t            party_id,
                                           osuCrypto::Channel       &chl,
                                           std::span<const uint64_t> x,
                                           std::span<const uint64_t> y,
                                           std::span<const uint64_t> c,
                                           std::span<uint64_t>       z) {
    if (x.size() != y.size() || x.size() != c.size() || z.size() != x.size()) {
        throw std::invalid_argument(LOC + " Size mismatch in EvaluateSelect(span): x(" + ToString(x.size()) +
                                    "), y(" + ToString(y.size()) +
                                    "), c(" + ToString(c.size()) +
                                    "), z(" + ToString(z.size()) + ").");
    }

    const size_t n = x.size();
    if (n == 0) {
        return;
    }

    // Temporary buffers (local only).
    std::vector<uint64_t> y_sub_x(n);
    std::vector<uint64_t> c_mul_y_sub_x(n);

    // y_sub_x = y - x
    EvaluateSubCore(y, x, std::span<uint64_t>(y_sub_x));

    // c_mul_y_sub_x = c * (y - x)
    EvaluateMultCore(party_id, chl, c, std::span<const uint64_t>(y_sub_x), std::span<uint64_t>(c_mul_y_sub_x));

    // z = x + c_mul_y_sub_x
    EvaluateAddCore(x, std::span<const uint64_t>(c_mul_y_sub_x), z);
}

void AdditiveSharing2P::EnsureTriples(size_t need, const std::string &func_name) const {
    const size_t total = triples_.size();
    const size_t idx   = triple_index_;
    const size_t avail = (idx <= total) ? (total - idx) : 0;

    if (need > avail) {
        throw std::runtime_error(LOC + " In " + func_name +
                                 ", no more Beaver triples available. Needed: " + ToString(need) +
                                 ", Available: " + ToString(avail) + ".");
    }
}

}    // namespace sharing
}    // namespace ringoa
