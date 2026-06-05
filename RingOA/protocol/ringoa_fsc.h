#ifndef PROTOCOL_RINGOA_FSC_H_
#define PROTOCOL_RINGOA_FSC_H_

#include "RingOA/fss/dpf_eval.h"
#include "RingOA/fss/dpf_gen.h"
#include "RingOA/fss/dpf_key.h"
#include "RingOA/sharing/beaver_triples.h"
#include "RingOA/sharing/share_aliases.h"
#include "context_3p.h"
#include "ringoa.h"

namespace ringoa {

class Channels;

namespace proto {

struct RingOaFscKeyView {
    uint64_t                party_id;
    const fss::dpf::DpfKey &key_from_prev;
    const fss::dpf::DpfKey &key_from_next;
    uint64_t                rsh_from_prev;
    uint64_t                rsh_from_next;
    uint64_t                w_from_prev;
    uint64_t                w_from_next;

    RingOaFscKeyView(uint64_t                party_id_,
                     const fss::dpf::DpfKey &key_from_prev_,
                     const fss::dpf::DpfKey &key_from_next_,
                     const uint64_t          rsh_from_prev_,
                     const uint64_t          rsh_from_next_,
                     const uint64_t          w_from_prev_,
                     const uint64_t          w_from_next_)
        : party_id(party_id_),
          key_from_prev(key_from_prev_),
          key_from_next(key_from_next_),
          rsh_from_prev(rsh_from_prev_),
          rsh_from_next(rsh_from_next_),
          w_from_prev(w_from_prev_),
          w_from_next(w_from_next_) {
    }
};

struct RingOaFscKeys {
    uint64_t                      party_id;
    std::vector<fss::dpf::DpfKey> key_from_prev;
    std::vector<fss::dpf::DpfKey> key_from_next;
    std::vector<uint64_t>         rsh_from_prev;
    std::vector<uint64_t>         rsh_from_next;
    std::vector<uint64_t>         w_from_prev;
    std::vector<uint64_t>         w_from_next;

    RingOaFscKeys() = default;
    explicit RingOaFscKeys(
        const uint64_t          party_id_,
        const RingOaParameters &params,
        size_t                  count);
    ~RingOaFscKeys() = default;

    RingOaFscKeys(const RingOaFscKeys &)                = delete;
    RingOaFscKeys &operator=(const RingOaFscKeys &)     = delete;
    RingOaFscKeys(RingOaFscKeys &&) noexcept            = default;
    RingOaFscKeys &operator=(RingOaFscKeys &&) noexcept = default;

    bool operator==(const RingOaFscKeys &rhs) const {
        return (party_id == rhs.party_id) &&
               (key_from_prev == rhs.key_from_prev) &&
               (key_from_next == rhs.key_from_next) &&
               (rsh_from_prev == rhs.rsh_from_prev) &&
               (rsh_from_next == rhs.rsh_from_next) &&
               (w_from_prev == rhs.w_from_prev) &&
               (w_from_next == rhs.w_from_next);
    }
    bool operator!=(const RingOaFscKeys &rhs) const {
        return !(*this == rhs);
    }

    size_t Size() const {
        return key_from_prev.size();
    }

    RingOaFscKeyView GetView(size_t index) const {
        if (index >= Size()) {
            throw std::out_of_range("RingOaFscKeys::GetView: index out of range");
        }
        return RingOaFscKeyView(
            party_id,
            key_from_prev[index],
            key_from_next[index],
            rsh_from_prev[index],
            rsh_from_next[index],
            w_from_prev[index],
            w_from_next[index]);
    }

    size_t CalculateSerializedSize() const;
    void   Serialize(std::vector<uint8_t> &buffer) const;
    void   Deserialize(const std::vector<uint8_t> &buffer, const RingOaParameters &params);
    void   Deserialize(const std::vector<uint8_t> &buffer, const RingOaParameters &params, size_t &offset);
};

struct RingOaFscPreprocessMsg {
    std::vector<fss::dpf::DpfKey> dpf_keys;
    std::vector<uint64_t>         r_share;
    std::vector<uint64_t>         w;

    size_t CalculateSerializedSize() const;
    void   Serialize(std::vector<uint8_t> &buffer) const;
    void   Deserialize(const std::vector<uint8_t> &buffer, const RingOaParameters &params);
    void   Deserialize(const std::vector<uint8_t> &buffer, const RingOaParameters &params, size_t &offset);
};

struct RingOaFscPreprocessData {
    RingOaFscKeys keys;

    RingOaFscPreprocessData() = default;
    RingOaFscPreprocessData(RingOaFscKeys &&keys_in)
        : keys(std::move(keys_in)) {
    }

