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
#include "fc/gpio/GPIOAdapter.h"
#include "fc/gpio/BoardVariant.h"
#include "fc/i2c/I2CAdapter.h"
#include "fc/spi/SPIAdapter.h"
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
#include <sys/stat.h>
#include <dirent.h>

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
        std::printf("[drone_app] ✓ EtherCAT backend active (%d slaves)\n", ec->slaveCount());
    } else {
        common::log::logInfo(messages::MessageId::MAIN_ETHERCAT_UNAVAILABLE);
        std::printf("[drone_app] ✗ EtherCAT backend unavailable (stub)\n");
    }
    registry.addBackend(std::move(ec));

    // I2C backend — probe for available buses
    {
        bool hasI2CBus = false;
        std::string i2cBusPath;
        DIR* dev = opendir("/dev");
        if (dev) {
            struct dirent* entry;
            while ((entry = readdir(dev)) != nullptr) {
                if (entry->d_name[0] == 'i' && entry->d_name[1] == '2' &&
                    entry->d_name[2] == 'c' && entry->d_name[3] == '-') {
                    hasI2CBus = true;
                    i2cBusPath = "/dev/" + std::string(entry->d_name);
                    break;  // Use first available bus
                }
            }
            closedir(dev);
        }
        if (hasI2CBus) {
            auto i2c = std::make_unique<fc::i2c::I2CAdapter>(i2cBusPath);
            i2c->setCatalog(&catalog);
            const bool hasI2C = i2c->initialize();
            if (hasI2C) {
                std::printf("[drone_app] ✓ I2C backend active on %s\n", i2cBusPath.c_str());
            } else {
                std::printf("[drone_app] ⚠ I2C bus available at %s — no devices configured\n",
                            i2cBusPath.c_str());
                std::printf("[drone_app]   Add I2C devices to hardware.json to enable\n");
            }
            registry.addBackend(std::move(i2c));
        } else {
            std::printf("[drone_app] ✗ I2C backend unavailable (no /dev/i2c-* found)\n");
        }
    }

    // SPI backend — probe for available buses
    {
        bool hasSPIBus = false;
        std::string spiBusPath;
        DIR* dev = opendir("/dev");
        if (dev) {
            struct dirent* entry;
            while ((entry = readdir(dev)) != nullptr) {
                if (entry->d_name[0] == 's' && entry->d_name[1] == 'p' &&
                    entry->d_name[2] == 'i' && entry->d_name[3] == 'd' &&
                    entry->d_name[4] == 'e' && entry->d_name[5] == 'v') {
                    hasSPIBus = true;
                    spiBusPath = "/dev/" + std::string(entry->d_name);
                    break;  // Use first available bus
                }
            }
            closedir(dev);
        }
        if (hasSPIBus) {
            auto spi = std::make_unique<fc::spi::SPIAdapter>(spiBusPath);
            spi->setCatalog(&catalog);
            const bool hasSPI = spi->initialize();
            if (hasSPI) {
                std::printf("[drone_app] ✓ SPI backend active on %s\n", spiBusPath.c_str());
            } else {
                std::printf("[drone_app] ⚠ SPI bus available at %s — no devices configured\n",
                            spiBusPath.c_str());
                std::printf("[drone_app]   Add SPI devices to hardware.json to enable\n");
            }
            registry.addBackend(std::move(spi));
        } else {
            std::printf("[drone_app] ✗ SPI backend unavailable (no /dev/spidev* found)\n");
        }
    }

    // GPIO backend — probe for gpiochip, auto-detect board variant
    {
        auto boardVariant = fc::gpio::detectBoardVariant();
        std::string chipPath = fc::gpio::gpioChipPath(boardVariant);
        bool hasGPIO = fc::gpio::gpioChipAvailable(boardVariant);

        if (hasGPIO) {
            std::printf("[drone_app] %s detected — GPIO backend on %s\n",
                        fc::gpio::boardVariantName(boardVariant).c_str(),
                        chipPath.c_str());
            auto gpio = std::make_unique<fc::gpio::GPIOAdapter>(boardVariant, chipPath);
            gpio->setCatalog(&catalog);
            const bool gpioReady = gpio->initialize();
            if (gpioReady && !gpio->isStubMode()) {
                std::printf("[drone_app] ✓ GPIO backend active (%s, %zu lines)\n",
                            fc::gpio::boardVariantName(boardVariant).c_str(),
                            gpio->lineCount());
            } else if (gpioReady) {
                std::printf("[drone_app] ⚠ GPIO backend initialized in stub mode (no libgpiod)\n");
                std::printf("[drone_app]   Install libgpiod on target: sudo apt install libgpiod-dev\n");
            } else {
                std::printf("[drone_app] ⚠ GPIO chip available but no lines configured\n");
                std::printf("[drone_app]   Add GPIO lines to hardware.json to enable\n");
            }
            registry.addBackend(std::move(gpio));
        } else {
            std::printf("[drone_app] ✗ GPIO backend unavailable (no /dev/gpiochip* found)\n");
        }
    }

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

    // --- Startup diagnostics ------------------------------------------------
    const bool hasRealHardware = hasEthercat;
    const uint64_t totalChannels = registry.entryCount();
    std::printf("[drone_app] Backends: %zu | Channels: %lu | Cycle: %d us\n",
                registry.backendCount(), (unsigned long)totalChannels, cfg.cycleTimeUs);
    if (!hasRealHardware) {
        std::printf("[drone_app] ⚠ Running in simulation-only mode — no real hardware backends active\n");
        std::printf("[drone_app]   To add EtherCAT hardware:\n");
        std::printf("[drone_app]     1. Load kernel module: sudo modprobe ethercat\n");
        std::printf("[drone_app]     2. Connect EtherCAT slaves to the bus\n");
        std::printf("[drone_app]     3. Define channels in config/default/hardware.json\n");
        std::printf("[drone_app]   To run with RT scheduling: sudo %s\n",
                    argc > 0 ? argv[0] : "./bin/drone_app");
    }
    std::printf("[drone_app] RT loop starting... (Ctrl+C to stop)\n");
    fflush(stdout);

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
