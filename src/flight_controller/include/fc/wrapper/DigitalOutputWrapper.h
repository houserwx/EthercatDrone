#pragma once
#include "dynamichardware/pdo/PDO.h"

#include <string>

// ============================================================
// DigitalOutputWrapper — typed read-write accessor for a boolean
// digital output PDOEntry.
//
// Holds a stable PDOEntry& resolved once at init time via
// DynamicHardwareContext::lookupByUuid().  In the RT loop each
// accessor is a single struct member call — zero lookup,
// zero virtual dispatch.
// Pulse timing logic lives inside the PDOEntry (PulseMachine).
//
// Lifetime: the referenced PDOEntry lives in the frozen PDO owned
// by a backend.  Backends outlive all wrappers.
// ============================================================

namespace fc::wrapper {

class DigitalOutputWrapper final {
public:
    DigitalOutputWrapper(std::string name, dynamichardware::pdo::PDOEntry& entry) noexcept
        : name_(std::move(name)), entry_(entry) {}

    [[nodiscard]] const std::string& getName() const noexcept { return name_; }

    // True if the output is currently active (or mid-pulse).
    [[nodiscard]] bool isActive() const noexcept { return entry_.getBool(); }

    // Arm the output HIGH.  PulseMachine inside the PDOEntry handles
    // the timed auto-reset on the next writeAll() if pulseMs was configured.
    void setActive(bool state) noexcept { entry_.setBool(state); }

private:
    std::string name_;
    dynamichardware::pdo::PDOEntry& entry_;  // stable after freeze
};

} // namespace fc::wrapper
