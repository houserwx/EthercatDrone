#include "common/log/sinks/ConsoleSink.h"
#include "common/messages/Message.h"

#include <cstdio>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace common::log {

void ConsoleSink::write(std::span<const messages::LogEntry> entries)
{
    for (const auto& entry : entries) {
        char levelChar = '?';
        switch (entry.level) {
            case messages::LogLevel::Debug:    levelChar = 'D'; break;
            case messages::LogLevel::Info:     levelChar = 'I'; break;
            case messages::LogLevel::Warning:  levelChar = 'W'; break;
            case messages::LogLevel::Error:    levelChar = 'E'; break;
            case messages::LogLevel::Critical: levelChar = 'C'; break;
        }

        // Format timestamp from nanoseconds
        const uint64_t sec  = entry.timestampNs / 1'000'000'000ULL;
        const uint64_t nsec = entry.timestampNs % 1'000'000'000ULL;

        std::time_t t = static_cast<std::time_t>(sec);
        struct tm tmBuf;
        localtime_r(&t, &tmBuf);

        char timeStr[32];
        std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &tmBuf);

        std::string_view label = messages::messageIdLabel(entry.id);

        std::printf("[%s.%03lld] [%c] [%s] %s\n",
                    timeStr,
                    static_cast<long long>(nsec / 1'000'000),
                    levelChar,
                    std::to_string(static_cast<uint16_t>(entry.id)).c_str(),
                    label.data());
    }
}

void ConsoleSink::flush()
{
    std::fflush(stdout);
}

} // namespace common::log
