#include "imu/ImuCalibration.h"

#include <cmath>

namespace imu {

ImuCalibrated ImuCalibration::calibrate(const ImuRaw& raw) const noexcept
{
    ImuCalibrated cal;
    cal.accel.x = static_cast<float>(raw.accX) * accelScale[0] + accelOffset[0];
    cal.accel.y = static_cast<float>(raw.accY) * accelScale[1] + accelOffset[1];
    cal.accel.z = static_cast<float>(raw.accZ) * accelScale[2] + accelOffset[2];
    cal.gyro.x  = static_cast<float>(raw.gyroX) * gyroScale[0] + gyroOffset[0];
    cal.gyro.y  = static_cast<float>(raw.gyroY) * gyroScale[1] + gyroOffset[1];
    cal.gyro.z  = static_cast<float>(raw.gyroZ) * gyroScale[2] + gyroOffset[2];
    return cal;
}

ImuHealth ImuCalibration::checkHealth(const ImuCalibrated& cal) const noexcept
{
    ImuHealth health;

    // Accelerometer magnitude check (should be ≈ 9.81 m/s² when stationary)
    float mag = cal.accel.magnitude();
    health.accelMagnitudeOk = (mag >= accelMinMagnitude && mag <= accelMaxMagnitude);

    // Gyro range check
    health.gyroRangeOk = (std::abs(cal.gyro.x) <= gyroMaxRadPerSec &&
                          std::abs(cal.gyro.y) <= gyroMaxRadPerSec &&
                          std::abs(cal.gyro.z) <= gyroMaxRadPerSec);

    health.valid = health.accelMagnitudeOk && health.gyroRangeOk;
    return health;
}

} // namespace imu
