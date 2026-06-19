// test_common.cpp — Unit tests for common library
// VectorBuffer, Logger, Config, SignalProcess

#include <catch2/catch_test_macros.hpp>

#include "common/rt/ThreadBuffer.h"
#include "common/config/Config.h"
#include "dynamichardware/rt/SignalProcess.h"
#include "common/messages/Message.h"

#include <thread>
#include <chrono>
#include <sstream>

// ---- VectorBuffer tests ----------------------------------------------------

TEST_CASE("VectorBuffer basic push/pop", "[common][vectorbuffer]") {
    common::rt::VectorBuffer<int> buf(16);
    CHECK(buf.tryPush(42));
    int val = 0;
    CHECK(buf.tryPop(val));
    CHECK(val == 42);
    CHECK(!buf.tryPop(val));  // empty after pop
}

TEST_CASE("VectorBuffer overflow drops on full", "[common][vectorbuffer]") {
    common::rt::VectorBuffer<int> buf(4);
    // Fill to capacity (N-1 slots usable)
    CHECK(buf.tryPush(1));
    CHECK(buf.tryPush(2));
    CHECK(buf.tryPush(3));
    // 4th push should fail (buffer full)
    CHECK(!buf.tryPush(4));
}

TEST_CASE("VectorBuffer drain callback", "[common][vectorbuffer]") {
    common::rt::VectorBuffer<int> buf(16);
    REQUIRE(buf.tryPush(10));
    REQUIRE(buf.tryPush(20));
    REQUIRE(buf.tryPush(30));

    std::vector<int> drained;
    buf.drain([&drained](std::span<const int> batch) {
        for (auto v : batch) drained.push_back(static_cast<int>(v));
    });
    CHECK(drained.size() == 3);
    CHECK(drained[0] == 10);
    CHECK(drained[1] == 20);
    CHECK(drained[2] == 30);
}

TEST_CASE("VectorBuffer SPSC from separate threads", "[common][vectorbuffer]") {
    common::rt::VectorBuffer<int64_t> buf(256);
    std::atomic<bool> done{false};
    std::vector<int64_t> consumed;

    // Producer thread
    std::thread producer([&]() {
        for (int i = 0; i < 100; ++i) {
            while (!buf.tryPush(i)) {
                std::this_thread::yield();
            }
        }
        done.store(true);
    });

    // Consumer (main thread) - with timeout to prevent infinite loop
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (consumed.size() < 100 && std::chrono::steady_clock::now() < deadline) {
        int64_t val = 0;
        while (buf.tryPop(val)) {
            consumed.push_back(val);
        }
        if (!done.load()) std::this_thread::yield();
    }

    producer.join();

    CHECK(consumed.size() == 100);
    for (int i = 0; i < 100; ++i) {
        CHECK(consumed[i] == i);
    }
}

// ---- Config tests ----------------------------------------------------------

TEST_CASE("Config load from JSON", "[common][config]") {
    // Create a temporary JSON config
    std::ostringstream json;
    json << R"({
        "cycleTimeUs": 1000,
        "pdoEntries": [
            {
                "name": "test_di",
                "hwUuid": "00000000-0000-0000-0000-000000000001",
                "channelType": "DigitalInput",
                "sim": { "partsPerMin": 60.0 }
            },
            {
                "name": "test_do",
                "hwUuid": "00000000-0000-0000-0000-000000000002",
                "channelType": "DigitalOutput",
                "pulseMs": 500
            }
        ]
    })";

    // Write to temp file
    const char* path = "/tmp/test_config.json";
    std::ofstream(path) << json.str();

    auto cfg = common::config::Config::loadFromJson(path);
    CHECK(cfg.cycleTimeUs == 1000);
    CHECK(cfg.pdoEntries.size() == 2);
    CHECK(cfg.pdoEntries[0].name == "test_di");
    CHECK(cfg.pdoEntries[0].channelType == "DigitalInput");
    CHECK(cfg.pdoEntries[1].name == "test_do");
    CHECK(cfg.pdoEntries[1].pulseMs == 500);
    CHECK(cfg.cycleNs() == 1'000'000);

    std::remove(path);
}

TEST_CASE("Config throws on missing file", "[common][config]") {
    REQUIRE_THROWS_AS(common::config::Config::loadFromJson("/nonexistent/path.json"),
                      std::runtime_error);
}

// ---- Message tests ---------------------------------------------------------

TEST_CASE("MessageId label returns non-empty string", "[common][message]") {
    auto label = messages::messageIdLabel(messages::MessageId::SYSTEM_INIT_COMPLETE);
    CHECK(!label.empty());
}

TEST_CASE("LogEntry is trivially copyable", "[common][message]") {
    STATIC_REQUIRE(std::is_trivially_copyable_v<messages::LogEntry>);
}

// ---- SignalProcess tests ---------------------------------------------------

TEST_CASE("SignalProcess nowNs returns increasing values", "[common][signalprocess]") {
    auto t1 = common::rt::signalProcessTickNow();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    auto t2 = common::rt::signalProcessTickNow();
    CHECK(t2 > t1);
}
