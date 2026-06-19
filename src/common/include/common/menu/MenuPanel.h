#pragma once
// ============================================================================
// MenuPanel.h — abstract base for all menu panels.
//
// Each panel renders itself and handles its own input loop.
// Returns a MenuAction to signal the parent (MainMenu) what to do next.
// ============================================================================

#include <string>
#include <vector>
#include <functional>

namespace common::menu {

// Action returned by a panel to its parent
enum class MenuAction {
    Back,           // Go back to parent menu
    Quit,           // Exit the menu system (save and restart)
    SaveAndRestart, // Save config, exit menu, restart RT loop
    NoOp            // Continue in same panel
};

// ---------------------------------------------------------------------------
// MenuPanel — abstract interface every panel must implement
// ---------------------------------------------------------------------------
class MenuPanel {
public:
    MenuPanel() = default;
    virtual ~MenuPanel() = default;

    MenuPanel(const MenuPanel&)            = delete;
    MenuPanel& operator=(const MenuPanel&) = delete;

    /// Display this panel and process user input until a navigation action.
    /// @return MenuAction indicating next step
    virtual MenuAction run() = 0;

    /// Override the panel title (default is set in derived class).
    void setTitle(std::string title) { title_ = std::move(title); }
    [[nodiscard]] const std::string& title() const noexcept { return title_; }

protected:
    std::string title_;
};

} // namespace common::menu
