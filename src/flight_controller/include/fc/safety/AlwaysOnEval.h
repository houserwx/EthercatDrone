#pragma once

#include <array>
#include <cstdint>

namespace fc::app {
class WrapperPool;
}

namespace fc::safety {

using WrapperPool = fc::app::WrapperPool;

inline constexpr std::size_t MaxAlwaysOnRules = 16;

// ---------------------------------------------------------------------------
// AlwaysOnEval — Phase 1 unconditional rule evaluator.
// Fixed-size array of int indices, unconditional tick every cycle.
// ---------------------------------------------------------------------------
class AlwaysOnEval {
public:
    AlwaysOnEval() = default;

    void addRuleIndex(int rulesDataIdx);
    void freeze() noexcept {}
    void tick(WrapperPool& pool, int32_t encoderNow, bool rising, bool falling, uint64_t nowNs) noexcept;

    [[nodiscard]] uint8_t ruleCount() const noexcept { return count_; }

private:
    std::array<int, MaxAlwaysOnRules> indices_{};
    uint8_t count_{0};
};

}  // namespace fc::safety
