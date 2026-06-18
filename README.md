# EtherCatDrone

Hard real-time EtherCAT-controlled UAV platform with dual-master redundancy, deterministic control loops, and gRPC telemetry.

---

## Hardware Architecture

Three embedded devices, two hard-real-time and one best-effort:

```
┌──────────────────────┐   ┌──────────────────────┐
│  RT Flyboard A       │   │  RT Flyboard B       │
│  (Primary Master)    │◄─►│  (Hot-Standby Master)│
│  • EtherCAT master   │   │  • EtherCAT master   │
│  • IMU + Gyro        │   │  • IMU + Gyro        │
│  • Motor/ESC control │   │  • Motor/ESC control │
│  • Safety interlocks │   │  • Safety interlocks │
└──────────┬───────────┘   └──────────┬───────────┘
           │ EtherCAT bus              │ Heartbeat
           ▼                           │ sync
     ESCs / Motors / Sensors          │
                                      ▼
                       ┌──────────────────────┐
                       │  Companion Board     │
                       │  (UI / Vision)       │
                       │  • gRPC telemetry UI │
                       │  • Machine vision    │
                       │  • Targeting / VIO   │
                       │  • Mission planning  │
                       └──────────────────────┘
```

| Board | Role | RT Class | Software |
|---|---|---|---|
| **Flyboard A** | Primary EtherCAT master, IMU, motor control | Hard RT (`SCHED_FIFO`) | `drone_app` — C++20 |
| **Flyboard B** | Hot-standby master, redundant IMU, motor control | Hard RT (`SCHED_FIFO`) | `drone_app` — C++20 |
| **Companion** | UI, vision, targeting, mission planning | Best-effort | gRPC clients, Blazor UI (TBD) |

### Hardware Requirements

- **Flyboards (×2)**: Raspberry Pi 5 / CM5 or equivalent with EtherCAT HAT (SOEM or Kunbus/EDATEC compatible)
- **Companion Board**: Toradex i.MX8/95, Jetson Orin, or equivalent with GPU for vision
- **Propulsion**: High-power ESCs/motors compatible with EtherCAT (Hobbywing X-series, VESC, or custom)
- **Sensors**: EtherCAT/I²C IMU per flyboard, GPS on companion
- **Network**: Ethernet switch for flyboard heartbeat sync; separate link to companion for gRPC telemetry

---

## Software Architecture

Modular monorepo with internal CMake libraries:

```
navi  →  flight_controller  →  imu  →  common
                              ↑
                          main (drone_app executable links all)
```

| Library | Purpose |
|---|---|
| `libcommon` | RT threading, logging, math, config, messages |
| `libimu` | IMU reading, calibration, sanity checks |
| `libfc` | PDO system, EtherCAT, motors, safety, redundancy, RT app |
| `libnavi` | VIO, path planning, high-level commands (Phase 2+) |

---

## Hardware Backend Adapters

All hardware communication goes through the **`IHardwareAdapter`** interface — a single abstract class with only 2 virtual calls per backend per RT cycle. This keeps the hot path deterministic while allowing any transport protocol to plug in.

### Backend Status

| Backend | Transport | Status | Notes |
|---|---|---|---|
| **EthercatAdapter** | EtherCAT (IgH Master) | ✅ Production | Full PDO mapping, DC sync, hardware catalog, slave scan |
| **SimulatedAdapter** | In-process synthetic | ✅ Production | Physics-based IMU/motor simulation, fault injection |
| **I2CAdapter** | Linux I²C sysfs/dev | 🔧 Stub | Structure complete; real I²C comms pending hardware |
| **SPIAdapter** | Linux SPI sysfs/dev | 🔧 Stub | Structure complete; real SPI comms pending hardware |
| **GrpcAdapter** | gRPC message channels | ✅ Production | Bidirectional trigger/result relay with VectorBuffer |

### Planned Backends

| Backend | Transport | Phase | Notes |
|---|---|---|---|
| **GPIOAdapter** | Linux sysfs/gpiochip | Phase 2 | Digital I/O for E-Stop, indicators, relay control |
| **CANAdapter** | SocketCAN / UAVCAN | Phase 3 | DroneCAN/Cyphal ecosystem integration |
| **USBAdapter** | libusb | Phase 3 | Serial telemetry radios, USB-connected sensors |

### Adding a New Backend

Implementing a new hardware backend takes ~3 steps and ~100 lines of code:

