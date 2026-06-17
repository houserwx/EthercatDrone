# Phase 1: Bench / Core Proof of Concept

**Objective:** Validate the core real-time EtherCAT control architecture on the bench with redundant masters before any flight hardware is committed. Adapt the proven CIVControl-ARM conveyor inspection codebase into a drone/UAV control platform using a **modular monorepo architecture** with internal libraries, reusing the PDO system, RT thread infrastructure, gRPC services, and freeze-the-world patterns while adding drone-specific domains (motor/ESC control, IMU fusion, flight safety states, dual-master redundancy).

**Duration:** 6–8 weeks

**Success Criteria:**
- Redundant master failover < 50µs with no loss of control
- Deterministic <100µs control loops proven under load on both Pi and simulated backends
- All major subsystems (PDO, RT loop, safety, logging, gRPC, config) compile and run on target hardware
- SimulatedAdapter can drive a full synthetic flight bench test with zero physical hardware
- Fault injection (master crash, sensor loss, network drop) triggers correct safe-state responses
- Each library (`libcommon`, `libimu`, `libfc`) builds independently and can be unit-tested in isolation

---

## Architecture: Modular Monorepo

Single Git repo with internal CMake libraries. Clean dependency graph:

```
navi  →  flight_controller  →  imu  →  common
                              ↑
                          main (drone_app executable links all)
```

### Folder Structure

```
EtherCatDrone/
├── CMakeLists.txt              # Root superbuild: add_subdirectory(src/*)
├── CMakeUserPresets.json       # debug-linux, release-linux, debug-arm, release-arm
├── build.sh                    # Wrapper: --dev, --clean, --install, --analysis-only, --fix
├── .gitignore
├── README.md
├── docs/                       # WBS, architecture, BOM, wiring, setup guides
├── hardware/                   # Schematics, pinouts, ESI XML files
├── config/                     # hardware.json, vehicle configs, sim configs
├── benchmarks/                 # Latency, jitter, power measurement scripts
├── tests/                      # Unit + integration tests (CTest)
├── scripts/                    # Build, flash, deploy, bench scripts
├── thirdparty/                 # Git submodules (Eigen, etc.)
└── src/
    ├── common/                 # libcommon — shared RT infrastructure
    │   ├── CMakeLists.txt
    │   ├── include/common/
    │   │   ├── math/           # Vec3f, Mat3f, quaternion, fixed-point helpers
    │   │   ├── rt/             # Threadrunner, SignalProcess, VectorBuffer, ThreadBuffer
    │   │   ├── log/            # Logger, sinks (Console, File, LogStream)
    │   │   ├── config/         # Config.h (JSON loading, PdoEntryDef, SimParams)
    │   │   └── messages/       # MessageTypes, LogEntry, string formatters
    │   └── src/
    │       ├── rt/
    │       ├── log/
    │       └── messages/
    │
    ├── imu/                    # libimu — IMU abstraction + calibration
    │   ├── CMakeLists.txt
    │   ├── include/imu/
    │   │   ├── ImuReader.h     # Wraps 6× AnalogInput PDOEntries
    │   │   ├── ImuCalibration.h# Scale/offset calibration, sanity checks
    │   │   └── ImuTypes.h      # ImuRaw, ImuCalibrated, ImuHealth
    │   └── src/
    │
    ├── flight_controller/      # libfc — flight controller (RT core)
    │   ├── CMakeLists.txt
    │   ├── include/fc/
    │   │   ├── pdo/            # PDOEntry, PDO, IHardwareAdapter, HardwareRegistry
    │   │   ├── ethercat/       # EthercatAdapter, HardwareCatalog, SlaveTypeInfo
    │   │   ├── simulated/      # SimulatedAdapter (with IMU/motor sim physics)
    │   │   ├── grpc/           # GrpcAdapter, GrpcTriggerMessage, GrpcResultMessage
    │   │   ├── motor/          # MotorController (throttle, enable, RPM feedback)
    │   │   ├── safety/         # MachineStateController, AlwaysOnEval, RulesEvalData
    │   │   ├── redundancy/     # RedundancyController (heartbeat, failover, election)
    │   │   ├── app/            # Application (RT thread), WrapperPool, FunctionEvaluator
    │   │   └── services/       # GrpcService, proto definitions
    │   └── src/
    │
    ├── navi/                   # libnavi — higher-level navigation (Phase 2+)
    │   ├── CMakeLists.txt
    │   ├── include/navi/
    │   │   ├── vio/            # Visual-inertial odometry (Phase 2)
    │   │   ├── planning/       # Path planning, waypoint navigation (Phase 2)
    │   │   └── commands/       # High-level commands to FC (Phase 2)
    │   └── src/
    │
    └── main/                   # Executables — integration entry points
        ├── CMakeLists.txt
        └── src/
            ├── drone_app.cpp   # Main binary: signal handling, init, RT loop, gRPC
            └── bench_test.cpp  # Bench test harness (sim-only, fault injection)
```

### Library Dependency Rules

| Library | Depends On | Linked By | Purpose |
|---|---|---|---|
| `libcommon` | (none) | `libimu`, `libfc` | RT threading, logging, math, config, messages |
| `libimu` | `libcommon` | `libfc` | IMU reading, calibration, sanity checks |
| `libfc` | `libcommon`, `libimu` | `main`, `libnavi` | PDO system, EtherCAT, motors, safety, redundancy, RT app |
| `libnavi` | `libfc` (transitive: `libimu`, `libcommon`) | `main` | VIO, path planning, high-level commands (Phase 2+) |
| `main` | `libfc` (transitive: `libimu`, `libcommon`) | — | Entry point, integration, test harnesses |

