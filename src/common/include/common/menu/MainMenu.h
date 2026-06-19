#pragma once
// ============================================================================
// MainMenu.h — top-level menu orchestrator.
//
// Coordinates all sub-panels, handles save/restart workflow, and provides
// a clean entry point: MainMenu::run(config) blocks until the user quits.
//
// Workflow:
//   1. Show menu, user navigates panels.
//   2. Panels modify Config in-place.
//   3. User selects "Save & Restart" → writes config to disk, returns true.
//   4. Caller (drone_app) detects return, re-initializes context, starts RT.
// ============================================================================

#include "common/menu/MenuPanel.h"
#include "common/menu/ConfigDisplayPanel.h"
#include "common/menu/SimulatedEntryPanel.h"
#include "common/menu/PdoMappingPanel.h"
#include "common/menu/CycleTimePanel.h"
#include "common/config/Config.h"

#include <string>

namespace common::menu {

class MainMenu : public MenuPanel {
public:
    /// @param cfg        config to edit (modified in place)
    /// @param configPath path to persist config to on save
    MainMenu(common::config::Config& cfg, const std::string& configPath);
    MenuAction run() override;

    /// Returns true if config was saved and a restart is required.
    [[nodiscard]] bool shouldSaveAndRestart() const noexcept { return shouldSaveAndRestart_; }

private:
    void runPanel(std::unique_ptr<MenuPanel> panel);

    common::config::Config& cfg_;
    std::string             configPath_;
    bool                    shouldSaveAndRestart_{false};
};

} // namespace common::menu
