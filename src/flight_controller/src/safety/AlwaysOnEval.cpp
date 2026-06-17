#include "fc/safety/AlwaysOnEval.h"

namespace fc::safety {

void AlwaysOnEval::addRuleIndex(int rulesDataIdx)
{
    if (count_ < MaxAlwaysOnRules) {
        indices_[count_++] = rulesDataIdx;
    }
}

void AlwaysOnEval::tick(WrapperPool& pool, int32_t encoderNow, bool rising, bool falling, uint64_t nowNs) noexcept
{
    // Phase 1: Stub — iterate indices_ and evaluate each rule.
    // Will be implemented when RulesEvalData compile/evaluate is complete.
    (void)pool;
    (void)encoderNow;
    (void)rising;
    (void)falling;
    (void)nowNs;
}

} // namespace fc::safety
