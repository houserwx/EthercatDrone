// ============================================================================
// mission_bench_test.cpp — Mission execution simulation with mock GPS data.
//
// Validates:
//   - Mission loading from JSON
//   - Leg transition logic
//   - Arrival detection
//   - Dwell time enforcement
//   - Mission completion
//
// No hardware required — pure simulation.
// ============================================================================

#include "fc/mission/MissionQueue.h"
#include "fc/mission/MissionEvaluator.h"
#include "common/math/Math.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Helper: build LegEvalState vector from MissionQueue
// ---------------------------------------------------------------------------
static std::vector<fc::mission::LegEvalState>
buildEvalStates(const fc::mission::MissionQueue& mission)
{
    std::vector<fc::mission::LegEvalState> states;
    states.reserve(mission.legCount());

    for (std::size_t i = 0; i < mission.legCount(); ++i) {
        const auto& leg = mission.leg(i);

        fc::mission::LegEvalState state;
        state.legIdx = static_cast<int>(i);
        state.criterion = leg.criterion;
        state.targetPosition = leg.targetPosition;
        state.targetAltitude = leg.targetAltitude;
        state.arrivalRadius = leg.arrivalRadius;
        state.altitudeTolerance = leg.altitudeTolerance;
        state.dwellTimeSeconds = leg.dwellTimeSeconds;
        state.timeoutNs = leg.timeoutNs;
        state.msgOutIdx = leg.msgOutIdx;
        state.msgInIdx = leg.msgInIdx;

        states.push_back(std::move(state));
    }

    return states;
}

// ---------------------------------------------------------------------------
// Helper: interpolate position along a path through waypoints
// Returns position at given time (nanoseconds), assuming constant speed
// ---------------------------------------------------------------------------
static common::math::Vec3f
interpolatePosition(const std::vector<common::math::Vec3f>& waypoints,
                    float speed, uint64_t nowNs)
{
    if (waypoints.empty()) return common::math::Vec3f{0.0f, 0.0f, 0.0f};
    if (waypoints.size() == 1) return waypoints[0];

    // Calculate total path distance
    float totalDist = 0.0f;
    std::vector<float> segmentDist;
    segmentDist.reserve(waypoints.size() - 1);

    for (std::size_t i = 1; i < waypoints.size(); ++i) {
        float dx = waypoints[i].x - waypoints[i-1].x;
        float dy = waypoints[i].y - waypoints[i-1].y;
        float dz = waypoints[i].z - waypoints[i-1].z;
        float seg = std::sqrt(dx*dx + dy*dy + dz*dz);
        segmentDist.push_back(seg);
        totalDist += seg;
    }

    // How far along the path are we?
    float elapsedSeconds = static_cast<float>(nowNs) / 1e9f;
    float traveledDist = speed * elapsedSeconds;

    if (traveledDist >= totalDist) {
        return waypoints.back(); // Reached end
    }

    // Find which segment we're on
    float remaining = traveledDist;
    for (std::size_t i = 0; i < segmentDist.size(); ++i) {
        if (remaining <= segmentDist[i]) {
            // Interpolate within this segment
            float t = (segmentDist[i] > 0.0f) ? remaining / segmentDist[i] : 0.0f;
            return common::math::Vec3f{
                waypoints[i].x + t * (waypoints[i+1].x - waypoints[i].x),
                waypoints[i].y + t * (waypoints[i+1].y - waypoints[i].y),
                waypoints[i].z + t * (waypoints[i+1].z - waypoints[i].z)
            };
        }
        remaining -= segmentDist[i];
    }

    return waypoints.back();
}

// ---------------------------------------------------------------------------
// Main — run the simulation
// ---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    std::string missionPath = (argc > 1) ? argv[1] : "config/default/demo_mission.json";

    printf("=== Mission Bench Test ===\n");
    printf("Loading mission: %s\n\n", missionPath.c_str());

    // --- Load mission ------------------------------------------------------
    auto mission = fc::mission::MissionQueue::loadFromJson(missionPath);
    mission->freeze();

    printf("Mission: %s (%s)\n", mission->missionId().data(), mission->displayName().data());
    printf("Legs: %zu\n\n", mission->legCount());

    for (std::size_t i = 0; i < mission->legCount(); ++i) {
        const auto& leg = mission->leg(i);
        printf("  [%zu] %s\n", i, leg.name.c_str());
        printf("      Target: (%.1f, %.1f, %.1f) alt=%.1f\n",
               leg.targetPosition.x, leg.targetPosition.y, leg.targetPosition.z,
               leg.targetAltitude);
        printf("      Criterion: %d, Radius: %.1fm, Dwell: %.1fs\n",
               static_cast<int>(leg.criterion), leg.arrivalRadius, leg.dwellTimeSeconds);
    }
    printf("\n");

    // --- Build evaluator ---------------------------------------------------
    auto evalStates = buildEvalStates(*mission);
    fc::mission::MissionEvaluator evaluator;
    evaluator.load(std::move(evalStates));
    evaluator.freeze();

    // --- Extract waypoints for simulation ----------------------------------
    std::vector<common::math::Vec3f> waypoints;
    waypoints.reserve(mission->legCount());
    for (std::size_t i = 0; i < mission->legCount(); ++i) {
        waypoints.push_back(mission->leg(i).targetPosition);
    }

    // --- Run simulation ----------------------------------------------------
    // Simulate at 1ms cycle time, drone travels at 5 m/s
    constexpr float simSpeed = 5.0f;
    constexpr uint64_t cycleNs = 1'000'000; // 1ms
    constexpr uint64_t maxCycles = 100'000; // 100 seconds max

    printf("Running simulation (speed=%.1f m/s, cycle=1ms)...\n\n", simSpeed);

    int lastLegIdx = -1;
    float lastAlt = 0.0f;

    for (uint64_t cycle = 0; cycle < maxCycles; ++cycle) {
        uint64_t nowNs = cycle * cycleNs;

        // Simulate GPS position along the path
        auto pos = interpolatePosition(waypoints, simSpeed, nowNs);
        float alt = static_cast<float>(cycle % 100) * 0.3f; // Simulated altitude ramp

        // Evaluate mission
        evaluator.tick(pos, alt, nowNs);

        // Check current leg
        int currentLeg = mission->currentLegIndex();

        // Print leg transitions
        if (currentLeg != lastLegIdx) {
            lastLegIdx = currentLeg;
            if (currentLeg < static_cast<int>(mission->legCount())) {
                printf("[cycle %6lu] → Leg %d: %s\n",
                       static_cast<unsigned long>(cycle),
                       currentLeg,
                       mission->leg(currentLeg).name.c_str());
            }
        }

        // Check completion
        if (evaluator.missionComplete()) {
            printf("\n[cycle %6lu] Mission COMPLETE!\n",
                   static_cast<unsigned long>(cycle));
            printf("Total time: %.1fs\n",
                   static_cast<float>(nowNs) / 1e9f);
            return 0;
        }
    }

    printf("\n[cycle %6lu] Simulation TIMEOUT (mission not complete)\n",
           static_cast<unsigned long>(maxCycles));
    return 1;
}
