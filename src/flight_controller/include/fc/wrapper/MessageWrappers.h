#pragma once
#include "dynamichardware/pdo/PDO.h"

#include <string>

// ============================================================
// MessageWrappers — typed accessors for gRPC message channel
// PDOEntries owned by GrpcWrapper.
//
// MessageOutWrapper arms outbound messages; GrpcWrapper drains
// them in drainOutputs().
// MessageInWrapper reads inbound messages latched by GrpcWrapper
// in flushInputs().
//
// Lifetime: the referenced PDOEntry lives in the GrpcWrapper.
// GrpcWrapper outlives all wrappers.
// ============================================================

namespace fc::wrapper {

// ------------------------------------------------------------
// MessageOutWrapper
// ------------------------------------------------------------
class MessageOutWrapper final {
public:
    MessageOutWrapper(std::string name, dynamichardware::pdo::PDOEntry& entry) noexcept
        : name_(std::move(name)), entry_(entry) {}

    [[nodiscard]] const std::string& getName() const noexcept { return name_; }

    // Arm the outbound message slot.  GrpcWrapper::drainOutputs()
    // pops this once per cycle.
    template<typename T>
    void arm(const T& msg) noexcept {
        entry_.armOutMessage(msg);
    }

    // Diagnostics — is the message slot currently pending?
    [[nodiscard]] bool isArmed() const noexcept { return entry_.msgSlot_.pending; }

private:
    std::string name_;
    dynamichardware::pdo::PDOEntry& entry_;
};

// ------------------------------------------------------------
// MessageInWrapper
// ------------------------------------------------------------
class MessageInWrapper final {
public:
    MessageInWrapper(std::string name, dynamichardware::pdo::PDOEntry& entry) noexcept
        : name_(std::move(name)), entry_(entry) {}

    [[nodiscard]] const std::string& getName() const noexcept { return name_; }

    // True if a message has been latched by GrpcWrapper::flushInputs()
    // and not yet consumed.
    [[nodiscard]] bool hasPending() const noexcept { return entry_.hasInMessage(); }

    // Read the latched message (call only when hasPending() is true).
    template<typename T>
    [[nodiscard]] bool get(T& out) const noexcept {
        return entry_.tryGetInMessage(out);
    }

    // Mark the current message consumed so onBeforeReadInputs() can latch next.
    void consume() noexcept { entry_.consumeInMessage(); }

private:
    std::string name_;
    dynamichardware::pdo::PDOEntry& entry_;
};

} // namespace fc::wrapper
