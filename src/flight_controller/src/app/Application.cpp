#include "fc/app/Application.h"
#include "fc/app/Queue.h"
#include "common/rt/SignalProcess.h"
#include "common/log/LogHelper.h"
#include "common/messages/MessageTypes.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <time.h>

namespace {

[[nodiscard]] struct timespec addNsToTs(struct timespec ts, int64_t ns) noexcept
{
    ts.tv_nsec += static_cast<long>(ns);
    while (ts.tv_nsec >= 1'000'000'000L) {
        ts.tv_sec++;
        ts.tv_nsec -= 1'000'000'000L;
    }
    while (ts.tv_nsec < 0L) {
        ts.tv_sec--;
        ts.tv_nsec += 1'000'000'000L;
    }
    return ts;
}

[[nodiscard]] int64_t diffNs(const struct timespec& a, const struct timespec& b) noexcept
{
    return ((a.tv_sec - b.tv_sec) * 1'000'000'000LL)
         + static_cast<int64_t>(a.tv_nsec - b.tv_nsec);
}

} // anonymous namespace

namespace fc::app {

Application::Application(fc::pdo::HardwareRegistry& registry, uint32_t cycleNs)
    : common::rt::Threadrunner(common::rt::ThreadConfiguration{
          .name               = "Application",
          .cpuCore            = -1,
          .priority           = 85,
          .useRealtime        = true,
          .stackPrefaultBytes = 0UL
      })
    , registry_(registry)
    , cycleNs_(cycleNs)
{
}

uint64_t Application::cycleCount()   const noexcept { return cycleCount_; }
int      Application::overrunCount() const noexcept { return overrunCount_; }
int64_t  Application::maxOverrunNs() const noexcept { return maxOverrunNs_; }

void Application::requestStop() noexcept
{
    running_.store(false, std::memory_order_release);
}

void Application::addQueue(std::unique_ptr<Queue> q)
{
    queues_.push_back(std::move(q));
}

void Application::setMission(std::unique_ptr<fc::mission::MissionQueue> mission)
{
    if (!mission) return;

    // Build evaluator states from mission legs
    std::vector<fc::mission::LegEvalState> evalStates;
    evalStates.reserve(mission->legCount());

    for (std::size_t i = 0; i < mission->legCount(); ++i) {
        const auto& leg = mission->leg(i);
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
        evalStates.push_back(std::move(state));
    }

    missionEvaluator_.load(std::move(evalStates));
    missionEvaluator_.freeze();
    mission_ = std::move(mission);
}

void Application::startMission() noexcept
{
    missionRunning_.store(true, std::memory_order_release);
}

void Application::pauseMission() noexcept
{
    missionRunning_.store(false, std::memory_order_release);
}

void Application::cancelMission() noexcept
{
    missionRunning_.store(false, std::memory_order_release);
    mission_.reset();
}

void Application::run()
{
    running_.store(true, std::memory_order_release);

    common::log::logInfo(messages::MessageId::SYSTEM_INIT_COMPLETE);
    common::log::logInfo(messages::MessageId::RT_CYCLE_STATS,
                         0LL, 0LL, static_cast<int64_t>(cycleNs_));

    struct timespec nextWakeup{};
    clock_gettime(CLOCK_MONOTONIC, &nextWakeup);
    nextWakeup = addNsToTs(nextWakeup, 100'000LL);

    while (running_.load(std::memory_order_acquire)) {
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &nextWakeup, nullptr);
        nextWakeup = addNsToTs(nextWakeup, static_cast<int64_t>(cycleNs_));
        ++cycleCount_;

        struct timespec now{};
        clock_gettime(CLOCK_MONOTONIC, &now);
        const int64_t overNs = diffNs(now, nextWakeup);
        if (overNs > 0) {
            ++overrunCount_;
            totalOverNs_ += overNs;
            maxOverrunNs_ = std::max(overNs, maxOverrunNs_);
        }

        common::rt::signalProcessTickNow();

        registry_.readAll();
        rtCycle();
        registry_.writeAll();

        if (cycleNs_ > 0) {
            const uint64_t dumpEvery = 5'000'000'000ULL / cycleNs_;
            if (dumpEvery > 0 && (cycleCount_ % dumpEvery) == 0) {
                const int64_t avgOverrunNs = (overrunCount_ > 0)
                    ? static_cast<int64_t>(totalOverNs_ / static_cast<int64_t>(overrunCount_))
                    : 0LL;
                common::log::logInfo(messages::MessageId::RT_CYCLE_STATS,
                        static_cast<int64_t>(cycleCount_),
                        maxOverrunNs_,
                        avgOverrunNs);
            }
        }
    }

    common::log::logInfo(messages::MessageId::APPLICATION_SHUTDOWN);
}

void Application::rtCycle() noexcept
{
    const uint64_t nowNs = common::rt::signalProcessNowNs();

    constexpr bool estopActive = false;  // TODO: wire E-Stop DI
    stateMachine_.tick(estopActive, nowNs);

    if (stateMachine_.haltActive()) {
        for (auto& q : queues_) {
            q->safeState();
        }
        return;
    }

    // Mission execution (if mission loaded and running)
    if (mission_ && missionRunning_.load(std::memory_order_acquire)) {
        // TODO: Get current position from GPSWrapper when GPS integration is complete
        // For now, use zero position (bench test validates logic separately)
        common::math::Vec3f currentPosition{0.0f, 0.0f, 0.0f};
        float currentAltitude = 0.0f;

        missionEvaluator_.tick(currentPosition, currentAltitude, nowNs);

        // Advance leg if criterion met
        int currentLeg = mission_->currentLegIndex();
        if (missionEvaluator_.shouldAdvanceLeg(currentLeg)) {
            missionEvaluator_.markLegComplete(currentLeg);
            mission_->advanceLeg();
        }
    }

    for (auto& q : queues_) {
        q->tick(cycleCount_, nowNs);
    }
}

} // namespace fc::app
