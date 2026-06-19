// ============================================================================
// bench_test.cpp — Bench test harness (sim-only, fault injection).
//
// Runs the full RT control loop with DynamicHardwareContext (simulation).
// No real hardware required.
// ============================================================================

#include "common/config/Config.h"
#include "dynamichardware/DynamicHardwareContext.h"
#include "fc/app/Application.h"
#include "common/log/Logger.h"
#include "common/log/LogHelper.h"
#include "common/log/sinks/ConsoleSink.h"

#include <csignal>
#include <cstdio>
#include <memory>
#include <string>
#include <sys/mman.h>

static fc::app::Application* gApp = nullptr;
static void sigHandler(int /*unused*/)
{
    if (gApp != nullptr) { gApp->requestStop(); }
}

int main(int argc, char* argv[])
{
    signal(SIGINT,  sigHandler);
    signal(SIGTERM, sigHandler);

    // --- Logger ------------------------------------------------------------
    common::log::ConsoleSink consoleSink;
    common::log::LoggerConfiguration logConfig;
    logConfig.minLevel = messages::LogLevel::Debug;
    common::log::Logger& logger = common::log::Logger::instance();
    logger.init(logConfig);
    logger.addSink(&consoleSink);
    logger.start();
    common::log::threadLoggerInit(false);

    // --- Lock memory -------------------------------------------------------
    if (mlockall(MCL_CURRENT) != 0) {
        common::log::logError(messages::MessageId::MAIN_MLOCKALL_FAILED);
    }

    // --- Load config -------------------------------------------------------
    std::string configPath = (argc > 1) ? argv[1] : "config/default/hardware.json";
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

    // --- Build DynamicHardwareContext (sim-only) ---------------------------
    auto ctx = dynamichardware::DynamicHardwareContext::builder()
        .catalogPath(cfg.hardwareCatalogPath)
        .withSimulation()
        .build();

    if (!ctx) {
        common::log::logError(messages::MessageId::MAIN_SIM_INIT_FAILED);
        common::log::Logger::instance().stop();
        return 1;
    }

    if (!ctx->build()) {
        common::log::logError(messages::MessageId::MAIN_SIM_INIT_FAILED);
        common::log::Logger::instance().stop();
        return 1;
    }

    if (!ctx->freeze()) {
        common::log::logError(messages::MessageId::MAIN_SIM_INIT_FAILED);
        common::log::Logger::instance().stop();
        return 1;
    }

    // --- Application -------------------------------------------------------
    fc::app::Application app(ctx.get(), cycleNs);
    gApp = &app;

    app.start();
    app.join();

    // --- Final diagnostics -------------------------------------------------
    std::printf("\n[bench_test] Cycles: %lu, Overruns: %d, MaxOverrun: %lld ns\n",
                app.cycleCount(), app.overrunCount(),
                static_cast<long long>(app.maxOverrunNs()));

    common::log::Logger::instance().stop();
    return 0;
}
