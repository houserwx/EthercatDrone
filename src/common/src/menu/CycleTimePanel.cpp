#include "common/menu/CycleTimePanel.h"
#include "common/menu/Console.h"

#include <iostream>

namespace common::menu {

CycleTimePanel::CycleTimePanel(int& cycleTimeUs)
    : cycleTimeUsRef_(cycleTimeUs), previousValue_(cycleTimeUs)
{
    setTitle("Cycle Time Configuration");
}

MenuAction CycleTimePanel::run()
{
    for (;;) {
        printBanner(title_);

        std::cout << "  Current cycle time: " << cycleTimeUsRef_ << " us\n";
        std::cout << "  (Previous: " << previousValue_ << " us)\n\n";

        if (hasUnsavedChanges_) {
            colorYellow();
            std::cout << "  ** Unsaved changes — save to persist **\n\n";
            colorReset();
        }

        std::cout << "  Recommended values:\n";
        std::cout << "    500   us — high-performance RT (requires SCHED_FIFO)\n";
        std::cout << "    1000  us — standard cycle (default)\n";
        std::cout << "    2000  us — relaxed cycle (less RT pressure)\n";
        std::cout << "    5000  us — slow cycle (development/testing)\n\n";

        colorBold();
        std::cout << "  [1] Set new cycle time\n";
        std::cout << "  [2] Quick-select: 500 us\n";
        std::cout << "  [3] Quick-select: 1000 us\n";
        std::cout << "  [4] Quick-select: 2000 us\n";
        std::cout << "  [5] Quick-select: 5000 us\n";
        if (hasUnsavedChanges_) {
            std::cout << "  [s] Save changes\n";
        }
        colorReset();
        std::cout << "  [b] Back\n";
        std::cout << "  [q] Quit\n";

        printFooter();

        std::string line = readLine("Select [1-5, s, b, q]");
        if (line == "b" || line == "B") return MenuAction::Back;
        if (line == "q" || line == "Q") return MenuAction::Quit;

        if (line == "s" || line == "S") {
            if (hasUnsavedChanges_) {
                colorGreen();
                std::cout << "  Cycle time saved: " << cycleTimeUsRef_ << " us\n";
                colorReset();
                hasUnsavedChanges_ = false;
                readLine("Press Enter...");
            }
            continue;
        }

        try {
            int sel = std::stoi(line);
            switch (sel) {
                case 1: {
                    int val = readInt("  Enter cycle time (us)", cycleTimeUsRef_);
                    if (val < 100) {
                        colorRed();
                        std::cout << "  Warning: cycle time < 100us may not be achievable.\n";
                        colorReset();
                    }
                    if (val > 100000) {
                        colorRed();
                        std::cout << "  Warning: cycle time > 100ms is unusually slow.\n";
                        colorReset();
                    }
                    cycleTimeUsRef_ = val;
                    hasUnsavedChanges_ = true;
                    break;
                }
                case 2: cycleTimeUsRef_ = 500;  hasUnsavedChanges_ = true; break;
                case 3: cycleTimeUsRef_ = 1000; hasUnsavedChanges_ = true; break;
                case 4: cycleTimeUsRef_ = 2000; hasUnsavedChanges_ = true; break;
                case 5: cycleTimeUsRef_ = 5000; hasUnsavedChanges_ = true; break;
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

} // namespace common::menu
