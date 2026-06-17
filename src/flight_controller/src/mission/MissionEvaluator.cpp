#include "fc/mission/MissionEvaluator.h"
#include "fc/app/WrapperPool.h"

#include <algorithm>
#include <cmath>

namespace fc::mission {

// ============================================================================
// load — populate from FlightLeg vector
// ============================================================================
void MissionEvaluator::load(std::vector<LegEvalState> states)
{
    states_ = std::move(states);
}

// ============================================================================
// freeze — reserve capacity for RT
// ============================================================================
void MissionEvaluator::freeze() noexcept
{
    states_.shrink_to_fit();
}

// ============================================================================
// tick — evaluate all leg criteria every RT cycle
// ============================================================================
void MissionEvaluator::tick(const common::math::Vec3f& currentPosition,
                            float currentAltitude,
                            uint64_t nowNs) noexcept
{
    for (auto& state : states_) {
        // Record entry time on first tick
        if (state.enterTimeNs == 0) {
            state.enterTimeNs = nowNs;
        }

        // Check timeout
        if (state.timeoutNs > 0) {
            uint64_t elapsed = nowNs - state.enterTimeNs;
            if (elapsed > state.timeoutNs) {
                state.canAdvance = true; // Timeout → advance (don't hang)
                continue;
            }
        }

        // Evaluate criterion
        state.canAdvance = checkCriterion(state, currentPosition, currentAltitude, nowNs);
    }
}

// ============================================================================
// checkCriterion — switch on LegCriterion (no virtual dispatch)
// ============================================================================
bool MissionEvaluator::checkCriterion(const LegEvalState& state,
                                       const common::math::Vec3f& position,
                                       float altitude,
                                       uint64_t nowNs) noexcept
{
    switch (state.criterion) {
        case LegCriterion::Always:
            return true;

        case LegCriterion::PositionReached: {
            // Horizontal distance check (z component ignored)
            float dx = position.x - state.targetPosition.x;
            float dy = position.y - state.targetPosition.y;
            float dist = std::sqrt(dx * dx + dy * dy);

            if (dist < state.arrivalRadius) {
                // Check dwell time
                if (state.dwellTimeSeconds > 0.0f) {
                    uint64_t elapsed = nowNs - state.enterTimeNs;
                    return elapsed > static_cast<uint64_t>(state.dwellTimeSeconds * 1e9);
                }
                return true;
            }
            return false;
        }

        case LegCriterion::AltitudeReached: {
            float altDiff = std::abs(altitude - state.targetAltitude);
            if (altDiff < state.altitudeTolerance) {
                if (state.dwellTimeSeconds > 0.0f) {
                    uint64_t elapsed = nowNs - state.enterTimeNs;
                    return elapsed > static_cast<uint64_t>(state.dwellTimeSeconds * 1e9);
                }
                return true;
            }
            return false;
        }

        case LegCriterion::TimeElapsed: {
            if (state.dwellTimeSeconds > 0.0f) {
                uint64_t elapsed = nowNs - state.enterTimeNs;
                return elapsed > static_cast<uint64_t>(state.dwellTimeSeconds * 1e9);
            }
            return true;
        }

        case LegCriterion::ImageMatch:
        case LegCriterion::ManualAck:
            // These require gRPC action completion
            return state.actionComplete;

        default:
            return true; // Safety: don't block on unknown criterion
    }
}

} // namespace fc::mission
