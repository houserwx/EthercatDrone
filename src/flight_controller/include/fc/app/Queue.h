#pragma once

#include "fc/safety/AlwaysOnEval.h"
#include "fc/safety/LineMonitor.h"
#include "fc/pdo/HardwareRegistry.h"
#include "fc/grpc/GrpcAdapter.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace fc::app {

// ============================================================================
// Queue — self-contained representation of one physical segment.
// Owned by Application; one per line segment or drone configuration.
// ============================================================================

class Queue final {
public:
    Queue(const Queue&)            = delete;
    Queue& operator=(const Queue&) = delete;
    Queue(Queue&&)                 = delete;
    Queue& operator=(Queue&&)      = delete;
    ~Queue()                       = default;

    [[nodiscard]] std::string_view id()          const noexcept { return id_; }
    [[nodiscard]] std::string_view displayName() const noexcept { return displayName_; }

    [[nodiscard]] fc::grpc::GrpcAdapter* grpcAdapter() noexcept { return grpcAdapter_; }

    [[nodiscard]] static std::unique_ptr<Queue>
        loadFromJson(const std::string&          path,
                     fc::pdo::HardwareRegistry& registry);

    void saveToJson(const std::string& path) const;
    void freeze();

    void start() noexcept { running_.store(true,  std::memory_order_release); }
    void stop()  noexcept { running_.store(false, std::memory_order_release); }

    [[nodiscard]] bool isRunning() const noexcept
    {
        return running_.load(std::memory_order_acquire);
    }

    void tick(uint64_t cycleCount, uint64_t nowNs) noexcept;
    void safeState() noexcept;

private:
    // Default constructor is private — use loadFromJson() to construct.
    // Made accessible via friend factory in Queue.cpp.
    Queue() = default;
    friend std::unique_ptr<Queue> createQueue();

    std::string id_;
    std::string displayName_;
    std::atomic<bool> running_{false};
    fc::grpc::GrpcAdapter* grpcAdapter_{nullptr};
};

} // namespace fc::app
