#pragma once

#include "common/log/ILogSink.h"

namespace common::log {

/// ConsoleSink — formatted stdout output with timestamp, level, message.
class ConsoleSink : public ILogSink {
public:
    void write(std::span<const messages::LogEntry> entries) override;
    void flush() override;
};

} // namespace common::log
