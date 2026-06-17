#include "common/log/sinks/FileSink.h"
#include "common/messages/Message.h"

#include <cstdio>
#include <filesystem>

namespace common::log {

FileSink::FileSink(std::string path, std::size_t maxBytes)
    : path_(std::move(path))
    , maxBytes_(maxBytes)
{
}

void FileSink::write(std::span<const messages::LogEntry> entries)
{
    if (currentSize_ >= maxBytes_) {
        rotate();
    }

    FILE* f = std::fopen(path_.c_str(), "a");
    if (!f) return;

    for (const auto& entry : entries) {
        std::string line = messages::formatLogEntry(entry);
        currentSize_ += static_cast<std::size_t>(std::fprintf(f, "%s\n", line.c_str()));
    }

    std::fclose(f);
}

void FileSink::flush()
{
    // File is flushed on each write (fclose).
}

void FileSink::rotate()
{
    // Simple rotation: shift existing file, start fresh
    std::string backup = path_ + ".1";
    std::filesystem::rename(path_, backup);
    currentSize_ = 0;
}

} // namespace common::log
