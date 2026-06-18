# EthercatDrone

**Real-time EtherCAT-enabled drone / robotic control platform**

A high-performance, deterministic control system combining EtherCAT fieldbus with drone/multirotor propulsion for advanced robotics, industrial automation, and research applications.

## Overview

This project integrates **EtherCAT** (sub-100µs cycle times) with modern drone hardware for precise, synchronized motor control, sensor fusion, and real-time decision making. It is designed to run on embedded platforms such as Toradex i.MX8/95 modules with custom real-time C++ controllers.

Key goals:
- Deterministic, low-latency motor control
- Rich telemetry and black-box logging
- Modular architecture for easy hardware swapping
- Support for custom flight algorithms and payloads

## Features

- Full EtherCAT master support with PDO mapping for motor ESCs
- High-rate sensor integration (IMU, GPS, etc.)
- Real-time FOC motor control via supported ESCs
- Fault detection and graceful degradation
- Configurable via Blazor UI or configuration files
- Bi-directional Postgres replication for config/outcome data (optional)
- Designed for 3+ days of independent operation with resync

## Hardware Requirements

- **Main Controller**: Toradex i.MX8/95 or equivalent ARM board
- **Fieldbus**: EtherCAT (or CAN / UAVCAN fallback)
- **Propulsion**: Compatible high-power ESCs/motors (Hobbywing X-series, VESC, or custom)
- **Sensors**: IMU, GPS, barometer, current/voltage monitoring

## Software Stack

- **Language**: C++ (real-time core)
- **UI**: C# Blazor
- **Database**: Postgres + pglogical
- **Build**: CMake / NativeAOT where applicable

## Getting Started

1. Clone the repository
   ```bash
   git clone https://github.com/houserwx/EthercatDrone.git

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

Repository Structure

/src/ — Core real-time C++ code
/ui/ — Blazor web interface
/docs/ — Architecture, setup, and integration guides
/config/ — Example configurations
/tools/ — Utilities and scripts

Roadmap

 Full DroneCAN / Cyphal support
 Advanced trajectory planning
 Multi-vehicle coordination
 ... (add your items)

Contributing
Pull requests are welcome. For major changes, please open an issue first.
License
Copyright © 2026 Jeffrey Houser. All rights reserved.
This repository contains proprietary code. No part of this project may be copied, modified, or distributed without explicit written permission from the author.
See LICENSE for details.
Contact

GitHub Issues (preferred for technical questions)
LinkedIn: Jeffrey Houser