#include "common/log/Logger.h"
#include "common/messages/Message.h"
#include "common/rt/ThreadBuffer.h"
#include "common/rt/SignalProcess.h"

#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;

namespace common::log {

Logger& Logger::instance() noexcept
{
    static Logger inst;
    return inst;
}

Logger::Logger() = default;
Logger::~Logger() { stop(); }

void Logger::init(const LoggerConfiguration& config)
{
    config_ = config;
}

void Logger::addSink(ILogSink* sink)
{
    if (!sink) return;
    std::lock_guard<std::mutex> lock(registrationMutex_);
    if (sinkCount_ < kMaxSinks) {
        sinks_[sinkCount_++] = sink;
    }
}

void Logger::start()
{
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true,
                                          std::memory_order_acq_rel)) {
        return;
    }
    serviceThread_ = std::thread(&Logger::serviceLoop, this);
}

void Logger::stop()
{
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false,
                                          std::memory_order_acq_rel)) {
        return;
    }
    if (serviceThread_.joinable()) {
        serviceThread_.join();
    }
    drainAll();
    for (std::size_t i = 0; i < sinkCount_; ++i) {
        if (sinks_[i]) sinks_[i]->flush();
    }
}

common::ThreadBuffer* Logger::registerThread(bool isRt)
{
    std::lock_guard<std::mutex> lock(registrationMutex_);
    if (registeredCount_ >= kMaxThreads) {
        std::cerr << "[Logger] ERROR: maximum thread registrations exceeded ("
                  << kMaxThreads << ")\n";
        return nullptr;
    }
    ThreadInfo& info = threadBuffers_[registeredCount_];
    info.buffer = std::make_unique<common::ThreadBuffer>(common::kThreadBufferCapacity);
    info.priority = isRt ? Priority::RealTime : Priority::Normal;
    ++registeredCount_;
    return info.buffer.get();
}

void Logger::serviceLoop()
{
    while (running_.load(std::memory_order_relaxed)) {
        drainAll();
        std::this_thread::sleep_for(10ms);
    }
}

void Logger::drainAll()
{
    for (std::size_t i = 0; i < registeredCount_; ++i) {
        if (threadBuffers_[i].priority == Priority::RealTime && threadBuffers_[i].buffer) {
            processBuffer(*threadBuffers_[i].buffer);
        }
    }
    for (std::size_t i = 0; i < registeredCount_; ++i) {
        if (threadBuffers_[i].priority == Priority::Normal && threadBuffers_[i].buffer) {
            processBuffer(*threadBuffers_[i].buffer);
        }
    }
}

void Logger::processBuffer(common::ThreadBuffer& buf) const
{
    buf.drain([this](std::span<const messages::LogEntry> batch) {
        for (const auto& entry : batch) {
            if (entry.level >= config_.minLevel) {
                formatAndOutput(entry);
            }
        }
    });
}

void Logger::formatAndOutput(const messages::LogEntry& entry) const
{
    for (std::size_t i = 0; i < sinkCount_; ++i) {
        if (sinks_[i]) {
            sinks_[i]->write(std::span<const messages::LogEntry>(&entry, 1));
        }
    }
}

// Thread-local pointer to this thread's SPSC buffer.
// Set by threadLoggerInit(), read by log().
static thread_local common::ThreadBuffer* gThreadBuffer = nullptr;

bool threadLoggerInit(bool isRt)
{
    common::ThreadBuffer* buf = Logger::instance().registerThread(isRt);
    if (buf) {
        gThreadBuffer = buf;
    }
    return buf != nullptr;
}

void log(messages::LogLevel level, messages::MessageId id,
         int64_t p1, int64_t p2, int64_t p3) noexcept
{
    common::ThreadBuffer* buf = gThreadBuffer;
    if (!buf) return;  // Thread not registered — drop entry

    messages::LogEntry entry;
    entry.level = level;
    entry.id = id;
    entry.timestampNs = common::rt::signalProcessNowNs();
    entry.p1 = p1;
    entry.p2 = p2;
    entry.p3 = p3;

    buf->tryPush(entry);  // Lock-free, noexcept, drops on overflow
}

} // namespace common::log
