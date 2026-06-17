# Phase 1.3: Mission Planning & Flight Path Abstraction

**Objective:** Reuse the existing manufacturing control architecture (Queue, FunctionEvaluator, LineMonitor, RulesEngine) as the foundation for autonomous drone mission planning. Treat the flight path as a conveyor line, waypoints as stations, and the drone as the product moving through the queue.

**Duration:** 6–8 weeks (3–4 sprints)

**Success Criteria:** A complete mission planning stack that loads waypoint sequences from JSON, executes legs with position/attitude control, triggers actions at waypoints via gRPC, and enforces safety rules through AlwaysOnEval — all without violating RT determinism.

---

## Conceptual Mapping

| Manufacturing Concept | Drone Equivalent | Mapping Quality |
|----------------------|------------------|-----------------|
| Conveyor Line | Flight Path / Mission Route | Excellent |
| Product in Queue | Drone itself (or mission state) | Very Good |
| Station along the line | Waypoint / Leg / Mission Phase | Excellent |
| Station Trigger | Reach waypoint or enter geofence | Strong |
| Station Result (gRPC) | Vision check, payload action, sensor confirmation | Perfect |
| Rules Engine (AlwaysOnEval) | Safety, contingency, leg transition rules | Excellent |
| ProductState / Position Tracking | Current leg progress + next expected state | Strong |
| LineMonitor | Flight parameter monitors (altitude, speed, battery) | Good |
| FunctionEvaluator | Leg transition logic, action triggers | Excellent |
| MachineStateController | Mission state machine (Arming, Flying, RTL, Landing) | Excellent |

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│  MissionQueue — per-mission representation                      │
│  (owns FlightLegs, MissionEvaluator, MissionMonitor)            │
└────────────────────────┬────────────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────────────┐
│  FlightLeg — one segment of the mission                         │
│  (target position, altitude, speed, completion criterion)       │
└────────────────────────┬────────────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────────────┐
│  MissionEvaluator — leg transition + action trigger logic       │
│  (reuses FunctionEvaluator pattern with new FunctionTypes)      │
└────────────────────────┬────────────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────────────┐
│  MissionMonitor — flight parameter safety monitors              │
│  (reuses LineMonitor pattern with new MonitorTypes)             │
└────────────────────────┬────────────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────────────┐
│  MissionStatePDO — projects mission state into process image    │
│  (reuses MachineStatePDO pattern)                               │
└────────────────────────┬────────────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────────────┐
│  WrapperPool — IMU, GPS, motor, sensor access                   │
└─────────────────────────────────────────────────────────────────┘
```

---

## Sprint 1: Core Mission Data Model (Weeks 1–2)

**Goal:** Define the flight leg data model and mission state machine. No RT code yet — pure data structures and JSON loading.

### 1.1 FlightLeg Data Structure

**File:** `src/flight_controller/include/fc/mission/FlightLeg.h`

```cpp
#pragma once
#include "common/math/Math.h"
#include <cstdint>
#include <string>

namespace fc::mission {

// Completion criterion for a leg — tagged enum for switch dispatch
enum class LegCriterion : uint8_t {
    PositionReached,      // Within radius of target
    AltitudeReached,      // Reach target altitude
    TimeElapsed,          // Minimum dwell time at waypoint
    ImageMatch,           // Vision confirmation via gRPC
    ManualAck,            // Ground station acknowledgment
    Always,               // Pass-through (no blocking)
};

// One segment of a mission — equivalent to a "Station" in manufacturing
struct FlightLeg {
    std::string   name;               // "Climb-Out", "Cruise-North", "Approach-Target"
    std::string   uuid;               // Stable identity for catalog mapping

    // Target state
    common::math::Vec3f targetPosition{0.0f, 0.0f, 0.0f}; // GPS or local ENU
    float             targetAltitude{0.0f};                // MSL altitude (m)
    float             targetHeading{0.0f};                 // Degrees true
    float             maxSpeed{0.0f};                      // m/s

    // Completion
    LegCriterion      criterion{LegCriterion::PositionReached};
    float             arrivalRadius{5.0f};                 // meters
    float             dwellTimeSeconds{0.0f};              // minimum hold time
    uint64_t          timeoutNs{0};                        // max time for this leg

