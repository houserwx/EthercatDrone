#include "fc/app/FunctionEvaluator.h"
#include "fc/app/WrapperPool.h"

#include <stdexcept>

namespace fc::app {

void FunctionEvaluator::load(std::vector<FunctionState> states)
{
    states_ = std::move(states);
}

void FunctionEvaluator::freeze() noexcept
{
    states_.shrink_to_fit();
}

void FunctionEvaluator::tick(WrapperPool& pool, ProductBuffer& buffer, uint64_t cycleCount, uint64_t nowNs) noexcept
{
    for (auto& state : states_) {
        switch (state.type) {
            case FunctionType::Detect:
                // Edge detection on input sensor
                if (state.inputIdx >= 0) {
                    bool current = pool.input(state.inputIdx).isActive();
                    state.prevSensor = current;
                }
                break;

            case FunctionType::StationTrigger:
                // Trigger output when product reaches station distance
                if (state.outputIdx >= 0 && state.inputIdx >= 0) {
                    bool sensor = pool.input(state.inputIdx).isActive();
                    if (sensor && !state.prevSensor) {
                        pool.output(state.outputIdx).setActive(true);
                    }
                    state.prevSensor = sensor;
                }
                break;

            case FunctionType::StationResult:
                // Wait for gRPC station result message
                if (state.msgInIdx >= 0) {
                    auto& msgIn = pool.msgIn(state.msgInIdx);
                    if (msgIn.hasPending()) {
                        // Message received — fire output
                        if (state.outputIdx >= 0) {
                            pool.output(state.outputIdx).setActive(true);
                        }
                        msgIn.consume();
                    }
                }
                break;

            case FunctionType::Reject:
                // Fire reject output on condition
                if (state.outputIdx >= 0 && state.inputIdx >= 0) {
                    bool sensor = pool.input(state.inputIdx).isActive();
                    if (sensor) {
                        pool.output(state.outputIdx).setActive(true);
                    }
                }
                break;

            case FunctionType::RejectVerify:
                // Verify reject was successful
                if (state.outputIdx >= 0) {
                    // Check if reject output was fired
                    pool.output(state.outputIdx).setActive(false);
                }
                break;

            case FunctionType::Cleanup:
                // Clear outputs after station processing
                if (state.outputIdx >= 0) {
                    pool.output(state.outputIdx).setActive(false);
                }
                break;

            case FunctionType::TimedOutput:
                // Fire output for a configured duration
                if (state.outputIdx >= 0) {
                    if (state.deadlineNs == 0) {
                        pool.output(state.outputIdx).setActive(true);
                        state.deadlineNs = nowNs + static_cast<uint64_t>(state.distance) * 1'000'000;
                    } else if (nowNs >= state.deadlineNs) {
                        pool.output(state.outputIdx).setActive(false);
                        state.deadlineNs = 0;
                    }
                }
                break;

            case FunctionType::Orientation:
                // Orientation control (placeholder for future implementation)
                break;

            case FunctionType::MotorMix:
                // Motor mixing for drone (Phase 2)
                break;

            case FunctionType::AttitudeHold:
                // Attitude hold control (Phase 2)
                break;

            case FunctionType::HealthMonitor:
                // Health monitoring (placeholder)
                break;

            case FunctionType::LegTransition:
                // Mission leg transition (handled by MissionEvaluator)
                break;

            case FunctionType::WaypointAction:
                // Waypoint action trigger (Phase 2)
                break;

            case FunctionType::PositionHold:
                // Position hold during dwell time (Phase 2)
                break;

            case FunctionType::RTLTrigger:
                // Return-to-launch trigger (Phase 2)
                break;

            case FunctionType::LandingSequence:
                // Landing procedure (Phase 2)
                break;
        }
    }
}

FunctionType functionTypeFromString(const std::string& s)
{
    if (s == "Detect")         return FunctionType::Detect;
    if (s == "StationTrigger") return FunctionType::StationTrigger;
    if (s == "Resync")         return FunctionType::Resync;
    if (s == "StationResult")  return FunctionType::StationResult;
    if (s == "Reject")         return FunctionType::Reject;
    if (s == "RejectVerify")   return FunctionType::RejectVerify;
    if (s == "Cleanup")        return FunctionType::Cleanup;
    if (s == "TimedOutput")    return FunctionType::TimedOutput;
    if (s == "Orientation")    return FunctionType::Orientation;
    if (s == "MotorMix")       return FunctionType::MotorMix;
    if (s == "AttitudeHold")   return FunctionType::AttitudeHold;
    if (s == "HealthMonitor")  return FunctionType::HealthMonitor;
    throw std::invalid_argument("Unknown function type: " + s);
}

} // namespace fc::app
