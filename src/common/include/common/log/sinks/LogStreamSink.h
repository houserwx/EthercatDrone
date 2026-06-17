#pragma once

#include "common/log/ILogSink.h"

namespace common::log {

/// LogStreamSink — pushes log entries to a gRPC server-streaming RPC.
/// Compiled only when GRPC_AVAILABLE is defined.
class LogStreamSink : public ILogSink {
public:
    void write(std::span<const messages::LogEntry> entries) override;
    void flush() override;
};

} // namespace common::log
