#include "fc/safety/RulesEvalData.h"

namespace fc::safety {

void RulesEvalData::freeze() noexcept
{
    // Phase 1: Stub — compile JSON rules → bitmask rules.
}

void RulesEvalData::evaluate(WrapperPool& pool, int32_t encoderNow, bool rising, bool falling, uint64_t nowNs) noexcept
{
    // Phase 1: Stub — evaluate compiled rules.
    (void)pool;
    (void)encoderNow;
    (void)rising;
    (void)falling;
    (void)nowNs;
}

} // namespace fc::safety
