#ifndef PROTOCOL_RINGOA_H_
#define PROTOCOL_RINGOA_H_

#include "RingOA/fss/dpf_eval.h"
#include "RingOA/fss/dpf_gen.h"
#include "RingOA/fss/dpf_key.h"
#include "RingOA/sharing/beaver_triples.h"
#include "RingOA/sharing/share_aliases.h"
#include "context_3p.h"

namespace ringoa {

class Channels;

namespace proto {

struct RingOaConfig {
    // User-facing: database size in bits.
    // Size of the database is equal to dpf_input_domain = 2^{db_index_bits}.
    explicit RingOaConfig(uint64_t db_bits)
        : db_index_bits(db_bits) {
    }

    uint64_t db_index_bits;

    fss::EvalType eval_type = fss::kOptimizedEvalType;
};

class RingOaParameters {
public:
    RingOaParameters() = delete;
    RingOaParameters(const RingOaConfig &cfg, const sharing::ShareConfig &share)
        : db_index_bits_(cfg.db_index_bits),
          dpf_out_bits_(1),
          share_bitsize_(share.arith_bits),
          params_(cfg.db_index_bits, dpf_out_bits_, cfg.eval_type) {
    }

    uint64_t GetDbIndexBits() const {
        return db_index_bits_;
    }
    uint64_t GetDpfOutBits() const {
        return dpf_out_bits_;
    }
    uint64_t GetShareBitsize() const {
        return share_bitsize_;
    }

    const fss::dpf::DpfParameters &GetParameters() const {
        return params_;
    }

    void PrintParametersDebug(bool with_header = true, int key_width = 18) const;

private:
    uint64_t                db_index_bits_ = 0;
    uint64_t                dpf_out_bits_  = 0;
    uint64_t                share_bitsize_ = 0;
    fss::dpf::DpfParameters params_;
};

struct RingOaKeyView {
    uint64_t                party_id;
    const fss::dpf::DpfKey &key_from_prev;
    const fss::dpf::DpfKey &key_from_next;
    uint64_t                rsh_from_prev;
    uint64_t                rsh_from_next;
    uint64_t                wsh_from_prev;
    uint64_t                wsh_from_next;

    RingOaKeyView(uint64_t                party_id_,
                  const fss::dpf::DpfKey &key_from_prev_,
                  const fss::dpf::DpfKey &key_from_next_,
                  uint64_t                rsh_from_prev_,
                  uint64_t                rsh_from_next_,
                  uint64_t                wsh_from_prev_,
                  uint64_t                wsh_from_next_)
        : party_id(party_id_),
          key_from_prev(key_from_prev_),
          key_from_next(key_from_next_),
          rsh_from_prev(rsh_from_prev_),
          rsh_from_next(rsh_from_next_),
          wsh_from_prev(wsh_from_prev_),
          wsh_from_next(wsh_from_next_) {
    }
};

struct RingOaKeys {
    uint64_t                      party_id;
    std::vector<fss::dpf::DpfKey> key_from_prev;
    std::vector<fss::dpf::DpfKey> key_from_next;
    std::vector<uint64_t>         rsh_from_prev;
    std::vector<uint64_t>         rsh_from_next;
    std::vector<uint64_t>         wsh_from_prev;
    std::vector<uint64_t>         wsh_from_next;

    RingOaKeys() = default;
    explicit RingOaKeys(
        const uint64_t          party_id_,
        const RingOaParameters &params,
        size_t                  count);
    ~RingOaKeys() = default;

    RingOaKeys(const RingOaKeys &)                = delete;
    RingOaKeys &operator=(const RingOaKeys &)     = delete;
    RingOaKeys(RingOaKeys &&) noexcept            = default;
    RingOaKeys &operator=(RingOaKeys &&) noexcept = default;

    bool operator==(const RingOaKeys &rhs) const {
        return (party_id == rhs.party_id) &&
               (key_from_prev == rhs.key_from_prev) &&
               (key_from_next == rhs.key_from_next) &&
               (rsh_from_prev == rhs.rsh_from_prev) &&
               (rsh_from_next == rhs.rsh_from_next) &&
               (wsh_from_prev == rhs.wsh_from_prev) &&
               (wsh_from_next == rhs.wsh_from_next);
    }
    bool operator!=(const RingOaKeys &rhs) const {
        return !(*this == rhs);
    }

    size_t Size() const {
        return key_from_prev.size();
    }

    RingOaKeyView GetView(size_t index) const {
        if (index >= Size()) {
            throw std::out_of_range("RingOaKeys::GetView: index out of range");
        }
        return RingOaKeyView(
            party_id,
            key_from_prev[index],
            key_from_next[index],
            rsh_from_prev[index],
            rsh_from_next[index],
            wsh_from_prev[index],
            wsh_from_next[index]);
    }