    // Optional gRPC action at this leg
    bool              hasAction{false};
    std::string       actionName;                          // "VisionInspect", "PayloadDrop"
    int               msgOutIdx{-1};                       // WrapperPool message index
    int               msgInIdx{-1};

    // RT-mutable state (populated during execution)
    float             progress{0.0f};                      // 0.0–1.0 completion
    uint64_t          enterTimeNs{0};                      // When leg was entered
    bool              actionComplete{false};               // gRPC action confirmed
};

} // namespace fc::mission
```

**Acceptance Criteria:**
- FlightLeg compiles as a standalone header
- All fields have sensible defaults
- No heap allocation in the struct (std::string is allowed for init-time only)

### 1.2 MissionState Machine

**File:** `src/flight_controller/include/fc/mission/MissionState.h`

Extend the existing `MachineState` enum in `MachineStateController.h` with mission-specific states:

```cpp
enum class MachineState : uint8_t {
    // Existing manufacturing states
    Running  = 0,
    Faulted  = 1,
    Halted   = 2,
    EStop    = 3,
    // New mission states
    Idle         = 4,   // Powered on, no mission loaded
    Arming       = 5,   // Pre-flight checks, motor arming
    MissionReady = 6,   // Armed, awaiting launch command
    Flying       = 7,   // Active mission execution
    Holding      = 8,   // Position hold (paused mission)
    RTL          = 9,   // Return-to-launch
    Landing      = 10,  // Landing sequence
    Landed       = 11,  // On ground, motors stopped
    MissionComplete = 12, // All legs executed successfully
};
```

**Acceptance Criteria:**
- MachineStateController tick() handles new states
- State transitions are documented in a state diagram
- No new virtual calls added

### 1.3 Mission JSON Schema

**File:** `config/default/mission_example.json`

```json
{
    "missionId": "demo-patrol-001",
    "displayName": "Demo Patrol Route",
    "legs": [
        {
            "name": "Arm-and-Climb",
            "targetPosition": [0.0, 0.0, 0.0],
            "targetAltitude": 15.0,
            "maxSpeed": 2.0,
            "criterion": "AltitudeReached",
            "arrivalRadius": 1.0,
            "dwellTimeSeconds": 0.0
        },
        {
            "name": "Cruise-North",
            "targetPosition": [0.0, 100.0, 0.0],
            "targetAltitude": 30.0,
            "maxSpeed": 5.0,
            "criterion": "PositionReached",
            "arrivalRadius": 5.0,
            "dwellTimeSeconds": 3.0
        },
        {
            "name": "Inspect-Target",
            "targetPosition": [50.0, 100.0, 0.0],
            "targetAltitude": 25.0,
            "maxSpeed": 2.0,
            "criterion": "ImageMatch",
            "arrivalRadius": 3.0,
            "dwellTimeSeconds": 10.0,
            "hasAction": true,
            "actionName": "VisionInspect"
        },
        {
            "name": "Return-Home",
            "targetPosition": [0.0, 0.0, 0.0],
            "targetAltitude": 10.0,
            "maxSpeed": 5.0,
            "criterion": "PositionReached",
            "arrivalRadius": 2.0
        }
    ]
}
```

**Acceptance Criteria:**
- JSON loads without error using nlohmann/json
- All fields map to FlightLeg struct
- Invalid JSON produces clear error message

### 1.4 MissionQueue Data Structure

**File:** `src/flight_controller/include/fc/mission/MissionQueue.h`

```cpp
#pragma once
#include "fc/mission/FlightLeg.h"
#include <atomic>
#include <cstddef>
#include <string>
#include <vector>

namespace fc::mission {

class MissionQueue {
public:
    // Init-time (non-RT)
    static std::unique_ptr<MissionQueue> loadFromJson(const std::string& path);
    void freeze() noexcept;

    // RT-safe accessors (noexcept, O(1))
    [[nodiscard]] std::string_view  missionId() const noexcept;
    [[nodiscard]] std::string_view  displayName() const noexcept;
    [[nodiscard]] std::size_t       legCount() const noexcept;
    [[nodiscard]] const FlightLeg&  leg(std::size_t idx) const noexcept;
    [[nodiscard]] int               currentLegIndex() const noexcept;
    void                            advanceLeg() noexcept;
    [[nodiscard]] bool              isComplete() const noexcept;

