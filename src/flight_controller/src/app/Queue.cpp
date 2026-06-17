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
    // Phase 1: Stub — will serialize Queue config to JSON.
    (void)path;
}

void Queue::freeze()
{
    // Phase 1: Stub — shrink_to_fit on all containers.
}

void Queue::tick(uint64_t cycleCount, uint64_t nowNs) noexcept
{
    if (!isRunning()) return;
    // Phase 1: Stub — will call AlwaysOnEval, LineMonitor, FunctionEvaluator.
    (void)cycleCount;
    (void)nowNs;
}

void Queue::safeState() noexcept
{
    // Phase 1: Stub — will call WrapperPool::safeStateAllOutputs().
}

} // namespace fc::app
