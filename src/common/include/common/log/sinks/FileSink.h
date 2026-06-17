#pragma once

#include "common/log/ILogSink.h"

#include <string>

namespace common::log {

/// FileSink — rotating file output with size-based rotation.
class FileSink : public ILogSink {
public:
    explicit FileSink(std::string path, std::size_t maxBytes = 10 * 1024 * 1024);

    void write(std::span<const messages::LogEntry> entries) override;
    void flush() override;

private:
    std::string path_;
    std::size_t maxBytes_;
    std::size_t currentSize_{0};
    void rotate();
};

} // namespace common::log
