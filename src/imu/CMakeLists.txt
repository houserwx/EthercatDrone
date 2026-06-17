# libimu — IMU abstraction + calibration

set(IMU_SOURCES
    src/ImuReader.cpp
    src/ImuCalibration.cpp
)

add_library(imu STATIC ${IMU_SOURCES})

target_include_directories(imu PUBLIC include)

target_compile_options(imu PRIVATE -Wall -Wextra -Wpedantic)

target_link_libraries(imu PUBLIC common)
