// test_fc.cpp — Unit tests for flight controller library
// PDOEntry, PulseMachine, SignalProcess

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <thread>
#include <chrono>

#include "fc/pdo/PDO.h"
#include "dynamichardware/rt/SignalProcess.h"

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

// ---- SimulatedAdapter tests (Item 21) ----------------------------------------
// New API: adapter reads a JSON definitions file via loadDefinitions(),
// populating the HardwareCatalog with simulated entries, then initialize()
// builds the PDO from those catalog entries.

#include "fc/simulated/SimulatedAdapter.h"
#include "fc/ethercat/HardwareCatalog.h"
#include <fstream>
#include <nlohmann/json.hpp>

namespace {
// Helper: write a SimulatedAdapter definitions JSON to a temp path, return path.
std::string writeSimDefinitions(int cycleTimeUs, std::vector<std::string> channels) {
    using json = nlohmann::json;
    json def;
    def["cycleTimeUs"] = cycleTimeUs;
    def["channels"] = json::array();
    for (auto& ch : channels) {
        // ch is a simple name like "do-0:DigitalOutput" or "enc-0:Encoder"
        auto pos = ch.rfind(':');
        std::string name = ch.substr(0, pos);
        std::string type = ch.substr(pos + 1);
        json entry;
        entry["name"] = "virt-" + name;
        entry["uuid"] = "virt-" + name;
        entry["channelType"] = type;
        def["channels"].push_back(std::move(entry));
    }
    std::string path = "/tmp/sim_def_test.json";
    std::ofstream f(path);
    f << def.dump(2);
    f.close();
    return path;
}

// Helper: write a JSON with physics-based sim params
std::string writeSimDefinitionsPhysics(int cycleTimeUs,
    const std::string& name, const std::string& type, float rpm, float rollerDiamMm,
    uint32_t resolutionPpr, bool quadrature, float partsPerMin, float variancePercent) {
    using json = nlohmann::json;
    json def;
    def["cycleTimeUs"] = cycleTimeUs;
    json entry;
    entry["name"] = "virt-" + name;
    entry["uuid"] = "virt-" + name;
    entry["channelType"] = type;
    json sim;
    sim["rpm"] = rpm;
    sim["rollerDiamMm"] = rollerDiamMm;
    sim["resolutionPpr"] = resolutionPpr;
    sim["quadrature"] = quadrature;
    sim["partsPerMin"] = partsPerMin;
    sim["variancePercent"] = variancePercent;
    entry["sim"] = sim;
    def["channels"] = {std::move(entry)};
    std::string path = "/tmp/sim_def_physics_test.json";
    std::ofstream f(path);
    f << def.dump(2);
    f.close();
    return path;
}

// Helper: setup adapter from JSON definitions file
void setupAdapterFromJson(fc::simulated::SimulatedAdapter& adapter,
                          fc::pdo::HardwareCatalog& catalog,
                          const std::string& jsonPath, int cycleTimeUs) {
    adapter.setCatalog(&catalog);
    adapter.loadDefinitions(jsonPath);
    adapter.setCycleTimeUs(cycleTimeUs);
    CHECK(adapter.initialize());
}
} // namespace

TEST_CASE("SimulatedAdapter initialize creates PDO entries", "[fc][sim]") {
    fc::pdo::HardwareCatalog catalog;
    std::string jsonPath = writeSimDefinitions(500, {
        "do-0:DigitalOutput", "di-0:DigitalInput",
        "enc-0:Encoder", "ai-0:AnalogInput"
    });

    fc::simulated::SimulatedAdapter adapter;
    setupAdapterFromJson(adapter, catalog, jsonPath, 500);
    CHECK(adapter.getPDOs().size() == 1);
    CHECK(adapter.getPDOs()[0].entries.size() == 4);
}

TEST_CASE("SimulatedAdapter encoder simulation produces increasing counts", "[fc][sim]") {
    fc::pdo::HardwareCatalog catalog;
    std::string jsonPath = writeSimDefinitions(500, {"enc-0:Encoder"});

    fc::simulated::SimulatedAdapter adapter;
    setupAdapterFromJson(adapter, catalog, jsonPath, 500);

    int64_t prevCount = 0;
    for (int i = 0; i < 100; ++i) {
        adapter.onBeforeReadInputs();
        adapter.getPDOs()[0].entries[0].read();
        int64_t count = adapter.getPDOs()[0].entries[0].getCount();
        CHECK(count >= prevCount);  // monotonically non-decreasing
        prevCount = count;
    }
    CHECK(prevCount > 0);  // count should have increased
}

