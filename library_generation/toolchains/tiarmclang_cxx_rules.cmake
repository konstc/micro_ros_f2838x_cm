# C++ rules for TI Arm Clang, read by CMake after its own TIClang module
# (CMAKE_USER_MAKE_RULES_OVERRIDE_CXX in tiarmclang_cm4.cmake).
#
# CMake's TIClang module stops at C++17, but tiarmclang 5.x supports C++20.
# Since Lyrical, ament_cmake_ros_core's ament_ros_defaults target requires
# cxx_std_20, which CMake rejects for a compiler without this option.

set(CMAKE_CXX20_STANDARD_COMPILE_OPTION "-std=c++20")
set(CMAKE_CXX20_EXTENSION_COMPILE_OPTION "-std=gnu++20")
set(CMAKE_CXX_STANDARD_LATEST 20)
