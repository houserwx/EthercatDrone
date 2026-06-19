#pragma once
// ============================================================================
// PdoMappingPanel.h — configure which PDO entries map to which wrappers.
//
// Wrappers are the application-layer abstraction (DigitalInputWrapper,
// AnalogOutputWrapper, etc.).  This panel lets the user assign a PDO entry
// UUID to a named wrapper slot, or clear an assignment.
//
// The mapping is stored as a name→uuid association within the PdoEntryDef
// (the "name" field is the wrapper name, hwUuid is the resolved UUID).
// ============================================================================

#include "common/menu/MenuPanel.h"
#include "common/config/Config.h"

#include <string>
#include <vector>
#include <cstddef>

namespace common::menu {

class PdoMappingPanel : public MenuPanel {
public:
    /// @param entries  reference to Config::pdoEntries (modified in place)
    PdoMappingPanel(std::vector<common::config::PdoEntryDef>& entries);
    MenuAction run() override;

    [[nodiscard]] bool hasUnsavedChanges() const noexcept { return hasUnsavedChanges_; }

private:
    void listMappings();
    void addMapping();
    void editMapping(size_t idx);
    void removeMapping(size_t idx);

    /// List all available catalog entries (from discovered hardware + simulated)
    /// that are not yet mapped, so user can pick one.
    void showAvailableEntries();

    std::vector<common::config::PdoEntryDef>& entries_;
    bool hasUnsavedChanges_{false};
};

} // namespace common::menu
