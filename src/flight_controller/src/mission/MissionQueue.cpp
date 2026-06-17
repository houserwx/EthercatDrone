#include "fc/mission/MissionQueue.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

using json = nlohmann::json;

namespace fc::mission {

// ============================================================================
// Helper: parse LegCriterion from string
// ============================================================================
static LegCriterion criterionFromString(const std::string& s)
{
    if (s == "PositionReached") return LegCriterion::PositionReached;
    if (s == "AltitudeReached") return LegCriterion::AltitudeReached;
    if (s == "TimeElapsed")     return LegCriterion::TimeElapsed;
    if (s == "ImageMatch")      return LegCriterion::ImageMatch;
    if (s == "ManualAck")       return LegCriterion::ManualAck;
    if (s == "Always")          return LegCriterion::Always;
    return LegCriterion::PositionReached; // default
}

// ============================================================================
// loadFromJson
// ============================================================================
std::unique_ptr<MissionQueue> MissionQueue::loadFromJson(const std::string& path)
{
    auto mission = std::unique_ptr<MissionQueue>(new MissionQueue());

    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        throw std::runtime_error("Cannot open mission file: " + path);
    }

    json j = json::parse(ifs);

    mission->missionId_ = j.value("missionId", "");
    mission->displayName_ = j.value("displayName", mission->missionId_);

    if (!j.contains("legs") || j["legs"].empty()) {
        throw std::runtime_error("Mission file has no legs: " + path);
    }

    for (const auto& leg_j : j["legs"]) {
        FlightLeg leg;
        leg.name = leg_j.value("name", "");
        leg.uuid = leg_j.value("uuid", leg.name);

        // Target position (array of 3 floats)
        if (leg_j.contains("targetPosition") && leg_j["targetPosition"].size() >= 3) {
            leg.targetPosition = common::math::Vec3f{
                leg_j["targetPosition"][0].get<float>(),
                leg_j["targetPosition"][1].get<float>(),
                leg_j["targetPosition"][2].get<float>()
            };
        }

        leg.targetAltitude = leg_j.value("targetAltitude", 0.0f);
        leg.targetHeading = leg_j.value("targetHeading", 0.0f);
        leg.maxSpeed = leg_j.value("maxSpeed", 0.0f);

        // Criterion
        if (leg_j.contains("criterion")) {
            leg.criterion = criterionFromString(leg_j["criterion"].get<std::string>());
        }

        leg.arrivalRadius = leg_j.value("arrivalRadius", 5.0f);
        leg.altitudeTolerance = leg_j.value("altitudeTolerance", 2.0f);
        leg.dwellTimeSeconds = leg_j.value("dwellTimeSeconds", 0.0f);

        // Timeout (convert seconds to nanoseconds if provided)
        if (leg_j.contains("timeoutSeconds")) {
            leg.timeoutNs = static_cast<uint64_t>(leg_j["timeoutSeconds"].get<float>()) * 1'000'000'000ULL;
        } else {
            leg.timeoutNs = leg_j.value("timeoutNs", 0ULL);
        }

        // Optional gRPC action
        leg.hasAction = leg_j.value("hasAction", false);
        leg.actionName = leg_j.value("actionName", "");
        leg.msgOutIdx = leg_j.value("msgOutIdx", -1);
        leg.msgInIdx = leg_j.value("msgInIdx", -1);

        mission->legs_.push_back(std::move(leg));
    }

    return mission;
}

// ============================================================================
// freeze — reserve capacity, mark RT-ready
// ============================================================================
void MissionQueue::freeze() noexcept
{
    legs_.shrink_to_fit();

    if (currentLegIdx_.load(std::memory_order_relaxed) < 0) {
        currentLegIdx_.store(0, std::memory_order_release);
    }
}

// ============================================================================
// leg — RT-safe accessor with bounds check
// ============================================================================
const FlightLeg& MissionQueue::leg(std::size_t idx) const noexcept
{
    if (idx >= legs_.size()) {
        // Return first leg as fallback (should not happen in correct usage)
        return legs_[0];
    }
    return legs_[idx];
}

// ============================================================================
// advanceLeg — move to next leg or mark complete
// ============================================================================
void MissionQueue::advanceLeg() noexcept
{
    int current = currentLegIdx_.load(std::memory_order_acquire);
    int next = current + 1;

    if (static_cast<std::size_t>(next) >= legs_.size()) {
        complete_.store(true, std::memory_order_release);
    } else {
        currentLegIdx_.store(next, std::memory_order_release);
    }
}

// ============================================================================
// missionProgress — 0.0 (start) → 1.0 (complete)
// ============================================================================
float MissionQueue::missionProgress() const noexcept
{
    std::size_t count = legs_.size();
    if (count == 0) return 0.0f;

    if (complete_.load(std::memory_order_acquire)) {
        return 1.0f;
    }

    int current = currentLegIdx_.load(std::memory_order_acquire);
    // Progress = (current leg index + 1) / total legs
    return static_cast<float>(current + 1) / static_cast<float>(count);
}

} // namespace fc::mission
