# WBS1.1 Implementation Plan

## Architecture Overview

WBS1.1 extends the existing backend → catalog → wrapper pattern to support:
1. **Foundational wrappers** from CIVControl-ARM reference (DigitalInput, DigitalOutput, AnalogInput, AnalogOutput, Encoder, MessageIn/Out, Detect)
2. **New IMU sensor support** via I2C/SPI backends (not EtherCAT)

### Current Architecture
```
EtherCAT Bus → EthercatAdapter → PDO entries → HardwareRegistry → UUID lookup → (no wrappers yet)
```

### Extended Architecture (after WBS1.1)
```
EtherCAT Bus → EthercatAdapter → PDO entries ─┐
I2C Bus    → I2CAdapter      → PDO entries ──┼→ HardwareRegistry → UUID lookup → WrapperPool → Wrappers
SPI Bus    → SPIAdapter      → PDO entries ──┘
```

**Key insight:** I2C/SPI adapters use the SAME `IHardwareAdapter` interface and populate the SAME `PDO` structures. The RT cycle treats all backends identically. The only difference is how each backend fills its process image buffers in `onBeforeReadInputs()` / `onAfterWriteOutputs()`.

**Reference implementation:** All wrapper patterns are adapted from `CIVControl-ARM/src/application/Wrappers/` with the following design principles:
- **Header-only wrappers** — all in `.h` files, no `.cpp`, maximum inlining
- **No inheritance** — each wrapper is a standalone concrete `final` class
- **Reference semantics** — wrappers hold `PDOEntry&` (or `PDOEntry*` for optional entries), resolved once at init
- **UUID → PDOEntry& → Wrapper** mapping chain, with UUID lookup at init only
- **Integer-indexed access** — `WrapperPool::addOutput()` returns stable `int` index
- **Freeze pattern** — `shrink_to_fit()` all vectors before RT thread starts
- **`noexcept` everywhere** — all RT-path methods are `noexcept`
- **Direct accessors** — `pool.output(idx)` returns reference, not wrapped in getter

---

## Implementation Order

### Phase A: Foundation (EntryType + PDOEntry extensions)

#### 1. Extend EntryType enum (`fc/pdo/PDO.h`)
Add new entry types for IMU sensor data:
```cpp
enum class EntryType : uint8_t {
    DigitalInput  = 0,
    DigitalOutput = 1,
    Encoder       = 2,
    AnalogInput   = 3,
    AnalogOutput  = 4,
    MessageOut    = 5,
    MessageIn     = 6,
    // New sensor types for I2C/SPI backends
    IMU_GyroX     = 7,
    IMU_GyroY     = 8,
    IMU_GyroZ     = 9,
    IMU_AccelX    = 10,
    IMU_AccelY    = 11,
    IMU_AccelZ    = 12,
    MagnetometerX = 13,
    MagnetometerY = 14,
    MagnetometerZ = 15,
    Barometer     = 16,
};
```

#### 2. Extend PDOEntry with typed accessors (`fc/pdo/PDO.h` + `fc/pdo/PDO.cpp`)
Add cached values and accessors for new types:
```cpp
// New cached values in PDOEntry private section:
float gyroXVal_{0.0f};
float gyroYVal_{0.0f};
float gyroZVal_{0.0f};
float accelXVal_{0.0f};
float accelYVal_{0.0f};
float accelZVal_{0.0f};
float magXVal_{0.0f};
float magYVal_{0.0f};
float magZVal_{0.0f};
float baroPressureVal_{0.0f};
float baroAltitudeVal_{0.0f};

// New public accessors:
[[nodiscard]] float getGyroX()  const noexcept { return gyroXVal_; }
[[nodiscard]] float getGyroY()  const noexcept { return gyroYVal_; }
[[nodiscard]] float getGyroZ()  const noexcept { return gyroZVal_; }
[[nodiscard]] float getAccelX() const noexcept { return accelXVal_; }
[[nodiscard]] float getAccelY() const noexcept { return accelYVal_; }
[[nodiscard]] float getAccelZ() const noexcept { return accelZVal_; }
[[nodiscard]] float getMagX()   const noexcept { return magXVal_; }
[[nodiscard]] float getMagY()   const noexcept { return magYVal_; }
[[nodiscard]] float getMagZ()   const noexcept { return magZVal_; }
[[nodiscard]] float getBaroPressure() const noexcept { return baroPressureVal_; }
[[nodiscard]] float getBaroAltitude() const noexcept { return baroAltitudeVal_; }

void setGyroX(float v) noexcept { gyroXVal_ = v; }
// ... etc for all axes
```

Update `read()` to handle new types (read from image buffer into cached values).

### Phase B: Sensor Catalog (non-EtherCAT device discovery)

