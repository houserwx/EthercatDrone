# Phase 2 — EtherCAT Backend & PDO System

> **Goal:** Complete EtherCAT backend with DC sync, slave discovery, typed PDO accessors, hardware catalog integration, and basic dual-master redundancy.
> **Target Duration:** 1–1.5 weeks (Sprint 2 from WBS3.md)

---

## Sprint 2.1: EtherCAT Backend Completeness

- [ ] **1.** Implement DC (Distributed Clock) synchronization — Add DC sync setup in `EthercatAdapter::initialize()` for deterministic timing
- [ ] **2.** Complete `discoverSlaves()` — Finish slave iteration, vendor/product code extraction, and ESI-based type lookup
- [ ] **3.** Complete `buildEntries()` — Map slave PDOs to domain entries, create `PDOEntry` objects from discovered slave channels
- [ ] **4.** Complete `applyConfig()` — Load hardware.json config and match catalog UUIDs to PDO entries
- [ ] **5.** Implement EtherCAT Config struct — Define `fc::ethercat::Config` (currently forward-declared)
- [ ] **6.** Populate `kSlaveTypes.cpp` — Add known slave definitions (EL2124, EL1124, EL3632) or implement ESI XML parser
- [ ] **7.** Add slave type registry lookup — `SlaveTypeInfo` with PDO mapping for each known device

## Sprint 2.2: HardwareCatalog & Registry Enhancements

- [ ] **8.** Extend HardwareCatalog for multi-backend registration — Add backend type enum, support non-EtherCAT registration
- [ ] **9.** Add catalog entry registration from EtherCAT discovery — `EthercatAdapter::discoverSlaves()` calls `catalog_->addEntry()` per channel
- [ ] **10.** Improve `HardwareRegistry::freezeForRt()` — Add capacity pre-allocation, validation, and debug output
- [ ] **11.** Add `HardwareRegistry` backend health monitoring — Track communication status per backend

## Sprint 2.3: PDO System Enhancements

- [ ] **12.** Extend `PDOEntry::read()`/`write()` for all EntryTypes — Complete switch cases for IMU_GyroX-Z, IMU_AccelX-Z, Magnetometer, Barometer, GPS types
- [ ] **13.** Add `PDOEntry` bit-field extraction — Implement proper bit-level read/write for entries that aren't byte-aligned
- [ ] **14.** Add `PDO::freeze()` validation — Verify all entry offsets are within image bounds after freeze
- [ ] **15.** Add typed accessors for new sensor types — Verify completeness of `getGyroX()`, `getAccel()`, `getPressure()`, etc.

## Sprint 2.4: Redundancy Implementation

- [ ] **16.** Implement `RedundancyController::run()` — Complete UDP heartbeat send/receive loop
- [ ] **17.** Implement role election logic — MAC-based primary preference, standby promotion
- [ ] **18.** Implement failover coordination — Graceful handoff when primary fails, standby takes over EtherCAT master
- [ ] **19.** Add heartbeat socket management — UDP socket creation, bind, send/receive on private link
- [ ] **20.** Integrate RedundancyController with Application — Wire `currentRole()` to control loop enable/disable

## Sprint 2.5: Testing & Integration

- [ ] **21.** Add EtherCAT adapter tests (simulated mode) — Test `SimulatedAdapter` full lifecycle in `test_fc.cpp`
- [ ] **22.** Add HardwareCatalog unit tests — Test UUID generation, load/save, key lookup, addEntry
- [ ] **23.** Add HardwareRegistry unit tests — Test `readAll()`/`writeAll()` with mock backends
- [ ] **24.** Add redundancy controller tests — Test role election, heartbeat timeout, failover trigger
- [ ] **25.** Add PDOEntry type accessor tests — Verify all EntryType read/write paths work correctly
- [ ] **26.** Integration test: SimulatedAdapter + HardwareRegistry + Wrappers — Full RT cycle simulation
- [ ] **27.** Benchmark RT cycle timing — Verify <100µs cycle with simulated load

---

## Notes

- Items are checked off `[x]` when built, tested, and committed.
- Dependent items should be done in order (e.g., `discoverSlaves()` → `buildEntries()` → `applyConfig()`).
- Sprint 2.1-2.3 are foundational. Sprint 2.4 (redundancy) can run in parallel once 2.1 is functional.
- Sprint 2.5 tests should be written alongside implementation, not after.
