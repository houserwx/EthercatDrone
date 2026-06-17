#include "fc/services/GrpcService.h"

namespace fc::services {

GrpcService::GrpcService(LogStreamSink& logSink,
                         std::atomic<bool>& restartFlag,
                         fc::app::Application* app,
                         std::string address)
    : common::rt::Threadrunner(common::rt::ThreadConfiguration{
          .name        = "GrpcService",
          .cpuCore     = -1,
          .priority    = 0,
          .useRealtime = false,
      })
    , logSink_(logSink)
    , restartFlag_(restartFlag)
    , app_(app)
    , address_(std::move(address))
{
}

void GrpcService::requestStop() noexcept
{
    running_.store(false, std::memory_order_release);
}

void GrpcService::run()
{
    running_.store(true, std::memory_order_acquire);

#ifdef GRPC_AVAILABLE
    // Phase 1: Stub — will build grpc::Server, register services, start.
    // Real implementation will be added when proto files are compiled.
    (void)logSink_;
    (void)restartFlag_;
    (void)app_;
    (void)address_;
#else
    // Stub mode: no-op loop
    while (running_.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
#endif

    running_.store(false, std::memory_order_release);
}

} // namespace fc::services
