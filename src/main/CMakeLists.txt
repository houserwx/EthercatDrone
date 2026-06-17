# Executables — integration entry points

add_executable(drone_app src/drone_app.cpp)
target_link_libraries(drone_app PRIVATE fc)
target_compile_options(drone_app PRIVATE -Wall -Wextra -Wpedantic)

add_executable(bench_test src/bench_test.cpp)
target_link_libraries(bench_test PRIVATE fc)
target_compile_options(bench_test PRIVATE -Wall -Wextra -Wpedantic)
