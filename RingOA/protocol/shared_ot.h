#ifndef PROTOCOL_SHARED_OT_H_
#define PROTOCOL_SHARED_OT_H_

#include "RingOA/fss/dpf_eval.h"
#include "RingOA/fss/dpf_gen.h"
#include "RingOA/fss/dpf_key.h"
#include "RingOA/sharing/share_aliases.h"
#include "context_3p.h"

namespace ringoa {

class Channels;

namespace proto {

struct SharedOtConfig {
    // User-facing: database size in bits.
    // Size of the database is equal to dpf_input_domain = 2^{db_index_bits}.
    explicit SharedOtConfig(uint64_t db_bits)
        : db_index_bits(db_bits) {
    }

    uint64_t db_index_bits;

    fss::EvalType eval_type = fss::kOptimizedEvalType;
};

class SharedOtParameters {
public:
    SharedOtParameters() = delete;
    SharedOtParameters(const SharedOtConfig &cfg, const sharing::ShareConfig &share)
        : db_index_bits_(cfg.db_index_bits),
          share_bitsize_(share.arith_bits),
          params_(cfg.db_index_bits, share.arith_bits, cfg.eval_type) {
    }

    uint64_t GetDbIndexBits() const {
        return db_index_bits_;
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
    uint64_t                share_bitsize_ = 0;
    fss::dpf::DpfParameters params_;
};

struct SharedOtKeyView {
    const fss::dpf::DpfKey &key_from_prev;
    const fss::dpf::DpfKey &key_from_next;
    const uint64_t          rsh_from_prev;
    const uint64_t          rsh_from_next;

    SharedOtKeyView(const fss::dpf::DpfKey &key_from_prev_,
                    const fss::dpf::DpfKey &key_from_next_,
                    const uint64_t          rsh_from_prev_,
                    const uint64_t          rsh_from_next_)
        : key_from_prev(key_from_prev_),
          key_from_next(key_from_next_),
          rsh_from_prev(rsh_from_prev_),
          rsh_from_next(rsh_from_next_) {
    }
};

struct SharedOtKeys {
    uint64_t                      party_id;
    std::vector<fss::dpf::DpfKey> key_from_prev;
    std::vector<fss::dpf::DpfKey> key_from_next;
    std::vector<uint64_t>         rsh_from_prev;
    std::vector<uint64_t>         rsh_from_next;

    SharedOtKeys() = default;
    explicit SharedOtKeys(
        const uint64_t            party_id_,
        const SharedOtParameters &params,
        size_t                    count);
    ~SharedOtKeys() = default;

    SharedOtKeys(const SharedOtKeys &)                = delete;
    SharedOtKeys &operator=(const SharedOtKeys &)     = delete;
    SharedOtKeys(SharedOtKeys &&) noexcept            = default;
    SharedOtKeys &operator=(SharedOtKeys &&) noexcept = default;

    bool operator==(const SharedOtKeys &rhs) const {
        return (party_id == rhs.party_id) &&
               (key_from_prev == rhs.key_from_prev) &&
               (key_from_next == rhs.key_from_next) &&
               (rsh_from_prev == rhs.rsh_from_prev) &&
               (rsh_from_next == rhs.rsh_from_next);
    }
    bool operator!=(const SharedOtKeys &rhs) const {
        return !(*this == rhs);
    }

    size_t Size() const {
        return key_from_prev.size();
    }

    SharedOtKeyView GetView(size_t index) const {
        if (index >= Size()) {
            throw std::out_of_range("SharedOtKeys::GetView: index out of range");
        }
        return SharedOtKeyView(
            key_from_prev[index],
            key_from_next[index],
            rsh_from_prev[index],
            rsh_from_next[index]);
    }

    size_t CalculateSerializedSize() const;
    void   Serialize(std::vector<uint8_t> &buffer) const;
    void   Deserialize(const std::vector<uint8_t> &buffer, const SharedOtParameters &params);
    void   Deserialize(const std::vector<uint8_t> &buffer, const SharedOtParameters &params, size_t &offset);
};

struct SharedOtPreprocessMsg {
    std::vector<fss::dpf::DpfKey> dpf_keys;
    std::vector<uint64_t>         r_share;

