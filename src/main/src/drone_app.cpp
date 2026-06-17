// ============================================================================
// drone_app.cpp — EtherCatDrone entry point.
//
// Responsibilities:
//   1. Signal handling (SIGINT / SIGTERM → Application::requestStop)
//   2. Config resolution and load
//   3. Logger init + start
//   4. Memory locking (mlockall)
//   5. Hardware registry construction (EtherCAT + Simulated + freezeForRt)
//   6. Application thread create / start / join
//   7. gRPC service start / join
//   8. Final diagnostics + Logger stop
// ============================================================================

#include "common/config/Config.h"
#include "fc/ethercat/EthercatAdapter.h"
#include "fc/ethercat/HardwareCatalog.h"
#include "fc/simulated/SimulatedAdapter.h"
#include "fc/pdo/HardwareRegistry.h"
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

    // --- Hardware catalog --------------------------------------------------
    fc::pdo::HardwareCatalog catalog;
    catalog.load(cfg.hardwareCatalogPath);

    // --- Build hardware registry -------------------------------------------
    fc::pdo::HardwareRegistry registry;

    // EtherCAT backend (non-fatal if no hardware present)
    auto ec = std::make_unique<fc::ethercat::EthercatAdapter>(cycleNs);
    ec->setCatalog(&catalog);
    const bool hasEthercat = ec->initialize();
    if (!catalog.empty()) {
        std::filesystem::create_directories("config/shared");
        catalog.save(cfg.hardwareCatalogPath);
    }
    if (hasEthercat) {
        common::log::logInfo(messages::MessageId::MAIN_ETHERCAT_READY,
                static_cast<int64_t>(ec->slaveCount()),
                static_cast<int64_t>(ec->workingCounter()),
                static_cast<int64_t>(ec->isFullyCommunicating() ? 1 : 0));
    } else {
        common::log::logInfo(messages::MessageId::MAIN_ETHERCAT_UNAVAILABLE);
    }
    registry.addBackend(std::move(ec));

    // Simulated adapter — always present
    auto sim = std::make_unique<fc::simulated::SimulatedAdapter>(cfg);
    if (!sim->initialize()) {
        common::log::logError(messages::MessageId::MAIN_SIM_INIT_FAILED);
        common::log::Logger::instance().stop();
        return 1;
    }
    registry.addBackend(std::move(sim));

    // --- Freeze for RT -----------------------------------------------------
    registry.freezeForRt();

    // --- Application -------------------------------------------------------
    fc::app::Application app(registry, cycleNs);
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
