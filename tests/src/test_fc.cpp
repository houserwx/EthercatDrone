// test_fc.cpp — Unit tests for flight controller library
// PDOEntry, PulseMachine, SignalProcess

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <thread>
#include <chrono>

#include "fc/pdo/PDO.h"
#include "common/rt/SignalProcess.h"

// ---- PDOEntry tests --------------------------------------------------------

TEST_CASE("PDOEntry DigitalOutput setBool arms pulse", "[fc][pdo]") {
    fc::pdo::PDOEntry entry;
    entry.type = fc::pdo::EntryType::DigitalOutput;
    entry.bitLength = 1;

    uint8_t image[8] = {0};
    entry.image = image;
    entry.byteOffset = 0;
    entry.bitOffset = 0;

    // setBool on DigitalOutput arms the pulse machine
    entry.setBool(true);
    CHECK(entry.getBool());  // pulse is active

    entry.setBool(false);
    CHECK(!entry.getBool());  // pulse cleared
}

TEST_CASE("PDOEntry DigitalInput getBool reads cached value", "[fc][pdo]") {
    fc::pdo::PDOEntry entry;
    entry.type = fc::pdo::EntryType::DigitalInput;
    entry.bitLength = 1;

    // DigitalInput uses read() to populate boolVal_ from image
    // getBool() returns the cached boolVal_
    CHECK(!entry.getBool());  // default false
}

TEST_CASE("PDOEntry IMU gyro accessors", "[fc][pdo]") {
    fc::pdo::PDOEntry entry;
    entry.type = fc::pdo::EntryType::IMU_GyroX;
    entry.bitLength = 16;

    entry.setGyroX(1.23f);
    CHECK(entry.getGyroX() == Catch::Approx(1.23f));

    entry.setGyroY(4.56f);
    CHECK(entry.getGyroY() == Catch::Approx(4.56f));

    entry.setGyroZ(7.89f);
    CHECK(entry.getGyroZ() == Catch::Approx(7.89f));
}

TEST_CASE("PDOEntry barometer accessors", "[fc][pdo]") {
    fc::pdo::PDOEntry entry;
    entry.type = fc::pdo::EntryType::Barometer;
    entry.bitLength = 16;

    entry.setBaroPressure(1013.25f);
    CHECK(entry.getBaroPressure() == Catch::Approx(1013.25f));

    entry.setBaroAltitude(150.0f);
    CHECK(entry.getBaroAltitude() == Catch::Approx(150.0f));
}

TEST_CASE("PDOEntry magnetometer accessors", "[fc][pdo]") {
    fc::pdo::PDOEntry entry;
    entry.type = fc::pdo::EntryType::MagnetometerX;
    entry.bitLength = 16;

    entry.setMagX(0.2f);
    CHECK(entry.getMagX() == Catch::Approx(0.2f));

    entry.setMagY(-0.1f);
    CHECK(entry.getMagY() == Catch::Approx(-0.1f));

    entry.setMagZ(0.5f);
    CHECK(entry.getMagZ() == Catch::Approx(0.5f));
}

TEST_CASE("PDOEntry float accessor", "[fc][pdo]") {
    fc::pdo::PDOEntry entry;
    entry.type = fc::pdo::EntryType::GPS_Latitude;
    entry.bitLength = 32;

    entry.setFloat(37.7749f);
    CHECK(entry.getFloat() == Catch::Approx(37.7749f));
}

// ---- PulseMachine tests ----------------------------------------------------

TEST_CASE("PulseMachine one-shot pulse", "[fc][pulse]") {
    common::rt::PulseMachine pm;
    pm.configure(100);  // 100ms pulse

    uint64_t now = 0;
    pm.arm(true, now);
    CHECK(pm.isHighOrLatched());

    // Before pulse expires
    now = 50'000'000ULL;  // 50ms
    CHECK(pm.tick(now));

    // After pulse expires
    now = 150'000'000ULL;  // 150ms
    CHECK(!pm.tick(now));
    CHECK(!pm.isHighOrLatched());
}

TEST_CASE("PulseMachine latched mode (no duration)", "[fc][pulse]") {
    common::rt::PulseMachine pm;
    // No configure() call — latched mode (durationNs_ == 0)

    pm.arm(true, 0);
    CHECK(pm.isHighOrLatched());

    pm.arm(false, 0);
    CHECK(!pm.isHighOrLatched());
}

// ---- SignalProcess tests ---------------------------------------------------

TEST_CASE("SignalProcess nowNs returns increasing values", "[fc][signal]") {
    auto t1 = common::rt::signalProcessTickNow();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    auto t2 = common::rt::signalProcessTickNow();
    CHECK(t2 > t1);
}
