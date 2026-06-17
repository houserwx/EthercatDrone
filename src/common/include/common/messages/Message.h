#pragma once

#include "common/messages/MessageTypes.h"

#include <cstdint>
#include <string_view>

namespace messages {

// ---------------------------------------------------------------------------
// LogEntry — compact binary log entry (trivially copyable).
// Carried in VectorBuffer SPSC; service thread formats and outputs.
// ---------------------------------------------------------------------------
struct LogEntry {
    LogLevel   level;
    MessageId  id;
    uint64_t   timestampNs;
    int64_t    p1{0};
    int64_t    p2{0};
    int64_t    p3{0};
};

/// Format a LogEntry into a human-readable string.
std::string formatLogEntry(const LogEntry& entry);

/// Get a static string label for a MessageId.
std::string_view messageIdLabel(MessageId id);

} // namespace messages
