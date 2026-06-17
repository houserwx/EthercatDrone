#pragma once
#include "fc/pdo/PDO.h"

#include <string>

// ============================================================
// AnalogOutputWrapper — typed read-write accessor for an analog
// output PDOEntry.
//
// Holds a stable PDOEntry& resolved once at init time via
// HardwareRegistry::lookupByUuid().  In the RT loop each accessor
// is a single struct member call — zero lookup, zero virtual dispatch.
//
// Lifetime: the referenced PDOEntry lives in the frozen PDO owned by
// a backend.  Backends outlive all wrappers.
// ============================================================

namespace fc::wrapper {

class AnalogOutputWrapper final {
public:
    AnalogOutputWrapper(std::string name, fc::pdo::PDOEntry& entry) noexcept
        : name_(std::move(name)), entry_(entry) {}

    [[nodiscard]] const std::string& getName() const noexcept { return name_; }

    // Current desired ADC value (int16_t).
    [[nodiscard]] int16_t desiredAdc() const noexcept { return entry_.getRawAdc(); }

    // Set desired ADC value.  Will be written on the next writeAll().
    void setAdc(int16_t value) noexcept { entry_.setRawAdc(value); }

private:
    std::string name_;
    fc::pdo::PDOEntry&   entry_;  // stable after HardwareRegistry::freezeForRt()
};

} // namespace fc::wrapper
