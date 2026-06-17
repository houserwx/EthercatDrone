#include "fc/app/FunctionEvaluator.h"

#include <stdexcept>

namespace fc::app {

void FunctionEvaluator::load(std::vector<FunctionState> states)
{
    states_ = std::move(states);
}

void FunctionEvaluator::freeze() noexcept
{
    states_.shrink_to_fit();
}

void FunctionEvaluator::tick(WrapperPool& pool, ProductBuffer& buffer, uint64_t cycleCount, uint64_t nowNs) noexcept
{
    // Phase 1: Stub — switch-dispatch over FunctionType.
    (void)pool;
    (void)buffer;
    (void)cycleCount;
    (void)nowNs;
}

FunctionType functionTypeFromString(const std::string& s)
{
    if (s == "Detect")         return FunctionType::Detect;
    if (s == "StationTrigger") return FunctionType::StationTrigger;
    if (s == "Resync")         return FunctionType::Resync;
    if (s == "StationResult")  return FunctionType::StationResult;
    if (s == "Reject")         return FunctionType::Reject;
    if (s == "RejectVerify")   return FunctionType::RejectVerify;
    if (s == "Cleanup")        return FunctionType::Cleanup;
    if (s == "TimedOutput")    return FunctionType::TimedOutput;
    if (s == "Orientation")    return FunctionType::Orientation;
    if (s == "MotorMix")       return FunctionType::MotorMix;
    if (s == "AttitudeHold")   return FunctionType::AttitudeHold;
    if (s == "HealthMonitor")  return FunctionType::HealthMonitor;
    throw std::invalid_argument("Unknown function type: " + s);
}

} // namespace fc::app