TEST_CASE("SimulatedAdapter digital input simulation toggles", "[fc][sim]") {
    fc::pdo::HardwareCatalog catalog;
    std::string jsonPath = writeSimDefinitions(500, {"di-0:DigitalInput"});

    fc::simulated::SimulatedAdapter adapter;
    setupAdapterFromJson(adapter, catalog, jsonPath, 500);

    bool sawHigh = false;
    bool sawLow = false;
    for (int i = 0; i < 100; ++i) {
        adapter.onBeforeReadInputs();
        adapter.getPDOs()[0].entries[0].read();
        if (adapter.getPDOs()[0].entries[0].getBool()) sawHigh = true;
        else sawLow = true;
        if (sawHigh && sawLow) break;
    }
    CHECK(sawHigh);
    CHECK(sawLow);
}

TEST_CASE("SimulatedAdapter digital output can be set and read", "[fc][sim]") {
    fc::pdo::HardwareCatalog catalog;
    std::string jsonPath = writeSimDefinitions(500, {"do-0:DigitalOutput"});

    fc::simulated::SimulatedAdapter adapter;
    setupAdapterFromJson(adapter, catalog, jsonPath, 500);

    auto& entry = adapter.getPDOs()[0].entries[0];
    entry.setBool(true);
    CHECK(entry.getBool());
    entry.setBool(false);
    CHECK(!entry.getBool());
}

TEST_CASE("SimulatedAdapter full lifecycle initialize-read-write cycle", "[fc][sim]") {
    fc::pdo::HardwareCatalog catalog;
    std::string jsonPath = writeSimDefinitions(500, {
        "do-0:DigitalOutput", "di-0:DigitalInput", "enc-0:Encoder"
    });

    fc::simulated::SimulatedAdapter adapter;
    setupAdapterFromJson(adapter, catalog, jsonPath, 500);

    for (int cycle = 0; cycle < 20; ++cycle) {
        adapter.onBeforeReadInputs();
        for (auto& e : adapter.getPDOs()[0].entries) e.read();
        for (auto& e : adapter.getPDOs()[0].entries) e.write();
    }
}

TEST_CASE("SimulatedAdapter analog input produces static ADC value", "[fc][sim]") {
    fc::pdo::HardwareCatalog catalog;
    std::string jsonPath = writeSimDefinitions(500, {"ai-0:AnalogInput"});

    fc::simulated::SimulatedAdapter adapter;
    setupAdapterFromJson(adapter, catalog, jsonPath, 500);

    adapter.onBeforeReadInputs();
    adapter.getPDOs()[0].entries[0].read();
    int16_t adc = adapter.getPDOs()[0].entries[0].getRawAdc();
    CHECK(adc == 0);  // default static ADC value
}

TEST_CASE("SimulatedAdapter physics-based encoder simulation", "[fc][sim]") {
    fc::pdo::HardwareCatalog catalog;
    std::string jsonPath = writeSimDefinitionsPhysics(500,
        "enc-physics", "Encoder", 60.0f, 100.0f, 1024, true, 0.0f, 0.0f);

    fc::simulated::SimulatedAdapter adapter;
    setupAdapterFromJson(adapter, catalog, jsonPath, 500);

    int64_t prevCount = 0;
    for (int i = 0; i < 50; ++i) {
        adapter.onBeforeReadInputs();
        adapter.getPDOs()[0].entries[0].read();
        int64_t count = adapter.getPDOs()[0].entries[0].getCount();
        CHECK(count >= prevCount);
        prevCount = count;
    }
    CHECK(prevCount > 0);
}

TEST_CASE("SimulatedAdapter physics-based digital input simulation", "[fc][sim]") {
    fc::pdo::HardwareCatalog catalog;
    std::string jsonPath = writeSimDefinitionsPhysics(500,
        "di-physics", "DigitalInput", 0.0f, 0.0f, 0, false, 600.0f, 5.0f);

    fc::simulated::SimulatedAdapter adapter;
    setupAdapterFromJson(adapter, catalog, jsonPath, 500);

    bool sawHigh = false;
    bool sawLow = false;
    for (int i = 0; i < 2000; ++i) {
        adapter.onBeforeReadInputs();
        adapter.getPDOs()[0].entries[0].read();
        if (adapter.getPDOs()[0].entries[0].getBool()) sawHigh = true;
        else sawLow = true;
        if (sawHigh && sawLow) break;
    }
    CHECK(sawHigh);
    CHECK(sawLow);
}

