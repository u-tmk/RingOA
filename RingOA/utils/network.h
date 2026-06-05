#ifndef UTILS_NETWORK_H_
#define UTILS_NETWORK_H_

#include <functional>
#include <thread>

#include <cryptoTools/Network/Channel.h>
#include <cryptoTools/Network/IOService.h>

namespace ringoa {

/**
 * TwoPartyNetworkManager
 *
 * Usage:
 *   TwoPartyNetworkManager nm("chan");
 *   nm.AutoConfigure(party_id,
 *     [](osuCrypto::Channel& chl){ ** server task ** },
 *     [](osuCrypto::Channel& chl){ ** client task ** });
 *   nm.WaitForCompletion();
 *
 * Notes:
 *   - Not thread-safe across methods. Call from a single thread per instance.
 *   - Destructor joins threads and stops IO service if still running.
 */
class TwoPartyNetworkManager {
public:
    static constexpr const char *DEFAULT_IP   = "127.0.0.1";
    static constexpr uint16_t    DEFAULT_PORT = 54321;

    TwoPartyNetworkManager(const std::string &channel_name,
                           const std::string &ip_address = DEFAULT_IP,
                           uint16_t           port       = DEFAULT_PORT);

    // Non-copyable, movable
    TwoPartyNetworkManager(const TwoPartyNetworkManager &)                = delete;
    TwoPartyNetworkManager &operator=(const TwoPartyNetworkManager &)     = delete;
    TwoPartyNetworkManager(TwoPartyNetworkManager &&) noexcept            = default;
    TwoPartyNetworkManager &operator=(TwoPartyNetworkManager &&) noexcept = default;

    void StartServer(std::function<void(osuCrypto::Channel &)> server_task);
    void StartClient(std::function<void(osuCrypto::Channel &)> client_task);

    // party_id: 0 -> server, 1 -> client, others -> both
    void AutoConfigure(int                                       party_id,
                       std::function<void(osuCrypto::Channel &)> server_task,
                       std::function<void(osuCrypto::Channel &)> client_task);

    void WaitForCompletion();

private:
    std::string          channel_name_;
    std::string          ip_address_;
    uint16_t             port_;
    osuCrypto::IOService ios_;
    std::thread          server_thread_;
    std::thread          client_thread_;
    std::exception_ptr   server_exception_ = nullptr;
    std::exception_ptr   client_exception_ = nullptr;
    std::mutex           exception_mutex_;
};

/**
 * ThreePartyNetworkManager
 *
 * Usage:
 *   ThreePartyNetworkManager nm;
 *   nm.AutoConfigure(party_id, task_p0, task_p1, task_p2);
 *   nm.WaitForCompletion();
 *
 * Notes:
 *   - Each party connects to prev/next neighbors: ids (i-1) mod 3, (i+1) mod 3.
 *   - Destructor joins threads and stops IO service if still running.
 */
class ThreePartyNetworkManager {
public:
    static constexpr const char *DEFAULT_IP   = "127.0.0.1";
    static constexpr uint16_t    DEFAULT_PORT = 55555;

    ThreePartyNetworkManager(const std::string &ip_address = DEFAULT_IP,
                             uint16_t           port       = DEFAULT_PORT);

    // Non-copyable, movable
    ThreePartyNetworkManager(const ThreePartyNetworkManager &)                = delete;
    ThreePartyNetworkManager &operator=(const ThreePartyNetworkManager &)     = delete;
    ThreePartyNetworkManager(ThreePartyNetworkManager &&) noexcept            = default;
    ThreePartyNetworkManager &operator=(ThreePartyNetworkManager &&) noexcept = default;

    void Start(const uint64_t party_id, std::function<void(osuCrypto::Channel &, osuCrypto::Channel &)> task);

    void AutoConfigure(int                                                             party_id,
                       std::function<void(osuCrypto::Channel &, osuCrypto::Channel &)> party0_task,
                       std::function<void(osuCrypto::Channel &, osuCrypto::Channel &)> party1_task,
                       std::function<void(osuCrypto::Channel &, osuCrypto::Channel &)> party2_task);

    void WaitForCompletion();

private:
    std::string          ip_address_;
    uint16_t             port_;
    osuCrypto::IOService ios_;
    std::thread          party0_thread_;
    std::thread          party1_thread_;
    std::thread          party2_thread_;
    std::exception_ptr   exception_ = nullptr;
    std::mutex           exception_mutex_;
};

// Channels structure for managing three-party communication
struct Channels {
    uint64_t            party_id;
    osuCrypto::Channel &prev;
    osuCrypto::Channel &next;

    Channels(uint64_t party_id_, osuCrypto::Channel &prev_, osuCrypto::Channel &next_)
        : party_id(party_id_), prev(prev_), next(next_) {
    }

    uint64_t GetStatsSent() const {
        return prev.getTotalDataSent() + next.getTotalDataSent();
    }

    uint64_t GetStatsRecv() const {
        return prev.getTotalDataRecv() + next.getTotalDataRecv();
    }

    void ResetStats() {
        prev.resetStats();
        next.resetStats();
    }
};
}    // namespace ringoa

#endif    // UTILS_NETWORK_H_
