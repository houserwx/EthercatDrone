#pragma once

#include "common/math/Math.h"

#include <cstdint>
#include <string>

namespace fc::mission {

// ============================================================================
// LegCriterion — tagged enum for switch dispatch (no vtable).
// Determines when a leg is considered complete.
// ============================================================================
enum class LegCriterion : uint8_t {
    PositionReached,  // Within arrivalRadius of target
    AltitudeReached,  // Within altitude tolerance
    TimeElapsed,      // Minimum dwell time at waypoint
    ImageMatch,       // Vision confirmation via gRPC
    ManualAck,        // Ground station acknowledgment
    Always,           // Pass-through (never blocks)
};

// ============================================================================
// FlightLeg — one segment of a mission (equivalent to a "Station").
// Immutable after construction; RT-mutable fields are separate.
// ============================================================================
struct FlightLeg {
    std::string   name;                       // "Climb-Out", "Cruise-North"
    std::string   uuid;                       // Stable identity

    // Target state
    common::math::Vec3f targetPosition{0.0f, 0.0f, 0.0f};
    float             targetAltitude{0.0f};    // MSL altitude (m)
    float             targetHeading{0.0f};     // Degrees true (-180 to 180)
    float             maxSpeed{0.0f};          // m/s

    // Completion criterion
    LegCriterion      criterion{LegCriterion::PositionReached};
    float             arrivalRadius{5.0f};     // meters
    float             altitudeTolerance{2.0f}; // meters
    float             dwellTimeSeconds{0.0f};  // minimum hold time at waypoint
    uint64_t          timeoutNs{0};            // max time for this leg (0 = no timeout)

    // Optional gRPC action at this leg
    bool              hasAction{false};
    std::string       actionName;
    int               msgOutIdx{-1};
    int               msgInIdx{-1};
};

} // namespace fc::mission
