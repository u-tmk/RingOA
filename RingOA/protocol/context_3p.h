#ifndef PROTOCOL_CONTEXT_3P_H_
#define PROTOCOL_CONTEXT_3P_H_

#include "RingOA/sharing/rep3_sharing_binary.h"
#include "RingOA/sharing/rep3_sharing_ring.h"
#include "RingOA/sharing/share_config.h"
#include "RingOA/sharing/sharing_2p_binary.h"
#include "RingOA/sharing/sharing_2p_ring.h"
#include "RingOA/utils/timer.h"

namespace ringoa {
namespace proto {

class ProtocolContext3P {
public:
    ProtocolContext3P() = delete;

    explicit ProtocolContext3P(const sharing::ShareConfig &config)
        : config_(config),
          rss_(config),
          ass_with_prev_(config),
          ass_with_next_(config) {
    }

    const sharing::ShareConfig &Config() const {
        return config_;
    }
    sharing::ReplicatedSharing3P &Rss() {
        return rss_;
    }
    const sharing::ReplicatedSharing3P &Rss() const {
        return rss_;
    }
    sharing::AdditiveSharing2P &AssWithPrev() {
        return ass_with_prev_;
    }
    const sharing::AdditiveSharing2P &AssWithPrev() const {
        return ass_with_prev_;
    }
    sharing::AdditiveSharing2P &AssWithNext() {
        return ass_with_next_;
    }
    const sharing::AdditiveSharing2P &AssWithNext() const {
        return ass_with_next_;
    }

    TimerManager &Timers() {
        return timers_;
    }
    const TimerManager &Timers() const {
        return timers_;
    }

    // Communication measurement (sent only)
    void     StartCommStats(Channels &chls);
    uint64_t StopCommStats(Channels &chls) const;

private:
    sharing::ShareConfig         config_;
    sharing::ReplicatedSharing3P rss_;
    sharing::AdditiveSharing2P   ass_with_prev_;
    sharing::AdditiveSharing2P   ass_with_next_;

    TimerManager timers_;

    uint64_t comm_begin_sent_ = 0;
};

class ProtocolContext3PBinary {
public:
    ProtocolContext3PBinary() = delete;

    explicit ProtocolContext3PBinary(const sharing::ShareConfig &config)
        : config_(config),
          brss_(config),
          bss_(config) {
    }

    const sharing::ShareConfig &Config() const {
        return config_;
    }
    sharing::BinaryReplicatedSharing3P &Brss() {
        return brss_;
    }
    const sharing::BinaryReplicatedSharing3P &Brss() const {
        return brss_;
    }
    sharing::BinarySharing2P &Bss() {
        return bss_;
    }
    const sharing::BinarySharing2P &Bss() const {
        return bss_;
    }

    TimerManager &Timers() {
        return timers_;
    }
    const TimerManager &Timers() const {
        return timers_;
    }

    // Communication measurement (sent only)
    void     StartCommStats(Channels &chls);
    uint64_t StopCommStats(Channels &chls) const;

private:
    sharing::ShareConfig               config_;
    sharing::BinaryReplicatedSharing3P brss_;
    sharing::BinarySharing2P           bss_;

    TimerManager timers_;

    uint64_t comm_begin_sent_ = 0;
};

}    // namespace proto
}    // namespace ringoa

#endif    // PROTOCOL_CONTEXT_3P_H_
