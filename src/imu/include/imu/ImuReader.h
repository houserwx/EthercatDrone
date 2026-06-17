#pragma once

#include "imu/ImuTypes.h"
#include "imu/ImuCalibration.h"

namespace imu {

// ---------------------------------------------------------------------------
// ImuReader — wraps 6× AnalogInput PDOEntries for IMU access.
//
// RT-safe: all accessors are noexcept, no allocation after construction.
// The PDOEntry references are resolved once at init time via HardwareRegistry.
// ---------------------------------------------------------------------------
class ImuReader {
public:
    /// Construct with raw PDOEntry references (resolved at init time).
    /// @param accXEntry  PDOEntry for accelerometer X
    /// @param accYEntry  PDOEntry for accelerometer Y
    /// @param accZEntry  PDOEntry for accelerometer Z
    /// @param gyroXEntry PDOEntry for gyroscope X
    /// @param gyroYEntry PDOEntry for gyroscope Y
    /// @param gyroZEntry PDOEntry for gyroscope Z
    /// @param cal        Calibration parameters
    ImuReader(void* accXEntry, void* accYEntry, void* accZEntry,
              void* gyroXEntry, void* gyroYEntry, void* gyroZEntry,
              const ImuCalibration& cal);

    /// Read raw IMU values from PDOEntry caches.
    /// RT-safe: noexcept, no allocation.
    [[nodiscard]] ImuRaw readRaw() const noexcept;

    /// Read calibrated IMU values (m/s², rad/s).
    /// RT-safe: noexcept, no allocation.
    [[nodiscard]] ImuCalibrated readCalibrated() const noexcept;

    /// Run sanity checks on current calibrated data.
    [[nodiscard]] ImuHealth checkHealth() const noexcept;

private:
    // PDOEntry references (resolved at init, stable after freeze).
    // Stored as void* placeholders until PDOEntry is defined in libfc.
    void* accXEntry_{nullptr};
    void* accYEntry_{nullptr};
    void* accZEntry_{nullptr};
    void* gyroXEntry_{nullptr};
    void* gyroYEntry_{nullptr};
    void* gyroZEntry_{nullptr};

    const ImuCalibration& cal_;
};

} // namespace imu
