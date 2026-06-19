#include "common/menu/ConfigDisplayPanel.h"
#include "common/menu/Console.h"

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace common::menu {

ConfigDisplayPanel::ConfigDisplayPanel(const common::config::Config& cfg,
                                       const std::string& configPath)
    : cfg_(cfg), configPath_(configPath)
{
    setTitle("Current Configuration");
}

MenuAction ConfigDisplayPanel::run()
{
    for (;;) {
        printBanner(title_);
        std::cout << "  Config file: " << configPath_ << "\n\n";

        colorBold();
        std::cout << "  [1] General settings\n";
        std::cout << "  [2] Encoder entries\n";
        std::cout << "  [3] Digital input entries\n";
        std::cout << "  [4] Digital output entries\n";
        std::cout << "  [5] Analog input entries\n";
        std::cout << "  [6] Analog output entries\n";
        colorReset();
        std::cout << "  [b] Back\n";

        printFooter();

        std::string line = readLine("Select [1-6, b]");
        if (line == "b" || line == "B") return MenuAction::Back;
        if (line == "q" || line == "Q") return MenuAction::Quit;

        try {
            int sel = std::stoi(line);
            int idx = 0;
            switch (sel) {
                case 1: {
                    printBanner("General Settings");
                    std::cout << "  Cycle time      : " << cfg_.cycleTimeUs << " us\n";
                    std::cout << "  Demo cycles     : " << cfg_.demoCycles << "\n";
                    std::cout << "  Catalog path    : " << cfg_.hardwareCatalogPath << "\n";
                    std::cout << "  Total PDO entries: " << cfg_.pdoEntries.size() << "\n";
                    printFooter();
                    readLine("Press Enter to continue...");
                    break;
                }
                case 2: displayCategory("Encoder", idx); break;
                case 3: displayCategory("DigitalInput", idx); break;
                case 4: displayCategory("DigitalOutput", idx); break;
                case 5: displayCategory("AnalogInput", idx); break;
                case 6: displayCategory("AnalogOutput", idx); break;
                default:
                    std::cout << "  Invalid selection.\n";
                    readLine("Press Enter to continue...");
            }
        } catch (...) {
            std::cout << "  Invalid input.\n";
            readLine("Press Enter to continue...");
        }
    }
}

void ConfigDisplayPanel::displayCategory(const std::string& category, int& idx)
{
    auto& entries = cfg_.pdoEntries;
    std::vector<const common::config::PdoEntryDef*> matches;
    for (const auto& e : entries) {
        if (e.channelType == category) matches.push_back(&e);
    }

    printBanner(std::string(category) + " Entries (" + std::to_string(matches.size()) + ")");

    if (matches.empty()) {
        std::cout << "  (no entries)\n";
    } else {
        for (const auto* e : matches) {
            colorCyan();
            std::cout << "  --- " << e->name << " ---\n";
            colorReset();
            std::cout << "    UUID         : " << e->hwUuid << "\n";
            std::cout << "    Channel type : " << e->channelType << "\n";
            if (e->pulseMs > 0)    std::cout << "    Pulse ms     : " << e->pulseMs << "\n";
            if (e->debounceMs > 0) std::cout << "    Debounce ms  : " << e->debounceMs << "\n";
            if (e->pin >= 0)       std::cout << "    Pin          : " << e->pin << "\n";
            const auto& s = e->sim;
            if (s.rpm != 0.0f || s.partsPerMin != 0.0f) {
                std::cout << "    Sim RPM      : " << s.rpm << "\n";
                std::cout << "    Sim PPM      : " << s.partsPerMin << "\n";
                if (s.variancePercent != 0.0f)
                    std::cout << "    Sim variance : " << s.variancePercent << "%\n";
            }
            std::cout << "\n";
        }
    }

    printFooter();
    readLine("Press Enter to continue...");
}

} // namespace common::menu
