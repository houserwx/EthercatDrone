# Morphing Star Hybrid Quad-Glider UAV Concept

## Project Overview
This project explores a novel heavy-lift morphing hybrid UAV design combining quadrotor VTOL capabilities with a deployable cruciform (4-point star) membrane glider system for extended endurance, efficiency, and agile intercept operations.

**Key Features:**
- Central "X" linear actuator system for shape morphing via pushrods to four deployable arms.
- Fabric membrane over thin carbon rods for lightweight, collapsible wings.
- Variable collective pitch on large props (pushrod actuated) for rapid thrust response.
- Designed around high-power Hobbywing 45KV motors on 18S with 63x24" props (target 32.5-37.5kg takeoff, up to 72kg thrust).
- Modes: Compact Quad, Transition, Efficient Glide/Loiter (with morphing adaptation), High-Speed Attack/Dive.

The system enables persistent loitering with on-the-fly reconfiguration and rapid transition to aggressive maneuvers, ideal for surveillance, defense, or long-range missions.

## Novel Aspects & Differentiation
While variable-pitch quads and morphing-wing UAVs exist in research (e.g., QuadGlider bio-inspired designs, quadplanes), this concept integrates:
- Centralized heavy-lift actuation for a large-scale cruciform membrane star.
- Direction-adaptive morphing combined with industrial-grade variable pitch propulsion.
- Persistent loiter-to-intercept workflow optimized for real-world heavy payloads.

**Note on Prior Art**: Elements draw inspiration from existing bio-inspired morphing (flying squirrel drones) and hybrid VTOLs. This implementation focuses on unique scaling, central control architecture, and specific heavy-lift integration for practical applications.

## IP Protection Statement
**All rights reserved. Concept, designs, diagrams, and implementations herein are proprietary to Recursed Studios (2026).**

This document and associated code, CAD, and media are protected under:
- Copyright (© 2026 Recursed Studios) – All original text, diagrams, code, and creative expressions.
- Trade Secret protections for novel integration details, control algorithms, and specific morphing mechanisms.
- Patent Pending status recommended for unique central X-actuator + pushrod morphing in heavy-lift cruciform configuration (provisional filing advised).

**Usage Restrictions**:
- This is a public showcase portfolio piece for employment demonstration only.
- No commercial use, reproduction, or derivative works without explicit written permission.
- Viewers may not implement, build, or disclose details beyond personal review.
- Any forks, stars, or discussions must respect these protections. Contact for collaboration opportunities.

Violations will be pursued to the fullest extent of law. For licensing or partnership inquiries, reach out via GitHub.

## Technical Highlights
- **Morphing Mechanism**: Central actuators minimize arm mass; enables asymmetric camber/dihedral/sweep per flight condition.
- **Propulsion**: Hobbywing high-power setup with planned variable pitch for superior agility and efficiency.
- **Control**: Custom mixing for seamless quad-to-glider transitions and dynamic shape optimization, leveraging EtherCatDrone's real-time deterministic EtherCAT backbone with redundancy.
- **Applications**: Long-endurance ISR, autonomous intercept, heavy payload delivery with VTOL flexibility.

## Development Status
[Link to repo sections, CAD, simulations, prototypes as available]

**EtherCatDrone Platform (Foundational Control Layer)**: The system is built on https://github.com/houserwx/EthercatDrone — a hard real-time EtherCAT-controlled UAV platform featuring dual-master redundancy, deterministic control loops (<100μs cycles), gRPC telemetry, and backend-agnostic design for diverse motors/sensors. Modular C++20 architecture with libraries for common RT utilities, IMU, flight control (PDO/EtherCAT/safety), and navigation. Strict code quality (zero-allocation hot paths, clang-tidy, etc.) and WBS-driven phased development.

## Future Work
- Full variable pitch implementation.
- AI-driven autonomous morphing and threat response.
- Flight testing and optimization.
- Deeper integration with EtherCatDrone's redundant controllers, mission planning abstractions, and support for additional backends/sensors.

---

**Confidential & Proprietary – Recursed Studios 2026**  
This concept is presented as part of a professional portfolio to demonstrate innovative engineering capabilities in embedded systems, UAV design, and real-time control. All IP protections apply.