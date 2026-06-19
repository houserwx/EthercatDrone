#include "common/menu/SimulatedEntryPanel.h"
#include "common/menu/Console.h"

#include <algorithm>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace common::menu {

SimulatedEntryPanel::SimulatedEntryPanel(std::vector<common::config::PdoEntryDef>& entries)
    : entries_(entries)
{
    setTitle("Simulated Entries");
}

MenuAction SimulatedEntryPanel::run()
{
    for (;;) {
        printBanner(title_);
        std::cout << "  Simulated entries: " << entries_.size() << "\n\n";

        colorBold();
        std::cout << "  [1] List all simulated entries\n";
        std::cout << "  [2] Add new simulated entry\n";
        std::cout << "  [3] Edit simulated entry\n";
        std::cout << "  [4] Remove simulated entry\n";
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
                case 1: listEntries(); break;
                case 2: addEntry(); break;
                case 3: {
                    if (entries_.empty()) {
                        std::cout << "  No entries to edit.\n";
                        readLine("Press Enter...");
                        break;
                    }
                    int idx = readMenuChoice(static_cast<int>(entries_.size()),
                                             "Select entry to edit");
                    if (idx >= 0) editEntry(static_cast<size_t>(idx));
                    break;
                }
                case 4: {
                    if (entries_.empty()) {
                        std::cout << "  No entries to remove.\n";
                        readLine("Press Enter...");
                        break;
                    }
                    int idx = readMenuChoice(static_cast<int>(entries_.size()),
                                             "Select entry to remove");
                    if (idx >= 0) {
                        std::string name = entries_[idx].name;
                        entries_.erase(entries_.begin() + idx);
                        hasUnsavedChanges_ = true;
                        colorGreen();
                        std::cout << "  Removed: " << name << "\n";
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

void SimulatedEntryPanel::listEntries()
{
    printBanner("Simulated Entries List");
    if (entries_.empty()) {
        std::cout << "  (no simulated entries)\n";
    } else {
        for (size_t i = 0; i < entries_.size(); ++i) {
            const auto& e = entries_[i];
            std::cout << "  " << (i + 1) << ". " << e.name
                      << " [" << e.channelType << "]\n";
            std::cout << "     UUID: " << e.hwUuid << "\n";
            if (e.sim.rpm != 0.0f)   std::cout << "     RPM: " << e.sim.rpm << "\n";
            if (e.sim.partsPerMin != 0.0f)
                std::cout << "     Parts/min: " << e.sim.partsPerMin << "\n";
            if (e.pulseMs > 0)       std::cout << "     Pulse ms: " << e.pulseMs << "\n";
            if (e.debounceMs > 0)    std::cout << "     Debounce ms: " << e.debounceMs << "\n";
            std::cout << "\n";
        }
    }
    printFooter();
    readLine("Press Enter to continue...");
}

void SimulatedEntryPanel::addEntry()
{
    printBanner("Add Simulated Entry");

    std::string name = readLine("Entry name (e.g. MotorEncoder-A)");
    if (name.empty()) {
        std::cout << "  Cancelled (no name).\n";
        readLine("Press Enter...");
        return;
    }

    std::cout << "\nChannel types:\n";
    std::cout << "  [1] DigitalInput\n";
    std::cout << "  [2] DigitalOutput\n";
    std::cout << "  [3] AnalogInput\n";
    std::cout << "  [4] AnalogOutput\n";
    std::cout << "  [5] Encoder\n";

    int typeIdx = readMenuChoice(5, "Select channel type");
    if (typeIdx < 0) {
        std::cout << "  Cancelled.\n";
        readLine("Press Enter...");
        return;
    }

    const char* channelTypes[] = {"DigitalInput", "DigitalOutput", "AnalogInput", "AnalogOutput", "Encoder"};
    std::string channelType = channelTypes[typeIdx];

    // Count existing entries of this type for UUID generation
    int seq = 0;
    for (const auto& e : entries_) {
        if (e.channelType == channelType) ++seq;
    }

    common::config::PdoEntryDef def;
    def.name        = name;
    def.channelType = channelType;
    def.hwUuid      = generateUuid(channelType, seq + 1);
    def.pin         = -1;
    def.pulseMs     = (typeIdx == 1) ? 1000 : 0; // Default pulse for DO

    // Edit simulation parameters
    editSimParams(def);
    entries_.push_back(std::move(def));
    hasUnsavedChanges_ = true;

    colorGreen();
    std::cout << "  Added: " << name << " (" << channelType << ")\n";
    colorReset();
    readLine("Press Enter...");
}

void SimulatedEntryPanel::editEntry(size_t idx)
{
    auto& def = entries_[idx];
    printBanner("Edit: " + def.name);

    std::cout << "  Current name: " << def.name << "\n";
    std::string newName = readLine("New name (Enter to keep)");
    if (!newName.empty()) def.name = newName;

    std::cout << "\n  [1] Edit simulation parameters\n";
    std::cout << "  [2] Edit pulse/debounce\n";
    std::cout << "  [b] Done\n";

    std::string line = readLine("Select [1-2, b]");
    if (line == "1") {
        editSimParams(def);
        hasUnsavedChanges_ = true;
    } else if (line == "2") {
        if (def.channelType == "DigitalOutput") {
            def.pulseMs = static_cast<uint32_t>(
                readInt("  Pulse duration (ms)", static_cast<int>(def.pulseMs)));
            hasUnsavedChanges_ = true;
        }
        if (def.channelType == "DigitalInput" || def.channelType == "Encoder") {
            def.debounceMs = static_cast<uint32_t>(
                readInt("  Debounce (ms)", static_cast<int>(def.debounceMs)));
            hasUnsavedChanges_ = true;
        }
    }
    readLine("Press Enter...");
}

void SimulatedEntryPanel::removeEntry(size_t idx)
{
    (void)idx; // Handled in main loop
}

void SimulatedEntryPanel::editSimParams(common::config::PdoEntryDef& def)
{
    std::cout << "\n--- Simulation Parameters ---\n";

    if (def.channelType == "Encoder") {
        def.sim.rpm            = readFloat("  RPM", def.sim.rpm);
        def.sim.rollerDiamMm   = readFloat("  Roller diameter (mm)", def.sim.rollerDiamMm);
        def.sim.resolutionPpr  = static_cast<uint32_t>(
            readInt("  Resolution (PPR)", static_cast<int>(def.sim.resolutionPpr)));
        def.sim.quadrature     = readYesNo("  Quadrature encoding", def.sim.quadrature);
    }

    if (def.channelType == "DigitalInput") {
        def.sim.partsPerMin    = readFloat("  Parts per minute", def.sim.partsPerMin);
        def.sim.partWidthMm    = readFloat("  Part width (mm)", def.sim.partWidthMm);
        def.sim.variancePercent = readFloat("  Variance (%)", def.sim.variancePercent);
        def.debounceMs         = static_cast<uint32_t>(
            readInt("  Debounce (ms)", static_cast<int>(def.debounceMs)));
    }

    if (def.channelType == "DigitalOutput") {
        def.pulseMs            = static_cast<uint32_t>(
            readInt("  Pulse duration (ms)", static_cast<int>(def.pulseMs)));
    }

    if (def.channelType == "AnalogInput") {
        def.sim.variancePercent = readFloat("  Noise variance (%)", def.sim.variancePercent);
    }
}

std::string SimulatedEntryPanel::generateUuid(const std::string& channelType, int seq)
{
    // Generate a deterministic virt-* UUID based on type prefix + sequence
    std::string prefix;
    if (channelType == "DigitalInput")   prefix = "virt-di";
    else if (channelType == "DigitalOutput") prefix = "virt-do";
    else if (channelType == "AnalogInput")   prefix = "virt-ai";
    else if (channelType == "AnalogOutput")  prefix = "virt-ao";
    else if (channelType == "Encoder")       prefix = "virt-enc";
    else                                     prefix = "virt";

    // Use a simple UUID-like format: prefix-XXXX-XXXX-XXXXXXXXXXXXXXXX
    std::ostringstream oss;
    oss << prefix << "-" << std::nouppercase << std::setfill('0')
        << std::setw(4) << std::hex << seq
        << "-0000-0000000000" << std::setw(4) << seq;
    return oss.str();
}

} // namespace common::menu
