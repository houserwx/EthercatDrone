# Tests — unit + integration tests (CTest)
# Uses Catch2 via FetchContent (header-only, no external dependency required).

include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(Catch2)

# ---- Common library tests --------------------------------------------------
add_executable(test_common
    src/test_common.cpp
)
target_link_libraries(test_common PRIVATE common Catch2::Catch2WithMain)
target_compile_features(test_common PRIVATE cxx_std_20)
add_test(NAME test_common COMMAND test_common)

# ---- Flight controller tests -----------------------------------------------
add_executable(test_fc
    src/test_fc.cpp
)
target_link_libraries(test_fc PRIVATE fc Catch2::Catch2WithMain)
target_compile_features(test_fc PRIVATE cxx_std_20)
add_test(NAME test_fc COMMAND test_fc)

# ---- IMU library tests -----------------------------------------------------
add_executable(test_imu
    src/test_imu.cpp
)
target_link_libraries(test_imu PRIVATE imu Catch2::Catch2WithMain)
target_compile_features(test_imu PRIVATE cxx_std_20)
add_test(NAME test_imu COMMAND test_imu)
