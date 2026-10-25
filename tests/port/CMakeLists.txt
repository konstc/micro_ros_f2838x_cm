# Host unit tests for the port layer (built with the host compiler).
#
#   cmake -S tests/port -B build/port-tests && cmake --build build/port-tests
#   ctest --test-dir build/port-tests --output-on-failure
#
# tools/host_tests.sh runs these steps in the Docker builder image.
cmake_minimum_required(VERSION 3.16)
project(microros_f2838x_port_tests C)
enable_testing()

set(PORT_DIR ${CMAKE_CURRENT_LIST_DIR}/../../port)
set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra -Werror)

add_executable(test_allocator
    test_allocator.c
    ${PORT_DIR}/src/allocators_freertos.c
    ${PORT_DIR}/src/stats.c)
target_include_directories(test_allocator PRIVATE fakes ${PORT_DIR}/include ${PORT_DIR}/src)
add_test(NAME allocator COMMAND test_allocator)

add_executable(test_clock test_clock.c)
target_include_directories(test_clock PRIVATE ${PORT_DIR}/src)
add_test(NAME clock COMMAND test_clock)
