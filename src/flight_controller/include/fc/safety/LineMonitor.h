#pragma once

#include "common/rt/SignalProcess.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace fc::safety {

class WrapperPool;

// ---------------------------------------------------------------------------
// MonitorType — tagged discriminant for switch dispatch.
// ---------------------------------------------------------------------------
enum class MonitorType : uint8_t {
    // Manufacturing monitors
    Rate,
    Spacing,
    Width,
    Gap,
    Speed,
    DwellTime,
    DetectHealth,
    EncoderHealth,
    // Flight monitors
    AltitudeMin,          // Below minimum safe altitude
    AltitudeMax,          // Above maximum altitude
    AirspeedMin,          // Below minimum airspeed
    AirspeedMax,          // Above maximum airspeed
    BatteryVoltage,       // Below cutoff voltage
    GpsFixQuality,        // Lost GPS fix or dilution too high
    AttitudeRollLimit,    // Exceeds maximum roll angle
    AttitudePitchLimit,   // Exceeds maximum pitch angle
    MotorRpmDeviation,    // Motor RPM deviates from command
    HeartbeatTimeout,     // Lost heartbeat from ground station
};

// ---------------------------------------------------------------------------
// LineMonitor — signal-level queue monitors (edge/encoder triggered).
// ---------------------------------------------------------------------------
class LineMonitor {
public:
    void load(const std::vector<MonitorType>& types);
    void freeze() noexcept;
    void tick(int32_t encoderNow, bool rising, bool falling, uint64_t nowNs, WrapperPool& pool) noexcept;

private:
    struct MonitorState {
        MonitorType type{MonitorType::Rate};
        int64_t     id{0};
        std::string name;
        int         alarmFlagIdx{-1};
        int64_t     param1{0};
        int64_t     param2{0};
    };
    std::vector<MonitorState> states_;
};

} // namespace fc::safety
