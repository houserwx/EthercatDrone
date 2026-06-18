# libfc — flight controller (RT core)
# PDO system, EtherCAT, motors, safety, redundancy, RT application, gRPC services

set(FC_SOURCES
    # PDO system
    src/pdo/PDO.cpp
    src/pdo/HardwareRegistry.cpp
    src/pdo/MachineStatePDO.cpp
    # EtherCAT adapter
    src/ethercat/EthercatAdapter.cpp
    src/ethercat/HardwareCatalog.cpp
    src/ethercat/kSlaveTypes.cpp
    # I2C adapter
    src/i2c/I2CAdapter.cpp
    # SPI adapter
    src/spi/SPIAdapter.cpp
    # GPIO adapter
    src/gpio/GPIOAdapter.cpp
    src/gpio/BoardVariant.cpp
    # Simulated adapter
    src/simulated/SimulatedAdapter.cpp
    # gRPC adapter
    src/grpc/GrpcAdapter.cpp
    # Motor control
    src/motor/MotorController.cpp
    # Safety
    src/safety/MachineStateController.cpp
    src/safety/AlwaysOnEval.cpp
    src/safety/RulesEvalData.cpp
    src/safety/LineMonitor.cpp
    # Redundancy
    src/redundancy/RedundancyController.cpp
    # Application
    src/app/Application.cpp
    src/app/Queue.cpp
    src/app/FunctionEvaluator.cpp
    # Mission planning
    src/mission/MissionQueue.cpp
    src/mission/MissionEvaluator.cpp
    src/mission/MissionStatePDO.cpp
    # Services
    src/services/GrpcService.cpp
)

add_library(fc STATIC ${FC_SOURCES})

target_include_directories(fc PUBLIC include)

target_compile_options(fc PRIVATE -Wall -Wextra -Wpedantic)

target_link_libraries(fc PUBLIC common imu)

# libgpiod — GPIO adapter (required for Pi 4/5 GPIO support)
# Pi 4 uses libgpiod v1 API, Pi 5 uses libgpiod v2 API.
# Both are compatible at header level for our abstraction.
#
# When cross-compiling, libgpiod may be fetched from the target board
# (like libethercat).  Deploy.sh passes -DLIBGPIOD_LIB=<path> and
# -DLIBGPIOD_INCLUDE_DIR=<path> pointing to the fetched aarch64 files.
# We also accept -DLIBGPIOD_VERSION=v1|v2 to skip header inspection
# when the fetched headers might be stale or missing.

# For cross-compilation: accept LIBGPIOD_LIB as a cache variable from deploy.sh
if(CMAKE_CROSSCOMPILING)
    if(LIBGPIOD_LIB AND EXISTS "${LIBGPIOD_LIB}")
        set(LIBGPIOD_FOUND TRUE)
        message(STATUS "Cross-compiling with libgpiod from target: ${LIBGPIOD_LIB}")
    else()
        set(LIBGPIOD_FOUND FALSE)
        message(STATUS "libgpiod not provided for cross-compile — GPIO adapter will use stub mode")
    endif()
else()
    find_library(LIBGPIOD_LIB NAMES gpiod PATHS /usr/lib /usr/local/lib)
    find_path(LIBGPIOD_INCLUDE_DIR NAMES gpiod.h PATHS /usr/include /usr/local/include)
    if(LIBGPIOD_LIB AND LIBGPIOD_INCLUDE_DIR)
        set(LIBGPIOD_FOUND TRUE)
    else()
        set(LIBGPIOD_FOUND FALSE)
    endif()
endif()

if(LIBGPIOD_FOUND)
    # libgpiod 1.0+ (2019) uses the v2 API exclusively.
    # The "v1" API was removed from libgpiod in favor of the v2 API.
    # Pi 4 ships with libgpiod ~1.6.x which is v2 API.
    # Pi 5 ships with libgpiod ~2.x which is v2 API.
    # We always use the v2 API when libgpiod is available.
    target_compile_definitions(fc PRIVATE GPIO_LIBGPIOD_AVAILABLE)
    message(STATUS "libgpiod found — GPIO adapter enabled (v2 API)")
    target_link_libraries(fc PRIVATE ${LIBGPIOD_LIB})
    if(LIBGPIOD_INCLUDE_DIR)
        target_include_directories(fc PRIVATE ${LIBGPIOD_INCLUDE_DIR})
    endif()
else()
    message(STATUS "libgpiod not available — GPIO adapter will use stub mode")
endif()

if(ETHERCAT_AVAILABLE AND ETHERCAT_LIB)
    target_link_libraries(fc PRIVATE ${ETHERCAT_LIB})
    target_compile_definitions(fc PRIVATE ETHERCAT_AVAILABLE)
    if(ETHERCAT_INCLUDE_DIR)
        target_include_directories(fc PRIVATE ${ETHERCAT_INCLUDE_DIR})
    endif()
endif()

# gRPC proto compilation
if(GRPC_AVAILABLE)
    set(PROTO_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/services/proto")
    set(PROTO_GEN_DIR "${CMAKE_BINARY_DIR}/generated")

    file(MAKE_DIRECTORY "${PROTO_GEN_DIR}")

    macro(compile_proto PROTO_NAME)
        set(_proto     "${PROTO_SRC_DIR}/${PROTO_NAME}.proto")
        set(_pb_cc     "${PROTO_GEN_DIR}/${PROTO_NAME}.pb.cc")
        set(_pb_h      "${PROTO_GEN_DIR}/${PROTO_NAME}.pb.h")
        set(_grpc_cc   "${PROTO_GEN_DIR}/${PROTO_NAME}.grpc.pb.cc")
        set(_grpc_h    "${PROTO_GEN_DIR}/${PROTO_NAME}.grpc.pb.h")
        add_custom_command(
            OUTPUT  "${_pb_cc}" "${_pb_h}" "${_grpc_cc}" "${_grpc_h}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${PROTO_GEN_DIR}"
            COMMAND "${PROTOC_EXECUTABLE}"
                    "--cpp_out=${PROTO_GEN_DIR}"
                    "--grpc_out=${PROTO_GEN_DIR}"
                    "--plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}"
                    "-I${PROTO_SRC_DIR}"
                    "${_proto}"
            DEPENDS "${_proto}"
            COMMENT "Compiling ${PROTO_NAME}.proto"
            VERBATIM
        )
        list(APPEND FC_SOURCES "${_pb_cc}" "${_grpc_cc}")
    endmacro()

    compile_proto(log_service)
    compile_proto(config_service)
    compile_proto(station_service)

    list(APPEND FC_SOURCES
        src/services/LogStreamServiceImpl.cpp
        src/services/ConfigServiceImpl.cpp
        src/services/StationServiceImpl.cpp
    )

    target_sources(fc PRIVATE ${FC_SOURCES})

    include_directories("${PROTO_GEN_DIR}")

    target_compile_definitions(fc PRIVATE GRPC_AVAILABLE)
    target_link_libraries(fc PRIVATE gRPC::grpc++ gRPC::grpc)
endif()
