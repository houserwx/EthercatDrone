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

// ---- HardwareCatalog tests -------------------------------------------------

#include "fc/ethercat/HardwareCatalog.h"
#include <fstream>
#include <filesystem>

TEST_CASE("HardwareCatalog generate UUID format", "[fc][catalog]") {
    // generateUuid() is private, but we can test via addEntry
    fc::pdo::HardwareCatalog catalog;
    fc::pdo::CatalogEntry entry{
        "EC|00000002|00020001|REV00000001|POS0001|0600:01",
        "", "DigitalOutput", "EL2124[1] DO 0x0600:01",
        "EL2124", 1, 0x00020001u, 0x00000001u,
        0x0600, 0x01, true
    };
    catalog.addEntry(std::move(entry));

    auto* found = catalog.findByKey("EC|00000002|00020001|REV00000001|POS0001|0600:01");
    REQUIRE(found != nullptr);
    CHECK(found->uuid.size() == 36); // RFC-4122 UUID format: 8-4-4-4-12
    CHECK(found->uuid.substr(8, 1) == "-");
    CHECK(found->uuid.substr(13, 1) == "-");
    CHECK(found->uuid.substr(18, 1) == "-");
    CHECK(found->uuid.substr(23, 1) == "-");
}

TEST_CASE("HardwareCatalog registerEcChannel creates new entry", "[fc][catalog]") {
    fc::pdo::HardwareCatalog catalog;

    const auto& entry = catalog.registerEcChannel(
        0x00000002u, // vendor
        0x00020001u, // product
        0x00000001u, // revision
        1,           // position
        0x0600,      // pdo index
        0x01,        // pdo subindex
        "DigitalOutput",
        "EL2124",
        true
    );

    CHECK(entry.slaveName == "EL2124");
    CHECK(entry.slavePos == 1);
    CHECK(entry.isOutput == true);
    CHECK(entry.channelType == "DigitalOutput");
    CHECK(entry.uuid.size() == 36);
}

TEST_CASE("HardwareCatalog registerEcChannel reuses existing UUID", "[fc][catalog]") {
    fc::pdo::HardwareCatalog catalog;

    const auto& entry1 = catalog.registerEcChannel(
        0x00000002u, 0x00020001u, 0x00000001u,
        1, 0x0600, 0x01, "DigitalOutput", "EL2124", true);

    const auto& entry2 = catalog.registerEcChannel(
        0x00000002u, 0x00020001u, 0x00000001u,
        1, 0x0600, 0x01, "DigitalOutput", "EL2124", true);

    // Same key → same UUID preserved
    CHECK(entry1.uuid == entry2.uuid);
    CHECK(entry1.key == entry2.key);
}

TEST_CASE("HardwareCatalog findByUuid lookup", "[fc][catalog]") {
    fc::pdo::HardwareCatalog catalog;

    const auto& entry = catalog.registerEcChannel(
        0x00000002u, 0x00020001u, 0x00000001u,
        1, 0x0600, 0x01, "DigitalOutput", "EL2124", true);

    auto* found = catalog.findByUuid(entry.uuid);
    REQUIRE(found != nullptr);
    CHECK(found->uuid == entry.uuid);
    CHECK(found->slaveName == "EL2124");
}

TEST_CASE("HardwareCatalog load and save", "[fc][catalog]") {
    namespace fs = std::filesystem;
    std::string path = fs::temp_directory_path() / "test_catalog.json";

    fc::pdo::HardwareCatalog catalog;
    catalog.registerEcChannel(
        0x00000002u, 0x00020001u, 0x00000001u,
        1, 0x0600, 0x01, "DigitalOutput", "EL2124", true);
    catalog.registerEcChannel(
        0x00000002u, 0x00010001u, 0x00000001u,
        2, 0x0600, 0x01, "DigitalInput", "EL1124", false);

    CHECK(catalog.save(path));
    CHECK(fs::exists(path));

    // Load into a new catalog
    fc::pdo::HardwareCatalog catalog2;
    CHECK(catalog2.load(path));
    CHECK(catalog2.entries().size() == 2);

    // UUIDs should be preserved
    auto* entry = catalog2.findByKey(catalog.entries()[0].key);
    REQUIRE(entry != nullptr);
    CHECK(entry->uuid == catalog.entries()[0].uuid);

    fs::remove(path);
}

TEST_CASE("HardwareCatalog load nonexistent file returns true", "[fc][catalog]") {
    fc::pdo::HardwareCatalog catalog;
    // File absent on first run — should return true (fresh start)
    CHECK(catalog.load("/nonexistent/path/catalog.json"));
    CHECK(catalog.empty());
}

// ---- HardwareRegistry tests ------------------------------------------------

#include "fc/pdo/HardwareRegistry.h"

TEST_CASE("HardwareRegistry entryCount with no backends", "[fc][registry]") {
    fc::pdo::HardwareRegistry registry;
    CHECK(registry.entryCount() == 0);
    CHECK(registry.backendCount() == 0);
    CHECK(!registry.isFrozen());
}

TEST_CASE("HardwareRegistry lookupByUuid with empty uuid", "[fc][registry]") {
    fc::pdo::HardwareRegistry registry;
    CHECK(registry.lookupByUuid("") == nullptr);
}

// ---- SlaveTypeInfo tests ---------------------------------------------------

#include "fc/ethercat/SlaveTypeInfo.h"

TEST_CASE("SlaveTypeInfo lookup known slave", "[fc][slavetype]") {
    // Beckhoff EL2124
    auto* info = fc::ethercat::lookupSlaveType(0x00000002u, 0x00020001u);
    REQUIRE(info != nullptr);
    CHECK(std::string(info->type_name) == "EL2124");
    CHECK(info->vendor_id == 0x00000002u);
}

TEST_CASE("SlaveTypeInfo lookup unknown slave returns nullptr", "[fc][slavetype]") {
    auto* info = fc::ethercat::lookupSlaveType(0x00009999u, 0x00990001u);
    CHECK(info == nullptr);
}

TEST_CASE("SlaveTypeInfo DC mode lookup for EL3632", "[fc][slavetype]") {
    auto* info = fc::ethercat::lookupSlaveType(0x00000002u, 0x00050001u);
    REQUIRE(info != nullptr);

    const auto* dc = fc::ethercat::lookupDcMode(info);
    REQUIRE(dc != nullptr);
    CHECK(dc->assign_activate == 0x0730u);
    CHECK(std::string(dc->name) == "DcSync");
}

TEST_CASE("SlaveTypeInfo DC mode lookup for EL2124 (free-run)", "[fc][slavetype]") {
    auto* info = fc::ethercat::lookupSlaveType(0x00000002u, 0x00020001u);
    REQUIRE(info != nullptr);

    // EL2124 has FreeRun (assign_activate=0) — lookupDcMode returns first non-zero
    const auto* dc = fc::ethercat::lookupDcMode(info);
    CHECK(dc == nullptr); // FreeRun has assign_activate=0, so no DC mode
}
