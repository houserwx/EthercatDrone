#pragma once
#include "dynamichardware/pdo/PDO.h"
#include "imu/ImuTypes.h"
#include "imu/ImuCalibration.h"

#include <string>

// ============================================================
// IMUWrapper — typed accessor for 6 PDOEntries representing
// gyroscope (X/Y/Z) and accelerometer (X/Y/Z) channels.
//
// Domain semantics (scaling, units conversion) live here,
// not in the hardware library.
// Holds stable PDOEntry& references resolved once at init time.
// In the RT loop each accessor is a single struct member read — zero lookup,
// zero virtual dispatch.
//
// Calibration is applied at the wrapper level using ImuCalibration
// from libimu.  Raw values are also available for diagnostics.
//
// Lifetime: the referenced PDOEntries live in the frozen PDO owned
// by a backend.  Backends outlive all wrappers.
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
               dynamichardware::pdo::PDOEntry& gyroXEntry, dynamichardware::pdo::PDOEntry& gyroYEntry, dynamichardware::pdo::PDOEntry& gyroZEntry,
               dynamichardware::pdo::PDOEntry& accelXEntry, dynamichardware::pdo::PDOEntry& accelYEntry, dynamichardware::pdo::PDOEntry& accelZEntry,
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
    dynamichardware::pdo::PDOEntry& gyroX_;
    dynamichardware::pdo::PDOEntry& gyroY_;
    dynamichardware::pdo::PDOEntry& gyroZ_;
    dynamichardware::pdo::PDOEntry& accelX_;
    dynamichardware::pdo::PDOEntry& accelY_;
    dynamichardware::pdo::PDOEntry& accelZ_;

    const imu::ImuCalibration& cal_;
};

// ---------------------------------------------------------------------------
// Inline implementations for maximum inlining in RT path
// ---------------------------------------------------------------------------
inline imu::ImuRaw IMUWrapper::readRaw() const noexcept
{
    imu::ImuRaw raw;
    // getFloat() returns raw sensor values as floats (rad/s for gyro, m/s² for accel)
    // Scale by 1000 to match ImuRaw int16_t convention (milli-units)
    raw.gyroX  = static_cast<int16_t>(gyroX_.getFloat() * 1000.0f);
    raw.gyroY  = static_cast<int16_t>(gyroY_.getFloat() * 1000.0f);
    raw.gyroZ  = static_cast<int16_t>(gyroZ_.getFloat() * 1000.0f);
    raw.accX   = static_cast<int16_t>(accelX_.getFloat() * 1000.0f);
    raw.accY   = static_cast<int16_t>(accelY_.getFloat() * 1000.0f);
    raw.accZ   = static_cast<int16_t>(accelZ_.getFloat() * 1000.0f);
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