### Why This Structure

- **Clean boundaries:** `libimu` knows nothing about EtherCAT or motors. `libfc` knows about PDO/hardware but not about navigation. Each library has a single responsibility.
- **Independent testing:** `libcommon` and `libimu` can be unit-tested on x86 without any hardware. `libfc` can be tested with `SimulatedAdapter`.
- **GitHub showcase:** Recruiters see a professional monorepo with clear module boundaries, not a flat `src/` dump.
- **Extractable:** Any library can be spun out into its own repo later (e.g., `libimu` as a standalone sensor library) by changing `add_subdirectory` → `find_package`.
- **PX4/ArduPilot pattern:** Matches the industry standard for embedded flight software.

---

## 1.0 Project Setup & Environment

### 1.1 Repository & Monorepo Structure

> **Status:** 1.1.1–1.1.2 partial (git init, .gitignore done). Remaining items below.

- **1.1.1** Initialize GitHub repository with proper structure ✅
- **1.1.2** Create `.gitignore`, `README.md`, `Overview.md`, and CI/CD skeleton ✅ (`.gitignore` done; `README.md` and CI/CD pending)
- **1.1.3** Create monorepo directory structure
  - Create `src/common/`, `src/imu/`, `src/flight_controller/`, `src/navi/`, `src/main/` with `CMakeLists.txt` and `include/` + `src/` subdirectories
  - Create `tests/`, `scripts/`, `benchmarks/`, `hardware/`, `config/`, `thirdparty/`
  - Create placeholder headers in each library's `include/` to validate include paths
- **1.1.4** Create root CMake superbuild
  - Root `CMakeLists.txt`: `cmake_minimum_required(VERSION 3.20)`, `project(EtherCatDrone CXX)`, `set(CMAKE_CXX_STANDARD 20)`
  - Global `find_package(nlohmann_json QUIET)`, `find_library(ETHERCAT_LIB)`, `find_package(gRPC CONFIG QUIET)`, `find_program(PROTOC_EXECUTABLE)`, `find_program(GRPC_CPP_PLUGIN)`
  - Set `GRPC_AVAILABLE` as a cache bool based on gRPC + protoc availability
  - `add_subdirectory(src/common)`, `add_subdirectory(src/imu)`, `add_subdirectory(src/flight_controller)`, `add_subdirectory(src/navi)`, `add_subdirectory(src/main)`
  - `enable_testing()` + `add_subdirectory(tests)` if `BUILD_TESTING` ON
  - `target_compile_options(-Wall -Wextra -Wpedantic)` on all targets
- **1.1.5** Create per-library CMakeLists.txt
  - **`src/common/CMakeLists.txt`:** `add_library(common STATIC ...)`, `target_include_directories(common PUBLIC include)`, `target_link_libraries(common PUBLIC nlohmann_json::nlohmann_json)`, Linux-only `target_link_libraries(common PRIVATE rt)`
  - **`src/imu/CMakeLists.txt`:** `add_library(imu STATIC ...)`, `target_include_directories(imu PUBLIC include)`, `target_link_libraries(imu PUBLIC common)`
  - **`src/flight_controller/CMakeLists.txt`:** `add_library(fc STATIC ...)`, `target_include_directories(fc PUBLIC include)`, `target_link_libraries(fc PUBLIC common imu)`, conditional `target_link_libraries(fc PRIVATE ${ETHERCAT_LIB})`, conditional gRPC linking with `GRPC_AVAILABLE` define and proto compilation
  - **`src/navi/CMakeLists.txt`:** `add_library(navi STATIC ...)`, `target_include_directories(navi PUBLIC include)`, `target_link_libraries(navi PUBLIC fc)` — Phase 2+ library, stub for now
  - **`src/main/CMakeLists.txt`:** `add_executable(drone_app src/drone_app.cpp)`, `target_link_libraries(drone_app PRIVATE fc)`, `add_executable(bench_test src/bench_test.cpp)`, `target_link_libraries(bench_test PRIVATE fc)`
- **1.1.6** Set up cross-compilation toolchain for ARM (Toradex i.MX8/95 target)
  - Install `aarch64-linux-gnu-gcc` ≥ 12 cross-compiler on x86 dev host
  - Create `cmake/aarch64-linux-gnu.cmake` toolchain file (pattern from CIVControl-ARM reference)
  - Validate cross-compile of a trivial C++20 hello-world binary
- **1.1.7** Create build tooling
  - `CMakeUserPresets.json`: `debug-linux`, `release-linux`, `debug-arm`, `release-arm` presets
  - `build.sh`: wrapper script (`--dev`, `--clean`, `--install`, `--analysis-only`, `--fix`, `--build-dir=`)
  - `scripts/linux/install-deps.sh`: apt packages for build + dev dependencies

### 1.2 Development Environment

