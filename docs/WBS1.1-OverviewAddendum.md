WBS 1.1: Software-Based IMU via EtherCAT PDO Integration
Phase: Phase 1 – Bench / Core Proof of Concept
Objective: Replace (or augment) the separate hardware IMU with a software-based IMU that reads high-rate sensor data directly through EtherCAT PDO entries. This provides tighter integration, lower latency, and better synchronization with the control loop.
Duration: 2–3 weeks
Success Criteria: IMU data (gyro, accel, optionally magnetometer) is read via PDOs with deterministic timing, fused into a usable orientation estimate, and available to the control loop with sub-100µs latency.

1.0 Requirements & Design
1.1 Define IMU Requirements
1.1.1 Identify required sensors (3-axis gyro + 3-axis accel minimum; optionally magnetometer)
1.1.2 Define target update rate (≥ 1 kHz) and synchronization with main control cycle
1.1.3 Specify output format (raw counts, scaled values, or fused quaternion/Euler)
1.2 Architecture Decision

All devices (including IMU) share a single RT hotpath and one EtherCAT domain.
Recommendation: Do not give individual modules (like IMU) their own EtherCAT loop.
Single shared hotpath = predictable timing, easier redundancy, simpler state management.
Multiple hotpaths = increased complexity, potential jitter, harder synchronization.



2.0 Hardware & Catalog Integration
2.1 EtherCAT IMU Hardware Selection
2.1.1 Select EtherCAT-compatible IMU module (e.g., Beckhoff ELM360x series, or generic high-rate IMU slave)
2.1.2 Verify PDO mapping in ESI XML (gyro, accel registers)
2.2 Hardware Catalog Updates
2.2.1 Add new CatalogEntry entries for IMU channels (gyro X/Y/Z, accel X/Y/Z, etc.)
2.2.2 Update HardwareCatalog and HardwareRegistry to recognize IMU PDOs
2.2.3 Assign stable UUIDs for IMU sensors

3.0 Software Implementation
3.1 PDOEntry Extensions for IMU
3.1.1 Add new EntryType::IMU_Gyro and EntryType::IMU_Accel (or generic IMU_Vector)
3.1.2 Extend PDOEntry with getGyro() / getAccel() methods (scaled or raw)
3.2 New IMU Wrapper
3.2.1 Create IMUWrapper class (similar to EncoderWrapper)
3.2.2 Support both raw reading and basic sensor fusion (Madgwick or complementary filter)
3.2.3 Add calibration routines (bias/scale factor)
3.3 Integration into RT Path
3.3.1 Update HardwareRegistry and EthercatAdapter to include IMU PDO entries
3.3.2 Add IMU reading in onBeforeReadInputs()
3.3.3 Integrate IMUWrapper into WrapperPool (one per vehicle)
3.4 State Machine & Control Loop Integration
3.4.1 Feed IMU data into attitude estimation
3.4.2 Update Application::rtCycle() and PID controllers to use software IMU
3.4.3 Add fallback to separate hardware IMU if configured

4.0 Testing & Validation
4.1 Unit Tests
4.1.1 Test raw PDO reading for gyro/accel
4.1.2 Validate sensor fusion accuracy against reference IMU
4.2 Bench Integration Tests
4.2.1 Run full control loop using only software IMU
4.2.2 Compare timing and jitter vs separate IMU setup
4.2.3 Test failover between software and hardware IMU
4.3 Performance Characterization
4.3.1 Measure end-to-end latency from sensor read to control output
4.3.2 Verify determinism under load (motors + vision + logging)

5.0 Documentation & Cleanup
5.1 Update Documentation
5.1.1 Add IMU configuration section to Overview.md
5.1.2 Document PDO mapping and calibration procedure
5.2 Code Cleanup
5.2.1 Remove or deprecate old separate IMU code paths
5.2.2 Add configuration flag to enable/disable software IMU

Phase 1.1 Deliverables

Working software-based IMU reading via EtherCAT PDOs
IMUWrapper integrated into WrapperPool and control loop
Single shared RT hotpath architecture validated
Documentation and test results