#ifndef SHARING_ADDITIVE_2P_H_
#define SHARING_ADDITIVE_2P_H_

#include <array>
#include <span>
#include <string>

#include "beaver_triples.h"
#include "share_config.h"

// Forward declaration
namespace osuCrypto {

class Channel;

}    // namespace osuCrypto

namespace ringoa {
namespace sharing {

class AdditiveSharing2P {
public:
    AdditiveSharing2P() = delete;
    AdditiveSharing2P(const ShareConfig &config);

    // Share data (single value, std::array, std::vector, BeaverTriples)
    std::pair<uint64_t, uint64_t> Share(const uint64_t x) const;
    template <size_t N>
    std::pair<std::array<uint64_t, N>, std::array<uint64_t, N>> Share(const std::array<uint64_t, N> &x) const {
        std::array<uint64_t, N> x0{}, x1{};
        ShareCore(std::span<const uint64_t>(x),
                  std::span<uint64_t>(x0),
                  std::span<uint64_t>(x1));
        return {x0, x1};
    }
    std::pair<std::vector<uint64_t>, std::vector<uint64_t>> Share(const std::vector<uint64_t> &x) const;

    // Reconstruct data without interaction (single value, std::array, std::vector, BeaverTriples)
    void ReconstLocal(const uint64_t x_0, const uint64_t x_1, uint64_t &x) const;
    template <size_t N>
    void ReconstLocal(const std::array<uint64_t, N> &x_0,
                      const std::array<uint64_t, N> &x_1,
                      std::array<uint64_t, N>       &x) const {
        std::span<const uint64_t> s0(x_0.data(), N);
        std::span<const uint64_t> s1(x_1.data(), N);
        std::span<uint64_t>       so(x.data(), N);
        ReconstLocalCore(s0, s1, so);
    }
    void ReconstLocal(const std::vector<uint64_t> &x_0,
                      const std::vector<uint64_t> &x_1,
                      std::vector<uint64_t>       &x) const;
    void ReconstLocal(const BeaverTriples &triples_0,
                      const BeaverTriples &triples_1,
                      BeaverTriples       &triples) const;

    // Reconstruct data with interaction (single value, std::array, std::vector, std::array<std::vector, 2>)
    void Reconst(const uint64_t party_id, osuCrypto::Channel &chl, uint64_t &x_0, uint64_t &x_1, uint64_t &x) const;
    template <size_t N>
    void Reconst(const uint64_t           party_id,
                 osuCrypto::Channel      &chl,
                 std::array<uint64_t, N> &x_0,
                 std::array<uint64_t, N> &x_1,
                 std::array<uint64_t, N> &x) const {
        std::span<uint64_t> s0(x_0.data(), N);
        std::span<uint64_t> s1(x_1.data(), N);
        std::span<uint64_t> so(x.data(), N);
        ReconstCore(party_id, chl, s0, s1, so);
    }
    void Reconst(const uint64_t         party_id,
                 osuCrypto::Channel    &chl,
                 std::vector<uint64_t> &x_0,
                 std::vector<uint64_t> &x_1,
                 std::vector<uint64_t> &x) const;
    void Reconst(const uint64_t                        party_id,
                 osuCrypto::Channel                   &chl,
                 std::array<std::vector<uint64_t>, 2> &x_0,
                 std::array<std::vector<uint64_t>, 2> &x_1,
                 std::array<std::vector<uint64_t>, 2> &x) const;

    // Evaluate operations (addition, subtraction, multiplication, selection)
    void EvaluateAdd(const uint64_t x, const uint64_t y, uint64_t &z) const;
    template <size_t N>
    void EvaluateAdd(const std::array<uint64_t, N> &x,
                     const std::array<uint64_t, N> &y,
                     std::array<uint64_t, N>       &z) const {
        std::span<const uint64_t> sx(x.data(), N);
        std::span<const uint64_t> sy(y.data(), N);
        std::span<uint64_t>       sz(z.data(), N);
        EvaluateAddCore(sx, sy, sz);
    }
    void EvaluateAdd(const std::vector<uint64_t> &x, const std::vector<uint64_t> &y, std::vector<uint64_t> &z) const;

    void EvaluateSub(const uint64_t x, const uint64_t y, uint64_t &z) const;
    template <size_t N>
    void EvaluateSub(const std::array<uint64_t, N> &x,
                     const std::array<uint64_t, N> &y,
                     std::array<uint64_t, N>       &z) const {
        std::span<const uint64_t> sx(x.data(), N);
        std::span<const uint64_t> sy(y.data(), N);
        std::span<uint64_t>       sz(z.data(), N);
        EvaluateSubCore(sx, sy, sz);
    }
    void EvaluateSub(const std::vector<uint64_t> &x, const std::vector<uint64_t> &y, std::vector<uint64_t> &z) const;

