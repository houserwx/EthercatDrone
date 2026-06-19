#pragma once
#include "dynamichardware/pdo/PDO.h"

#include <string>

// ============================================================
// AnalogOutputWrapper — typed read-write accessor for an analog
// output PDOEntry.
//
// Holds a stable PDOEntry& resolved once at init time via
// DynamicHardwareContext::lookupByUuid().  In the RT loop each
// accessor is a single struct member call — zero lookup,
// zero virtual dispatch.
//
// Lifetime: the referenced PDOEntry lives in the frozen PDO owned
// by a backend.  Backends outlive all wrappers.
// ============================================================

namespace fc::wrapper {

class AnalogOutputWrapper final {
public:
    AnalogOutputWrapper(std::string name, dynamichardware::pdo::PDOEntry& entry) noexcept
        : name_(std::move(name)), entry_(entry) {}

    [[nodiscard]] const std::string& getName() const noexcept { return name_; }

    // Current desired value (int16_t).
    [[nodiscard]] int16_t value() const noexcept { return entry_.getInt16(); }

    // Set desired value.  Will be written on the next writeAll().
    void setValue(int16_t value) noexcept { entry_.setInt16(value); }

private:
    std::string name_;
    dynamichardware::pdo::PDOEntry& entry_;  // stable after freeze
};

} // namespace fc::wrapper
