#pragma once

// ============================================================================
// ThreadBuffer.h — SPSC ring buffer with batch drain via std::span callback.
// Used by the Logger to collect LogEntry structs from multiple threads.
// ============================================================================

#include "dynamichardware/rt/VectorBuffer.h"

namespace messages {
struct LogEntry;
}

namespace common {

// Alias for the Logger's per-thread buffer.
// Capacity is a power of two (256 entries per thread).
inline constexpr std::size_t kThreadBufferCapacity = 256;

/// ThreadBuffer — SPSC ring buffer of LogEntry (trivially copyable).
using ThreadBuffer = common::rt::VectorBuffer<messages::LogEntry>;

} // namespace common