    static RingOaFscPreprocessData FromMessage(uint64_t                 party_id,
                                               const RingOaParameters  &params,
                                               size_t                   count,
                                               RingOaFscPreprocessMsg &&from_prev,
                                               RingOaFscPreprocessMsg &&from_next);

    size_t CalculateSerializedSize() const;
    void   LogSerializedSizeBreakdown(const std::string &prefix = "") const;
    void   Serialize(std::vector<uint8_t> &buffer) const;
    void   Deserialize(const std::vector<uint8_t> &buffer, const RingOaParameters &params);
    void   Deserialize(const std::vector<uint8_t> &buffer, const RingOaParameters &params, size_t &offset);
};

struct RingOaFscNeighborMsg {
    RingOaFscPreprocessMsg to_prev;
    RingOaFscPreprocessMsg to_next;
};

struct RingOaFscNeighborMsgIn {
    RingOaFscPreprocessMsg from_prev;
    RingOaFscPreprocessMsg from_next;
};

class RingOaFsc {
public:
    RingOaFsc() = delete;
    RingOaFsc(const RingOaParameters &params,
              ProtocolContext3P      &ctx);

    void GenerateDatabaseShare(const std::vector<uint64_t>            &database,
                               std::array<sharing::Rep3ShareVec64, 3> &db_sh,
                               std::array<bool, 3>                    &v_sign) const;

    void GenerateDatabaseShare(const std::vector<uint64_t>            &database,
                               std::array<sharing::Rep3ShareMat64, 3> &db_sh,
                               size_t                                  rows,
                               size_t                                  cols,
                               std::array<bool, 3>                    &v_sign) const;

    // Preprocessing phase
    RingOaFscNeighborMsg    MakePreprocessMsg(size_t count, bool v_sign) const;
    RingOaFscNeighborMsgIn  ExchangePreprocessMsg(Channels &chls, RingOaFscNeighborMsg &&out) const;
    RingOaFscPreprocessData BuildPreprocessData(uint64_t                 party_id,
                                                size_t                   count,
                                                RingOaFscNeighborMsgIn &&in) const;
    RingOaFscPreprocessData Preprocess(Channels &chls, bool v_sign, size_t count) const;

    // Online phase
    void ObliviousAccess(Channels                       &chls,
                         const RingOaFscKeyView         &key,
                         std::vector<block>             &uv_prev,
                         std::vector<block>             &uv_next,
                         const sharing::Rep3ShareView64 &database,
                         const sharing::Rep3Share64     &index,
                         sharing::Rep3Share64           &result) const;

    void ObliviousAccessPair(Channels                       &chls,
                             const RingOaFscKeyView         &key1,
                             const RingOaFscKeyView         &key2,
                             std::vector<block>             &uv_prev,
                             std::vector<block>             &uv_next,
                             const sharing::Rep3ShareView64 &database,
                             const sharing::Rep3ShareVec64  &index,
                             sharing::Rep3ShareVec64        &result) const;

    std::pair<uint64_t, uint64_t> EvaluateFullDomainThenDotProduct(
        const uint64_t                  party_id,
        const fss::dpf::DpfKey         &key_from_prev,
        const fss::dpf::DpfKey         &key_from_next,
        std::vector<block>             &uv_prev,
        std::vector<block>             &uv_next,
        const sharing::Rep3ShareView64 &database,
        const uint64_t                  pr_prev,
        const uint64_t                  pr_next) const;

private:
    struct TimerIds {
        int32_t full_domain = -1;
        int32_t dot_product = -1;
        int32_t local_eval  = -1;
    };

    RingOaParameters          params_;
    fss::dpf::DpfKeyGenerator gen_;
    fss::dpf::DpfEvaluator    eval_;
    ProtocolContext3P        &ctx_;

    TimerIds timers_;

    // Internal functions
    uint64_t ComputeSignCorrection(
        block   &final_seed_0,
        block   &final_seed_1,
        bool     final_control_bit_1,
        bool     v_sign,
        uint64_t alpha_hat) const;

    std::pair<uint64_t, uint64_t> ReconstructMaskedValue(
        Channels                   &chls,
        const RingOaFscKeyView     &key,
        const sharing::Rep3Share64 &index) const;

    std::array<uint64_t, 4> ReconstructMaskedValue(
        Channels                      &chls,
        const RingOaFscKeyView        &key1,
        const RingOaFscKeyView        &key2,
        const sharing::Rep3ShareVec64 &index) const;
};

}    // namespace proto
}    // namespace ringoa

#endif    // PROTOCOL_RINGOA_FSC_H_