- **1.2.1** Install and configure EtherCAT master stack (IgH EtherCAT)
  - Install IgH EtherCAT Master ≥ 1.5.2 on dev Pi units (`apt install ethercat-master`)
  - Copy unpatched IgH 1.5.2 headers (`ecrt.h`, `ectty.h`) into `include/ethercat/` for container builds
  - Copy `libethercat.so.1.0.0` into `lib/` with symlinks (`libethercat.so.1`, `libethercat.so`)
  - Verify `ec_master_0` device node creation and `ethercat gen` ESI parsing
- **1.2.2** Set up PREEMPT_RT kernel on development Pi units
  - Flash Raspberry Pi OS with PREEMPT_RT patchset (or use Torizon OS 7.x with RT kernel)
  - Verify `uname -v` shows `PREEMPT_RT`
  - Add `isolcpus=` boot parameter for dedicated RT core (e.g., `isolcpus=3`)
  - Verify `SCHED_FIFO` priority 85+ assignment succeeds with `sudo` / `CAP_SYS_NICE`
- **1.2.3** Configure networking, SSH, and remote debugging tools
  - Static IP assignment for both Pi units (e.g., `192.168.1.100`, `192.168.1.101`)
  - SSH key-based auth from dev host
  - Install `gdb`, `strace`, `valgrind` for remote debugging
  - Set up `tmux` or `screen` for persistent sessions

---

## 2.0 Hardware Bench Setup

### 2.1 Dual Raspberry Pi Redundant Master Platform

- **2.1.1** Procure and assemble 2× Raspberry Pi 5 / CM4 + EtherCAT HATs
  - BOM: 2× RPi 5 (8GB), 2× EtherCAT HAT (e.g., EDATEC ECT1001 or Kunbus I825), PoE HATs optional
  - Active cooling (heatsinks + fans) for sustained RT load
  - Common EtherCAT bus: both HATs share the same slave chain via splitter or daisy-chain
- **2.1.2** Implement primary + hot-spare architecture with heartbeat monitoring
  - Dedicated heartbeat Ethernet link between Pi units (separate from EtherCAT bus)
  - UDP heartbeat packets at 1 kHz on private link (`169.254.0.x/16`)
  - Timeout threshold: 10 missed heartbeats (~10 ms) triggers failover
  - Role negotiation: MAC-address-based election (lower MAC = primary preference)
- **2.1.3** Wire Ethernet ring topology for cable redundancy
  - EtherCAT ring: Master → Slave 1 → Slave 2 → … → Last slave → back to Master (DC-capable)
  - Document physical wiring diagram in `docs/wiring.md`

### 2.2 Actuator & Sensor Bench Rig

- **2.2.1** Connect 4–6 ESCs/motors (or EtherCAT-compatible drives)
  - For bench: use EtherCAT servo drives (e.g., Beckhoff AX5000 or inverter + encoder) as motor substitutes
  - Map each drive to PDO entries: `DigitalOutput` (enable), `AnalogOutput` (speed/ref), `Encoder` (position/velocity feedback)
  - Register in `hardware.json` with UUIDs matching EtherCAT catalog
- **2.2.2** Integrate basic encoder and IMU sensors
  - EtherCAT encoder module (e.g., Beckhoff EL5151 or EL4102) for position reference
  - IMU via EtherCAT (e.g., TWK Krylov CRF58) or simulated via SimulatedAdapter initially
  - Register encoder PDO entries with correct `bitLength` (32-bit for absolute encoders)
- **2.2.3** Build isolated power distribution with redundant BECs
  - Separate power rails: logic (5V), motor (12–24V), EtherCAT bus (isolated)
  - Redundant BEC: primary + backup with diode OR-ing
  - Common ground reference between Pi units and EtherCAT slaves

---

## 3.0 Core Real-Time Software Framework

### 3.1 EtherCAT Master & Adapter Layer

> **Pattern reference:** `CIVControl-ARM/src/hardware/ethercat/EthercatAdapter.{h,cpp}` — already implements IgH master integration with domain registration, DC sync, slave discovery, and PDO mapping. Adapt for drone domain.

- **3.1.1** Port `EthercatAdapter` with domain registration and DC sync
  - Copy `EthercatAdapter.{h,cpp}` from CIVControl-ARM reference into `src/hardware/ethercat/`
  - Adapt `initialize()`: `ecrt_request_master()`, `ecrt_master_activate()`, `ecrt_master_domain_reg_process_cb()`
  - Adapt `onBeforeReadInputs()`: `ecrt_master_receive()` + `ecrt_domain_process()` → fills `domainData_`
  - Adapt `onAfterWriteOutputs()`: `ecrt_domain_queue()` + `ecrt_master_send()` → flushes `domainData_`
  - Verify `waitForCommunication()` blocks until `WC_COMPLETE` or timeout
- **3.1.2** Port slave discovery and PDO mapping from ESI XML
  - Copy `HardwareCatalog.{h,cpp}` and `kSlaveTypes.cpp` (auto-generated by `scripts/parse_esi.py`)
  - Port `discoverSlaves()`: iterate `ecrt_master_slave_count()`, extract vendorId/productCode/revision
  - Port `buildEntries()`: register PDO entries from catalog UUIDs into `pdos_[0].entries` with correct `byteOffset`/`bitOffset` into `domainData_`
  - Port `applyConfig()`: apply `pulseMs`/`debounceMs` from `Config` to PDOEntry signal machines
- **3.1.3** Verify `onBeforeReadInputs()` and `onAfterWriteOutputs()` hooks
  - Confirm zero-copy: `PDOEntry::image` pointers point directly into `domainData_` (IgH-managed buffer)
  - No memcpy between IgH buffer and PDO image — direct pointer aliasing via `std::memcpy` in typed accessors