// ---- RedundancyController tests (Item 24b) -----------------------------------

#include "fc/redundancy/RedundancyController.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>

TEST_CASE("RedundancyController initial role is STANDBY", "[fc][redundancy]") {
    fc::redundancy::RedundancyController ctrl("127.0.0.1", "127.0.0.2", 12345, 10, 200);
    CHECK(ctrl.currentRole() == fc::redundancy::Role::STANDBY);
    CHECK(!ctrl.isPrimary());
}

TEST_CASE("RedundancyController standby times out and promotes to PRIMARY", "[fc][redundancy]") {
    fc::redundancy::RedundancyController ctrl("127.0.0.1", "127.0.0.99", 12346, 10, 50);
    std::jthread worker([&ctrl]() { ctrl.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    ctrl.requestStop();
    worker.join();
    CHECK(ctrl.currentRole() == fc::redundancy::Role::PRIMARY);
    CHECK(ctrl.isPrimary());
}

TEST_CASE("RedundancyController heartbeat timeout detection", "[fc][redundancy]") {
    fc::redundancy::RedundancyController ctrl("127.0.0.1", "127.0.0.99", 12347, 10, 30);
    CHECK(ctrl.lastHeartbeatReceivedNs() == 0);

    std::jthread worker([&ctrl]() { ctrl.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    ctrl.requestStop();
    worker.join();
    CHECK(ctrl.currentRole() == fc::redundancy::Role::PRIMARY);
}

TEST_CASE("RedundancyController requestStop stops gracefully", "[fc][redundancy]") {
    fc::redundancy::RedundancyController ctrl("127.0.0.1", "127.0.0.99", 12348, 10, 30);
    ctrl.requestStop();
    CHECK(ctrl.currentRole() == fc::redundancy::Role::STANDBY);
}

TEST_CASE("RedundancyController role election with UDP loopback", "[fc][redundancy]") {
    fc::redundancy::RedundancyController ctrl1("127.0.0.10", "127.0.0.20", 12349, 10, 50);
    fc::redundancy::RedundancyController ctrl2("127.0.0.20", "127.0.0.10", 12349, 10, 50);
    CHECK(ctrl1.currentRole() == fc::redundancy::Role::STANDBY);
    CHECK(ctrl2.currentRole() == fc::redundancy::Role::STANDBY);
}

// ---- Integration: SimulatedAdapter + HardwareRegistry + Wrappers (Item 26) ---

#include "fc/wrapper/DigitalInputWrapper.h"
#include "fc/wrapper/DigitalOutputWrapper.h"
#include "fc/wrapper/AnalogInputWrapper.h"
#include "fc/wrapper/AnalogOutputWrapper.h"

TEST_CASE("Integration: SimulatedAdapter + HardwareRegistry basic setup", "[fc][integration]") {
    fc::pdo::HardwareCatalog catalog;
    std::string jsonPath = writeSimDefinitions(500, {
        "do-0:DigitalOutput", "di-0:DigitalInput"
    });

    auto sim = std::make_unique<fc::simulated::SimulatedAdapter>();
    sim->setCatalog(&catalog);
    sim->loadDefinitions(jsonPath);
    sim->setCycleTimeUs(500);
    CHECK(sim->initialize());
    fc::pdo::HardwareRegistry registry;
    registry.addBackend(std::move(sim));
    registry.buildUuidMap();
    registry.freezeForRt();

    CHECK(registry.backendCount() == 1);
    CHECK(registry.entryCount() == 2);
    CHECK(registry.isFrozen());
}

TEST_CASE("Integration: SimulatedAdapter + HardwareRegistry readAll/writeAll cycle", "[fc][integration]") {
    fc::pdo::HardwareCatalog catalog;
    std::string jsonPath = writeSimDefinitions(500, {
        "do-0:DigitalOutput", "di-0:DigitalInput", "enc-0:Encoder"
    });

    auto sim = std::make_unique<fc::simulated::SimulatedAdapter>();
    sim->setCatalog(&catalog);
    sim->loadDefinitions(jsonPath);
    sim->setCycleTimeUs(500);
    CHECK(sim->initialize());
    fc::pdo::HardwareRegistry registry;
    registry.addBackend(std::move(sim));
    registry.buildUuidMap();
    registry.freezeForRt();

    for (int i = 0; i < 50; ++i) {
        registry.readAll();
        registry.writeAll();
    }

    auto* doEntry = registry.lookupByUuid("virt-do-0");
    auto* diEntry = registry.lookupByUuid("virt-di-0");
    auto* encEntry = registry.lookupByUuid("virt-enc-0");
    CHECK(doEntry != nullptr);
    CHECK(diEntry != nullptr);
    CHECK(encEntry != nullptr);
    CHECK(encEntry->getCount() >= 0);
}

TEST_CASE("Integration: SimulatedAdapter + HardwareRegistry + Wrappers full RT cycle", "[fc][integration]") {
    fc::pdo::HardwareCatalog catalog;
    std::string jsonPath = writeSimDefinitions(500, {
        "do-0:DigitalOutput", "di-0:DigitalInput",
        "ai-0:AnalogInput", "ao-0:AnalogOutput"
    });

    auto sim = std::make_unique<fc::simulated::SimulatedAdapter>();
    sim->setCatalog(&catalog);
    sim->loadDefinitions(jsonPath);
    sim->setCycleTimeUs(500);
    CHECK(sim->initialize());
    fc::pdo::HardwareRegistry registry;
    registry.addBackend(std::move(sim));
    registry.buildUuidMap();
    registry.freezeForRt();

    auto* doEntry = registry.lookupByUuid("virt-do-0");
    auto* diEntry = registry.lookupByUuid("virt-di-0");
    auto* aiEntry = registry.lookupByUuid("virt-ai-0");
    auto* aoEntry = registry.lookupByUuid("virt-ao-0");
    REQUIRE(doEntry != nullptr);
    REQUIRE(diEntry != nullptr);
    REQUIRE(aiEntry != nullptr);
    REQUIRE(aoEntry != nullptr);

    fc::wrapper::DigitalOutputWrapper doWrap("DO-0", *doEntry);
    fc::wrapper::DigitalInputWrapper  diWrap("DI-0", *diEntry);
    fc::wrapper::AnalogInputWrapper   aiWrap("AI-0", *aiEntry);
    fc::wrapper::AnalogOutputWrapper  aoWrap("AO-0", *aoEntry);

    bool lastDiState = false;
    for (int cycle = 0; cycle < 20; ++cycle) {
        registry.readAll();
        bool diActive = diWrap.isActive();
        int16_t adcVal = aiWrap.rawAdc();
        (void)adcVal;
        doWrap.setActive(diActive);
        lastDiState = diActive;
        registry.writeAll();
    }
    CHECK(doWrap.isActive() == lastDiState);
}

TEST_CASE("Integration: Full RT cycle simulation with multiple backends", "[fc][integration]") {
    fc::pdo::HardwareCatalog catalog1;
    std::string jsonPath1 = writeSimDefinitions(500, {
        "seg1-do-0:DigitalOutput", "seg1-di-0:DigitalInput"
    });
    auto sim1 = std::make_unique<fc::simulated::SimulatedAdapter>();
    sim1->setCatalog(&catalog1);
    sim1->loadDefinitions(jsonPath1);
    sim1->setCycleTimeUs(500);
    CHECK(sim1->initialize());

    fc::pdo::HardwareCatalog catalog2;
    std::string jsonPath2 = writeSimDefinitions(500, {
        "seg2-enc-0:Encoder", "seg2-ai-0:AnalogInput"
    });
    auto sim2 = std::make_unique<fc::simulated::SimulatedAdapter>();
    sim2->setCatalog(&catalog2);
    sim2->loadDefinitions(jsonPath2);
    sim2->setCycleTimeUs(500);
    CHECK(sim2->initialize());

    fc::pdo::HardwareRegistry registry;
    registry.addBackend(std::move(sim1));
    registry.addBackend(std::move(sim2));
    registry.buildUuidMap();
    registry.freezeForRt();

    CHECK(registry.backendCount() == 2);
    CHECK(registry.entryCount() == 4);

    for (int i = 0; i < 100; ++i) {
        registry.readAll();
        registry.writeAll();
    }
}

// ---- Benchmark RT cycle timing (Item 27) ------------------------------------

TEST_CASE("Benchmark: RT cycle timing with simulated load < 100us", "[fc][benchmark]") {
    fc::pdo::HardwareCatalog catalog;
    std::vector<std::string> channels;
    for (int i = 0; i < 8; ++i) channels.push_back("virt-do-" + std::to_string(i) + ":DigitalOutput");
    for (int i = 0; i < 8; ++i) channels.push_back("virt-di-" + std::to_string(i) + ":DigitalInput");
    for (int i = 0; i < 4; ++i) channels.push_back("virt-enc-" + std::to_string(i) + ":Encoder");
    for (int i = 0; i < 4; ++i) channels.push_back("virt-ai-" + std::to_string(i) + ":AnalogInput");

    std::string jsonPath = writeSimDefinitions(500, channels);
    auto sim = std::make_unique<fc::simulated::SimulatedAdapter>();
    sim->setCatalog(&catalog);
    sim->loadDefinitions(jsonPath);
    sim->setCycleTimeUs(500);
    CHECK(sim->initialize());
    fc::pdo::HardwareRegistry registry;
    registry.addBackend(std::move(sim));
    registry.buildUuidMap();
    registry.freezeForRt();

    for (int i = 0; i < 100; ++i) {
        registry.readAll();
        registry.writeAll();
    }

    constexpr int kNumCycles = 10000;
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < kNumCycles; ++i) {
        registry.readAll();
        registry.writeAll();
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto totalNs = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    double avgCycleNs = static_cast<double>(totalNs) / kNumCycles;
    double avgCycleUs = avgCycleNs / 1000.0;
    INFO("Average cycle time: " << avgCycleUs << " us (" << avgCycleNs << " ns)");
    INFO("Total time for " << kNumCycles << " cycles: " << totalNs << " ns");
    INFO("Entries: " << registry.entryCount() << ", Backends: " << registry.backendCount());
    CHECK(avgCycleUs < 100.0);
}

TEST_CASE("Benchmark: RT cycle timing with wrapper overhead < 100us", "[fc][benchmark]") {
    fc::pdo::HardwareCatalog catalog;
    std::vector<std::string> channels;
    for (int i = 0; i < 8; ++i) channels.push_back("virt-do-" + std::to_string(i) + ":DigitalOutput");
    for (int i = 0; i < 8; ++i) channels.push_back("virt-di-" + std::to_string(i) + ":DigitalInput");

    std::string jsonPath = writeSimDefinitions(500, channels);
    auto sim = std::make_unique<fc::simulated::SimulatedAdapter>();
    sim->setCatalog(&catalog);
    sim->loadDefinitions(jsonPath);
    sim->setCycleTimeUs(500);
    CHECK(sim->initialize());
    fc::pdo::HardwareRegistry registry;
    registry.addBackend(std::move(sim));
    registry.buildUuidMap();
    registry.freezeForRt();

    std::vector<fc::wrapper::DigitalOutputWrapper> doWrappers;
    std::vector<fc::wrapper::DigitalInputWrapper>  diWrappers;
    for (int i = 0; i < 8; ++i) {
        auto* doE = registry.lookupByUuid("virt-do-" + std::to_string(i));
        auto* diE = registry.lookupByUuid("virt-di-" + std::to_string(i));
        if (doE) doWrappers.emplace_back("DO-" + std::to_string(i), *doE);
        if (diE) diWrappers.emplace_back("DI-" + std::to_string(i), *diE);
    }

    for (int i = 0; i < 100; ++i) {
        registry.readAll();
        for (auto& w : doWrappers) {
            bool anyHi = false;
            for (auto& r : diWrappers) if (r.isActive()) anyHi = true;
            w.setActive(anyHi);
        }
        registry.writeAll();
    }

    constexpr int kNumCycles = 10000;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < kNumCycles; ++i) {
        registry.readAll();
        for (auto& w : doWrappers) {
            bool anyHi = false;
            for (auto& r : diWrappers) if (r.isActive()) anyHi = true;
            w.setActive(anyHi);
        }
        registry.writeAll();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto totalNs = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    double avgCycleUs = static_cast<double>(totalNs) / kNumCycles / 1000.0;

    INFO("Average cycle time with wrappers: " << avgCycleUs << " us");
    INFO("Total time for " << kNumCycles << " cycles: " << totalNs << " ns");

    CHECK(avgCycleUs < 100.0);
}
