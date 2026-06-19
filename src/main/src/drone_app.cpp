// ============================================================================
// drone_app.cpp — EtherCatDrone entry point.
//
// Responsibilities:
//   1. Signal handling (SIGINT / SIGTERM → Application::requestStop)
//   2. Config resolution and load
//   3. Logger init + start
//   4. Memory locking (mlockall)
//   5. DynamicHardwareContext construction (facade manages all backends)
//   6. Application thread create / start / join
//   7. gRPC service start / join
//   8. Final diagnostics + Logger stop
// ============================================================================

#include "common/config/Config.h"
#include "dynamichardware/DynamicHardwareContext.h"
#include "fc/app/Application.h"
#include "common/log/Logger.h"
#include "common/log/LogHelper.h"
#include "common/log/sinks/ConsoleSink.h"
#include "common/log/sinks/LogStreamSink.h"
#include "fc/services/GrpcService.h"

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <sys/mman.h>

// ---------------------------------------------------------------------------
// Signal handling
// ---------------------------------------------------------------------------
static fc::app::Application* gApp = nullptr;
static void sigHandler(int /*unused*/)
{
    if (gApp != nullptr) { gApp->requestStop(); }
}

// ---------------------------------------------------------------------------
// main()
// ---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    signal(SIGINT,  sigHandler);
    signal(SIGTERM, sigHandler);

    // --- Resolve config path -----------------------------------------------
    std::string configPath = (argc > 1) ? argv[1] : "config/default/hardware.json";
    {
        std::ifstream probe(configPath);
        if (!probe && configPath.find('/') == std::string::npos) {
            std::string alt = "config/" + configPath;
            if (std::ifstream{alt}) { configPath = alt; }
        }
    }

    // --- Logger ------------------------------------------------------------
    common::log::ConsoleSink    consoleSink;
    common::log::LogStreamSink  logStreamSink;

    common::log::LoggerConfiguration logConfig;
    logConfig.minLevel = messages::LogLevel::Debug;
    common::log::Logger& logger = common::log::Logger::instance();
    logger.init(logConfig);
    logger.addSink(&consoleSink);
    logger.addSink(&logStreamSink);
    logger.start();
    common::log::threadLoggerInit(false);

    // --- Lock memory pages -------------------------------------------------
    if (mlockall(MCL_CURRENT) != 0) {
        common::log::logError(messages::MessageId::MAIN_MLOCKALL_FAILED);
    }

    // --- Load config -------------------------------------------------------
    common::config::Config cfg;
    try {
        cfg = common::config::Config::loadFromJson(configPath);
        common::log::logInfo(messages::MessageId::MAIN_CONFIG_LOADED,
                static_cast<int64_t>(cfg.pdoEntries.size()),
                static_cast<int64_t>(cfg.cycleTimeUs));
    } catch (const std::exception& ex) {
        (void)ex;
        common::log::logError(messages::MessageId::MAIN_CONFIG_ERROR);
    }

    const uint32_t cycleNs = cfg.cycleNs();

    // --- Build DynamicHardwareContext (facade manages all backends) ---------
    // The facade handles graceful degradation if backends are unavailable.
    auto ctx = dynamichardware::DynamicHardwareContext::builder()
        .catalogPath(cfg.hardwareCatalogPath)
        .withEthercat(cycleNs)
        .withGPIO()
        .withI2C()
        .withSPI()
        .withSimulation()
        .build();

    if (!ctx) {
        common::log::logError(messages::MessageId::MAIN_SIM_INIT_FAILED);
        common::log::Logger::instance().stop();
        return 1;
    }

    // Build: discover hardware, create adapters, populate catalog
    if (!ctx->build()) {
        common::log::logError(messages::MessageId::MAIN_SIM_INIT_FAILED);
        common::log::Logger::instance().stop();
        return 1;
    }

    // Freeze: lock PDOs, build UUID map, ready for RT
    if (!ctx->freeze()) {
        common::log::logError(messages::MessageId::MAIN_SIM_INIT_FAILED);
        common::log::Logger::instance().stop();
        return 1;
    }

    // --- Startup diagnostics ------------------------------------------------
    std::printf("[drone_app] Backends: %zu | Channels: %zu | Cycle: %d us\n",
                ctx->backendCount(), ctx->entryCount(), cfg.cycleTimeUs);
    std::printf("[drone_app] RT loop starting... (Ctrl+C to stop)\n");
    fflush(stdout);

    // --- Application -------------------------------------------------------
    fc::app::Application app(ctx.get(), cycleNs);
    gApp = &app;

    // --- gRPC service ------------------------------------------------------
    std::atomic<bool> restartFlag{false};
    fc::services::GrpcService grpcService(logStreamSink, restartFlag, &app);

    // --- Start threads -----------------------------------------------------
    grpcService.start();
    app.start();

    // --- Wait for RT loop to exit ------------------------------------------
    app.join();
    grpcService.requestStop();
    grpcService.join();

    // --- Final diagnostics -------------------------------------------------
    std::printf("\n[drone_app] Cycles: %lu, Overruns: %d, MaxOverrun: %lld ns\n",
                app.cycleCount(), app.overrunCount(),
                static_cast<long long>(app.maxOverrunNs()));

    // --- Cleanup -----------------------------------------------------------
    common::log::Logger::instance().stop();
    return 0;
}
