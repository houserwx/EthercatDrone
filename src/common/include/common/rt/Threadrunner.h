#pragma once

// ============================================================================
// Threadrunner.h — Base class for all managed threads.
//
// Responsibilities:
//   - CPU core pinning (pthread_setaffinity_np)
//   - Optional SCHED_FIFO real-time scheduling (pthread_setschedparam)
//   - Stack pre-faulting (mlockall must already have been called by main)
//
// Subclasses implement run(), which is called once after the thread is
// configured.  run() should loop until the thread's own stop condition is met.
//
// Thread safety:
//   - start()  may only be called once; subsequent calls are no-ops.
//   - join()   is safe to call multiple times.
//   - run()    is called exactly once, on the new thread.
//   - The destructor joins the thread if it is still running.
// ============================================================================

#include <atomic>
#include <cstddef>
#include <string>
#include <thread>

namespace common::rt {

// ---------------------------------------------------------------------------
// ThreadConfiguration — passed to the constructor; copied into the object.
// ---------------------------------------------------------------------------
struct ThreadConfiguration {
    std::string name            = "Thread";
    int         cpuCore         = -1;      // -1 = no pinning
    int         priority        = 0;       // 0 = no RT; 1-99 = SCHED_FIFO
    bool        useRealtime     = false;   // enables SCHED_FIFO when true
    std::size_t stackPrefaultBytes = 0;    // 0 = no prefault
};

// ---------------------------------------------------------------------------
// Threadrunner — non-copyable, non-movable base class.
// ---------------------------------------------------------------------------
class Threadrunner {
public:
    explicit Threadrunner(ThreadConfiguration config);

    virtual ~Threadrunner();

    Threadrunner(const Threadrunner&)            = delete;
    Threadrunner& operator=(const Threadrunner&) = delete;
    Threadrunner(Threadrunner&&)                 = delete;
    Threadrunner& operator=(Threadrunner&&)      = delete;

    /// Spawn the OS thread.  Idempotent (safe to call twice).
    void start();

    /// Block until the thread exits.  Idempotent.
    void join();

    /// True if the thread has been started at least once.
    [[nodiscard]] bool isStarted() const noexcept
    {
        return started_.load(std::memory_order_acquire);
    }

    [[nodiscard]] const ThreadConfiguration& config() const noexcept { return config_; }

    // -----------------------------------------------------------------------
    // Pure virtual — subclasses implement their thread loop here.
    // Called after RT configuration and logger registration.
    // -----------------------------------------------------------------------
    virtual void run() = 0;

private:
    void execute();
    void configureThread();
    void prefaultStack() const;

    ThreadConfiguration    config_;
    std::thread            thread_;
    std::atomic<bool>      started_;
};

} // namespace common::rt
