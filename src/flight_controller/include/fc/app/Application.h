#pragma once

#include "common/rt/Threadrunner.h"
#include "fc/pdo/HardwareRegistry.h"
#include "fc/safety/MachineStateController.h"
#include "fc/app/Queue.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

namespace fc::app {

// ============================================================================
// Application — Real-time application thread.
//
// RT DESIGN PATTERN: Final Class, Composition Over Inheritance.
// Inherits from Threadrunner to reuse CPU pinning + SCHED_FIFO setup.
// ============================================================================

class Application final : public common::rt::Threadrunner {
public:
    Application(fc::pdo::HardwareRegistry& registry, uint32_t cycleNs);

    ~Application() override = default;

    void requestStop() noexcept;
    void addQueue(std::unique_ptr<Queue> q);

    [[nodiscard]] uint64_t cycleCount()   const noexcept;
    [[nodiscard]] int      overrunCount() const noexcept;
    [[nodiscard]] int64_t  maxOverrunNs() const noexcept;

    [[nodiscard]] const std::vector<std::unique_ptr<Queue>>& queues() const noexcept {
        return queues_;
    }

    void run() override;

private:
    void rtCycle() noexcept;

    fc::pdo::HardwareRegistry& registry_;
    uint32_t                    cycleNs_;

    fc::safety::MachineStateController stateMachine_;

    std::vector<std::unique_ptr<Queue>> queues_;

    std::atomic<bool> running_{false};
    uint64_t  cycleCount_   {0};
    int64_t   maxOverrunNs_ {0};
    int64_t   totalOverNs_  {0};
    int       overrunCount_ {0};
};

} // namespace fc::app
