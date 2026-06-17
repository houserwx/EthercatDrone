#pragma once
#include "fc/pdo/PDO.h"

#include <string>

// ============================================================
// AnalogInputWrapper — typed read-only accessor for an analog
// input PDOEntry.
//
// Holds a stable PDOEntry& resolved once at init time via
// HardwareRegistry::lookupByUuid().  In the RT loop each accessor
// is a single struct member read — zero lookup, zero virtual dispatch.
//
// Lifetime: the referenced PDOEntry lives in the frozen PDO owned by
// a backend.  Backends outlive all wrappers.
// ============================================================

namespace fc::wrapper {

class AnalogInputWrapper final {
public:
    AnalogInputWrapper(std::string name, PDOEntry& entry) noexcept
        : name_(std::move(name)), entry_(entry) {}

    [[nodiscard]] const std::string& getName() const noexcept { return name_; }

    // Raw ADC value (int16_t) from the most recent readAll().
    [[nodiscard]] int16_t rawAdc() const noexcept { return entry_.getRawAdc(); }

private:
    std::string name_;
    PDOEntry&   entry_;  // stable after HardwareRegistry::freezeForRt()
};

} // namespace fc::wrapper
