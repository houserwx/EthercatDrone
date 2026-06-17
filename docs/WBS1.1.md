Updated Phase 1.1 WBS (Aligned with Your Existing Pattern)
Here’s the corrected, more accurate version that respects your current design philosophy:

Phase 1.1: Software-Based IMU Support
Objective: Extend the existing backend + catalog + wrapper pattern to support high-rate IMUs and auxiliary sensors (magnetometer, barometer).
Duration: 4–5 weeks (2–3 sprints)

Sprint 1: Backend Foundation & Catalog Extensions (1.5–2 weeks)
Goal: Enable new sensor types without breaking the current discovery/mapping system.
Tasks

Add new EntryType values: IMU_Gyro, IMU_Accel, Magnetometer, Barometer
Extend PDOEntry with appropriate typed accessors (getGyro(), getAccel(), getMagneticField(), etc.)
Update HardwareCatalog to support registering non-EtherCAT devices
Begin design of I2CAdapter (or GenericSensorAdapter) that can discover I2C devices and register them into the catalog
Update HardwareRegistry to support multiple backend types cleanly

Acceptance Criteria

Catalog can store IMU / magnetometer / barometer entries with proper metadata
Existing EtherCAT functionality is untouched
New entry types are recognized by the registry


Sprint 2: IMU Wrapper & Integration (1.5–2 weeks)
Goal: Create clean, catalog-driven wrappers following the same pattern as EncoderWrapper, DigitalInputWrapper, etc.
Tasks

Implement IMUWrapper class (following your existing wrapper style)
Add addIMU() method to WrapperPool
Implement basic calibration and scaling inside the wrapper
Integrate with Application / rtCycle() so IMU data is available to control logic
Update config loading so users can map catalog entries to IMU functions

Acceptance Criteria

IMU data is discoverable via catalog and usable via WrapperPool::imu()
Follows the same registration + mapping pattern as current digital I/O and encoders
No breakage to existing EtherCAT-only code paths


Sprint 3: Magnetometer, Barometer & Polish (1 week)
Tasks

Add MagnetometerWrapper and BarometerWrapper using the same pattern
Implement basic sensor fusion (e.g. Madgwick filter) in IMUWrapper
Add redundancy/fallback logic (e.g. EtherCAT IMU + I2C IMU)
Update documentation and example configs
Full bench validation with mixed backends

Acceptance Criteria

System supports mixed backends (EtherCAT + I2C/SPI) cleanly
All new sensors are configurable via the existing catalog + JSON mechanism
Phase 1.1 is complete and documented


Key Principle to Maintain
Yes — we should continue the pattern:
Backend → discovers devices → registers into Catalog → User maps UUIDs in config → WrapperPool provides typed access
This is the right architectural direction. The IMU (and mag/baro) should be added as new backends (or extensions to a generic sensor backend), not forced into the EtherCAT path.