#pragma once

// ============================================================================
// LogHelper.h — Free functions for logging from any thread.
//
// Usage:
//   // Register this thread once (during init):
//   threadLoggerInit(false);  // non-RT
//   threadLoggerInit(true);   // RT thread
//
//   // Log messages:
//   logInfo(MessageId::SYSTEM_INIT_COMPLETE);
//   logWarn(MessageId::RT_OVERRUN_DETECTED, overrunNs);
//   logError(MessageId::MAIN_CONFIG_ERROR);
// ============================================================================

#include "common/messages/MessageTypes.h"

namespace common::log {

/// Register the calling thread with the Logger.
/// @param isRt  true for RT-priority buffer (drained first).
/// Returns true on success, false if already registered or overflow.
bool threadLoggerInit(bool isRt);

/// Push a LogEntry into the calling thread's buffer.
/// RT-safe: noexcept, lock-free, no allocation.
void log(messages::LogLevel level, messages::MessageId id,
         int64_t p1 = 0, int64_t p2 = 0, int64_t p3 = 0) noexcept;

/// Convenience wrappers
inline void logInfo(messages::MessageId id,
                    int64_t p1 = 0, int64_t p2 = 0, int64_t p3 = 0) noexcept {
    log(messages::LogLevel::Info, id, p1, p2, p3);
}

inline void logWarn(messages::MessageId id,
                    int64_t p1 = 0, int64_t p2 = 0, int64_t p3 = 0) noexcept {
    log(messages::LogLevel::Warning, id, p1, p2, p3);
}

inline void logError(messages::MessageId id,
                     int64_t p1 = 0, int64_t p2 = 0, int64_t p3 = 0) noexcept {
    log(messages::LogLevel::Error, id, p1, p2, p3);
}

} // namespace common::log
