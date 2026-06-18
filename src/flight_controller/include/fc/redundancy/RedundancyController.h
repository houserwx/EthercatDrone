#pragma once

#include "common/rt/Threadrunner.h"

#include <atomic>
#include <cstdint>
#include <string>

namespace fc::redundancy {

// ---------------------------------------------------------------------------
// Role — primary or standby.
// ---------------------------------------------------------------------------
enum class Role : uint8_t {
    STANDBY  = 0,
    PRIMARY  = 1,
};

// ---------------------------------------------------------------------------
// HeartbeatPacket — UDP heartbeat payload (packed, 16 bytes total).
// ---------------------------------------------------------------------------
struct HeartbeatPacket {
    uint64_t  timestamp_ns; // Sender's monotonic timestamp (ns)
    uint32_t  cycle_count;  // Sender's current cycle count (4 bytes sufficient)
    uint8_t   role;         // Current role (Role enum)
    uint8_t   reserved[3];  // Reserved for future use
} __attribute__((packed));

static_assert(sizeof(HeartbeatPacket) == 16, "HeartbeatPacket must be 16 bytes");

// ---------------------------------------------------------------------------
// RedundancyController — non-RT thread managing dual-master redundancy.
//
// UDP heartbeat on private link, role election, failover coordination.
// ---------------------------------------------------------------------------
class RedundancyController : public common::rt::Threadrunner {
public:
    RedundancyController(std::string localIp, std::string peerIp,
                         uint16_t port = 12345,
                         uint32_t heartbeatIntervalMs = 1,
                         uint32_t timeoutMs = 10);

    ~RedundancyController() override = default;

    void run() override;

    void requestStop() noexcept;

    [[nodiscard]] Role currentRole() const noexcept {
        return role_.load(std::memory_order_acquire);
    }

    [[nodiscard]] bool isPrimary() const noexcept {
        return currentRole() == Role::PRIMARY;
    }

    [[nodiscard]] uint64_t lastHeartbeatReceivedNs() const noexcept {
        return lastHeartbeatReceivedNs_.load(std::memory_order_acquire);
    }

    [[nodiscard]] uint32_t peerCycleCount() const noexcept {
        return static_cast<uint32_t>(peerCycleCount_.load(std::memory_order_acquire));
    }

private:
    std::string localIp_;
    std::string peerIp_;
    uint16_t    port_;
    uint32_t    heartbeatIntervalMs_;
    uint32_t    timeoutMs_;

    std::atomic<Role> role_{Role::STANDBY};
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> lastHeartbeatReceivedNs_{0};
    std::atomic<uint64_t> peerCycleCount_{0};

    // Socket management
    int socket_{-1};
    bool createSocket();
    void closeSocket();
    bool sendHeartbeat(const HeartbeatPacket& packet);
    bool receiveHeartbeat(HeartbeatPacket& packet, uint32_t timeoutMs);
    void electRole();
    uint64_t nowNs() const noexcept;
};

} // namespace fc::redundancy
