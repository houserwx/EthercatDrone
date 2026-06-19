#include "common/menu/PdoMappingPanel.h"
#include "common/menu/Console.h"

#include <iostream>
#include <algorithm>

namespace common::menu {

PdoMappingPanel::PdoMappingPanel(std::vector<common::config::PdoEntryDef>& entries)
    : entries_(entries)
{
    setTitle("PDO Mappings");
}

MenuAction PdoMappingPanel::run()
{
    for (;;) {
        printBanner(title_);
        std::cout << "  Total PDO mappings: " << entries_.size() << "\n\n";

        colorBold();
        std::cout << "  [1] List all mappings\n";
        std::cout << "  [2] Add new mapping\n";
        std::cout << "  [3] Edit mapping\n";
        std::cout << "  [4] Remove mapping\n";
        colorReset();
        std::cout << "  [b] Back\n";
        std::cout << "  [q] Quit\n";

        printFooter();

        std::string line = readLine("Select [1-4, b, q]");
        if (line == "b" || line == "B") return MenuAction::Back;
        if (line == "q" || line == "Q") return MenuAction::Quit;

        try {
            int sel = std::stoi(line);
            switch (sel) {
                case 1: listMappings(); break;
                case 2: addMapping(); break;
                case 3: {
                    if (entries_.empty()) {
                        std::cout << "  No mappings to edit.\n";
                        readLine("Press Enter...");
                        break;
                    }
                    int idx = readMenuChoice(static_cast<int>(entries_.size()),
                                             "Select mapping to edit");
                    if (idx >= 0) editMapping(static_cast<size_t>(idx));
                    break;
                }
                case 4: {
                    if (entries_.empty()) {
                        std::cout << "  No mappings to remove.\n";
                        readLine("Press Enter...");
                        break;
                    }
                    int idx = readMenuChoice(static_cast<int>(entries_.size()),
                                             "Select mapping to remove");
                    if (idx >= 0) removeMapping(static_cast<size_t>(idx));
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

void PdoMappingPanel::listMappings()
{
    printBanner("PDO Mappings List");
    if (entries_.empty()) {
        std::cout << "  (no PDO mappings configured)\n";
    } else {
        for (size_t i = 0; i < entries_.size(); ++i) {
            const auto& e = entries_[i];
            colorCyan();
            std::cout << "  " << (i + 1) << ". " << e.name << "\n";
            colorReset();
            std::cout << "     UUID         : " << e.hwUuid << "\n";
            std::cout << "     Channel type : " << e.channelType << "\n";
            if (e.pin >= 0) std::cout << "     GPIO pin     : " << e.pin << "\n";
            std::cout << "\n";
        }
    }
    printFooter();
    readLine("Press Enter to continue...");
}

void PdoMappingPanel::addMapping()
{
    printBanner("Add PDO Mapping");

    std::string name = readLine("Wrapper name (e.g. MotorA-Encoder)");
    if (name.empty()) {
        std::cout << "  Cancelled.\n";
        readLine("Press Enter...");
        return;
    }

    std::cout << "\nChannel types:\n";
    std::cout << "  [1] BoolInput\n";
    std::cout << "  [2] BoolOutput\n";
    std::cout << "  [3] Int16Input\n";
    std::cout << "  [4] Int16Output\n";
    std::cout << "  [5] Int32Input\n";
    std::cout << "  [6] FloatInput\n";
    std::cout << "  [7] FloatOutput\n";

    int typeIdx = readMenuChoice(7, "Select channel type");
    if (typeIdx < 0) {
        std::cout << "  Cancelled.\n";
        readLine("Press Enter...");
        return;
    }

    const char* channelTypes[] = {
        "BoolInput", "BoolOutput", "Int16Input", "Int16Output",
        "Int32Input", "FloatInput", "FloatOutput"
    };

    common::config::PdoEntryDef def;
    def.name        = name;
    def.channelType = channelTypes[typeIdx];
    def.pin         = readInt("  GPIO pin (-1 for non-GPIO)", -1);

    std::string uuid = readLine("  Hardware UUID (leave empty for auto-generate)");
    if (uuid.empty()) {
        // Auto-generate a virt-* UUID for simulated, or let the user know
        // they'll need to provide a real UUID after hardware discovery
        uuid = "virt-auto-" + std::to_string(entries_.size() + 1);
        colorYellow();
        std::cout << "  Auto-generated UUID: " << uuid << "\n";
        std::cout << "  (replace after hardware discovery)\n";
        colorReset();
    }
    def.hwUuid = uuid;

    // Ask for sim params if it's a simulated entry
    if (def.hwUuid.rfind("virt-", 0) == 0) {
        std::cout << "\n  Simulated entry detected — set sim params?\n";
        if (readYesNo("  Configure simulation", false)) {
            def.sim.rpm            = readFloat("    RPM", 0.0f);
            def.sim.partsPerMin    = readFloat("    Parts/min", 0.0f);
            def.sim.variancePercent = readFloat("    Variance (%)", 0.0f);
            def.pulseMs            = static_cast<uint32_t>(readInt("    Pulse (ms)", 0));
            def.debounceMs         = static_cast<uint32_t>(readInt("    Debounce (ms)", 0));
        }
    }

    entries_.push_back(std::move(def));
    hasUnsavedChanges_ = true;

    colorGreen();
    std::cout << "  Added mapping: " << name << "\n";
    colorReset();
    readLine("Press Enter...");
}

void PdoMappingPanel::editMapping(size_t idx)
{
    auto& def = entries_[idx];
    printBanner("Edit Mapping: " + def.name);

    std::cout << "  Current: " << def.name << " → " << def.hwUuid << "\n";

    std::string newName = readLine("  New name (Enter to keep)");
    if (!newName.empty()) def.name = newName;

    std::string newUuid = readLine("  New UUID (Enter to keep)");
    if (!newUuid.empty()) def.hwUuid = newUuid;

    def.pin = readInt("  GPIO pin (-1 for none)", def.pin);

    hasUnsavedChanges_ = true;
    colorGreen();
    std::cout << "  Updated.\n";
    colorReset();
    readLine("Press Enter...");
}

void PdoMappingPanel::removeMapping(size_t idx)
{
    std::string name = entries_[idx].name;
    entries_.erase(entries_.begin() + idx);
    hasUnsavedChanges_ = true;

    colorGreen();
    std::cout << "  Removed mapping: " << name << "\n";
    colorReset();
    readLine("Press Enter...");
}

void PdoMappingPanel::showAvailableEntries()
{
    // Placeholder — would show catalog entries from DynamicHardwareContext
    // that are not yet mapped. Called when user wants to pick from discovered hw.
    std::cout << "  Hardware discovery not available in config-only mode.\n";
    std::cout << "  Run the app once to populate hardware_catalog.json.\n";
}

} // namespace common::menu
