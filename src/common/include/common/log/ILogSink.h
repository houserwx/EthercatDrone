#pragma once

// ============================================================================
// ILogSink.h — Abstract output sink interface for the Logger.
// ============================================================================

#include <span>

namespace messages {
struct LogEntry;
}

namespace common::log {

class ILogSink {
public:
    virtual ~ILogSink() = default;

    /// Called by the Logger service thread with a batch of log entries.
    virtual void write(std::span<const messages::LogEntry> entries) = 0;

    /// Flush any buffered output.  Called during Logger::stop().
    virtual void flush() = 0;
};

} // namespace common::log
