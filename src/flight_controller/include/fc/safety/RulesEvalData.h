#pragma once

#include "fc/safety/MachineStateController.h"

#include <array>
#include <cstdint>
#include <string>

namespace fc::safety {

class WrapperPool;

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
inline constexpr std::size_t MaxRules           = 8;
inline constexpr std::size_t MaxConditions      = 4;
inline constexpr std::size_t MaxActions         = 4;
inline constexpr std::size_t MaxCounters        = 4;

// ---------------------------------------------------------------------------
// ConditionType
// ---------------------------------------------------------------------------
enum class ConditionType : uint8_t {
    Always,
    InputHigh,
    InputLow,
    InputMatch,
    InputMismatch,
    CounterBelow,
    CounterAtMax,
    CounterEquals,
};

// ---------------------------------------------------------------------------
// ActionType
// ---------------------------------------------------------------------------
enum class ActionType : uint8_t {
    FireOutput,
    SetFlag,
    ClearFlag,
    IncrementCounter,
    RaiseAlarm,
};

// ---------------------------------------------------------------------------
// RulesEvalData — Configurable condition/action rules engine.
// Two-layer: JSON intent → compiled bitmask execution.
// ---------------------------------------------------------------------------
class RulesEvalData {
public:
    void freeze() noexcept;
    void evaluate(WrapperPool& pool, int32_t encoderNow, bool rising, bool falling, uint64_t nowNs) noexcept;

private:
    // Compiled rules (RT-facing, bitmask-folded)
    // Source rules (diagnostic, JSON-facing)
};

} // namespace fc::safety
