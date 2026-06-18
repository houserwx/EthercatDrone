cmake_minimum_required(VERSION 3.20)
project(EtherCatDrone CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Global dependencies ---------------------------------------------------
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann_json package not found; using system header")
    include_directories(/usr/local/include)
endif()

# EtherCAT library (optional — stub mode when absent)
# When cross-compiling, skip host libraries — the target board provides libethercat.
# Search thirdparty first, then system-wide (matches reference codebase pattern).
set(IGH_ETHERCAT_DIR "${CMAKE_SOURCE_DIR}/thirdparty/ethercat")

# Detect cross-compilation
if(CMAKE_CROSSCOMPILING)
    # Cross-compile: use local headers from thirdparty/,
    # ETERCAT_LIB must be passed as -DETHERCAT_LIB=<aarch64 lib path> from deploy.sh
    if(EXISTS "${IGH_ETHERCAT_DIR}/include")
        find_path(ETHERCAT_INCLUDE_DIR NAMES ecrt.h
            PATHS "${IGH_ETHERCAT_DIR}/include"
            NO_DEFAULT_PATH
        )
        if(ETHERCAT_INCLUDE_DIR AND ETHERCAT_LIB)
            set(ETHERCAT_AVAILABLE ON)
            message(STATUS "Cross-compiling with EtherCAT support (headers from thirdparty/, lib=${ETHERCAT_LIB})")
        else()
            if(NOT ETHERCAT_LIB)
                message(WARNING "EtherCAT cross-compile: pass -DETHERCAT_LIB=<aarch64 libethercat.so> or build stub")
            else()
                message(WARNING "ecrt.h not found in thirdparty/ethercat/include — EtherCAT stub mode")
            endif()
            set(ETHERCAT_AVAILABLE OFF)
        endif()
    else()
        message(STATUS "thirdparty/ethercat not found — EtherCAT stub mode for cross-compile")
        set(ETHERCAT_AVAILABLE OFF)
    endif()
else()
    find_library(ETHERCAT_LIB NAMES ethercat
        PATHS /usr/local/lib /usr/lib
        PATH_SUFFIXES lib
    )

    # If not found system-wide, check thirdparty build output
    if(NOT ETHERCAT_LIB AND EXISTS "${IGH_ETHERCAT_DIR}/lib")
        find_library(ETHERCAT_LIB NAMES ethercat PATHS "${IGH_ETHERCAT_DIR}/lib")
    endif()

    if(NOT ETHERCAT_LIB)
        message(STATUS "libethercat not found — EtherCAT adapter will be stub-only")
        set(ETHERCAT_AVAILABLE OFF)
    else()
        find_path(ETHERCAT_INCLUDE_DIR NAMES ecrt.h
            PATHS /usr/local/include /usr/include "${IGH_ETHERCAT_DIR}/include"
        )
        if(NOT ETHERCAT_INCLUDE_DIR)
            message(WARNING "ecrt.h not found — EtherCAT adapter will be stub-only")
            set(ETHERCAT_AVAILABLE OFF)
            set(ETHERCAT_LIB "")
        else()
            message(STATUS "libethercat found at ${ETHERCAT_LIB} — EtherCAT adapter enabled")
            set(ETHERCAT_AVAILABLE ON)
        endif()
    endif()
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