    // RT state
    [[nodiscard]] float             missionProgress() const noexcept; // 0.0–1.0

private:
    std::string missionId_;
    std::string displayName_;
    std::vector<FlightLeg> legs_;
    std::atomic<int> currentLegIdx_{0};
    std::atomic<bool> complete_{false};
};

} // namespace fc::mission
```

**Acceptance Criteria:**
- `loadFromJson()` parses mission JSON and populates FlightLeg vector
- `freeze()` reserves capacity and marks RT-ready
- RT accessors are all `noexcept` and O(1)
- No heap allocation after freeze

---

## Sprint 2: Mission Execution Engine (Weeks 3–4)

**Goal:** Implement the RT mission execution loop — leg transition logic, position tracking, and action triggers. Reuse FunctionEvaluator and LineMonitor patterns.

### 2.1 Extend FunctionType for Mission Operations

**File:** `src/flight_controller/include/fc/app/FunctionEvaluator.h`

Add new FunctionType values:

```cpp
enum class FunctionType : uint8_t {
    // Existing manufacturing functions
    Detect,
    StationTrigger,
    Resync,
    StationResult,
    Reject,
    RejectVerify,
    Cleanup,
    TimedOutput,
    Orientation,
    MotorMix,
    AttitudeHold,
    HealthMonitor,
    // New mission functions
    LegTransition,       // Check if current leg is complete → advance
    WaypointAction,      // Trigger gRPC action at waypoint
    PositionHold,        // Maintain position during dwell time
    RTLTrigger,          // Initiate return-to-launch
    LandingSequence,     // Execute landing procedure
};
```

Update `FunctionEvaluator::tick()` to handle new function types via switch dispatch (no virtual calls).

**Acceptance Criteria:**
- New function types compile and switch correctly
- Existing manufacturing function types still work
- No virtual calls in tick()

### 2.2 MissionEvaluator — Leg Transition Logic

**File:** `src/flight_controller/include/fc/mission/MissionEvaluator.h`

```cpp
#pragma once
#include "fc/mission/FlightLeg.h"
#include "common/math/Math.h"

#include <cstdint>
#include <vector>

namespace fc::mission {

class WrapperPool;

// One leg evaluation rule — compiled from JSON at init time
struct LegEvalState {
    int         legIdx{0};
    LegCriterion criterion{LegCriterion::PositionReached};
    float       arrivalRadius{5.0f};
    float       dwellTimeSeconds{0.0f};
    uint64_t    timeoutNs{0};
    int         msgOutIdx{-1};
    int         msgInIdx{-1};

    // RT-mutable
    uint64_t    enterTimeNs{0};
    bool        actionPending{false};
    bool        actionComplete{false};
    bool        canAdvance{false};
};

class MissionEvaluator {
public:
    // Init-time
    void load(std::vector<LegEvalState> states);
    void freeze() noexcept;

    // RT tick — evaluate all leg rules
    void tick(WrapperPool& pool, const common::math::Vec3f& currentPosition,
              float currentAltitude, uint64_t nowNs) noexcept;

    [[nodiscard]] bool shouldAdvanceLeg(int legIdx) const noexcept;
    [[nodiscard]] bool missionComplete() const noexcept;

private:
    std::vector<LegEvalState> states_;