    size_t CalculateSerializedSize() const;
    void   Serialize(std::vector<uint8_t> &buffer) const;
    void   Deserialize(const std::vector<uint8_t> &buffer, const SharedOtParameters &params);
    void   Deserialize(const std::vector<uint8_t> &buffer, const SharedOtParameters &params, size_t &offset);
};

struct SharedOtPreprocessData {
    SharedOtKeys keys;

    SharedOtPreprocessData() = default;
    SharedOtPreprocessData(SharedOtKeys &&keys_in)
        : keys(std::move(keys_in)) {
    }

    static SharedOtPreprocessData FromMessage(uint64_t                  party_id,
                                              const SharedOtParameters &params,
                                              size_t                    count,
                                              SharedOtPreprocessMsg   &&from_prev,
                                              SharedOtPreprocessMsg   &&from_next);

    size_t CalculateSerializedSize() const;
    void   Serialize(std::vector<uint8_t> &buffer) const;
    void   Deserialize(const std::vector<uint8_t> &buffer, const SharedOtParameters &params);
    void   Deserialize(const std::vector<uint8_t> &buffer, const SharedOtParameters &params, size_t &offset);
};

struct SharedOtNeighborMsg {
    SharedOtPreprocessMsg to_prev;
    SharedOtPreprocessMsg to_next;
};

struct SharedOtNeighborMsgIn {
    SharedOtPreprocessMsg from_prev;
    SharedOtPreprocessMsg from_next;
};

class SharedOt {
public:
    SharedOt() = delete;
    SharedOt(const SharedOtParameters &params,
             ProtocolContext3P        &ctx);

    // Preprocessing phase
    SharedOtNeighborMsg    MakePreprocessMsg(size_t count) const;
    SharedOtNeighborMsgIn  ExchangePreprocessMsg(Channels &chls, SharedOtNeighborMsg &&out) const;
    SharedOtPreprocessData BuildPreprocessData(uint64_t                party_id,
                                               size_t                  count,
                                               SharedOtNeighborMsgIn &&in) const;
    SharedOtPreprocessData Preprocess(Channels &chls, size_t count) const;

    // Online phase
    void ObliviousAccess(Channels                       &chls,
                         const SharedOtKeyView          &key,
                         std::vector<uint64_t>          &uv_prev,
                         std::vector<uint64_t>          &uv_next,
                         const sharing::Rep3ShareView64 &database,
                         const sharing::Rep3Share64     &index,
                         sharing::Rep3Share64           &result) const;

    void ObliviousAccessPair(Channels                       &chls,
                             const SharedOtKeyView          &key1,
                             const SharedOtKeyView          &key2,
                             std::vector<uint64_t>          &uv_prev,
                             std::vector<uint64_t>          &uv_next,
                             const sharing::Rep3ShareView64 &database,
                             const sharing::Rep3ShareVec64  &index,
                             sharing::Rep3ShareVec64        &result) const;

    std::pair<uint64_t, uint64_t> EvaluateFullDomainThenDotProduct(
        const uint64_t                  party_id,
        const fss::dpf::DpfKey         &key_from_prev,
        const fss::dpf::DpfKey         &key_from_next,
        std::vector<uint64_t>          &uv_prev,
        std::vector<uint64_t>          &uv_next,
        const sharing::Rep3ShareView64 &database,
        const uint64_t                  pr_prev,
        const uint64_t                  pr_next) const;

private:
    struct TimerIds {
        int32_t full_domain = -1;
        int32_t dot_product = -1;
        int32_t local_eval  = -1;
    };

    SharedOtParameters        params_;
    fss::dpf::DpfKeyGenerator gen_;
    fss::dpf::DpfEvaluator    eval_;
    ProtocolContext3P        &ctx_;

    TimerIds timers_;

    // Internal functions
    std::pair<uint64_t, uint64_t> ReconstructMaskedValue(
        Channels                   &chls,
        const SharedOtKeyView      &key,
        const sharing::Rep3Share64 &index) const;

    std::array<uint64_t, 4> ReconstructMaskedValue(
        Channels                      &chls,
        const SharedOtKeyView         &key1,
        const SharedOtKeyView         &key2,
        const sharing::Rep3ShareVec64 &index) const;
};

}    // namespace proto
}    // namespace ringoa

#endif    // PROTOCOL_SHARED_OT_H_
