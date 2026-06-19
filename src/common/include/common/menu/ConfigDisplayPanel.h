#pragma once
// ============================================================================
// ConfigDisplayPanel.h — display current configuration summary.
// ============================================================================

#include "common/menu/MenuPanel.h"
#include "common/config/Config.h"

#include <string>
#include <vector>

namespace common::menu {

class ConfigDisplayPanel : public MenuPanel {
public:
    ConfigDisplayPanel(const common::config::Config& cfg,
                       const std::string& configPath);
    MenuAction run() override;

private:
    void displayAll();
    void displayCategory(const std::string& category, int& idx);

    const common::config::Config& cfg_;
    std::string                   configPath_;
};

} // namespace common::menu