    // Helper: check criterion for a single leg
    bool checkCriterion(const LegEvalState& state,
                        const common::math::Vec3f& position,
                        float altitude,
                        uint64_t nowNs) noexcept;
};

} // namespace fc::mission
```

**Implementation Notes:**
- `checkCriterion()` uses switch on `LegCriterion` enum (no virtual dispatch)
- Position comparison uses `common::math::Vec3f::distance()` (inline, noexcept)
- Dwell time check: `nowNs - enterTimeNs > dwellTimeSeconds * 1e9`
- gRPC action check: poll `msgInIdx` for completion message

**Acceptance Criteria:**
- Evaluates all leg criteria correctly
- Advances leg when criterion met
- Handles timeout gracefully
- All methods are noexcept

### 2.3 Extend MonitorType for Flight Parameters

**File:** `src/flight_controller/include/fc/safety/LineMonitor.h`

Add new MonitorType values:

```cpp
enum class MonitorType : uint8_t {
    // Existing manufacturing monitors
    Rate,
    Spacing,
    Width,
    Gap,
    Speed,
    DwellTime,
    DetectHealth,
    EncoderHealth,
    // New flight monitors
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
```

Update `LineMonitor::tick()` to handle new monitor types. The existing `MonitorState` struct already supports param1/param2 for thresholds.

**Acceptance Criteria:**
- New monitor types trigger alarms when thresholds exceeded
- Alarms raised via MachineStateController
- No new virtual calls

### 2.4 MissionStatePDO — Project Mission State to Process Image

**File:** `src/flight_controller/include/fc/mission/MissionStatePDO.h`

Pattern: Same as `MachineStatePDO` — projects mission state bits into the process image so they're accessible as standard DigitalInput entries.

```cpp
#pragma once
#include "fc/pdo/IHardwareAdapter.h"
#include "fc/pdo/PDO.h"
#include "fc/safety/MachineStateController.h"

#include <cstdint>
#include <vector>

namespace fc::mission {

struct MissionFlagEntry {
    const bool* source;     // Points to MachineStateController flag
    uint8_t*    imageSlot;  // Destination in PDO image
};

class MissionStatePDO final : public IHardwareAdapter {
public:
    MissionStatePDO(std::vector<uint8_t>   image,
                    std::vector<MissionFlagEntry> flagEntries,
                    std::vector<PDOEntry>   entries) noexcept;

    bool initialize() override { return true; }
    void onBeforeReadInputs() noexcept override;
    // No outputs to write
    void onAfterWriteOutputs() noexcept override {}

private:
    std::vector<uint8_t>          image_;
    std::vector<MissionFlagEntry> flagEntries_;
};

} // namespace fc::mission
```

**Acceptance Criteria:**
- Mission state flags visible as DigitalInput wrappers
- No virtual calls in onBeforeReadInputs()
- Follows same pattern as MachineStatePDO

---

## Sprint 3: GPS Integration & Position Tracking (Weeks 5–6)

**Goal:** Add GPS/GNSS backend adapter and position tracking. Integrate with mission execution for leg progress and arrival detection.

### 3.1 GPSAdapter Backend

**File:** `src/flight_controller/include/fc/gps/GPSAdapter.h`

```cpp
#pragma once
#include "fc/pdo/IHardwareAdapter.h"
#include "fc/pdo/HardwareCatalog.h"

#include <memory>
#include <vector>

namespace fc::gps {

class GPSAdapter final : public fc::pdo::IHardwareAdapter {
public:
    void setCatalog(fc::pdo::HardwareCatalog* catalog) noexcept;

    // IHardwareAdapter
    bool initialize() override;
    void onBeforeReadInputs() noexcept override;
    void onAfterWriteOutputs() noexcept override {}

    // Register GPS PDO entries
    void registerPosition(const std::string& key);
    void registerAltitude(const std::string& key);
    void registerHeading(const std::string& key);
    void registerFixQuality(const std::string& key);

    [[nodiscard]] const std::vector<fc::pdo::PDO>& getPDOs() const noexcept override;

private:
    fc::pdo::HardwareCatalog* catalog_{nullptr};
    std::vector<fc::pdo::PDO> pdos_;

    // GPS data cache (updated by NMEA parser or MAVLink)
    float latitude_{0.0f};
    float longitude_{0.0f};
    float altitude_{0.0f};
    float heading_{0.0f};
    uint8_t fixQuality_{0};
};

} // namespace fc::gps
```

**Implementation Notes:**
- Phase 1: Stub that reads from a simulated position file or hardcoded values
- Phase 2: Integrate with actual GPS hardware (UART NMEA or MAVLink)
- Register entries with the central HardwareCatalog
- UUID format: `gps-<device_id>-position`, `gps-<device_id>-altitude`, etc.

**Acceptance Criteria:**
- GPSAdapter compiles and links
- PDO entries registered with catalog
- Stub returns valid (but simulated) position data
- Follows IHardwareAdapter pattern (2 virtual calls per cycle)

### 3.2 GPS Wrappers

**File:** `src/flight_controller/include/fc/wrapper/GPSWrapper.h`

```cpp
#pragma once
#include "fc/pdo/PDO.h"
#include "common/math/Math.h"

#include <cstdint>

namespace fc::wrapper {

class GPSWrapper final {
public:
    GPSWrapper(PDOEntry& latitude, PDOEntry& longitude,
               PDOEntry& altitude, PDOEntry& heading,
               PDOEntry& fixQuality) noexcept;

