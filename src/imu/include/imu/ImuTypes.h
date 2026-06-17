#pragma once

#include "common/math/Math.h"

#include <cstdint>

namespace imu {

// ---------------------------------------------------------------------------
// ImuRaw — raw ADC values from 6× AnalogInput PDOEntries.
// ---------------------------------------------------------------------------
struct ImuRaw {
    int16_t accX{0};
    int16_t accY{0};
    int16_t accZ{0};
    int16_t gyroX{0};
    int16_t gyroY{0};
    int16_t gyroZ{0};
};

// ---------------------------------------------------------------------------
// ImuCalibrated — calibrated m/s² and rad/s.
// ---------------------------------------------------------------------------
struct ImuCalibrated {
    common::math::Vec3f accel;  // m/s²
    common::math::Vec3f gyro;   // rad/s
};

// ---------------------------------------------------------------------------
// ImuHealth — sanity check results.
// ---------------------------------------------------------------------------
struct ImuHealth {
    bool accelMagnitudeOk{true};  // sqrt(accX²+accY²+accZ²) ≈ 9.81 when stationary
    bool gyroRangeOk{true};       // each axis within ±configured max rad/s
    bool valid{true};             // overall health flag
};

} // namespace imu
