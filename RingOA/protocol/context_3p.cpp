#include "context_3p.h"

#include <cryptoTools/Network/Channel.h>

namespace ringoa {
namespace proto {

void ProtocolContext3P::StartCommStats(Channels &chls) {
    comm_begin_sent_ = chls.GetStatsSent();
}

uint64_t ProtocolContext3P::StopCommStats(Channels &chls) const {
    const uint64_t now = chls.GetStatsSent();
    return now - comm_begin_sent_;
}

void ProtocolContext3PBinary::StartCommStats(Channels &chls) {
    comm_begin_sent_ = chls.GetStatsSent();
}

uint64_t ProtocolContext3PBinary::StopCommStats(Channels &chls) const {
    const uint64_t now = chls.GetStatsSent();
    return now - comm_begin_sent_;
}

}    // namespace proto
}    // namespace ringoa
