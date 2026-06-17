# libnavi — higher-level navigation (Phase 2+)
# VIO, path planning, high-level commands to FC

# Placeholder source for Phase 1 — will be populated in Phase 2.
add_library(navi STATIC src/navi_stub.cpp)

target_include_directories(navi PUBLIC include)

target_compile_options(navi PRIVATE -Wall -Wextra -Wpedantic)

target_link_libraries(navi PUBLIC fc)