    [[nodiscard]] common::math::Vec3f position() const noexcept;
    [[nodiscard]] float altitude() const noexcept;
    [[nodiscard]] float heading() const noexcept; // degrees true
    [[nodiscard]] uint8_t fixQuality() const noexcept;
    [[nodiscard]] bool hasFix() const noexcept;

private:
    PDOEntry& latitude_;
    PDOEntry& longitude_;
    PDOEntry& altitude_;
    PDOEntry& heading_;
    PDOEntry& fixQuality_;
};

// Inline implementations for maximum inlining
inline GPSWrapper::GPSWrapper(PDOEntry& lat, PDOEntry& lon,
                               PDOEntry& alt, PDOEntry& hdg,
                               PDOEntry& fix) noexcept
    : latitude_(lat), longitude_(lon), altitude_(alt),
      heading_(hdg), fixQuality_(fix) {}

inline common::math::Vec3f GPSWrapper::position() const noexcept {
    return common::math::Vec3f{latitude_.getFloat(), longitude_.getFloat(), 0.0f};
}

inline float GPSWrapper::altitude() const noexcept {
    return altitude_.getFloat();
}

inline float GPSWrapper::heading() const noexcept {
    return heading_.getFloat();
}

inline uint8_t GPSWrapper::fixQuality() const noexcept {
    return static_cast<uint8_t>(fixQuality_.getRawAdc());
}

inline bool GPSWrapper::hasFix() const noexcept {
    return fixQuality() >= 2; // DGPS or better
}

} // namespace fc::wrapper
```

### 3.3 Extend WrapperPool with GPS Support

Add to `WrapperPool.h`:

```cpp
// GPS access
int addGPS(fc::wrapper::GPSWrapper w);
[[nodiscard]] fc::wrapper::GPSWrapper& gps(int idx) noexcept;
[[nodiscard]] const fc::wrapper::GPSWrapper& gps(int idx) const noexcept;

// In private section:
std::vector<fc::wrapper::GPSWrapper> gpsWrappers_;
```

**Acceptance Criteria:**
- GPSWrapper provides typed access to GPS data
- WrapperPool freeze() includes GPS wrappers
- RT accessor is O(1) direct index

### 3.4 Position Tracking in MissionEvaluator

Update `MissionEvaluator::tick()` to:
1. Get current position from GPSWrapper
2. Calculate distance to leg target
3. Update leg progress (0.0–1.0)
4. Check arrival radius for PositionReached criterion
5. Calculate mission overall progress

```cpp
// Pseudocode for position-based leg evaluation
float distance = common::math::distance(currentPos, leg.targetPosition);
leg.progress = std::max(0.0f, 1.0f - distance / initialDistance);

if (distance < leg.arrivalRadius) {
    if (leg.dwellTimeSeconds > 0) {
        uint64_t elapsed = nowNs - state.enterTimeNs;
        if (elapsed > leg.dwellTimeSeconds * 1e9) {
            state.canAdvance = true;
        }
    } else {
        state.canAdvance = true;
    }
}
```

**Acceptance Criteria:**
- Position tracking updates leg progress correctly
- Arrival detection works with configurable radius
- Dwell time enforcement before leg advance

---

## Sprint 4: Integration & Testing (Weeks 7–8)

**Goal:** Wire everything together, create example mission configs, and validate with bench tests.

### 4.1 Integrate MissionQueue with Application

Update `Application::rtCycle()` to include mission execution:

```cpp
void Application::rtCycle() noexcept
{
    uint64_t nowNs = signalProcessTickNow();

    // Existing: safety state machine
    stateMachine_.tick(estopActive, nowNs);

    // NEW: mission execution (if mission loaded)
    if (mission_) {
        // Get current position from GPS
        auto& gps = queues_[0]->pool().gps(gpsIdx_);
        auto position = gps.position();
        auto altitude = gps.altitude();

        // Evaluate mission legs
        missionEvaluator_.tick(queues_[0]->pool(), position, altitude, nowNs);

        // Check if we should advance
        int currentLeg = mission_->currentLegIndex();
        if (missionEvaluator_.shouldAdvanceLeg(currentLeg)) {
            mission_->advanceLeg();
        }
    }

    // Existing: queue ticks
    for (auto& q : queues_) {
        q->tick(cycleCount_, nowNs);
    }
}
```

**Acceptance Criteria:**
- Mission execution integrated into RT cycle
- No new virtual calls added
- Graceful handling when no mission loaded

### 4.2 Mission gRPC Service

Create a gRPC service for mission management:

```protobuf
// mission_service.proto
service MissionService {
    rpc LoadMission(MissionConfig) returns (MissionStatus);
    rpc StartMission(Empty) returns (MissionStatus);
    rpc PauseMission(Empty) returns (MissionStatus);
    rpc ResumeMission(Empty) returns (MissionStatus);
    rpc CancelMission(Empty) returns (MissionStatus);
    rpc GetMissionStatus(Empty) returns (MissionStatus);
    rpc StreamMissionProgress(Empty) returns (stream MissionProgress);
}

message MissionConfig {
    string mission_id = 1;
    repeated FlightLeg legs = 2;
}

message FlightLeg {
    string name = 1;
    float target_latitude = 2;
    float target_longitude = 3;
    float target_altitude = 4;
    float max_speed = 5;
    string criterion = 6;
    float arrival_radius = 7;
    float dwell_time_seconds = 8;
}

message MissionStatus {
    string mission_id = 1;
    string state = 2;           // "Idle", "Flying", "Holding", "Complete"
    int32 current_leg_index = 3;
    float mission_progress = 4; // 0.0–1.0
    string current_leg_name = 5;
    string error_message = 6;
}

message MissionProgress {
    int32 leg_index = 1;
    string leg_name = 2;
    float leg_progress = 3;
    float distance_to_target = 4;
    float current_altitude = 5;
    float current_speed = 6;
    int64 timestamp_ns = 7;
}
```

**Acceptance Criteria:**
- MissionService proto compiles
- gRPC server exposes mission management endpoints
- Streaming progress updates work

### 4.3 Example Mission Configuration

Create `config/default/demo_mission.json` with a complete patrol route:

```json
{
    "missionId": "demo-patrol-001",
    "displayName": "Demo Patrol Route",
    "legs": [
        {
            "name": "Arm-and-Climb",
            "targetPosition": [0.0, 0.0, 0.0],
            "targetAltitude": 15.0,
            "maxSpeed": 2.0,
            "criterion": "AltitudeReached",
            "arrivalRadius": 1.0,
            "dwellTimeSeconds": 0.0
        },
        {
            "name": "Cruise-North",
            "targetPosition": [0.0, 100.0, 0.0],
            "targetAltitude": 30.0,
            "maxSpeed": 5.0,
            "criterion": "PositionReached",
            "arrivalRadius": 5.0,
            "dwellTimeSeconds": 3.0
        },
        {
            "name": "Inspect-Target",
            "targetPosition": [50.0, 100.0, 0.0],
            "targetAltitude": 25.0,
            "maxSpeed": 2.0,
            "criterion": "ImageMatch",
            "arrivalRadius": 3.0,
            "dwellTimeSeconds": 10.0,
            "hasAction": true,
            "actionName": "VisionInspect",
            "msgOutIdx": 0,
            "msgInIdx": 1
        },
        {
            "name": "Return-Home",
            "targetPosition": [0.0, 0.0, 0.0],
            "targetAltitude": 10.0,
            "maxSpeed": 5.0,
            "criterion": "PositionReached",
            "arrivalRadius": 2.0
        }
    ]
}
```

### 4.4 Bench Test: Mission Execution Simulation

**File:** `src/main/src/mission_bench_test.cpp`

```cpp
// Simulate a complete mission execution with mock GPS data
// Validates:
// - Mission loading from JSON
// - Leg transition logic
// - Arrival detection
// - Dwell time enforcement
// - gRPC action triggering
// - Mission completion

int main() {
    // Load mission
    auto mission = fc::mission::MissionQueue::loadFromJson("config/default/demo_mission.json");
    mission->freeze();

    // Create mock GPS that follows a path
    MockGPSAdapter gps;
    gps.setPath(mission->legs()); // Generate path through waypoints

    // Create evaluator
    fc::mission::MissionEvaluator evaluator;
    evaluator.load(buildEvalStates(mission));
    evaluator.freeze();

    // Run simulation
    for (uint64_t cycle = 0; cycle < 100000; ++cycle) {
        uint64_t nowNs = cycle * 1000000; // 1ms cycles
        auto pos = gps.simulatePosition(nowNs);
        auto alt = gps.simulateAltitude(nowNs);

        evaluator.tick(pool, pos, alt, nowNs);

        if (evaluator.missionComplete()) {
            printf("Mission complete after %lu cycles\n", cycle);
            break;
        }
    }

    return 0;
}
```

**Acceptance Criteria:**
- Bench test compiles and runs
- Mission completes successfully with simulated data
- All leg transitions fire correctly
- Dwell times are enforced
- Test output shows mission progress

### 4.5 Update copilot-instructions.md

Add mission planning patterns to the architecture documentation:
- MissionQueue lifecycle
- FlightLeg data model
- MissionEvaluator RT tick pattern
- GPS integration pattern
- State machine transitions for mission states

---

## Design Decisions & Rationale

### Why Reuse Manufacturing Patterns?

1. **Proven Architecture:** The existing Queue/FunctionEvaluator/LineMonitor pattern is already deterministic, freeze-safe, and RT-validated
2. **Code Reuse:** ~80% of the mission execution logic can reuse existing infrastructure
3. **Consistent API:** Wrappers, PDOEntry, and HardwareCatalog patterns are already established
4. **Showcase Value:** Demonstrates that industrial control patterns apply to autonomous systems

### What Changes from Manufacturing?

| Aspect | Manufacturing | Drone Mission |
|--------|--------------|---------------|
| Position tracking | 1D conveyor (encoder count) | 3D space (GPS + IMU) |
| Speed | Constant conveyor speed | Variable, wind-affected |
| Transitions | Fixed station sequence | Dynamic rerouting possible |
| Safety | Product quality alarms | Flight safety (altitude, battery, GPS) |
| Actions | Station result (gRPC) | Waypoint action (vision, payload) |

### Mitigation Strategies

1. **3D vs 1D:** Use `common::math::Vec3f` for position, calculate 3D distance for arrival
2. **Variable Speed:** Each leg has `maxSpeed`, control loop handles acceleration
3. **Dynamic Rerouting:** MissionQueue supports leg insertion/reordering (Phase 2 feature)
4. **Flight Safety:** New MonitorTypes for altitude, battery, GPS quality, attitude limits

---

## Dependencies

- **Phase 1.1 (IMU Support):** Required for attitude and orientation data
- **Phase 1.2 (HAL):** Required for GPSAdapter backend pattern
- **Phase 2 (Flight):** Mission planning feeds into actual flight control

## Deliverables

1. `FlightLeg.h` — Core data model
2. `MissionQueue.h/.cpp` — Mission loading and RT access
3. `MissionEvaluator.h/.cpp` — Leg transition logic
4. `GPSAdapter.h/.cpp` — GPS backend stub
5. `GPSWrapper.h` — Typed GPS access
6. `MissionStatePDO.h/.cpp` — Mission state projection
7. `mission_service.proto` — gRPC mission management
8. `demo_mission.json` — Example mission config
9. `mission_bench_test.cpp` — Simulation test
10. Updated `copilot-instructions.md` — Mission patterns documented

---

## Risk Assessment

| Risk | Impact | Mitigation |
|------|--------|------------|
| GPS accuracy insufficient for arrival detection | High | Configurable arrival radius, fallback to time-based criterion |
| Wind affects position hold during dwell | Medium | IMU-based drift compensation (Phase 2) |
| gRPC action timeout blocks mission | Medium | Configurable timeout per leg, fallback to "proceed anyway" |
| RT cycle too slow with mission logic | Low | Mission evaluation is O(leg_count), bounded by fixed array |
| State machine complexity | Low | Clear state diagram, exhaustive switch on MachineState |
