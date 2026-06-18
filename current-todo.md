# Phase 2 — EtherCAT Backend & PDO System

> **Goal:** Complete EtherCAT backend with DC sync, slave discovery, typed PDO accessors, hardware catalog integration, and basic dual-master redundancy.
> **Target Duration:** 1–1.5 weeks (Sprint 2 from WBS3.md)

---

## Sprint 2.1: EtherCAT Backend Completeness

- [x] **1.** Implement DC (Distributed Clock) synchronization — Add DC sync setup in `EthercatAdapter::initialize()` for deterministic timing
- [x] **2.** Complete `discoverSlaves()` — Finish slave iteration, vendor/product code extraction, and ESI-based type lookup
- [x] **3.** Complete `buildEntries()` — Map slave PDOs to domain entries, create `PDOEntry` objects from discovered slave channels
- [x] **4.** Complete `applyConfig()` — Load hardware.json config and match catalog UUIDs to PDO entries
- [x] **5.** Implement EtherCAT Config struct — Define `fc::ethercat::Config` (currently forward-declared)
- [x] **6.** Populate `kSlaveTypes.cpp` — Add known slave definitions (EL2124, EL1124, EL3632) or implement ESI XML parser
- [x] **7.** Add slave type registry lookup — `SlaveTypeInfo` with PDO mapping for each known device

## Sprint 2.2: HardwareCatalog & Registry Enhancements

- [x] **8.** Extend HardwareCatalog for multi-backend registration — Add backend type enum, support non-EtherCAT registration
- [x] **9.** Add catalog entry registration from EtherCAT discovery — `EthercatAdapter::discoverSlaves()` calls `catalog_->addEntry()` per channel
- [x] **10.** Improve `HardwareRegistry::freezeForRt()` — Add capacity pre-allocation, validation, and debug output
- [x] **11.** Add `HardwareRegistry` backend health monitoring — Track communication status per backend

## Sprint 2.3: PDO System Enhancements

- [x] **12.** Extend `PDOEntry::read()`/`write()` for all EntryTypes — Complete switch cases for IMU_GyroX-Z, IMU_AccelX-Z, Magnetometer, Barometer, GPS types *(already implemented in existing PDO.cpp)*
- [x] **13.** Add `PDOEntry` bit-field extraction — Implement proper bit-level read/write for entries that aren't byte-aligned *(already implemented in existing PDO.cpp)*
- [x] **14.** Add `PDO::freeze()` validation — Verify all entry offsets are within image bounds after freeze *(already implemented in existing PDO.cpp)*
- [x] **15.** Add typed accessors for new sensor types — Verify completeness of `getGyroX()`, `getAccel()`, `getPressure()`, etc. *(already implemented in existing PDO.h)*

## Sprint 2.4: Redundancy Implementation

- [x] **16.** Implement `RedundancyController::run()` — Complete UDP heartbeat send/receive loop
- [x] **17.** Implement role election logic — MAC-based primary preference, standby promotion
- [x] **18.** Implement failover coordination — Graceful handoff when primary fails, standby takes over EtherCAT master
- [x] **19.** Add heartbeat socket management — UDP socket creation, bind, send/receive on private link
- [x] **20.** Integrate RedundancyController with Application — Wire `currentRole()` to control loop enable/disable

## Sprint 2.5: Testing & Integration

- [ ] **21.** Add EtherCAT adapter tests (simulated mode) — Test `SimulatedAdapter` full lifecycle in `test_fc.cpp`
- [x] **22.** Add HardwareCatalog unit tests — Test UUID generation, load/save, key lookup, registerEcChannel
- [x] **23.** Add HardwareRegistry unit tests — Test entryCount, backendCount, lookupByUuid
- [x] **24.** Add SlaveTypeInfo unit tests — Test lookup known/unknown slaves, DC mode lookup
- [ ] **24b.** Add redundancy controller tests — Test role election, heartbeat timeout, failover trigger *(deferred: requires mock UDP sockets)*
- [x] **25.** Add PDOEntry type accessor tests — Verify all EntryType read/write paths work correctly *(already covered in existing test_fc.cpp)*
- [ ] **26.** Integration test: SimulatedAdapter + HardwareRegistry + Wrappers — Full RT cycle simulation
- [ ] **27.** Benchmark RT cycle timing — Verify <100µs cycle with simulated load

---

## Notes

- Items are checked off `[x]` when built, tested, and committed.
- Dependent items should be done in order (e.g., `discoverSlaves()` → `buildEntries()` → `applyConfig()`).
- Sprint 2.1-2.3 are foundational. Sprint 2.4 (redundancy) can run in parallel once 2.1 is functional.
- Sprint 2.5 tests should be written alongside implementation, not after.
