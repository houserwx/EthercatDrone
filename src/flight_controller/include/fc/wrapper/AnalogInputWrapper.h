#pragma once
#include "dynamichardware/pdo/PDO.h"

#include <string>

// ============================================================
// AnalogInputWrapper — typed read-only accessor for an analog
// input PDOEntry.
//
// Holds a stable PDOEntry& resolved once at init time via
// DynamicHardwareContext::lookupByUuid().  In the RT loop each
// accessor is a single struct member read — zero lookup,
// zero virtual dispatch.
//
// Lifetime: the referenced PDOEntry lives in the frozen PDO owned
// by a backend.  Backends outlive all wrappers.
// ============================================================

namespace fc::wrapper {

class AnalogInputWrapper final {
public:
    AnalogInputWrapper(std::string name, dynamichardware::pdo::PDOEntry& entry) noexcept
        : name_(std::move(name)), entry_(entry) {}

    [[nodiscard]] const std::string& getName() const noexcept { return name_; }

    // Raw analog value (int16 for integer types, otherwise getInt16).
    [[nodiscard]] int16_t value() const noexcept { return entry_.getInt16(); }

private:
    std::string name_;
    dynamichardware::pdo::PDOEntry& entry_;  // stable after freeze
};

} // namespace fc::wrapper
