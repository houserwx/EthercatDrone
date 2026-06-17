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

private:
    std::string localIp_;
    std::string peerIp_;
    uint16_t    port_;
    uint32_t    heartbeatIntervalMs_;
    uint32_t    timeoutMs_;

    std::atomic<Role> role_{Role::STANDBY};
    std::atomic<bool> running_{false};
};

} // namespace fc::redundancy