### 3.2 Hardware Abstraction (PDO System)

> **Pattern reference:** `CIVControl-ARM/src/hardware/PDO.{h,cpp}`, `HardwareRegistry.{h,cpp}`, `IHardwareAdapter.h` — mature, production-tested freeze-the-world pattern. Port as-is with minimal adaptation.

- **3.2.1** Port `PDOEntry`, `PDO`, and `HardwareRegistry`
  - Copy `PDO.{h,cpp}`: `PDOEntry` struct with `EntryType` enum, `DebounceMachine`, `PulseMachine`, `MessageSlot`, typed `read()`/`write()` accessors
  - Copy `IHardwareAdapter.h`: abstract interface with virtual `initialize()`, `onBeforeReadInputs()`, `onAfterWriteOutputs()`, `getPDOs()`
  - Copy `HardwareRegistry.{h,cpp}`: `addBackend()`, `buildUuidMap()`, `freezeForRt()`, `lookupByUuid()`, `readAll()`, `writeAll()`
  - Key invariant: `readAll()`/`writeAll()` iterate `backends_` vector directly (stride-1, cache-friendly, zero map lookup)
- **3.2.2** Port `freezeForRt()` and stable UUID resolution
  - `PDO::freeze()`: shrink `image` and `entries` vectors, re-base `PDOEntry::image` pointers from `PDO::image.data() + byteOffset`
  - `HardwareRegistry::freezeForRt()`: call `PDO::freeze()` on all backend PDOs, rebuild `uuidMap_`
  - After freeze: no `addBackend()`, no vector mutation, no allocation in RT path
- **3.2.3** Port `SimulatedAdapter` for bench testing without physical hardware
  - Copy `SimulatedAdapter.{h,cpp}` from CIVControl-ARM reference
  - Adapt `SimState` struct per-entry: encoder physics (rpm, rollerDiamMm → ticks/cycle fixed-point), DI toggle with variance
  - Verify `onBeforeReadInputs()` writes synthetic values into `pdos_[0].image` before registry read sweep
  - Validate physics path: `incScaled` (ticks/cycle × 2²⁰) + fractional accumulator for sub-cycle precision
  - **Drone-specific simulation additions:**
    - Add simulated IMU entries (accelerometer X/Y/Z, gyroscope X/Y/Z) as `AnalogInput` PDOEntries
    - Add simulated motor feedback entries (current, RPM) as `AnalogInput` PDOEntries
    - Add simulated GPS entries (latitude, longitude, altitude) as special `AnalogInput` pairs

### 3.3 Real-Time Application Loop

> **Pattern reference:** `CIVControl-ARM/src/application/Application.{h,cpp}`, `services/thread/Threadrunner.{h,cpp}`, `hardware/SignalProcess.h` — production RT thread with absolute deadline scheduling, jitter tracking, and SCHED_FIFO.

- **3.3.1** Port `Application::rtCycle()` with read → evaluate → write pattern
  - Copy `Application.{h,cpp}`: `final` class, composition over inheritance, owns `queues_` and `stateMachine_`
  - Copy `Threadrunner.{h,cpp}`: base class with `ThreadConfiguration` (name, cpuCore, priority, useRealtime, stackPrefaultBytes)
  - Port `execute()`: `configureThread()` (CPU pin + SCHED_FIFO) → `prefaultStack()` → `run()`
  - Port `run()`: absolute deadline loop with `clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME)`, jitter measurement, `signalProcessTickNow()`, then `registry_.readAll()` → `rtCycle()` → `registry_.writeAll()`
  - Port `rtCycle()`: `stateMachine_.tick()` → halt gate check → iterate `queues_` calling `q->tick()`
- **3.3.2** Port `signalProcessTickNow()` for deterministic timing
  - Copy `SignalProcess.h`: `signalProcessTickNow()` (calls `clock_gettime` once/cycle, caches in `gSignalProcessNowNs`), `signalProcessNowNs()` (zero-cost cached read)
  - Copy `PulseMachine` and `DebounceMachine` from `SignalProcess.h`: allocation-free, branch-predictable state machines embedded in `PDOEntry`
- **3.3.3** Add jitter monitoring and overrun detection
  - Port `addNsToTs()` and `diffNs()` helpers from `Application.cpp`
  - Track `cycleCount_`, `overrunCount_`, `maxOverrunNs_`, `totalOverNs_` in `Application`
  - Log RT cycle stats every ~5 seconds via `logInfo(MessageId::RT_CYCLE_STATS, ...)`
  - Expose diagnostic accessors: `cycleCount()`, `overrunCount()`, `maxOverrunNs()`

### 3.4 Drone-Specific Control Abstractions

> **New domain layer** — adapts the conveyor Queue/FunctionEvaluator pattern to drone motor/attitude control.

- **3.4.1** Create `MotorController` abstraction
  - One `MotorController` per ESC/motor channel, wraps `AnalogOutputWrapper` (speed reference) + `DigitalOutputWrapper` (enable) + `EncoderWrapper` (RPM feedback)
  - Methods: `setThrottle(float t)` (0.0–1.0), `enable()`, `disable()`, `getRpm()`
  - Throttle → `AnalogOutput::setRawAdc()` mapping with configurable min/max PWM or analog range
  - Safe state: `disable()` → `setThrottle(0.0)` → clear enable DO
