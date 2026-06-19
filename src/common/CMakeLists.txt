# libcommon — shared RT infrastructure
# Threadrunner, Logger, Config, Messages
# (SignalProcess, VectorBuffer now provided by libdynamichardware submodule)

set(COMMON_SOURCES
    src/rt/Threadrunner.cpp
    src/log/Logger.cpp
    src/log/sinks/ConsoleSink.cpp
    src/log/sinks/FileSink.cpp
    src/log/sinks/LogStreamSink.cpp
    src/config/Config.cpp
    src/messages/Message.cpp
    src/menu/ConfigDisplayPanel.cpp
    src/menu/SimulatedEntryPanel.cpp
    src/menu/PdoMappingPanel.cpp
    src/menu/CycleTimePanel.cpp
    src/menu/MainMenu.cpp
)

add_library(common STATIC ${COMMON_SOURCES})

target_include_directories(common PUBLIC include)

target_compile_options(common PRIVATE -Wall -Wextra -Wpedantic)

# dynamichardware provides nlohmann_json (vendored) and RT utilities
target_link_libraries(common PUBLIC DynamicHardware::dynamichardware)

# rt (POSIX realtime) is Linux-only
if(UNIX AND NOT APPLE)
    target_link_libraries(common PRIVATE rt)
endif()
