#pragma once
#include "fc/pdo/PDO.h"
#include "imu/ImuTypes.h"
#include "imu/ImuCalibration.h"

#include <string>

// ============================================================
// IMUWrapper — typed accessor for 6× PDOEntries representing
// gyroscope (X/Y/Z) and accelerometer (X/Y/Z) channels.
//
// Holds stable PDOEntry& references resolved once at init time
// via HardwareRegistry::lookupByUuid().  In the RT loop each
// accessor is a single struct member read — zero lookup,
// zero virtual dispatch.
//
// Calibration is applied at the wrapper level using ImuCalibration
// from libimu.  Raw values are also available for diagnostics.
//
// Lifetime: the referenced PDOEntries live in the frozen PDO owned
// by a backend (I2C, SPI, or EtherCAT).  Backends outlive all wrappers.
// ============================================================

namespace fc::wrapper {

class IMUWrapper final {
public:
    /// Construct with raw PDOEntry references (resolved at init time).
    /// @param name         Human-readable identifier (e.g. "IMU-Primary")
    /// @param gyroXEntry   PDOEntry for gyroscope X axis
    /// @param gyroYEntry   PDOEntry for gyroscope Y axis
    /// @param gyroZEntry   PDOEntry for gyroscope Z axis
    /// @param accelXEntry  PDOEntry for accelerometer X axis
    /// @param accelYEntry  PDOEntry for accelerometer Y axis
    /// @param accelZEntry  PDOEntry for accelerometer Z axis
    /// @param cal          Calibration parameters (scale + offset)
    IMUWrapper(std::string name,
               PDOEntry& gyroXEntry, PDOEntry& gyroYEntry, PDOEntry& gyroZEntry,
               PDOEntry& accelXEntry, PDOEntry& accelYEntry, PDOEntry& accelZEntry,
               const imu::ImuCalibration& cal) noexcept
        : name_(std::move(name))
        , gyroX_(gyroXEntry), gyroY_(gyroYEntry), gyroZ_(gyroZEntry)
        , accelX_(accelXEntry), accelY_(accelYEntry), accelZ_(accelZEntry)
        , cal_(cal)
    {}

    [[nodiscard]] const std::string& getName() const noexcept { return name_; }

    /// Read raw IMU values from PDOEntry caches.
    /// RT-safe: noexcept, no allocation.
    [[nodiscard]] imu::ImuRaw readRaw() const noexcept;

    /// Read calibrated IMU values (m/s², rad/s).
    /// RT-safe: noexcept, no allocation.
    [[nodiscard]] imu::ImuCalibrated readCalibrated() const noexcept;

    /// Run sanity checks on current calibrated data.
    /// RT-safe: noexcept, no allocation.
    [[nodiscard]] imu::ImuHealth checkHealth() const noexcept;

private:
    std::string name_;

    // PDOEntry references (resolved at init, stable after freeze).
    PDOEntry& gyroX_;
    PDOEntry& gyroY_;
    PDOEntry& gyroZ_;
    PDOEntry& accelX_;
    PDOEntry& accelY_;
    PDOEntry& accelZ_;

    const imu::ImuCalibration& cal_;
};

// ---------------------------------------------------------------------------
// Inline implementations for maximum inlining in RT path
// ---------------------------------------------------------------------------
inline imu::ImuRaw IMUWrapper::readRaw() const noexcept
{
    imu::ImuRaw raw;
    raw.gyroX  = static_cast<int16_t>(gyroX_.getGyroX() * 1000.0f);
    raw.gyroY  = static_cast<int16_t>(gyroY_.getGyroY() * 1000.0f);
    raw.gyroZ  = static_cast<int16_t>(gyroZ_.getGyroZ() * 1000.0f);
    raw.accX   = static_cast<int16_t>(accelX_.getAccelX() * 1000.0f);
    raw.accY   = static_cast<int16_t>(accelY_.getAccelY() * 1000.0f);
    raw.accZ   = static_cast<int16_t>(accelZ_.getAccelZ() * 1000.0f);
    return raw;
}

inline imu::ImuCalibrated IMUWrapper::readCalibrated() const noexcept
{
    return cal_.calibrate(readRaw());
}

inline imu::ImuHealth IMUWrapper::checkHealth() const noexcept
{
    return cal_.checkHealth(readCalibrated());
}

} // namespace fc::wrapper
