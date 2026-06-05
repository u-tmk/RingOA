#ifndef PROTOCOL_OBLIV_SELECT_H_
#define PROTOCOL_OBLIV_SELECT_H_

#include "RingOA/fss/dpf_eval.h"
#include "RingOA/fss/dpf_gen.h"
#include "RingOA/fss/dpf_key.h"
#include "RingOA/sharing/beaver_triples.h"
#include "RingOA/sharing/share_aliases.h"
#include "context_3p.h"

namespace ringoa {

class Channels;

namespace fss {
namespace prg {

class PseudoRandomGenerator;

}    // namespace prg
}    // namespace fss

namespace proto {

struct OblivSelectConfig {
    // User-facing: database size in bits.
    // Size of the database is equal to dpf_input_domain = 2^{db_index_bits}.
    explicit OblivSelectConfig(uint64_t db_bits)
        : db_index_bits(db_bits) {
    }

    uint64_t db_index_bits;

    fss::EvalType   eval_type   = fss::kOptimizedEvalType;
    fss::OutputType output_mode = fss::OutputType::kShiftedAdditive;
};

class OblivSelectParameters {
public:
    OblivSelectParameters() = delete;
    OblivSelectParameters(const OblivSelectConfig &cfg, const sharing::ShareConfig &share)
        : db_index_bits_(cfg.db_index_bits),
          dpf_out_bits_(1),
          share_bitsize_(share.arith_bits),
          params_(cfg.db_index_bits, dpf_out_bits_, cfg.eval_type, cfg.output_mode) {
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

struct OblivSelectKeyView {
    uint64_t                party_id;
    const fss::dpf::DpfKey &key_from_prev;
    const fss::dpf::DpfKey &key_from_next;
    uint64_t                rsh_from_prev;
    uint64_t                rsh_from_next;

    OblivSelectKeyView(const uint64_t          id,
                       const fss::dpf::DpfKey &key_prev,
                       const fss::dpf::DpfKey &key_next,
                       const uint64_t          rsh_prev,
                       const uint64_t          rsh_next)
        : party_id(id),
          key_from_prev(key_prev),
          key_from_next(key_next),
          rsh_from_prev(rsh_prev),
          rsh_from_next(rsh_next) {
    }
};

struct OblivSelectKeys {
    uint64_t                      party_id;
    std::vector<fss::dpf::DpfKey> key_from_prev;
    std::vector<fss::dpf::DpfKey> key_from_next;
    std::vector<uint64_t>         rsh_from_prev;
    std::vector<uint64_t>         rsh_from_next;

    OblivSelectKeys() = default;
    explicit OblivSelectKeys(
        const uint64_t               party_id,
        const OblivSelectParameters &params,
        size_t                       count);
    ~OblivSelectKeys() = default;

    OblivSelectKeys(const OblivSelectKeys &)            = delete;
    OblivSelectKeys &operator=(const OblivSelectKeys &) = delete;
    OblivSelectKeys(OblivSelectKeys &&)                 = default;
    OblivSelectKeys &operator=(OblivSelectKeys &&)      = default;

    bool operator==(const OblivSelectKeys &rhs) const {
        return (party_id == rhs.party_id) &&
               (key_from_prev == rhs.key_from_prev) &&
               (key_from_next == rhs.key_from_next) &&
               (rsh_from_prev == rhs.rsh_from_prev) &&
               (rsh_from_next == rhs.rsh_from_next);
    }
    bool operator!=(const OblivSelectKeys &rhs) const {
        return !(*this == rhs);
    }

    size_t Size() const {
        return key_from_prev.size();
    }

    OblivSelectKeyView GetView(size_t index) const {
        if (index >= Size()) {
            throw std::out_of_range("OblivSelectKeys::GetView: index out of range");
        }
        return OblivSelectKeyView(
            party_id,
            key_from_prev[index],
            key_from_next[index],
            rsh_from_prev[index],
            rsh_from_next[index]);
    }

    size_t CalculateSerializedSize() const;
    void   Serialize(std::vector<uint8_t> &buffer) const;
    void   Deserialize(const std::vector<uint8_t> &buffer, const OblivSelectParameters &params);
    void   Deserialize(const std::vector<uint8_t> &buffer, const OblivSelectParameters &params, size_t &offset);
};

struct OblivSelectPreprocessMsg {
    std::vector<fss::dpf::DpfKey> dpf_keys;
    std::vector<uint64_t>         r_share;

    size_t CalculateSerializedSize() const;
    void   Serialize(std::vector<uint8_t> &buffer) const;
    void   Deserialize(const std::vector<uint8_t> &buffer, const OblivSelectParameters &params);
    void   Deserialize(const std::vector<uint8_t> &buffer, const OblivSelectParameters &params, size_t &offset);
};

struct OblivSelectPreprocessData {
    OblivSelectKeys keys;

    OblivSelectPreprocessData() = default;
    OblivSelectPreprocessData(OblivSelectKeys &&keys_in)
        : keys(std::move(keys_in)) {
    }

    static OblivSelectPreprocessData FromMessage(uint64_t                     party_id,
                                                 const OblivSelectParameters &params,
                                                 size_t                       count,
                                                 OblivSelectPreprocessMsg   &&from_prev,
                                                 OblivSelectPreprocessMsg   &&from_next);

    size_t CalculateSerializedSize() const;
    void   Serialize(std::vector<uint8_t> &buffer) const;
    void   Deserialize(const std::vector<uint8_t> &buffer, const OblivSelectParameters &params);
    void   Deserialize(const std::vector<uint8_t> &buffer, const OblivSelectParameters &params, size_t &offset);
};

struct OblivSelectNeighborMsg {
    OblivSelectPreprocessMsg to_prev;
    OblivSelectPreprocessMsg to_next;
};

struct OblivSelectNeighborMsgIn {
    OblivSelectPreprocessMsg from_prev;
    OblivSelectPreprocessMsg from_next;
};

class OblivSelect {
public:
    OblivSelect() = delete;
    OblivSelect(const OblivSelectParameters &params,
                ProtocolContext3PBinary     &ctx);

    // Preprocessing phase
    OblivSelectNeighborMsg    MakePreprocessMsg(size_t count) const;
    OblivSelectNeighborMsgIn  ExchangePreprocessMsg(Channels &chls, OblivSelectNeighborMsg &&out) const;
    OblivSelectPreprocessData BuildPreprocessData(uint64_t                   party_id,
                                                  size_t                     count,
                                                  OblivSelectNeighborMsgIn &&in) const;
    OblivSelectPreprocessData Preprocess(Channels &chls, size_t count) const;

    // Online phase
    void ObliviousAccess(Channels                          &chls,
                         const OblivSelectKeyView          &key,
                         const sharing::Rep3ShareViewBlock &database,
                         const sharing::Rep3Share64        &index,
                         sharing::Rep3ShareBlock           &result) const;

    void ObliviousAccess(Channels                       &chls,
                         const OblivSelectKeyView       &key,
                         std::vector<block>             &uv_prev,
                         std::vector<block>             &uv_next,
                         const sharing::Rep3ShareView64 &database,
                         const sharing::Rep3Share64     &index,
                         sharing::Rep3Share64           &result) const;

    void ObliviousAccessPair(Channels                       &chls,
                             const OblivSelectKeyView       &key1,
                             const OblivSelectKeyView       &key2,
                             std::vector<block>             &uv_prev,
                             std::vector<block>             &uv_next,
                             const sharing::Rep3ShareView64 &database,
                             const sharing::Rep3ShareVec64  &index,
                             sharing::Rep3ShareVec64        &result) const;

    block ComputeDotProductBlockSIMD(const fss::dpf::DpfKey       &key,
                                     const std::span<const block> &database,
                                     const uint64_t                pr) const;

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
    };

    OblivSelectParameters            params_;
    fss::dpf::DpfKeyGenerator        gen_;
    fss::dpf::DpfEvaluator           eval_;
    ProtocolContext3PBinary         &ctx_;
    fss::prg::PseudoRandomGenerator &G_;

    TimerIds timers_;

    // Internal functions
    std::pair<uint64_t, uint64_t> ReconstructMaskedValue(
        Channels                   &chls,
        const OblivSelectKeyView   &key,
        const sharing::Rep3Share64 &index) const;

    std::array<uint64_t, 4> ReconstructMaskedValue(
        Channels                      &chls,
        const OblivSelectKeyView      &key1,
        const OblivSelectKeyView      &key2,
        const sharing::Rep3ShareVec64 &index) const;

    void EvaluateNextSeed(const uint64_t current_level, const block &current_seed, const bool &current_control_bit,
                          std::array<block, 2> &expanded_seeds, std::array<bool, 2> &expanded_control_bits,
                          const fss::dpf::DpfKey &key) const;
};

}    // namespace proto
}    // namespace ringoa

#endif    // PROTOCOL_OBLIV_SELECT_H_
