#pragma once

#include "common/rt/SignalProcess.h"

#include <cstdint>
#include <string>
#include <vector>

namespace fc::app {

class WrapperPool;
class ProductBuffer;

// ---------------------------------------------------------------------------
// FunctionType — tagged discriminant for switch dispatch.
// ---------------------------------------------------------------------------
enum class FunctionType : uint8_t {
    // Manufacturing functions
    Detect,
    StationTrigger,
    Resync,
    StationResult,
    Reject,
    RejectVerify,
    Cleanup,
    TimedOutput,
    Orientation,
    // Drone-specific
    MotorMix,
    AttitudeHold,
    HealthMonitor,
    // Mission functions
    LegTransition,       // Check if current leg is complete → advance
    WaypointAction,      // Trigger gRPC action at waypoint
    PositionHold,        // Maintain position during dwell time
    RTLTrigger,          // Initiate return-to-launch
    LandingSequence,     // Execute landing procedure
};

FunctionType functionTypeFromString(const std::string& s);

// ---------------------------------------------------------------------------
// FunctionState — one configured evaluator.
// ---------------------------------------------------------------------------
struct FunctionState {
    FunctionType type         {FunctionType::Detect};
    int64_t      id           {0};
    std::string  name;
    int          outputIdx    {-1};
    int          inputIdx     {-1};
    int          msgOutIdx    {-1};
    int          msgInIdx     {-1};
    int64_t      distance     {0};
    bool         alwaysVerify {false};
    int          missFlagIdx  {-1};
    uint64_t     verifyTimeoutNs{0};

    // RT-mutable state
    int          cursorPos    {0};
    bool         prevSensor   {false};
    int64_t      detectCount  {0};
    uint64_t     seqOut       {0};
    uint64_t     seqIn        {0};
    uint64_t     deadlineNs   {0};
};

// ---------------------------------------------------------------------------
// FunctionEvaluator — owns and iterates the FunctionState array.
// ---------------------------------------------------------------------------
class FunctionEvaluator {
public:
    void load(std::vector<FunctionState> states);
    void freeze() noexcept;
    void tick(WrapperPool& pool, ProductBuffer& buffer, uint64_t cycleCount, uint64_t nowNs) noexcept;

private:
    std::vector<FunctionState> states_;
};

} // namespace fc::app
