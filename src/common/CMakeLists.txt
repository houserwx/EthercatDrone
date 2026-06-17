# libcommon — shared RT infrastructure
# Threadrunner, SignalProcess, VectorBuffer, Logger, Config, Messages

set(COMMON_SOURCES
    src/rt/Threadrunner.cpp
    src/log/Logger.cpp
    src/log/sinks/ConsoleSink.cpp
    src/log/sinks/FileSink.cpp
    src/log/sinks/LogStreamSink.cpp
    src/config/Config.cpp
    src/messages/Message.cpp
)

add_library(common STATIC ${COMMON_SOURCES})

target_include_directories(common PUBLIC include)

target_compile_options(common PRIVATE -Wall -Wextra -Wpedantic)

if(nlohmann_json_FOUND)
    target_link_libraries(common PUBLIC nlohmann_json::nlohmann_json)
endif()

# rt (POSIX realtime) is Linux-only
if(UNIX AND NOT APPLE)
    target_link_libraries(common PRIVATE rt)
endif()
