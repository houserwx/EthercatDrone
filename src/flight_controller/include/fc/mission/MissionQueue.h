#pragma once

#include "fc/mission/FlightLeg.h"

#include <atomic>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace fc::mission {

// ============================================================================
// MissionQueue — per-mission representation.
// Owned by Application; one active mission at a time.
//
// Init-time: loadFromJson() → freeze()
// RT path: currentLeg(), leg(idx), advanceLeg() — all noexcept, O(1)
// ============================================================================
class MissionQueue {
public:
    MissionQueue(const MissionQueue&)            = delete;
    MissionQueue& operator=(const MissionQueue&) = delete;
    MissionQueue(MissionQueue&&)                 = delete;
    MissionQueue& operator=(MissionQueue&&)      = delete;
    ~MissionQueue()                              = default;

    // -----------------------------------------------------------------------
    // Init-time (non-RT)
    // -----------------------------------------------------------------------
    [[nodiscard]] static std::unique_ptr<MissionQueue>
        loadFromJson(const std::string& path);

    void freeze() noexcept;

    // -----------------------------------------------------------------------
    // RT-safe accessors (noexcept, O(1))
    // -----------------------------------------------------------------------
    [[nodiscard]] std::string_view  missionId() const noexcept { return missionId_; }
    [[nodiscard]] std::string_view  displayName() const noexcept { return displayName_; }
    [[nodiscard]] std::size_t       legCount() const noexcept { return legs_.size(); }
    [[nodiscard]] const FlightLeg&  leg(std::size_t idx) const noexcept;

    [[nodiscard]] int               currentLegIndex() const noexcept {
        return currentLegIdx_.load(std::memory_order_acquire);
    }

    void                            advanceLeg() noexcept;

    [[nodiscard]] bool              isComplete() const noexcept {
        return complete_.load(std::memory_order_acquire);
    }

    // Overall mission progress: 0.0 (start) → 1.0 (complete)
    [[nodiscard]] float             missionProgress() const noexcept;

private:
    MissionQueue() = default;
    friend std::unique_ptr<MissionQueue> createMissionQueue();

    std::string missionId_;
    std::string displayName_;
    std::vector<FlightLeg> legs_;

    std::atomic<int>   currentLegIdx_{0};
    std::atomic<bool>  complete_{false};
};

} // namespace fc::mission
