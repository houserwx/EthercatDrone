#include "fc/app/Queue.h"

#include <stdexcept>

namespace fc::app {

std::unique_ptr<Queue> createQueue()
{
    // Use raw new instead of std::make_unique to access private constructor
    // from friend function (some compilers reject make_unique in this context).
    return std::unique_ptr<Queue>(new Queue());
}

std::unique_ptr<Queue> Queue::loadFromJson(const std::string& path,
                                           fc::pdo::HardwareRegistry& registry)
{
    (void)path;
    (void)registry;
    return createQueue();
}

void Queue::saveToJson(const std::string& path) const
{
    // TODO: serialize Queue config to JSON (id, displayName, pool state)
    (void)path;
}

void Queue::freeze()
{
    pool_.freeze();
    alwaysOnEval_.freeze();
    lineMonitor_.freeze();
    functionEvaluator_.freeze();
}

void Queue::tick(uint64_t cycleCount, uint64_t nowNs) noexcept
{
    if (!isRunning()) return;

    // Evaluate always-on safety rules
    alwaysOnEval_.tick(pool_, 0, false, false, nowNs);

    // Evaluate line monitors (encoder-based for manufacturing, position-based for drone)
    lineMonitor_.tick(0, false, false, nowNs, pool_);

    // Evaluate function states (station triggers, actions, leg transitions)
    // TODO: Wire up ProductBuffer when manufacturing logic is implemented
    // functionEvaluator_.tick(pool_, buffer, cycleCount, nowNs);
}

void Queue::safeState() noexcept
{
    // Deactivate all outputs for safety
    pool_.safeStateAllOutputs();
}

} // namespace fc::app
