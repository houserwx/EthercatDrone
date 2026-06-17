#pragma once

#include "imu/ImuTypes.h"

#include <array>
#include <cstdint>

namespace imu {

// ---------------------------------------------------------------------------
// ImuCalibration — scale/offset calibration for each IMU axis.
// Loaded from config; applied once at init, never modified in RT path.
// ---------------------------------------------------------------------------
struct ImuCalibration {
    // Accelerometer: calibrated = (raw * scale) + offset
    std::array<float, 3> accelScale{1.0f, 1.0f, 1.0f};
    std::array<float, 3> accelOffset{0.0f, 0.0f, 0.0f};

    // Gyroscope: calibrated = (raw * scale) + offset
    std::array<float, 3> gyroScale{1.0f, 1.0f, 1.0f};
    std::array<float, 3> gyroOffset{0.0f, 0.0f, 0.0f};

    // Sanity check thresholds
    float accelMinMagnitude{8.5f};   // m/s² (≈ 0.87g)
    float accelMaxMagnitude{11.5f};  // m/s² (≈ 1.17g)
    float gyroMaxRadPerSec{34.9f};   // rad/s (≈ 2000°/s)

    /// Apply calibration to raw IMU data.
    [[nodiscard]] ImuCalibrated calibrate(const ImuRaw& raw) const noexcept;

    /// Run sanity checks on calibrated data.
    [[nodiscard]] ImuHealth checkHealth(const ImuCalibrated& cal) const noexcept;
};

} // namespace imu
