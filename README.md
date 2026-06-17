# EtherCatDrone

Hard real-time EtherCAT-controlled UAV platform with dual-master redundancy, deterministic control loops, and gRPC telemetry.

## Architecture

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

## Requirements

| Tool | Version | Notes |
|---|---|---|
| CMake | 3.20+ | |
| C++ compiler | GCC 12+ or Clang 14+ (C++20) | |
| Ninja | latest | Preferred generator |
| IgH EtherCAT Master | latest | Required for EtherCAT hardware; not needed for simulated mode |
| gRPC + protobuf | 1.51.1 / 3.21.x | `apt install libgrpc++-dev protobuf-compiler-grpc` |

## Build

```bash
./build.sh                    # Standard build → build/debug-linux/
./build.sh --dev              # Build + cppcheck + clang-tidy
./build.sh --clean            # Clean build directory first
./build.sh --install          # Install build and dev dependencies
./build.sh --analysis-only    # Run static analysis without rebuilding
./build.sh --fix              # Apply safe clang-tidy fixes automatically
```

## Run

```bash
./build/debug-linux/drone_app                                    # default config
./build/debug-linux/drone_app config/factory-test/hardware.json  # alternate config
./build/debug-linux/bench_test                                   # sim-only bench test
```

## gRPC Services (port 50051)

| Service | Package | Key RPCs |
|---|---|---|
| **LogService** | `ethercat_drone.logger.v1` | `TailLogs` — server-streaming live log feed |
| **ConfigService** | `ethercat_drone.config.v1` | Config CRUD + `SaveAllConfigs`, `RequestRestart` |
| **StationService** | `ethercat_drone.station.v1` | `StreamStationChannel` — bidirectional trigger/result relay |

## Run Tests

```bash
cd build/debug-linux && ctest --output-on-failure
```

## Documentation

- [Phase 1 WBS](docs/WBS1.md) — Detailed work breakdown structure
- [Project Overview](docs/Overview.md) — High-level phased development plan

## Code Quality

See [Copilot-Agent-Code-Quality-Guidelines.md](Copilot-Agent-Code-Quality-Guidelines.md) for the coding standards enforced in this project — zero-allocation RT paths, freeze-the-world pattern, `noexcept` everywhere in the hot path.
