#pragma once
#include "fc/pdo/PDO.h"

#include <string>

// ============================================================
// MessageWrappers — typed accessors for gRPC message channel
// PDOEntries owned by GrpcAdapter.
//
// MessageOutWrapper arms outbound messages; GrpcAdapter flushes
// them in onAfterWriteOutputs().
// MessageInWrapper reads inbound messages latched by GrpcAdapter
// in onBeforeReadInputs().
//
// Lifetime: the referenced PDOEntry lives in the frozen PDO owned by
// GrpcAdapter.  Backends outlive all wrappers.
// ============================================================

namespace fc::wrapper {

// ------------------------------------------------------------
// MessageOutWrapper
// ------------------------------------------------------------
class MessageOutWrapper final {
public:
    MessageOutWrapper(std::string name, fc::pdo::PDOEntry& entry) noexcept
        : name_(std::move(name)), entry_(entry) {}

    [[nodiscard]] const std::string& getName() const noexcept { return name_; }

    // Arm the outbound message slot.  GrpcAdapter::onAfterWriteOutputs()
    // pops this once per cycle.
    template<typename T>
    void arm(const T& msg) noexcept {
        entry_.armOutMessage(msg);
    }

    // Diagnostics — is the message slot currently pending?
    [[nodiscard]] bool isArmed() const noexcept { return entry_.msgSlot_.pending; }

private:
    std::string name_;
    fc::pdo::PDOEntry&   entry_;
};

// ------------------------------------------------------------
// MessageInWrapper
// ------------------------------------------------------------
class MessageInWrapper final {
public:
    MessageInWrapper(std::string name, fc::pdo::PDOEntry& entry) noexcept
        : name_(std::move(name)), entry_(entry) {}

    [[nodiscard]] const std::string& getName() const noexcept { return name_; }

    // True if a message has been latched by GrpcAdapter::onBeforeReadInputs()
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
    fc::pdo::PDOEntry&   entry_;
};

} // namespace fc::wrapper
