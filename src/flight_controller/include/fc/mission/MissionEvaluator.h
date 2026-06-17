#pragma once

#include "fc/mission/FlightLeg.h"
#include "common/math/Math.h"

#include <cstdint>
#include <vector>

namespace fc::mission {

class WrapperPool;

// ============================================================================
// LegEvalState — one compiled leg evaluation rule.
// Populated from FlightLeg at init time, mutated during RT execution.
// ============================================================================
struct LegEvalState {
    int         legIdx{0};
    LegCriterion criterion{LegCriterion::PositionReached};

    // Target state (copied from FlightLeg at init time)
    common::math::Vec3f targetPosition{0.0f, 0.0f, 0.0f};
    float       targetAltitude{0.0f};

    // Thresholds
    float       arrivalRadius{5.0f};
    float       altitudeTolerance{2.0f};
    float       dwellTimeSeconds{0.0f};
    uint64_t    timeoutNs{0};
    int         msgOutIdx{-1};
    int         msgInIdx{-1};

    // RT-mutable state
    uint64_t    enterTimeNs{0};
    bool        actionPending{false};
    bool        actionComplete{false};
    bool        canAdvance{false};
};

// ============================================================================
// MissionEvaluator — evaluates leg completion criteria every RT cycle.
// Tagged enum + switch dispatch (no virtual calls).
// ============================================================================
class MissionEvaluator {
public:
    // Init-time
    void load(std::vector<LegEvalState> states);
    void freeze() noexcept;

    // RT tick — evaluate all leg rules
    // WrapperPool is reserved for gRPC action polling (Phase 2).
    void tick(const common::math::Vec3f& currentPosition,
              float currentAltitude, uint64_t nowNs) noexcept;

    [[nodiscard]] bool shouldAdvanceLeg(int legIdx) const noexcept;
    [[nodiscard]] bool missionComplete() const noexcept;

private:
    std::vector<LegEvalState> states_;

    // Helper: check criterion for a single leg (inline for max inlining)
    bool checkCriterion(const LegEvalState& state,
                        const common::math::Vec3f& position,
                        float altitude,
                        uint64_t nowNs) noexcept;
};

// ============================================================================
// Inline implementations for maximum inlining
// ============================================================================
inline bool MissionEvaluator::shouldAdvanceLeg(int legIdx) const noexcept
{
    if (legIdx < 0 || legIdx >= static_cast<int>(states_.size())) return false;
    return states_[legIdx].canAdvance;
}

inline bool MissionEvaluator::missionComplete() const noexcept
{
    for (const auto& state : states_) {
        if (!state.canAdvance) return false;
    }
    return true;
}

} // namespace fc::mission