    size_t CalculateSerializedSize() const;
    void   Serialize(std::vector<uint8_t> &buffer) const;
    void   Deserialize(const std::vector<uint8_t> &buffer, const RingOaParameters &params);
    void   Deserialize(const std::vector<uint8_t> &buffer, const RingOaParameters &params, size_t &offset);
};

struct RingOaPreprocessMsg {
    std::vector<fss::dpf::DpfKey>  dpf_keys;
    std::vector<uint64_t>          r_share;
    std::vector<uint64_t>          w_share;
    ringoa::sharing::BeaverTriples triples{};

    size_t CalculateSerializedSize() const;
    void   Serialize(std::vector<uint8_t> &buffer) const;
    void   Deserialize(const std::vector<uint8_t> &buffer, const RingOaParameters &params);
    void   Deserialize(const std::vector<uint8_t> &buffer, const RingOaParameters &params, size_t &offset);
};

struct RingOaPreprocessData {
    RingOaKeys                     keys;
    ringoa::sharing::BeaverTriples triples_with_prev{};
    ringoa::sharing::BeaverTriples triples_with_next{};

    RingOaPreprocessData() = default;
    RingOaPreprocessData(
        RingOaKeys                     &&keys_in,
        ringoa::sharing::BeaverTriples &&prev,
        ringoa::sharing::BeaverTriples &&next)
        : keys(std::move(keys_in)),
          triples_with_prev(std::move(prev)),
          triples_with_next(std::move(next)) {
    }

    static RingOaPreprocessData FromMessage(uint64_t                party_id,
                                            const RingOaParameters &params,
                                            size_t                  count,
                                            RingOaPreprocessMsg   &&from_prev,
                                            RingOaPreprocessMsg   &&from_next);

    size_t CalculateSerializedSize() const;
    void   LogSerializedSizeBreakdown(const std::string &prefix) const;
    void   Serialize(std::vector<uint8_t> &buffer) const;
    void   Deserialize(const std::vector<uint8_t> &buffer, const RingOaParameters &params);
    void   Deserialize(const std::vector<uint8_t> &buffer, const RingOaParameters &params, size_t &offset);
};

struct RingOaNeighborMsg {
    RingOaPreprocessMsg to_prev;
    RingOaPreprocessMsg to_next;
};

struct RingOaNeighborMsgIn {
    RingOaPreprocessMsg from_prev;
    RingOaPreprocessMsg from_next;
};

class RingOa {
public:
    RingOa() = delete;
    RingOa(const RingOaParameters &params,
           ProtocolContext3P      &ctx);

    // Preprocessing phase
    RingOaNeighborMsg         MakePreprocessMsg(size_t count) const;
    RingOaNeighborMsgIn       ExchangePreprocessMsg(Channels &chls, RingOaNeighborMsg &&out) const;
    RingOaPreprocessData      BuildPreprocessData(uint64_t              party_id,
                                                  size_t                count,
                                                  RingOaNeighborMsgIn &&in) const;
    RingOaPreprocessData      Preprocess(Channels &chls, size_t count) const;
    std::array<RingOaKeys, 3> GenerateKeys(size_t count) const;

    // Online phase
    void ConsumePreprocessData(RingOaPreprocessData &data) const;
    void ObliviousAccess(Channels                       &chls,
                         const RingOaKeyView            &key,
                         std::vector<block>             &uv_prev,
                         std::vector<block>             &uv_next,
                         const sharing::Rep3ShareView64 &database,
                         const sharing::Rep3Share64     &index,
                         sharing::Rep3Share64           &result) const;

    void ObliviousAccessPair(Channels                       &chls,
                             const RingOaKeyView            &key1,
                             const RingOaKeyView            &key2,
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

    std::pair<uint64_t, uint64_t> EvaluateFullDomainThenDotProduct(
        const uint64_t                  party_id,
        const fss::dpf::DpfKey         &key_from_prev,
        const fss::dpf::DpfKey         &key_from_next,
        std::vector<block>             &uv_prev,
        std::vector<block>             &uv_next,
        const sharing::Rep3ShareView32 &database,
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
        uint64_t alpha_hat) const;

    std::pair<uint64_t, uint64_t> ReconstructMaskedValue(
        Channels                   &chls,
        const RingOaKeyView        &key,
        const sharing::Rep3Share64 &index) const;

    std::array<uint64_t, 4> ReconstructMaskedValue(
        Channels                      &chls,
        const RingOaKeyView           &key1,
        const RingOaKeyView           &key2,
        const sharing::Rep3ShareVec64 &index) const;
};

}    // namespace proto
}    // namespace ringoa

#endif    // PROTOCOL_RINGOA_H_
