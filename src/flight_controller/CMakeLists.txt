# libfc — flight controller (RT core)
# Safety, redundancy, RT application, gRPC services, wrappers, mission planning.
# PDO system + backend adapters now provided by libdynamichardware submodule.

set(FC_SOURCES
    # gRPC wrapper (application-layer message relay, stays in project)
    src/grpc/GrpcWrapper.cpp
    # Motor control (control logic, not hardware transport)
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
    # Services
    src/services/GrpcService.cpp
)

add_library(fc STATIC ${FC_SOURCES})

target_include_directories(fc PUBLIC include)

target_compile_options(fc PRIVATE -Wall -Wextra -Wpedantic)

target_link_libraries(fc PUBLIC common imu DynamicHardware::dynamichardware)

# Compile definitions propagated from submodule.
# The submodule (DynamicHardware::dynamichardware) already links libethercat/libgpiod
# and sets ETHERCAT_AVAILABLE / GPIO_LIBGPIOD_AVAILABLE as PUBLIC definitions.
# If project code needs these definitions explicitly (e.g., for #ifdef guards in
# project-specific headers), propagate them here too.
if(ETHERCAT_AVAILABLE)
    target_compile_definitions(fc PRIVATE ETHERCAT_AVAILABLE)
endif()
if(LIBGPIOD_FOUND)
    target_compile_definitions(fc PRIVATE GPIO_LIBGPIOD_AVAILABLE)
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
