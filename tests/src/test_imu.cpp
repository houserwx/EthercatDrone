// test_imu.cpp — Unit tests for IMU library
// ImuCalibration, ImuTypes

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "imu/ImuTypes.h"
#include "imu/ImuCalibration.h"

// ---- ImuCalibration tests --------------------------------------------------

TEST_CASE("ImuCalibration default values", "[imu][calibration]") {
    imu::ImuCalibration cal;
    CHECK(cal.accelScale[0] == Catch::Approx(1.0f));
    CHECK(cal.accelScale[1] == Catch::Approx(1.0f));
    CHECK(cal.accelScale[2] == Catch::Approx(1.0f));
    CHECK(cal.accelOffset[0] == Catch::Approx(0.0f));
    CHECK(cal.gyroScale[0] == Catch::Approx(1.0f));
    CHECK(cal.gyroOffset[0] == Catch::Approx(0.0f));
}

TEST_CASE("ImuCalibration calibrate with identity", "[imu][calibration]") {
    imu::ImuCalibration cal;
    imu::ImuRaw raw;
    raw.accX = 100; raw.accY = 200; raw.accZ = 300;
    raw.gyroX = 10; raw.gyroY = 20; raw.gyroZ = 30;

    auto calib = cal.calibrate(raw);
    CHECK(calib.accel.x == Catch::Approx(100.0f));
    CHECK(calib.accel.y == Catch::Approx(200.0f));
    CHECK(calib.accel.z == Catch::Approx(300.0f));
    CHECK(calib.gyro.x == Catch::Approx(10.0f));
    CHECK(calib.gyro.y == Catch::Approx(20.0f));
    CHECK(calib.gyro.z == Catch::Approx(30.0f));
}

TEST_CASE("ImuCalibration calibrate with scale and offset", "[imu][calibration]") {
    imu::ImuCalibration cal;
    cal.accelScale = {2.0f, 2.0f, 2.0f};
    cal.accelOffset = {10.0f, 20.0f, 30.0f};
    cal.gyroScale = {0.5f, 0.5f, 0.5f};
    cal.gyroOffset = {-1.0f, -2.0f, -3.0f};

    imu::ImuRaw raw;
    raw.accX = 100; raw.accY = 200; raw.accZ = 300;
    raw.gyroX = 40; raw.gyroY = 60; raw.gyroZ = 80;

    auto calib = cal.calibrate(raw);
    CHECK(calib.accel.x == Catch::Approx(210.0f));
    CHECK(calib.accel.y == Catch::Approx(420.0f));
    CHECK(calib.accel.z == Catch::Approx(630.0f));
    CHECK(calib.gyro.x == Catch::Approx(19.0f));
    CHECK(calib.gyro.y == Catch::Approx(28.0f));
    CHECK(calib.gyro.z == Catch::Approx(37.0f));
}

TEST_CASE("ImuCalibration checkHealth passes for valid data", "[imu][calibration]") {
    imu::ImuCalibration cal;
    imu::ImuCalibrated calib;
    calib.accel = common::math::Vec3f{0.0f, 0.0f, 9.81f};
    calib.gyro = common::math::Vec3f{0.0f, 0.0f, 0.0f};

    auto health = cal.checkHealth(calib);
    CHECK(health.valid);
}

TEST_CASE("ImuCalibration checkHealth fails for accel out of range", "[imu][calibration]") {
    imu::ImuCalibration cal;
    imu::ImuCalibrated calib;
    calib.accel = common::math::Vec3f{0.0f, 0.0f, 50.0f};
    calib.gyro = common::math::Vec3f{0.0f, 0.0f, 0.0f};

    auto health = cal.checkHealth(calib);
    CHECK(!health.valid);
}

TEST_CASE("ImuCalibration checkHealth fails for gyro out of range", "[imu][calibration]") {
    imu::ImuCalibration cal;
    imu::ImuCalibrated calib;
    calib.accel = common::math::Vec3f{0.0f, 0.0f, 9.81f};
    calib.gyro = common::math::Vec3f{40.0f, 0.0f, 0.0f};

    auto health = cal.checkHealth(calib);
    CHECK(!health.valid);
}

// ---- ImuTypes tests --------------------------------------------------------

TEST_CASE("ImuRaw is trivially copyable", "[imu][types]") {
    STATIC_REQUIRE(std::is_trivially_copyable_v<imu::ImuRaw>);
}

TEST_CASE("ImuCalibrated is trivially copyable", "[imu][types]") {
    STATIC_REQUIRE(std::is_trivially_copyable_v<imu::ImuCalibrated>);
}

TEST_CASE("ImuHealth default is healthy", "[imu][types]") {
    imu::ImuHealth health;
    CHECK(health.valid);
}
