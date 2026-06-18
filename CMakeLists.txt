cmake_minimum_required(VERSION 3.20)
project(EtherCatDrone CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- libdynamichardware submodule ------------------------------------------
# Handles: PDO system, backend adapters (EtherCAT, GPIO, I2C, SPI, Simulated),
#          RT utilities (SignalProcess, VectorBuffer), nlohmann_json (vendored).
# Optional deps detected by submodule: libethercat, libgpiod.
add_subdirectory(thirdparty/libdynamichardware)

# Propagate optional dependency flags from submodule to project targets
# (ETHERCAT_AVAILABLE, GPIO_LIBGPIOD_AVAILABLE are needed by project code).
# The submodule detects these internally; we mirror them for project targets.
if(NOT DEFINED ETHERCAT_AVAILABLE)
    set(ETHERCAT_AVAILABLE OFF)
endif()
if(NOT DEFINED LIBGPIOD_FOUND)
    set(LIBGPIOD_FOUND FALSE)
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
