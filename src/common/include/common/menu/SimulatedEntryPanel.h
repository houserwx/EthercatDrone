#pragma once
// ============================================================================
// SimulatedEntryPanel.h — add/edit/remove simulated hardware entries.
//
// Simulated entries (virt-* UUIDs) are virtual channels that can be mapped
// to wrappers.  This panel lets the user create them, set sim params,
// and delete them.  Changes are persisted to the config on save.
// ============================================================================

#include "common/menu/MenuPanel.h"
#include "common/config/Config.h"

#include <string>
#include <vector>
#include <cstddef>

namespace common::menu {

class SimulatedEntryPanel : public MenuPanel {
public:
    /// @param entries  reference to Config::pdoEntries vector (modified in place)
    /// @param entries  index offset within the full pdoEntries vector
    SimulatedEntryPanel(std::vector<common::config::PdoEntryDef>& entries);
    MenuAction run() override;

    /// Returns true if the user saved changes (requires restart notice).
    [[nodiscard]] bool hasUnsavedChanges() const noexcept { return hasUnsavedChanges_; }

private:
    void listEntries();
    void addEntry();
    void editEntry(size_t idx);
    void removeEntry(size_t idx);
    void editSimParams(common::config::PdoEntryDef& def);

    std::string generateUuid(const std::string& channelType, int seq);

    std::vector<common::config::PdoEntryDef>& entries_;
    bool hasUnsavedChanges_{false};
};

} // namespace common::menu