    // TODO: Add public addition and subtraction
    // TODO: Add public multiplication
    void EvaluatePubMult(const uint64_t x, const uint64_t c, uint64_t &z) const;
    template <size_t N>
    void EvaluatePubMult(const std::array<uint64_t, N> &x,
                         const std::array<uint64_t, N> &c,
                         std::array<uint64_t, N>       &z) const {
        std::span<const uint64_t> sx(x.data(), N);
        std::span<const uint64_t> sc(c.data(), N);
        std::span<uint64_t>       sz(z.data(), N);
        EvaluatePubMultCore(sx, sc, sz);
    }
    void EvaluatePubMult(const std::vector<uint64_t> &x, const std::vector<uint64_t> &c, std::vector<uint64_t> &z) const;

    void EvaluateMult(const uint64_t      party_id,
                      osuCrypto::Channel &chl,
                      const uint64_t      x,
                      const uint64_t      y,
                      uint64_t           &z);
    template <size_t N>
    void EvaluateMult(const uint64_t                 party_id,
                      osuCrypto::Channel            &chl,
                      const std::array<uint64_t, N> &x,
                      const std::array<uint64_t, N> &y,
                      std::array<uint64_t, N>       &z) {
        std::span<const uint64_t> sx(x.data(), N);
        std::span<const uint64_t> sy(y.data(), N);
        std::span<uint64_t>       sz(z.data(), N);
        EvaluateMultCore(party_id, chl, sx, sy, sz);
    }
    void EvaluateMult(const uint64_t               party_id,
                      osuCrypto::Channel          &chl,
                      const std::vector<uint64_t> &x,
                      const std::vector<uint64_t> &y,
                      std::vector<uint64_t>       &z);

    void EvaluateSelect(const uint64_t      party_id,
                        osuCrypto::Channel &chl,
                        const uint64_t      x,
                        const uint64_t      y,
                        const uint64_t      c,
                        uint64_t           &z);
    template <size_t N>
    void EvaluateSelect(const uint64_t                 party_id,
                        osuCrypto::Channel            &chl,
                        const std::array<uint64_t, N> &x,
                        const std::array<uint64_t, N> &y,
                        const std::array<uint64_t, N> &c,
                        std::array<uint64_t, N>       &z) {
        std::span<const uint64_t> sx(x.data(), N);
        std::span<const uint64_t> sy(y.data(), N);
        std::span<const uint64_t> sc(c.data(), N);
        std::span<uint64_t>       sz(z.data(), N);
        EvaluateSelectCore(party_id, chl, sx, sy, sc, sz);
    }
    void EvaluateSelect(const uint64_t               party_id,
                        osuCrypto::Channel          &chl,
                        const std::vector<uint64_t> &x,
                        const std::vector<uint64_t> &y,
                        const std::vector<uint64_t> &c,
                        std::vector<uint64_t>       &z);

    uint64_t GetBitSize() const;

    void     SetTriples(BeaverTriples triples);
    void     ClearTriples();
    bool     HasTriples() const;
    void     RequireTriples(const std::string &func_name) const;
    uint64_t GetNumTriples() const;
    uint64_t GetRemainingTripleCount() const;
    void     ResetTripleIndex();
    void     PrintTriples(const size_t limit = 0) const;

private:
    const uint64_t bitsize_;      /**< The size of the bits used for secret sharing operations. */
    BeaverTriples  triples_;      /**< The Beaver triples used for secure multiplication. */
    size_t         triple_index_; /**< The index of the current Beaver triple. */

    void ShareCore(std::span<const uint64_t> x,
                   std::span<uint64_t>       x_0,
                   std::span<uint64_t>       x_1) const;
    void ReconstLocalCore(std::span<const uint64_t> x_0,
                          std::span<const uint64_t> x_1,
                          std::span<uint64_t>       x) const;
    void ReconstCore(const uint64_t      party_id,
                     osuCrypto::Channel &chl,
                     std::span<uint64_t> x_0,
                     std::span<uint64_t> x_1,
                     std::span<uint64_t> x) const;
    void EvaluateAddCore(std::span<const uint64_t> x,
                         std::span<const uint64_t> y,
                         std::span<uint64_t>       z) const;
    void EvaluateSubCore(std::span<const uint64_t> x,
                         std::span<const uint64_t> y,
                         std::span<uint64_t>       z) const;
    void EvaluatePubMultCore(std::span<const uint64_t> x,
                             std::span<const uint64_t> c,
                             std::span<uint64_t>       z) const;
    void EvaluateMultCore(const uint64_t            party_id,
                          osuCrypto::Channel       &chl,
                          std::span<const uint64_t> x,
                          std::span<const uint64_t> y,
                          std::span<uint64_t>       z);
    void EvaluateSelectCore(const uint64_t            party_id,
                            osuCrypto::Channel       &chl,
                            std::span<const uint64_t> x,
                            std::span<const uint64_t> y,
                            std::span<const uint64_t> c,
                            std::span<uint64_t>       z);

    // Internal functions
    void EnsureTriples(size_t need, const std::string &func_name) const;
};

}    // namespace sharing
}    // namespace ringoa

#endif
