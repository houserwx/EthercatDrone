#pragma once
#include "dynamichardware/pdo/PDO.h"
#include "fc/grpc/GrpcTriggerMessage.h"
#include "fc/grpc/GrpcResultMessage.h"
#include "common/rt/ThreadBuffer.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace fc::grpc {

// ============================================================================
// GrpcWrapper — application-layer message channel manager for gRPC relay.
//
// Owns a set of message channel pairs (out/in PDOEntries) and the associated
// SPSC queues for inter-thread communication with the gRPC service thread.
//
// This is an application concept — NOT a hardware backend.
// PDOEntries are the hardware/lib transport; the wrapper manages the
// application-side queue semantics and channel lifecycle.
//
// RT cycle:
//   drainOutputs()  — Read armed outbound messages from PDOEntries, push to
//                     triggerOut queues (gRPC service thread consumes these).
//   flushInputs()   — Pop results from resultIn queues, write to PDOEntry
//                     inbound message slots.
//
// Data flow:
//   RT thread:  MessageOutWrapper::arm() → PDOEntry.msgSlot
//   RT thread:  GrpcWrapper::drainOutputs() → triggerOut queue
//   gRPC thread: triggerOut queue → gRPC response → resultIn queue
//   RT thread:  GrpcWrapper::flushInputs() → PDOEntry.msgSlot
//   RT thread:  MessageInWrapper::get() → application logic
// ============================================================================

class GrpcWrapper final {
public:
    static constexpr std::size_t kQueueCapacity = 256;

    explicit GrpcWrapper(std::string name);

    // Init-time (non-RT)
    void reserve(std::size_t n);
    [[nodiscard]] int addChannel(std::string name, uint32_t simFailMod = 0);

    // RT-time accessors (noexcept, safe for concurrent drain/fill)
    [[nodiscard]] dynamichardware::pdo::PDOEntry& outEntry(int ch) noexcept;
    [[nodiscard]] dynamichardware::pdo::PDOEntry& inEntry(int ch) noexcept;

    [[nodiscard]] std::size_t channelCount() const noexcept { return channels_.size(); }

    // RT cycle hooks (call explicitly from Application::rtCycle)
    // drainOutputs: after ctx->writeAll(), reads armed messages from PDOs
    void drainOutputs() noexcept;

    // flushInputs: before ctx->readAll(), writes results into PDOs
    void flushInputs() noexcept;

    // Queue access (gRPC service thread uses these)
    [[nodiscard]] common::rt::VectorBuffer<GrpcTriggerMessage>&
        triggerOut(int ch) noexcept { return channels_[static_cast<std::size_t>(ch)]->triggerOut; }

    [[nodiscard]] common::rt::VectorBuffer<GrpcResultMessage>&
        resultIn(int ch) noexcept { return channels_[static_cast<std::size_t>(ch)]->resultIn; }

private:
    struct Channel {
        common::rt::VectorBuffer<GrpcTriggerMessage> triggerOut{kQueueCapacity};
        common::rt::VectorBuffer<GrpcResultMessage>  resultIn{kQueueCapacity};
        std::string  name;
        uint32_t     simFailMod{0};
        uint64_t     simSeq{0};
    };

    std::string                        name_;
    std::vector<std::unique_ptr<Channel>> channels_;

    // PDOEntry storage: outEntry at [i*2], inEntry at [i*2+1]
    // These are standalone entries (not part of a hardware PDO image).
    // Message type entries don't need process image memory.
    std::vector<dynamichardware::pdo::PDOEntry> entries_;
};

} // namespace fc::grpc
