#include "context_2p.h"

#include <cryptoTools/Network/Channel.h>

namespace ringoa {
namespace proto {

void ProtocolContext2P::StartCommStats(osuCrypto::Channel &chl) {
    comm_begin_sent_ = chl.getTotalDataSent();
}

uint64_t ProtocolContext2P::StopCommStats(osuCrypto::Channel &chl) const {
    const uint64_t now = chl.getTotalDataSent();
    return now - comm_begin_sent_;
}

}    // namespace proto
}    // namespace ringoa
