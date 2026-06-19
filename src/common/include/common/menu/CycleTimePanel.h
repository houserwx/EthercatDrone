#pragma once
// ============================================================================
// CycleTimePanel.h — update the RT loop cycle time.
// ============================================================================

#include "common/menu/MenuPanel.h"

namespace common::menu {

class CycleTimePanel : public MenuPanel {
public:
    /// @param cycleTimeUs  reference to Config::cycleTimeUs (modified in place)
    explicit CycleTimePanel(int& cycleTimeUs);
    MenuAction run() override;

    [[nodiscard]] bool hasUnsavedChanges() const noexcept { return hasUnsavedChanges_; }

private:
    int&  cycleTimeUsRef_;
    int   previousValue_;
    bool  hasUnsavedChanges_{false};
};

} // namespace common::menu
