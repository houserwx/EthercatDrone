#pragma once

#include "common/rt/VectorBuffer.h"

#include <array>
#include <atomic>
#include <cstdint>

namespace fc::safety {

// ---------------------------------------------------------------------------
// MachineState — closed set of machine run states.
// ---------------------------------------------------------------------------
enum class MachineState : uint8_t {
    // Manufacturing states
    Running  = 0,
    Faulted  = 1,
    Halted   = 2,
    EStop    = 3,
    // Mission states
    Idle            = 4,   // Powered on, no mission loaded
    Arming          = 5,   // Pre-flight checks, motor arming
    MissionReady    = 6,   // Armed, awaiting launch command
    Flying          = 7,   // Active mission execution
    Holding         = 8,   // Position hold (paused mission)
    RTL             = 9,   // Return-to-launch
    Landing         = 10,  // Landing sequence
    Landed          = 11,  // On ground, motors stopped
    MissionComplete = 12,  // All legs executed successfully
};

// ---------------------------------------------------------------------------
// AlarmSeverity
// ---------------------------------------------------------------------------
enum class AlarmSeverity : uint8_t {
    Fault      = 0,
    HaltFault  = 1,
};

// ---------------------------------------------------------------------------
// AlarmId — closed set of alarm identifiers.
// ---------------------------------------------------------------------------
enum class AlarmId : uint8_t {
    kNoAlarm = 0,
    kEncoderHealthFault,
    kRateOutOfRange,
    kSpacingTooShort,
    kWidthOutOfRange,
    kGapOutOfRange,
    kDetectHealthMissed,
    kVerifyMissed,
    kRejectVerifyFailed,
    // Drone-specific alarms
    kMotorOverRpm,
    kMotorUnderRpm,
    kImuSensorFault,
    kHeartbeatLost,
    kBatteryLow,
    kGpsFixLost,
    kAttitudeExceedLimit,
    kAltitudeExceeded,
    // Mission-specific alarms
    kLegTimeout,
    kArrivalMissed,
    kActionFailed,
    kWindExceeded,
    kGeofenceViolation,
};

constexpr std::size_t MaxAlarmIds = 128;

// ---------------------------------------------------------------------------
// MachineStatusEvent — RT-pushable event.
// ---------------------------------------------------------------------------
struct MachineStatusEvent {
    MachineState state;
    AlarmId      alarmId;
    uint64_t     timestamp;
};

// ---------------------------------------------------------------------------
// MachineStateController — PDO-style RT-owned state image.
// ---------------------------------------------------------------------------
class MachineStateController {
public:
    MachineStateController();

    MachineStateController(const MachineStateController&)            = delete;
    MachineStateController& operator=(const MachineStateController&) = delete;
    MachineStateController(MachineStateController&&)                 = delete;
    MachineStateController& operator=(MachineStateController&&)      = delete;

    void tick(bool estop, uint64_t nowNs) noexcept;
    void raiseAlarm(AlarmId id, AlarmSeverity sev, uint64_t nowNs) noexcept;
    void clearFault(AlarmId id) noexcept;

    [[nodiscard]] MachineState state() const noexcept { return state_; }
    [[nodiscard]] bool faultActive() const noexcept { return faultActive_; }
    [[nodiscard]] bool haltActive()  const noexcept { return haltActive_; }

    std::atomic<bool> clearAlarmsRequested_{false};

private:
    MachineState state_{MachineState::Running};
    bool         faultActive_{false};
    bool         haltActive_{false};
    uint64_t     activeFaultMask_{0};
    std::array<bool, MaxAlarmIds> latchTable_{};
    int          activeLatchCount_{0};
};

} // namespace fc::safety
