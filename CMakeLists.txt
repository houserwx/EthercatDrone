cmake_minimum_required(VERSION 3.20)
project(EtherCatDrone CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- libdynamichardware submodule ------------------------------------------
# Handles: PDO system, backend adapters (EtherCAT, GPIO, I2C, SPI, Simulated),
#          RT utilities (SignalProcess, VectorBuffer), nlohmann_json (vendored).
# The submodule detects and links optional deps: libethercat, libgpiod.
# It sets ETHERCAT_AVAILABLE, ETHERCAT_LIB, ETHERCAT_INCLUDE_DIR,
# LIBGPIOD_FOUND, LIBGPIOD_LIB, LIBGPIOD_INCLUDE_DIR as cache variables
# that project targets consume below.
#
# For cross-compilation, pass these via CMake cache BEFORE add_subdirectory:
#   -DETHERCAT_LIB=<aarch64 libethercat.so>
#   -DETHERCAT_INCLUDE_DIR=<path to ecrt.h>
#   -DLIBGPIOD_LIB=<aarch64 libgpiod.so>
#   -DLIBGPIOD_INCLUDE_DIR=<path to gpiod.h>

# Ensure include dirs point to local headers for cross-compile
# (toolchain can't search host paths, submodule needs them pre-set).
if(CMAKE_CROSSCOMPILING)
    if(NOT ETHERCAT_INCLUDE_DIR)
        set(LOCAL_ETHERCAT_INCLUDE "${CMAKE_SOURCE_DIR}/thirdparty/ethercat/include")
        if(EXISTS "${LOCAL_ETHERCAT_INCLUDE}/ecrt.h")
            set(ETHERCAT_INCLUDE_DIR "${LOCAL_ETHERCAT_INCLUDE}" CACHE PATH "EtherCAT header dir" FORCE)
        endif()
    endif()
    # LIBGPIOD_INCLUDE_DIR may be passed via deploy.sh staging area,
    # or the submodule will search common paths as fallback.
endif()

add_subdirectory(thirdparty/libdynamichardware)

# After submodule configure, ensure fallback defaults for project targets
if(NOT DEFINED ETHERCAT_AVAILABLE)
    set(ETHERCAT_AVAILABLE OFF CACHE BOOL "EtherCAT available" FORCE)
endif()
if(NOT DEFINED LIBGPIOD_FOUND)
    set(LIBGPIOD_FOUND OFF CACHE BOOL "libgpiod found" FORCE)
endif()

# gRPC and Protobuf (optional — stub mode when absent)
find_package(gRPC CONFIG QUIET)
find_program(PROTOC_EXECUTABLE    protoc          HINTS /usr/bin /usr/local/bin)
find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin      HINTS /usr/bin /usr/local/bin)

if(gRPC_FOUND AND PROTOC_EXECUTABLE AND GRPC_CPP_PLUGIN)
    set(GRPC_AVAILABLE ON CACHE BOOL "gRPC available" FORCE)
    message(STATUS "gRPC ${gRPC_VERSION} found — building real gRPC server")
else()
    set(GRPC_AVAILABLE OFF CACHE BOOL "gRPC available" FORCE)
    if(NOT gRPC_FOUND)
        message(STATUS "gRPC not found — GrpcService will run as stub")
    elseif(NOT PROTOC_EXECUTABLE)
        message(WARNING "protoc not found — gRPC stub mode (install protobuf-compiler)")
    else()
        message(WARNING "grpc_cpp_plugin not found — gRPC stub mode (install protobuf-compiler-grpc)")
    endif()
endif()

# ---- Subdirectories --------------------------------------------------------
add_subdirectory(src/common)
add_subdirectory(src/imu)
add_subdirectory(src/flight_controller)
add_subdirectory(src/navi)
add_subdirectory(src/main)

# ---- Testing ---------------------------------------------------------------
option(BUILD_TESTING "Build tests" ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