- **3.4.2** Create `ImuReader` abstraction
  - Wraps 6× `AnalogInputWrapper` entries (accX, accY, accZ, gyroX, gyroY, gyroZ)
  - Methods: `getAccel(Vec3f&)`, `getGyro(Vec3f&)`, `getRaw()`
  - Raw ADC → calibrated m/s² and rad/s conversion (configurable scale/offset in `hardware.json`)
- **3.4.3** Create `DroneQueue` (extends Queue pattern for drone topology)
  - Reuse `Queue` structure but with drone-specific function types:
    - `MotorMix`: map throttle command → individual motor outputs (configurable mix matrix)
    - `AttitudeHold`: basic PID attitude hold using IMU feedback (Phase 1: P-only, Phase 2: full PID)
    - `HealthMonitor`: check motor RPM bounds, IMU sanity, heartbeat health
  - Register drone functions in `FunctionEvaluator` via new `FunctionType` enum entries
  - **Alternative (simpler for Phase 1):** Skip `DroneQueue` and use a single `Application::rtCycle()` with direct motor/IMU access via `WrapperPool`. Add `DroneQueue` in Phase 2 when mission planning requires pipeline semantics.

---

## 4.0 Redundancy & Safety Core

### 4.1 Master Redundancy

> **New domain** — not present in CIVControl-ARM (single-master conveyor). This is the primary drone-specific addition.

- **4.1.1** Implement heartbeat and failover logic between primary and hot-spare Pi
  - Create `RedundancyController` class (non-RT thread, runs on each Pi):
    - UDP socket on private heartbeat link (`169.254.0.1:12345` ↔ `169.254.0.2:12345`)
    - Send heartbeat packet every 1 ms: `{ role, cycleCount, timestamp, checksum }`
    - Receive thread: track last heartbeat time, detect timeout (>10 ms gap)
    - State machine: `STANDBY` → `PRIMARY` (on startup or peer loss), `PRIMARY` → `STANDBY` (on higher-priority peer return)
  - Failover sequence on primary loss:
    1. Spare detects heartbeat timeout
    2. Spare calls `Application::requestStop()` on its (idle) RT thread
    3. Spare initializes its own `EthercatAdapter` (if not already warm)
    4. Spare calls `registry_.readAll()` to sync current slave state
    5. Spare starts its RT thread → resumes control loop
    6. Target: < 50µs from last primary cycle to first spare cycle (warm spare with pre-initialized EtherCAT)
- **4.1.2** Add state synchronization between masters
  - Shared state packet (sent with each heartbeat): current motor throttle values, machine state, active fault mask
  - Spare maintains shadow copy of all PDO output values
  - On failover: spare writes shadow state to outputs before first RT cycle (minimizes output discontinuity)
  - Use `MachineStateController` state projection for sync: serialize `MachineStatusEvent` into heartbeat payload
- **4.1.3** Test seamless switchover under load
  - Bench test: run both Pis, kill primary process (`kill -9`), measure output gap on oscilloscope/log
  - Verify no motor glitch: throttle values within ±1% across switchover
  - Test split-brain prevention: both Pis claim PRIMARY → higher MAC yields (graceful demotion)

### 4.2 Safety & Machine State

> **Pattern reference:** `CIVControl-ARM/src/application/MachineStateController.{h,cpp}`, `MachineStatePDO.{h,cpp}`, `AlwaysOnEval.{h,cpp}` — production-tested state machine with E-Stop, fault latching, and PDO projection.

- **4.2.1** Port `MachineStateController` with E-Stop, Fault, and Halted states
  - Copy `MachineStateController.{h,cpp}`: `MachineState` enum (`Running`, `Faulted`, `Halted`, `EStop`)
  - Port `tick(bool estop, uint64_t nowNs)`: service `clearAlarmsRequested_`, recompute `faultActive_`/`haltActive_`, publish `MachineStatusEvent` on state change
  - Port `raiseAlarm()` and `clearFault()`: bitmask-based O(1) fault tracking
  - **Drone-specific alarm additions** to `AlarmId` enum:
    - `kMotorOverRpm`, `kMotorUnderRpm`, `kImuSensorFault`, `kHeartbeatLost`, `kBatteryLow`, `kGpsFixLost`, `kAttitudeExceedLimit`
- **4.2.2** Port `AlwaysOnEval` for failsafe rules
  - Copy `AlwaysOnEval.{h,cpp}`: fixed-size array of rule indices, unconditional tick every cycle
  - Wire drone failsafe rules:
    - `EStopActive → MotorDisableAll` (combinatorial: InputHigh on E-Stop DI → FireOutput on all motor enables)
    - `FaultActive → ThrottleLimit(0.0)` (FaultActive flag → set all motor throttles to zero)
    - `HeartbeatLost → RaiseAlarm(kHeartbeatLost, HaltFault)` (redundancy heartbeat timeout → halt)
- **4.2.3** Add global safe-state output drive on halt/E-Stop
  - Port `Queue::safeState()` → `WrapperPool::safeStateAllOutputs()`: iterate all `DigitalOutputWrapper`, clear pulse; iterate all `AnalogOutputWrapper`, zero output
  - In `Application::rtCycle()`: if `stateMachine_.haltActive()` → call `q->safeState()` on all queues, return early (skip normal tick)
  - Verify safe state is reached within 1 RT cycle of E-Stop activation

