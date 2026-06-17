#pragma once

#include "common/rt/Threadrunner.h"
#include "common/log/sinks/LogStreamSink.h"

#include <atomic>
#include <string>

namespace fc::app {
class Application;
}

namespace fc::services {

// ============================================================================
// GrpcService — gRPC server for EtherCatDrone.
// Hosts all gRPC service implementations on a single port (default 50051).
// ============================================================================

class GrpcService final : public common::rt::Threadrunner {
public:
    explicit GrpcService(LogStreamSink&              logSink,
                         std::atomic<bool>&          restartFlag,
                         fc::app::Application*       app,
                         std::string                 address = "0.0.0.0:50051");
    ~GrpcService() override = default;

    void requestStop() noexcept;
    void run() override;

private:
    LogStreamSink&              logSink_;
    std::atomic<bool>&          restartFlag_;
    fc::app::Application*       app_;
    std::string                 address_;
    std::atomic<bool>           running_{false};
};

} // namespace fc::services