#### 3. Create SensorCatalog (`fc/sensor/SensorCatalog.h` + `.cpp`)
General-purpose catalog for I2C/SPI/GPIO devices:
```cpp
namespace fc::sensor {

struct SensorEntry {
    std::string  uuid;
    std::string  name;           // "MPU6050", "BMP280", "IST8310"
    std::string  manufacturer;
    std::string  busType;        // "I2C", "SPI", "GPIO"
    std::string  busPath;        // "/dev/i2c-1", "/dev/spidev0.0"
    uint16_t     busAddress;     // I2C address or SPI chip select
    uint32_t     vendorId;       // Optional: vendor-specific ID
    uint32_t     productCode;    // Optional: product-specific ID
    bool         allocated{false};
};

class SensorCatalog {
public:
    bool load(const std::string& path);
    void save(const std::string& path) const;
    [[nodiscard]] const SensorEntry* find(const std::string& uuid) const noexcept;
    [[nodiscard]] const SensorEntry* findByName(const std::string& name) const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    void addEntry(SensorEntry entry);
    [[nodiscard]] const std::vector<SensorEntry>& entries() const noexcept;
private:
    std::vector<SensorEntry> entries_;
};

} // namespace fc::sensor
```

### Phase C: I2C/SPI Backend Adapters

#### 4. Create I2CAdapter (`fc/i2c/I2CAdapter.h` + `.cpp`)
Implements `IHardwareAdapter` for I2C sensors:
```cpp
namespace fc::i2c {

class I2CAdapter final : public fc::pdo::IHardwareAdapter {
public:
    I2CAdapter(const std::string& busPath, fc::sensor::SensorCatalog& catalog);
    ~I2CAdapter() override = default;

    bool initialize() override;
    void onBeforeReadInputs()  noexcept override;
    void onAfterWriteOutputs() noexcept override;

    // I2C-specific helpers
    bool writeRegister(uint16_t deviceAddr, uint8_t reg, uint8_t value);
    bool readRegister(uint16_t deviceAddr, uint8_t reg, uint8_t& value);
    bool readRegisters(uint16_t deviceAddr, uint8_t reg, std::vector<uint8_t>& values, size_t count);

private:
    std::string busPath_;
    fc::sensor::SensorCatalog& catalog_;
    int i2cFd_{-1};
    std::vector<uint16_t> deviceAddresses_;
};

} // namespace fc::i2c
```

**Key design:** Each I2C sensor gets its own PDO with entries for each sensor axis. The `onBeforeReadInputs()` reads raw data from I2C devices into the PDO image buffers.

#### 5. Create SPIAdapter (`fc/spi/SPIAdapter.h` + `.cpp`)
Same pattern as I2CAdapter but for SPI devices:
```cpp
namespace fc::spi {

class SPIAdapter final : public fc::pdo::IHardwareAdapter {
public:
    SPIAdapter(const std::string& busPath, fc::sensor::SensorCatalog& catalog);
    // ... same interface as I2CAdapter
};

} // namespace fc::spi
```

### Phase D: Wrapper Classes

#### 6. Create WrapperPool (`fc/app/WrapperPool.h` + `.cpp`)
Central typed access to all hardware:
```cpp
namespace fc::app {

class WrapperPool final {
public:
    // Registration (init-time only)
    void addIMU(std::unique_ptr<IMUWrapper> wrapper);
    void addMagnetometer(std::unique_ptr<MagnetometerWrapper> wrapper);
    void addBarometer(std::unique_ptr<BarometerWrapper> wrapper);

    // Accessors (RT-safe)
    [[nodiscard]] const IMUWrapper* imu(size_t index) const noexcept;
    [[nodiscard]] const MagnetometerWrapper* magnetometer(size_t index) const noexcept;
    [[nodiscard]] const BarometerWrapper* barometer(size_t index) const noexcept;

    // Counts
    [[nodiscard]] size_t imuCount() const noexcept;
    [[nodiscard]] size_t magnetometerCount() const noexcept;
    [[nodiscard]] size_t barometerCount() const noexcept;

    // Safety
    void safeStateAllOutputs() noexcept;

private:
    std::vector<std::unique_ptr<IMUWrapper>> imus_;
    std::vector<std::unique_ptr<MagnetometerWrapper>> magnetometers_;
    std::vector<std::unique_ptr<BarometerWrapper>> barometers_;
};

} // namespace fc::app
```

#### 7. Create IMUWrapper (`fc/wrapper/IMUWrapper.h` + `.cpp`)
Thin wrapper around 6 PDOEntry pointers (gyro X/Y/Z + accel X/Y/Z):
```cpp
namespace fc::wrapper {

class IMUWrapper final {
public:
    IMUWrapper(const std::string& id,
               fc::pdo::PDOEntry* gyroX, fc::pdo::PDOEntry* gyroY, fc::pdo::PDOEntry* gyroZ,
               fc::pdo::PDOEntry* accelX, fc::pdo::PDOEntry* accelY, fc::pdo::PDOEntry* accelZ,
               const imu::ImuCalibration& cal);

    [[nodiscard]] std::string_view id() const noexcept;
    [[nodiscard]] imu::ImuRaw readRaw() const noexcept;
    [[nodiscard]] imu::ImuCalibrated readCalibrated() const noexcept;
    [[nodiscard]] imu::ImuHealth checkHealth() const noexcept;

private:
    std::string id_;
    fc::pdo::PDOEntry* gyroX_{nullptr};
    fc::pdo::PDOEntry* gyroY_{nullptr};
    fc::pdo::PDOEntry* gyroZ_{nullptr};
    fc::pdo::PDOEntry* accelX_{nullptr};
    fc::pdo::PDOEntry* accelY_{nullptr};
    fc::pdo::PDOEntry* accelZ_{nullptr};
    imu::ImuCalibration cal_;
};

} // namespace fc::wrapper
```

