#ifndef PROTOCOL_CONTEXT_2P_H_
#define PROTOCOL_CONTEXT_2P_H_

#include "RingOA/sharing/share_config.h"
#include "RingOA/sharing/sharing_2p_binary.h"
#include "RingOA/sharing/sharing_2p_ring.h"
#include "RingOA/utils/timer.h"

namespace osuCrypto {

class Channel;

}    // namespace osuCrypto

namespace ringoa {
namespace proto {

class ProtocolContext2P {
public:
    ProtocolContext2P() = delete;

    explicit ProtocolContext2P(const sharing::ShareConfig &config)
        : config_(config),
          arith_(config_),
          bool_(sharing::ShareConfig::Custom(1)) {
    }

    const sharing::ShareConfig &Config() const {
        return config_;
    }
    sharing::AdditiveSharing2P &Arith() {
        return arith_;
    }
    const sharing::AdditiveSharing2P &Arith() const {
        return arith_;
    }
    sharing::AdditiveSharing2P &Bool() {
        return bool_;
    }
    const sharing::AdditiveSharing2P &Bool() const {
        return bool_;
    }

    TimerManager &Timers() {
        return timers_;
    }
    const TimerManager &Timers() const {
        return timers_;
    }

    // Communication measurement (sent only)
    void     StartCommStats(osuCrypto::Channel &chl);
    uint64_t StopCommStats(osuCrypto::Channel &chl) const;    // returns sent bytes since StartCommStats

private:
    sharing::ShareConfig       config_;
    sharing::AdditiveSharing2P arith_;
    sharing::AdditiveSharing2P bool_;

    TimerManager timers_;

    uint64_t comm_begin_sent_ = 0;
};

}    // namespace proto
}    // namespace ringoa

#endif    // PROTOCOL_CONTEXT_2P_H_
