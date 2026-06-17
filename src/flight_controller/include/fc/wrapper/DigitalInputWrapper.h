#pragma once
#include "fc/pdo/PDO.h"

#include <string>

// ============================================================
// DigitalInputWrapper — typed read-only accessor for a boolean
// digital input PDOEntry.
//
// Holds a stable PDOEntry& resolved once at init time via
// HardwareRegistry::lookupByUuid().  In the RT loop each accessor
// is a single struct member read — zero lookup, zero virtual dispatch.
//
// Lifetime: the referenced PDOEntry lives in the frozen PDO owned by
// a backend.  Backends outlive all wrappers.
// ============================================================

namespace fc::wrapper {

class DigitalInputWrapper final {
public:
    DigitalInputWrapper(std::string name, fc::pdo::PDOEntry& entry) noexcept
        : name_(std::move(name)), entry_(entry) {}

    [[nodiscard]] const std::string& getName() const noexcept { return name_; }

    // True if the input is HIGH (debounce settled) this cycle.
    [[nodiscard]] bool isActive() const noexcept { return entry_.getBool(); }

private:
    std::string name_;
    fc::pdo::PDOEntry&   entry_;  // stable after HardwareRegistry::freezeForRt()
};

} // namespace fc::wrapper
