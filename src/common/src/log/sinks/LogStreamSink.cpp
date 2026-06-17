#include "common/log/sinks/LogStreamSink.h"
#include "common/messages/Message.h"

#include <iostream>

namespace common::log {

void LogStreamSink::write(std::span<const messages::LogEntry> entries)
{
    // TODO: implement gRPC streaming when available
    // For now, fall back to stderr
    for (const auto& entry : entries) {
        std::cerr << "[GRPC-LOG] " << static_cast<int>(entry.id) << "\n";
    }
}

void LogStreamSink::flush()
{
    // No-op for streaming sink
}

} // namespace common::log
