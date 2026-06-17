#include "fc/safety/LineMonitor.h"

namespace fc::safety {

void LineMonitor::load(const std::vector<MonitorType>& types)
{
    states_.reserve(types.size());
    for (auto t : types) {
        MonitorState ms;
        ms.type = t;
        states_.push_back(std::move(ms));
    }
}

void LineMonitor::freeze() noexcept
{
    states_.shrink_to_fit();
}

void LineMonitor::tick(int32_t encoderNow, bool rising, bool falling, uint64_t nowNs, WrapperPool& pool) noexcept
{
    // Phase 1: Stub — switch-dispatch over MonitorType.
    (void)encoderNow;
    (void)rising;
    (void)falling;
    (void)nowNs;
    (void)pool;
}

} // namespace fc::safety
