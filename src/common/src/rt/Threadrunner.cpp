#include "common/rt/Threadrunner.h"

#include <alloca.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <pthread.h>
#include <sched.h>
#include <utility>

namespace common::rt {

Threadrunner::Threadrunner(ThreadConfiguration config)
    : config_(std::move(config))
    , started_(false)
{
}

Threadrunner::~Threadrunner()
{
    if (thread_.joinable()) {
        thread_.join();
    }
}

void Threadrunner::start()
{
    bool expected = false;
    if (!started_.compare_exchange_strong(expected, true,
                                          std::memory_order_acq_rel)) {
        return;
    }
    thread_ = std::thread(&Threadrunner::execute, this);
}

void Threadrunner::join()
{
    if (thread_.joinable()) {
        thread_.join();
    }
}

void Threadrunner::execute()
{
    configureThread();

    if (config_.stackPrefaultBytes > 0) {
        prefaultStack();
    }

    std::cout << "[Thread:" << config_.name << "] configured and starting\n";
    run();
}

void Threadrunner::configureThread()
{
    if (config_.cpuCore >= 0) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(config_.cpuCore, &cpuset);

        if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0) {
            std::cout << "[Thread:" << config_.name
                      << "] pinned to CPU " << config_.cpuCore << '\n';
        } else {
            std::cerr << "[Thread:" << config_.name
                      << "] WARNING: failed to set CPU affinity to core "
                      << config_.cpuCore << ": " << std::strerror(errno)
                      << " (try isolcpus=" << config_.cpuCore << " boot param)\n";
        }
    }

    if (config_.useRealtime && config_.priority > 0) {
        struct sched_param param{};
        param.sched_priority = config_.priority;

        if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &param) == 0) {
            std::cout << "[Thread:" << config_.name
                      << "] SCHED_FIFO priority=" << config_.priority << '\n';
        } else {
            std::cerr << "[Thread:" << config_.name
                      << "] WARNING: failed to set SCHED_FIFO: "
                      << std::strerror(errno)
                      << " (run with sudo or set CAP_SYS_NICE)\n";
        }
    }
}

void Threadrunner::prefaultStack() const
{
    volatile char* stack = static_cast<volatile char*>(alloca(config_.stackPrefaultBytes));

    for (std::size_t i = 0; i < config_.stackPrefaultBytes; i += 4096) {
        stack[i] = 0;
    }

    asm volatile("" : : "g"(stack) : "memory");

    std::cout << "[Thread:" << config_.name
              << "] prefaulted " << config_.stackPrefaultBytes << " bytes of stack\n";
}

} // namespace common::rt