### 4.3 Drone-Specific Safety Layer

> **New domain** — flight-specific safety not present in conveyor domain.

- **4.3.1** Implement geofence and altitude limit checks
  - `SafetyLimits` struct in config: `maxAltitudeM`, `maxDistanceM`, `minBatteryPercent`
  - Checked in `HealthMonitor` function each cycle (non-blocking: raise alarm, don't halt unless critical)
  - Critical violation → `raiseAlarm(kAltitudeExceeded, HaltFault)` → safe landing sequence
- **4.3.2** Implement motor health monitoring
  - Per-motor RPM bounds check: if `abs(actualRpm - commandedRpm) > threshold` for N consecutive cycles → `raiseAlarm(kMotorOverRpm, Fault)`
  - Motor enable verification: if enable DO set but encoder feedback = 0 for N cycles → `raiseAlarm(kMotorUnderRpm, Fault)`
- **4.3.3** Implement IMU sanity checks
  - Accelerometer magnitude check: `sqrt(accX² + accY² + accZ²)` should be ≈ 9.81 m/s² when stationary
  - Gyro range check: each axis within ±configured max rad/s
  - Out-of-range → `raiseAlarm(kImuSensorFault, HaltFault)`

---

## 5.0 Logging, Config & Diagnostics

### 5.1 Logging System

> **Pattern reference:** `CIVControl-ARM/src/services/log/Logger.{h,cpp}`, `VectorBuffer.h`, `ThreadBuffer.h`, `sinks/ConsoleSink.{h,cpp}`, `sinks/LogStreamSink.{h,cpp}`, `sinks/FileSink.{h,cpp}` — production-tested SPSC ring buffer logger with multiple sinks.

- **5.1.1** Port `Logger`, `VectorBuffer`, and sinks (Console + File)
  - Copy `Logger.{h,cpp}`: singleton with `registerThread()`, `serviceLoop()`, `drainAll()` (RT-priority buffers first)
  - Copy `VectorBuffer.h`: lock-free SPSC ring buffer template, power-of-2 capacity, `tryPush()`/`tryPop()`/`drain()`
  - Copy `ThreadBuffer.h`: SPSC with batch drain via `std::span` callback
  - Copy `ConsoleSink.{h,cpp}`: formatted stdout output with timestamp, level, message
  - Copy `FileSink.{h,cpp}`: rotating file output with size-based rotation
  - Copy `LogHelper.h`: `threadLoggerInit()`, `log()`, `logInfo()` free functions
  - Copy `Message.{h,cpp}` and `MessageTypes.h`: `LogEntry` struct, `MessageId` enum, string formatter
- **5.1.2** Port gRPC `TailLogs` streaming
  - Copy `LogStreamSink.{h,cpp}`: `ILogSink` implementation that pushes to gRPC server-streaming RPC
  - Copy `log_service.proto`: `civ_control.logger.v1.LogService` with `TailLogs` RPC
  - Verify multiple concurrent clients (up to 16) receive fan-out log stream

### 5.2 Configuration Service

> **Pattern reference:** `CIVControl-ARM/src/services/grpc/ConfigServiceImpl.{h,cpp}`, `config_service.proto` — production-tested config CRUD with atomic disk flush.

- **5.2.1** Port `ConfigServiceImpl` with hardware and lane config CRUD
  - Copy `ConfigServiceImpl.{h,cpp}`: `GetHardwareConfig`, `UpdateHardwareConfig`, `ListLanes`, `GetLane`, `CreateOrUpdateLane`, `DeleteLane`
  - Adapt for drone domain: rename "lanes" to "vehicles" or "configurations" (one config per drone setup)
  - Copy `Config.h`: `Config::loadFromJson()`, `PdoEntryDef` with `SimParams` for simulated adapter
  - **Drone-specific config additions:**
    - `motorConfig`: array of motor definitions (uuid, throttleMin, throttleMax, rpmScale, rpmOffset)
    - `imuConfig`: scale/offset calibration for each IMU axis
    - `safetyLimits`: geofence radius, max altitude, min battery
    - `redundancyConfig`: heartbeat interval, timeout, peer IP, role preference
- **5.2.2** Port `SaveAllConfigs` and `RequestRestart`
  - `SaveAllConfigs()`: flush all staged in-memory config changes to disk atomically (write to temp file, rename)
  - `RequestRestart()`: set `restartFlag` atomic → `main()` detects → graceful RT stop → config reload → RT restart
  - Verify zero RT thread involvement: config read only during init phase before RT cycle starts

### 5.3 Station Service (gRPC)

> **Pattern reference:** `CIVControl-ARM/src/services/grpc/StationServiceImpl.{h,cpp}`, `station_service.proto`, `GrpcAdapter.{h,cpp}` — bidirectional streaming relay between RT and external systems.

- **5.3.1** Port `StationServiceImpl` with bidirectional trigger/result relay
  - Copy `StationServiceImpl.{h,cpp}`: `StreamStationChannel` bidirectional streaming RPC
  - Adapt for drone domain: rename "station" to "telemetry" or "mission" service
  - Channel routing: `channel_id = "{vehicle_id}:{channel_id}"` for multiplexing
  - Copy `GrpcTriggerMessage.h` and `GrpcResultMessage.h`: trivially copyable message structs
- **5.3.2** Port `GrpcAdapter` integration for message channels
  - Copy `GrpcAdapter.{h,cpp}`: multi-channel message backend, one per Queue
  - `MessageOut`/`MessageIn` PDOEntry pairs: RT arms once per trigger, adapter pops in `onAfterWriteOutputs()`
  - Sim mode: `simFailMod > 0` → synthesize result immediately (no gRPC relay needed)
  - **Drone-specific message types:**
    - `TelemetryOut`: motor RPMs, IMU data, battery, GPS, machine state → streamed to ground station
    - `MissionIn`: waypoint commands, throttle overrides, mode changes → received from ground station

### 5.4 gRPC Service Host

> **Pattern reference:** `CIVControl-ARM/src/services/grpc/GrpcService.{h,cpp}` — single-port gRPC server hosting all services.

- **5.4.1** Port `GrpcService` thread
  - Copy `GrpcService.{h,cpp}`: `Threadrunner` subclass, hosts `grpc::Server` on port 50051
  - Register all services: `LogStreamServiceImpl`, `ConfigServiceImpl`, `StationServiceImpl`
  - Wire `LogStreamSink` fan-out and `restartFlag` atomic
  - **Drone-specific additions:** register telemetry/mission service implementations
- **5.4.2** Port `main.cpp` entry point
  - Copy `main.cpp` pattern: signal handling, config resolution, Logger init → `mlockall` → hardware registry → Application start/join → gRPC service start/join
  - Add `RedundancyController` startup (non-RT thread)
  - Add graceful shutdown: signal handler → `app->requestStop()` + `grpcService->requestStop()` + `redundancyController->stop()`

---

## 6.0 Testing & Validation

### 6.1 Unit & Integration Tests

> **Pattern reference:** `CIVControl-ARM/tests/` — test infrastructure with CTest.

- **6.1.1** Write tests for core PDO and signal processing types
  - `PDOEntry` read/write: verify bit extraction from image buffer for all `EntryType` values
  - `PulseMachine`: arm → tick → verify pulse duration, rising-edge-only arming, latch mode
  - `DebounceMachine`: verify settle time, noise rejection, edge detection
  - `VectorBuffer`: SPSC correctness, full buffer drop, power-of-2 capacity enforcement
  - `SignalProcess`: `signalProcessTickNow()` caching, `signalProcessNowNs()` zero-cost read
- **6.1.2** Write tests for `RulesEvalData` and `FunctionEvaluator`
  - `RulesEvalData`: compile → evaluate → verify bitmask folding, counter conditions, action execution
  - `FunctionEvaluator`: tick with simulated encoder positions → verify cursor advancement, function dispatch
  - `AlwaysOnEval`: unconditional tick → verify all registered rules execute each cycle
- **6.1.3** Write tests for redundancy failover scenarios
  - `RedundancyController`: heartbeat send/receive, timeout detection, role transition
  - Shadow state sync: verify output values match across primary/spare
  - Split-brain: both claim PRIMARY → verify MAC-based election resolves correctly
- **6.1.4** Write tests for drone-specific abstractions
  - `MotorController`: throttle → ADC mapping, enable/disable, safe state
  - `ImuReader`: raw ADC → calibrated value conversion, sanity check thresholds
  - `MachineStateController`: E-Stop → halt transition, fault raise/clear, alarm latching

### 6.2 Bench System Tests

- **6.2.1** Run full control loop with simulated sensors and actuators
  - Build with `SimulatedAdapter` only (no EtherCAT hardware required)
  - Configure `hardware.json` with simulated motors, IMU, and encoder entries
  - Run `pdo_model` → verify RT cycle runs at target frequency (500 µs or 1 ms)
  - Verify synthetic encoder counts increment, DI toggles, DO pulses fire
  - Verify gRPC `TailLogs` streams startup messages, cycle stats, and simulated events
- **6.2.2** Validate timing, jitter, and determinism (<100µs cycles)
  - Run on PREEMPT_RT Pi with `isolcpus=3` and `SCHED_FIFO` priority 85
  - Measure cycle jitter over 1 hour: `maxOverrunNs` should be < 10µs on idle RT core
  - Load test: add synthetic CPU work in `rtCycle()` → verify overrun detection triggers
  - Compare simulated vs EtherCAT backend cycle times (EtherCAT adds ~10–20µs for receive/process/send)
- **6.2.3** Perform fault injection
  - **Master failure:** `kill -9` primary Pi process → verify spare takes over within 50µs
  - **Sensor loss:** disconnect simulated encoder → verify `kEncoderHealthFault` alarm raised
  - **Network drop:** disable heartbeat link → verify `kHeartbeatLost` alarm and role transition
  - **E-Stop:** assert E-Stop DI → verify all motors disabled within 1 RT cycle
  - **IMU fault:** inject out-of-range IMU values → verify `kImuSensorFault` and halt

### 6.3 Documentation & Knowledge Transfer

- **6.3.1** Update `Overview.md` and create Phase 1 completion report
  - Document architecture decisions: why PDO system, why freeze pattern, why switch dispatch over virtual
  - Document thread model: RT thread (Application), service thread (Logger), gRPC thread (GrpcService), redundancy thread (RedundancyController)
  - Document failover sequence with timing diagram
  - Create `docs/Phase1-Completion-Report.md` with test results, timing measurements, and known limitations
- **6.3.2** Document hardware BOM and wiring diagrams
  - `docs/BOM.md`: complete parts list with part numbers, quantities, estimated cost
  - `docs/Wiring.md`: EtherCAT bus topology, power distribution, heartbeat link, sensor/actuator wiring
  - `docs/Setup.md`: step-by-step Pi image flash, PREEMPT_RT install, EtherCAT master config, network setup

---

## Phase 1 Deliverables Summary

| Deliverable | Description | Verification |
|---|---|---|
| **Dual-Pi redundant EtherCAT master** | Primary + hot-spare with < 50µs failover | Oscilloscope measurement of output continuity across switchover |
| **Real-time control loop < 100µs** | Deterministic RT cycle on PREEMPT_RT with jitter tracking | 1-hour jitter log showing `maxOverrunNs` < 10µs |
| **Motor/ESC control via EtherCAT** | Throttle → PWM/analog mapping with enable/disable | Bench test: motors spin at commanded RPM, stop on disable |
| **IMU sensor integration** | 6-axis IMU read via EtherCAT or simulated | Raw and calibrated values logged via gRPC |
| **Safety state machine** | E-Stop, fault, halt with PDO projection | Fault injection: E-Stop → all motors off within 1 cycle |
| **Logging infrastructure** | SPSC Logger with Console, File, and gRPC sinks | `grpc_cli call TailLogs` shows real-time stream |
| **Config service** | gRPC CRUD for hardware and vehicle configs | Remote config update → `SaveAllConfigs` → `RequestRestart` → reload |
| **Telemetry/mission service** | Bidirectional gRPC streaming for ground station | Ground station receives telemetry, sends mission commands |
| **SimulatedAdapter** | Full synthetic bench test without physical hardware | `pdo_model` runs with sim-only config, produces realistic I/O |
| **Unit & integration tests** | CTest suite for PDO, RT, redundancy, safety | `ctest --output-on-failure` passes with 100% success |
| **Documentation** | Architecture, BOM, wiring, setup, completion report | All `docs/` files complete and reviewed |

---

## Reuse Strategy from CIVControl-ARM Reference

The following subsystems are **directly reusable** (copy + minimal adaptation) from the CIVControl-ARM reference codebase:

| Subsystem | Source Path | Reuse Level | Adaptation Needed |
|---|---|---|---|
| PDO system | `src/hardware/PDO.{h,cpp}` | Direct copy | None — domain-agnostic |
| HardwareRegistry | `src/hardware/HardwareRegistry.{h,cpp}` | Direct copy | None — domain-agnostic |
| IHardwareAdapter | `src/hardware/IHardwareAdapter.h` | Direct copy | None — domain-agnostic |
| EthercatAdapter | `src/hardware/ethercat/EthercatAdapter.{h,cpp}` | Direct copy | None — IgH API is stable |
| SimulatedAdapter | `src/hardware/simulated/SimulatedAdapter.{h,cpp}` | Copy + extend | Add IMU/motor sim entries |
| GrpcAdapter | `src/hardware/grpc/GrpcAdapter.{h,cpp}` | Direct copy | None — domain-agnostic |
| MachineStatePDO | `src/hardware/MachineStatePDO.{h,cpp}` | Direct copy | None — domain-agnostic |
| SignalProcess | `src/hardware/SignalProcess.h` | Direct copy | None — domain-agnostic |
| Application | `src/application/Application.{h,cpp}` | Copy + extend | Add drone control in `rtCycle()` |
| Threadrunner | `src/services/thread/Threadrunner.{h,cpp}` | Direct copy | None — domain-agnostic |
| Logger | `src/services/log/Logger.{h,cpp}` | Direct copy | None — domain-agnostic |
| VectorBuffer | `src/services/thread/VectorBuffer.h` | Direct copy | None — domain-agnostic |
| MachineStateController | `src/application/MachineStateController.{h,cpp}` | Copy + extend | Add drone-specific `AlarmId` entries |
| AlwaysOnEval | `src/application/AlwaysOnEval.{h,cpp}` | Direct copy | Wire drone failsafe rules |
| RulesEvalData | `src/application/RulesEvalData.{h,cpp}` | Direct copy | None — domain-agnostic |
| Config | `src/Config.h` | Copy + extend | Add motor/imu/safety/redundancy config sections |
| gRPC services | `src/services/grpc/` | Copy + extend | Rename station → telemetry/mission |
| Proto files | `src/services/grpc/proto/` | Copy + extend | Add drone-specific message types |
| Build system | `CMakeLists.txt`, `build.sh`, `Dockerfile` | Copy + adapt | Adjust for drone project name |
| Code quality | `Copilot-Agent-Code-Quality-Guidelines.md` | Direct copy | None — applies to all RT C++ |

**New code required (not in CIVControl-ARM):**
- `RedundancyController` (heartbeat, failover, role election, shadow state sync)
- `MotorController` abstraction (throttle mapping, enable/disable, RPM feedback)
- `ImuReader` abstraction (6-axis read, calibration, sanity checks)
- Drone-specific `FunctionType` entries (`MotorMix`, `AttitudeHold`, `HealthMonitor`)
- Drone-specific `AlarmId` entries (motor, IMU, GPS, battery, attitude faults)
- Telemetry/mission gRPC service implementations
- Simulated IMU/motor physics in `SimulatedAdapter`