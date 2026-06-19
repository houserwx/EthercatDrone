#pragma once

// ============================================================================
// Logger.h — Singleton logger with multiple sinks and per-thread SPSC buffers.
//
// Thread model:
//   - Each thread registers once via registerThread() → gets a ThreadBuffer*.
//   - RT threads push LogEntry into their ThreadBuffer (lock-free, noexcept).
//   - A dedicated service thread drains all buffers and fans out to sinks.
//   - RT-priority buffers are drained first to minimize latency.
//
// Lifecycle:
//   1. Logger::instance().init(config)
//   2. Logger::instance().addSink(&sink) × N
//   3. Logger::instance().start()
//   4. Threads call registerThread(isRt) → get ThreadBuffer*
//   5. Logger::instance().stop()
// ============================================================================

#include "common/log/ILogSink.h"
#include "common/messages/MessageTypes.h"
#include "common/rt/ThreadBuffer.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <span>
#include <thread>
#include <vector>

namespace messages {
struct LogEntry;
}

namespace common::log {

// ---------------------------------------------------------------------------
// LoggerConfiguration
// ---------------------------------------------------------------------------
struct LoggerConfiguration {
    messages::LogLevel minLevel = messages::LogLevel::Info;
};

// ---------------------------------------------------------------------------
// Logger — singleton
// ---------------------------------------------------------------------------
class Logger {
public:
    static constexpr std::size_t kMaxThreads = 16;
    static constexpr std::size_t kMaxSinks   = 8;

    static Logger& instance() noexcept;

    Logger(const Logger&)            = delete;
    Logger& operator=(const Logger&) = delete;

    void init(const LoggerConfiguration& config);
    void addSink(ILogSink* sink);
    void start();
    void stop();

    /// Temporarily pause the service thread (e.g. during interactive TTY input).
    /// Blocks until the service thread has actually stopped draining.
    /// Buffered entries are preserved — will be drained after resume.
    void pause();
    /// Resume a previously paused service thread. No-op if not started.
    void resume();

    /// Register a thread and get its SPSC buffer.  Safe from any thread.
    /// Returns nullptr on overflow.
    common::ThreadBuffer* registerThread(bool isRt);

private:
    Logger();
    ~Logger();

    void serviceLoop();
    void drainAll();
    void processBuffer(common::ThreadBuffer& buf) const;
    void formatAndOutput(const messages::LogEntry& entry) const;

    enum class Priority : uint8_t { Normal, RealTime };

    struct ThreadInfo {
        std::unique_ptr<common::ThreadBuffer> buffer;
        Priority priority = Priority::Normal;
    };

    LoggerConfiguration config_;
    std::atomic<bool>   running_{false};
    std::atomic<bool>   paused_{false};
    std::atomic<bool>   pausedAcknowledged_{false};
    std::thread         serviceThread_;

    std::mutex           pauseMutex_;
    std::condition_variable pauseCv_;

    std::mutex              registrationMutex_;
    std::vector<ThreadInfo> threadBuffers_;
    std::size_t             registeredCount_{0};

    ILogSink*   sinks_[kMaxSinks]{};
    std::size_t sinkCount_{0};
};

} // namespace common::log
