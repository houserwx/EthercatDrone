// ============================================================================
// drone_app.cpp — EtherCatDrone entry point.
//
// Responsibilities:
//   1. Signal handling (SIGINT / SIGTERM → Application::requestStop)
//   2. Config resolution and load
//   3. Logger init + start
//   4. Memory locking (mlockall)
//   5. DynamicHardwareContext construction (facade manages all backends)
//   6. Configuration menu (pre-RT, optional)
//   7. Application thread create / start / join
//   8. gRPC service start / join
//   9. Final diagnostics + Logger stop
//
// Restart loop:
//   The app supports a "save & restart" workflow. When the user saves config
//   changes from the menu, the RT loop shuts down, the config is reloaded,
//   a new DynamicHardwareContext is built, and the RT loop restarts.
// ============================================================================

#include "common/config/Config.h"
#include "common/menu/MainMenu.h"
#include "common/menu/Console.h"
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
// initContext — build and freeze a DynamicHardwareContext from config
// ---------------------------------------------------------------------------
static std::shared_ptr<dynamichardware::DynamicHardwareContext>
initContext(const common::config::Config& cfg)
{
    auto ctx = dynamichardware::DynamicHardwareContext::builder()
        .catalogPath(cfg.hardwareCatalogPath)
        .withEthercat(cfg.cycleNs())
        .withGPIO()
        .withI2C()
        .withSPI()
        .withSimulation()
        .build();

    if (!ctx) return nullptr;
    if (!ctx->build()) return nullptr;
    if (!ctx->freeze()) return nullptr;
    return ctx;
}