#### 8. Create MagnetometerWrapper (`fc/wrapper/MagnetometerWrapper.h` + `.cpp`)
```cpp
namespace fc::wrapper {

class MagnetometerWrapper final {
public:
    MagnetometerWrapper(const std::string& id,
                        fc::pdo::PDOEntry* magX, fc::pdo::PDOEntry* magY, fc::pdo::PDOEntry* magZ);

    [[nodiscard]] common::math::Vec3f read() const noexcept;
    // ...
};

} // namespace fc::wrapper
```

#### 9. Create BarometerWrapper (`fc/wrapper/BarometerWrapper.h` + `.cpp`)
```cpp
namespace fc::wrapper {

class BarometerWrapper final {
public:
    BarometerWrapper(const std::string& id,
                     fc::pdo::PDOEntry* pressure, fc::pdo::PDOEntry* altitude);

    [[nodiscard]] float pressure() const noexcept;
    [[nodiscard]] float altitude() const noexcept;
    // ...
};

} // namespace fc::wrapper
```

### Phase E: Integration

#### 10. Update Queue to own WrapperPool
Add `WrapperPool` member to Queue and expose it to safety evaluators:
```cpp
class Queue final {
public:
    [[nodiscard]] WrapperPool& wrapperPool() noexcept { return wrapperPool_; }
    [[nodiscard]] const WrapperPool& wrapperPool() const noexcept { return wrapperPool_; }
private:
    WrapperPool wrapperPool_;
};
```

#### 11. Update Queue::tick() to call safety evaluators
```cpp
void Queue::tick(uint64_t cycleCount, uint64_t nowNs) noexcept {
    if (!isRunning()) return;
    
    // Call safety evaluators with wrapper pool
    alwaysOnEval_.tick(wrapperPool_, cycleCount, nowNs);
    lineMonitor_.tick(wrapperPool_, cycleCount, nowNs);
    functionEvaluator_.tick(wrapperPool_, cycleCount, nowNs);
}
```

#### 12. Update CMakeLists.txt
Add new source files to `fc` library.

---

## File Structure After Implementation

```
src/flight_controller/
├── include/fc/
│   ├── pdo/
│   │   ├── PDO.h                    (modified: new EntryType, accessors)
│   │   ├── HardwareRegistry.h       (unchanged)
│   │   └── IHardwareAdapter.h       (unchanged)
│   ├── sensor/
│   │   └── SensorCatalog.h          (new)
│   ├── i2c/
│   │   └── I2CAdapter.h             (new)
│   ├── spi/
│   │   └── SPIAdapter.h             (new)
│   ├── wrapper/
│   │   ├── IMUWrapper.h             (new)
│   │   ├── MagnetometerWrapper.h    (new)
│   │   └── BarometerWrapper.h       (new)
│   └── app/
│       ├── Queue.h                  (modified: add WrapperPool)
│       ├── WrapperPool.h            (new)
│       └── Application.h            (unchanged)
└── src/
    ├── pdo/
    │   ├── PDO.cpp                  (modified: new read/write cases)
    │   └── HardwareRegistry.cpp     (unchanged)
    ├── sensor/
    │   └── SensorCatalog.cpp        (new)
    ├── i2c/
    │   └── I2CAdapter.cpp           (new)
    ├── spi/
    │   └── SPIAdapter.cpp           (new)
    ├── wrapper/
    │   ├── IMUWrapper.cpp           (new)
    │   ├── MagnetometerWrapper.cpp  (new)
    │   └── BarometerWrapper.cpp     (new)
    └── app/
        ├── Queue.cpp                (modified: WrapperPool integration)
        ├── WrapperPool.cpp          (new)
        └── Application.cpp          (unchanged)
```

---

## Key Design Decisions

1. **I2C/SPI adapters use the same IHardwareAdapter interface** — no need to change HardwareRegistry or the RT cycle. The two-phase read/write pattern works for any backend.

2. **SensorCatalog is separate from HardwareCatalog** — HardwareCatalog remains EtherCAT-specific (SlaveEntry with vendorId/productCode). SensorCatalog is for I2C/SPI devices with bus paths and addresses.

3. **Wrappers are thin typed accessors** — they hold raw `PDOEntry*` pointers resolved at init time via UUID lookup. No virtual dispatch in the RT path.

4. **WrapperPool lives in Queue** — each Queue has its own WrapperPool, allowing multiple independent control domains.

5. **ImuReader (libimu) remains unchanged** — it uses `void*` placeholders. The new IMUWrapper in libfc will use actual `PDOEntry*` pointers. ImuReader can be deprecated later.

6. **Phase 1 stubs for I2C/SPI drivers** — actual I2C/SPI communication will be stubbed (return zeros) until hardware is available. The architecture is in place for real drivers.
