WBS for Next Steps (Phase 1 Focus)
Phase 1 Goal: Get a working bench prototype with dual Pi redundancy, basic control loop, and sensor integration.
Sprint 1: Core Infrastructure & Build (1 week)

Finalize CMake structure and library dependencies
Implement basic RT Threadrunner with CPU pinning and SCHED_FIFO
Add VectorBuffer + Logger with lock-free sinks
Set up SimulatedAdapter for bench testing without hardware
Add unit test framework (Catch2 or GoogleTest)

Sprint 2: EtherCAT Backend & PDO System (1–1.5 weeks)

Implement full EthercatAdapter with DC sync and slave discovery
Complete HardwareRegistry and central HardwareCatalog
Add PDOEntry + typed accessors (Gyro, Accel, Digital I/O, etc.)
Integrate EJ/EL modules (EL2124, EL1124, EL3632, etc.)
Basic redundancy logic (primary + hot-spare master)

Sprint 3: Sensor Integration & Wrappers (1.5 weeks)

I2CAdapter for your purchased sensors (ICM-42688-P, BMP581, QMC5883L)
Create IMUWrapper, BarometerWrapper, MagnetometerWrapper
Implement basic sensor fusion (Madgwick)
Add PCA9685 PWM driver for motor control
Config-driven mapping (UUID → wrapper)

Sprint 4: Safety, Logging & gRPC (1–1.5 weeks)

MachineStateController with E-Stop and latching alarms
Full logging + gRPC TailLogs
StationService for trigger/result streaming
Basic failsafe rules (AlwaysOnEval)

Sprint 5: Bench Testing & Documentation (1 week)

Full bench test with simulated + real sensors
Redundancy failover testing
Update Overview.md, WBS, and README
Add CI/CD basics (build + static analysis)