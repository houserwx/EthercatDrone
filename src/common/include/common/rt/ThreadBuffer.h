#pragma once

// ============================================================================
// ThreadBuffer.h — SPSC ring buffer with batch drain via std::span callback.
// Used by the Logger to collect LogEntry structs from multiple threads.
// ============================================================================

#include "dynamichardware/rt/VectorBuffer.h"
#include "dynamichardware/rt/SignalProcess.h"

namespace messages {
struct LogEntry;
}

namespace common::rt {

// Re-export dynamichardware::rt symbols under common::rt for backward compatibility.
template<typename T>
using VectorBuffer = dynamichardware::rt::VectorBuffer<T>;
using PulseMachine = dynamichardware::rt::PulseMachine;
using DebounceMachine = dynamichardware::rt::DebounceMachine;
using dynamichardware::rt::signalProcessTickNow;
using dynamichardware::rt::signalProcessNowNs;
using dynamichardware::rt::gSignalProcessNowNs;

} // namespace common::rt

namespace common {

// Alias for the Logger's per-thread buffer.
// Capacity is a power of two (256 entries per thread).
inline constexpr std::size_t kThreadBufferCapacity = 256;

/// ThreadBuffer — SPSC ring buffer of LogEntry (trivially copyable).
using ThreadBuffer = dynamichardware::rt::VectorBuffer<messages::LogEntry>;

} // namespace common
