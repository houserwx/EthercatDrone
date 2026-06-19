#include "common/menu/MainMenu.h"
#include "common/menu/Console.h"

#include <iostream>
#include <memory>
#include <string>

namespace common::menu {

MainMenu::MainMenu(common::config::Config& cfg, const std::string& configPath)
    : cfg_(cfg), configPath_(configPath)
{
    setTitle("EtherCatDrone — Configuration Menu");
}

MenuAction MainMenu::run()
{
    // Inform user about the workflow
    clearScreen();
    colorBold(); colorCyan();
    std::cout << "========================================\n";
    std::cout << "  EtherCatDrone Configuration Editor\n";
    std::cout << "========================================\n";
    colorReset();
    std::cout << "\n";
    std::cout << "  Changes are applied to the config file.\n";
    std::cout << "  A restart is required for changes to take effect.\n";
    std::cout << "  The current RT session will NOT be affected.\n";
    std::cout << "\n";
    std::cout << "  Config file: " << configPath_ << "\n";
    std::cout << "\n";
    colorYellow();
    std::cout << "  Press Enter to continue...\n";
    colorReset();
    std::cin.ignore();

    for (;;) {
        printBanner(title_);

        colorBold();
        std::cout << "  [1] View current configuration\n";
        std::cout << "  [2] Simulated entries manager\n";
        std::cout << "  [3] PDO mappings manager\n";
        std::cout << "  [4] Cycle time settings\n";
        std::cout << "  [5] Load config from file\n";
        colorReset();
        std::cout << "  [s] Save config to file\n";
        std::cout << "  [r] Save and mark for restart\n";
        std::cout << "  [q] Quit without saving\n";

        printFooter();

        std::string line = readLine("Select [1-5, s, r, q]");

        if (line == "q" || line == "Q") {
            if (readYesNo("  Discard unsaved changes?", true)) {
                return MenuAction::Quit;
            }
            continue;
        }

        if (line == "s" || line == "S") {
            if (cfg_.saveToJson(configPath_)) {
                colorGreen();
                std::cout << "  Config saved to " << configPath_ << "\n";
                colorReset();
                readLine("Press Enter...");
            } else {
                colorRed();
                std::cout << "  Failed to save config!\n";
                colorReset();
                readLine("Press Enter...");
            }
            continue;
        }

        if (line == "r" || line == "R") {
            if (cfg_.saveToJson(configPath_)) {
                colorGreen();
                std::cout << "  Config saved successfully.\n";
                colorYellow();
                std::cout << "  Restart required for changes to take effect.\n";
                colorReset();
                shouldSaveAndRestart_ = true;
                return MenuAction::SaveAndRestart;
            } else {
                colorRed();
                std::cout << "  Failed to save config!\n";
                colorReset();
                readLine("Press Enter...");
            }
            continue;
        }

        try {
            int sel = std::stoi(line);
            switch (sel) {
                case 1: {
                    runPanel(std::make_unique<ConfigDisplayPanel>(cfg_, configPath_));
                    break;
                }
                case 2: {
                    runPanel(std::make_unique<SimulatedEntryPanel>(cfg_.pdoEntries));
                    break;
                }
                case 3: {
                    runPanel(std::make_unique<PdoMappingPanel>(cfg_.pdoEntries));
                    break;
                }
                case 4: {
                    auto panel = std::make_unique<CycleTimePanel>(cfg_.cycleTimeUs);
                    runPanel(std::move(panel));
                    break;
                }
                case 5: {
                    try {
                        cfg_ = common::config::Config::loadFromJson(configPath_);
                        colorGreen();
                        std::cout << "  Config reloaded from " << configPath_ << "\n";
                        colorReset();
                        readLine("Press Enter...");
                    } catch (const std::exception& ex) {
                        colorRed();
                        std::cout << "  Failed to load config: " << ex.what() << "\n";
                        colorReset();
                        readLine("Press Enter...");
                    }
                    break;
                }
                default:
                    std::cout << "  Invalid selection.\n";
                    readLine("Press Enter...");
            }
        } catch (...) {
            std::cout << "  Invalid input.\n";
            readLine("Press Enter...");
        }
    }
}

void MainMenu::runPanel(std::unique_ptr<MenuPanel> panel)
{
    panel->run();
}

} // namespace common::menu