```
1. Create a class inheriting from fc::pdo::IHardwareAdapter
2. Implement initialize(), onBeforeReadInputs(), onAfterWriteOutputs()
3. Register PDO entries during init — the RT cycle handles the rest
```

The `I2CAdapter` and `SPIAdapter` are good reference implementations — both are short, well-commented, and follow the same pattern. No changes to the core RT cycle, PDO system, or application layer are needed.

```cpp
// Minimal example: custom ADC backend
class MyADCAdapter : public fc::pdo::IHardwareAdapter {
public:
    bool initialize() override {
        // Open device, register PDOs, create entries
        return true;
    }
    void onBeforeReadInputs() noexcept override {
        // Read raw ADC values into PDO image buffers
    }
    void onAfterWriteOutputs() noexcept override {
        // Flush any DAC outputs from PDO image buffers
    }
};
```

---

## Requirements

| Tool | Version | Notes |
|---|---|---|
| CMake | 3.20+ | |
| C++ compiler | GCC 12+ or Clang 14+ (C++20) | |
| Ninja | latest | Preferred generator |
| IgH EtherCAT Master | latest | Required for EtherCAT hardware; not needed for simulated mode |
| gRPC + protobuf | 1.51.1 / 3.21.x | `apt install libgrpc++-dev protobuf-compiler-grpc` |

---

## Quick Start

```bash
# 1. Install dependencies
./build.sh --install               # Build deps only
./build.sh --install --dev         # Build + dev tools (clang-tidy, cppcheck)

# 2. Build
./build.sh                         # Standard debug build
./build.sh --release               # Release build
./build.sh --dev                   # Build + static analysis
./build.sh --test                  # Build + run CTest suite
./build.sh --clean                 # Clean build dir first
./build.sh --fix                   # Apply clang-tidy auto-fixes

# 3. Run
./build/debug-linux/src/main/drone_app       # Default config
./build/debug-linux/src/main/bench_test      # Sim-only bench test
```

## Cross-Compile & Deploy

```bash
# Cross-compile for ARM64 and deploy to target boards
./scripts/deploy.sh --flyboard-a          # Deploy to primary RT board
./scripts/deploy.sh --flyboard-b          # Deploy to hot-standby board
./scripts/deploy.sh --companion           # Deploy to companion board
./scripts/deploy.sh --all                 # Deploy to all targets
./scripts/deploy.sh --all --dry-run       # Preview without deploying
./scripts/deploy.sh --all --test          # Host tests before deploy
./scripts/deploy.sh --clean --all         # Fresh cross-compile + deploy
```

Target IPs and paths are configurable via environment variables. See `scripts/deploy.sh --help`.

---

## Run

```bash
./build/debug-linux/src/main/drone_app             # default config
./build/debug-linux/src/main/drone_app <config>    # alternate config
./build/debug-linux/src/main/bench_test            # sim-only bench test
./build/debug-linux/src/main/mission_bench_test    # mission simulation
```

---

## gRPC Services (port 50051)

| Service | Package | Key RPCs |
|---|---|---|
| **LogService** | `ethercat_drone.logger.v1` | `TailLogs` — server-streaming live log feed |
| **ConfigService** | `ethercat_drone.config.v1` | Config CRUD + `SaveAllConfigs`, `RequestRestart` |
| **StationService** | `ethercat_drone.station.v1` | `StreamStationChannel` — bidirectional trigger/result relay |

---

## Tests

```bash
cd build/debug-linux && ctest --output-on-failure
```

---

## Documentation

- [Project Overview](docs/Overview.md) — High-level phased development plan
- [Phase 1 WBS](docs/WBS1.md) — Bench / core proof of concept
- [Phase 2 Mission Planning](docs/WBS1.3.md) — Navigation and mission capabilities
- [Copilot Agent Guidelines](Copilot-Agent-Code-Quality-Guidelines.md) — RT determinism rules, coding standards

---

## Key RT Design Principles

- **Zero-allocation RT paths** — All heap allocation completes before the RT loop starts
- **Freeze-the-world pattern** — No mutable shared state during `rtCycle()`
- **`noexcept` hot path** — Every method in the call graph from `run()` through `tick()` is `noexcept`
- **Sub-100µs control loops** — Proven on bench with both real EtherCAT and simulated backends
- **Dual-master failover** — < 50µs switchover with no loss of motor control

---

## License

Copyright © 2026 Jeffrey Houser. All rights reserved.

This repository contains proprietary code. No part of this project may be copied, modified, or distributed without explicit written permission from the author.

https://www.linkedin.com/in/jeffrey-houser-0558855/
