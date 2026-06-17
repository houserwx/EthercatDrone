Phase 1.2: Hardware Abstraction Layer — Deep Dive & Full Implementation
Objective: Build a clean, extensible, multi-backend hardware abstraction layer that follows your existing dynamic discovery → catalog → wrapper pattern. This will serve as the foundation for all future sensors and actuators.
Duration: 6–8 weeks
Success Criteria: All major backends (EtherCAT, I2C, SPI, GPIO, GRPC) are implemented consistently. New sensors can be added by creating a backend + catalog entry + wrapper without touching core RT code.

1.0 Overall Architecture & Design (Week 1)
1.1 Define Clear Layer Responsibilities

Wrapper Layer (User-facing): IMUWrapper, DigitalInputWrapper, etc. — high-level, typed, easy to use in rules/functions.
PDO Layer (Data transport): PDOEntry + PDO — raw memory image, type-safe accessors.
Adapter Layer (Backend-specific): EthercatAdapter, I2CAdapter, etc. — knows how to talk to physical bus.
Registry Layer: HardwareRegistry — owns all adapters, builds catalog, provides lookup.

1.2 Core Design Principles (Document These)

Each backend owns its PDOs.
Discovery → Catalog registration is automatic where possible.
UUIDs are the only stable identity.
RT hot path uses direct wrapper access (no virtual calls per entry).


2.0 Wrapper Layer — Full Implementation (Weeks 1–2)
Goal: Make wrappers the primary API for application code.
Detailed Tasks & Code
2.1 Base Wrapper
C++// WrapperBase.h
#pragma once
#include "PDO.h"

class WrapperBase {
protected:
    std::string name_;
    PDOEntry& entry_;

public:
    WrapperBase(std::string name, PDOEntry& entry)
        : name_(std::move(name)), entry_(entry) {}

    virtual ~WrapperBase() = default;
    [[nodiscard]] const std::string& name() const { return name_; }
    [[nodiscard]] PDOEntry& rawEntry() { return entry_; }
};
2.2 Core Wrappers (Extend Existing)

DigitalInputWrapper, DigitalOutputWrapper
EncoderWrapper (already good)
New: AnalogInputWrapper, AnalogOutputWrapper, PWMWrapper

2.3 Sensor Wrappers
C++// IMUWrapper.h
class IMUWrapper : public WrapperBase {
public:
    IMUWrapper(std::string name, PDOEntry& gyroEntry, PDOEntry& accelEntry);

    [[nodiscard]] Eigen::Vector3f getGyro() const;      // rad/s
    [[nodiscard]] Eigen::Vector3f getAccel() const;     // m/s²
    void update();                                      // called every RT cycle

private:
    PDOEntry& gyroEntry_;
    PDOEntry& accelEntry_;
    // Optional fusion state
};
Similar structure for MagnetometerWrapper and BarometerWrapper.
2.4 Message Wrappers (already partially exist — formalize them)

3.0 PDO Layer Enhancements (Week 2)
3.1 Extend EntryType
C++enum class EntryType : uint8_t {
    DigitalInput, DigitalOutput,
    Encoder,
    AnalogInput, AnalogOutput,
    IMU_Gyro, IMU_Accel,
    Magnetometer, Barometer,
    MessageOut, MessageIn,
    PWM,
    Raw
};
3.2 Rich Accessors in PDOEntry
C++class PDOEntry {
    // ... existing fields ...

    [[nodiscard]] Eigen::Vector3f getGyro() const;
    [[nodiscard]] Eigen::Vector3f getAccel() const;
    [[nodiscard]] Eigen::Vector3f getMagneticField() const;
    [[nodiscard]] float getPressure() const;           // hPa
    [[nodiscard]] float getPWM() const;
};
3.3 Improve PDO::freeze() — add validation and debug printing.

4.0 Backend Adapters — Full Implementation (Weeks 3–6)
4.1 EtherCAT Backend (Refine existing — 1 week)

Ensure support for high-speed terminals (EL3632, ELM360x)
Improve DC sync and distributed clock handling

4.2 I2C Backend (New — 1.5 weeks)
C++class I2CAdapter : public IHardwareAdapter {
public:
    bool initialize() override;
    void onBeforeReadInputs() override;
    void onAfterWriteOutputs() override;

private:
    int i2cBus_;
    std::vector<I2CDevice*> devices_;   // discovered devices
};
4.3 SPI Backend (New — 1 week)
Similar structure to I2CAdapter.
4.4 GPIO Backend (New — 0.5 week)
Simple direct GPIO pin control.
4.5 GRPC Backend (Enhance existing)
Already strong — ensure it fits the new wrapper pattern cleanly.
4.6 CAN Backend (Stub for future — 0.5 week)

5.0 HardwareRegistry & Catalog Integration (Week 6)
Tasks

Update addBackend() to support new adapter types
Improve catalog registration for I2C/SPI devices
Add automatic backend selection logic based on catalog metadata