// ---------------------------------------------------------------------------
// printConfigSummary — display current configuration before RT loop
// ---------------------------------------------------------------------------
static void printConfigSummary(const common::config::Config& cfg,
                               const std::shared_ptr<dynamichardware::DynamicHardwareContext>& ctx)
{
    std::printf("\n");
    std::printf("========== Configuration Summary ==========\n");
    std::printf("  Cycle time      : %d us\n", cfg.cycleTimeUs);
    std::printf("  Demo cycles     : %d\n", cfg.demoCycles);
    std::printf("  Catalog path    : %s\n", cfg.hardwareCatalogPath.c_str());
    std::printf("  PDO entries     : %zu\n", cfg.pdoEntries.size());
    std::printf("  Backends        : %zu\n", ctx ? ctx->backendCount() : 0);
    std::printf("  Channels        : %zu\n", ctx ? ctx->entryCount() : 0);

    // Per-category counts
    int enc = 0, di = 0, do_ = 0, ai = 0, ao = 0;
    for (const auto& e : cfg.pdoEntries) {
        if (e.channelType == "Encoder")       ++enc;
        else if (e.channelType == "DigitalInput") ++di;
        else if (e.channelType == "DigitalOutput") ++do_;
        else if (e.channelType == "AnalogInput")  ++ai;
        else if (e.channelType == "AnalogOutput") ++ao;
    }
    std::printf("    Encoders      : %d\n", enc);
    std::printf("    Digital inputs: %d\n", di);
    std::printf("    Digital outputs: %d\n", do_);
    std::printf("    Analog inputs : %d\n", ai);
    std::printf("    Analog outputs: %d\n", ao);
    std::printf("==========================================\n\n");
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

    bool running = true;
    while (running) {
        // --- Load config ---------------------------------------------------
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

        // --- Build DynamicHardwareContext ----------------------------------
        auto ctx = initContext(cfg);
        if (!ctx) {
            common::log::logError(messages::MessageId::MAIN_SIM_INIT_FAILED);
            common::log::Logger::instance().stop();
            return 1;
        }

        // --- Display config summary ----------------------------------------
        printConfigSummary(cfg, ctx);

        // --- Configuration menu (optional, pre-RT) -------------------------
        {
            std::string skipArg;
            for (int i = 1; i < argc; ++i) {
                if (std::string{argv[i]} == "--skip-menu") {
                    skipArg = argv[i];
                    break;
                }
            }

            if (skipArg.empty()) {
                common::log::Logger::instance().pause();
                std::printf("[drone_app] Enter configuration menu? (y/n) [n]: ");
                std::string line;
                std::getline(std::cin, line);
                if (!line.empty() && std::tolower(line[0]) == 'y') {
                    common::menu::MainMenu menu(cfg, configPath);
                    auto action = menu.run();
                    common::log::Logger::instance().resume();

                    if (action == common::menu::MenuAction::SaveAndRestart) {
                        // Config was already saved by MainMenu; just restart the loop
                        std::printf("\n[drone_app] Config saved — restarting with new settings...\n");
                        fflush(stdout);
                        running = true; // Continue outer loop (will rebuild context)
                        break;
                    }

                    if (action == common::menu::MenuAction::Quit) {
                        std::printf("\n[drone_app] Quitting without starting RT loop.\n");
                        fflush(stdout);
                        common::log::Logger::instance().stop();
                        return 0;
                    }

                    // Re-display config summary after menu edits
                    printConfigSummary(cfg, ctx);
                } else {
                    common::log::Logger::instance().resume();
                }
            }
        }

        // --- Startup diagnostics -------------------------------------------
        std::printf("[drone_app] Backends: %zu | Channels: %zu | Cycle: %d us\n",
                    ctx->backendCount(), ctx->entryCount(), cfg.cycleTimeUs);
        std::printf("[drone_app] RT loop starting... (Ctrl+C to stop)\n");
        fflush(stdout);

        // Pause logger during RT loop to avoid TTY I/O contention on ARM devices
        common::log::Logger::instance().pause();

        const uint32_t cycleNs = cfg.cycleNs();

        // --- Application ---------------------------------------------------
        fc::app::Application app(ctx.get(), cycleNs);
        gApp = &app;

        // --- gRPC service --------------------------------------------------
        std::atomic<bool> restartFlag{false};
        fc::services::GrpcService grpcService(logStreamSink, restartFlag, &app);

        // --- Start threads -------------------------------------------------
        grpcService.start();
        app.start();

        // --- Wait for RT loop to exit --------------------------------------
        app.join();
        grpcService.requestStop();
        grpcService.join();

        // Resume logger after RT loop exits
        common::log::Logger::instance().resume();

        // --- Final diagnostics ---------------------------------------------
        std::printf("\n[drone_app] Cycles: %lu, Overruns: %d, MaxOverrun: %lld ns\n",
                    app.cycleCount(), app.overrunCount(),
                    static_cast<long long>(app.maxOverrunNs()));
        fflush(stdout);

        // After RT loop exits (Ctrl+C), ask if user wants to re-enter menu or exit
        common::log::Logger::instance().pause();
        std::printf("\n[drone_app] RT loop stopped. Enter menu? (y/n) [n]: ");
        std::string line;
        std::getline(std::cin, line);
        if (!line.empty() && std::tolower(line[0]) == 'y') {
            // Reload config (may have been saved by menu in a previous iteration)
            try {
                cfg = common::config::Config::loadFromJson(configPath);
            } catch (...) {}

            common::menu::MainMenu menu(cfg, configPath);
            auto action = menu.run();
            common::log::Logger::instance().resume();

            if (action == common::menu::MenuAction::SaveAndRestart) {
                std::printf("\n[drone_app] Config saved — restarting with new settings...\n");
                fflush(stdout);
                running = true; // Rebuild context with new config
                continue;
            }

            if (action == common::menu::MenuAction::Quit) {
                std::printf("\n[drone_app] Exiting.\n");
                running = false;
                break;
            }

            // If Back (no save), ask to start RT loop
            common::log::Logger::instance().pause();
            std::printf("\n[drone_app] Start RT loop with current config? (y/n) [y]: ");
            std::string startLine;
            std::getline(std::cin, startLine);
            if (startLine.empty() || std::tolower(startLine[0]) == 'y') {
                common::log::Logger::instance().resume();
                running = true; // Rebuild context (config may have been modified in memory)
                // Save in-memory config so restart picks it up
                cfg.saveToJson(configPath);
                continue;
            }
            common::log::Logger::instance().resume();
            running = false;
        } else {
            common::log::Logger::instance().resume();
            running = false;
        }
    }

    // --- Cleanup -----------------------------------------------------------
    common::log::Logger::instance().stop();
    return 0;
}
