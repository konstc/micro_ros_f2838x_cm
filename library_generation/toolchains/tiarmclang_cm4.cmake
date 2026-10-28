# CMake toolchain: F2838x Connectivity Manager (Cortex-M4 without FPU), TI Arm Clang.
#
# build_library.sh (native mode) and library_generation.sh (micro-ROS static
# library builder image) set TIARMCLANG_ROOT (compiler install) and
# MICROROS_SHIM_DIR (library_generation/posix_shim) in the environment.
#
# The target flags below must match the CCS projects that link the library;
# build_info.json records the flags actually used.

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_CROSSCOMPILING 1)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

if(NOT DEFINED ENV{TIARMCLANG_ROOT} OR NOT DEFINED ENV{MICROROS_SHIM_DIR})
  message(FATAL_ERROR "TIARMCLANG_ROOT and MICROROS_SHIM_DIR must be set (see build_library.sh)")
endif()

file(TO_CMAKE_PATH "$ENV{TIARMCLANG_ROOT}" TIARMCLANG_ROOT)
file(TO_CMAKE_PATH "$ENV{MICROROS_SHIM_DIR}" MICROROS_SHIM_DIR)

set(CMAKE_C_COMPILER   ${TIARMCLANG_ROOT}/bin/tiarmclang)
set(CMAKE_CXX_COMPILER ${TIARMCLANG_ROOT}/bin/tiarmclang)
set(CMAKE_AR           ${TIARMCLANG_ROOT}/bin/tiarmar CACHE FILEPATH "" FORCE)
set(CMAKE_USER_MAKE_RULES_OVERRIDE_CXX ${CMAKE_CURRENT_LIST_DIR}/tiarmclang_cxx_rules.cmake)

# tiarmar is llvm-ar: it writes the symbol index itself, no ranlib step needed.
foreach(lang C CXX)
  set(CMAKE_${lang}_ARCHIVE_CREATE "<CMAKE_AR> qcs <TARGET> <OBJECTS>")
  set(CMAKE_${lang}_ARCHIVE_APPEND "<CMAKE_AR> qs <TARGET> <OBJECTS>")
  set(CMAKE_${lang}_ARCHIVE_FINISH "")
endforeach()

# -mfloat-abi=soft: CM has no FPU
# __TI_TIME_USES_64: 64-bit signed time_t in the TI C library (POSIX-like);
# application must define it too, posix_shim/time.h enforces that.
# -Werror=implicit-function-declaration: a POSIX function missing from the TI C
# library must fail here, not turn into a call with a guessed signature.
set(MICROROS_TARGET_FLAGS "-mcpu=cortex-m4 -mthumb -mfloat-abi=soft -mlittle-endian")
set(MICROROS_COMMON_FLAGS "-ffunction-sections -fdata-sections -D__TI_TIME_USES_64=1 -isystem ${MICROROS_SHIM_DIR} -Werror=implicit-function-declaration")

set(CMAKE_C_FLAGS_INIT           "${MICROROS_TARGET_FLAGS} ${MICROROS_COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT         "${MICROROS_TARGET_FLAGS} ${MICROROS_COMMON_FLAGS} -fno-exceptions -fno-rtti")
# Cache entries, not *_INIT: CMake's TIClang module appends "-O3 -DNDEBUG" to the
# *_INIT values, which would override -Os.
set(CMAKE_C_FLAGS_RELEASE        "-Os -DNDEBUG" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE      "-Os -DNDEBUG" CACHE STRING "" FORCE)

# Micro-CDR reads this instead of probing the target.
set(__BIG_ENDIAN__ 0)

# Since Jazzy, rosidl generates full type descriptions (REP 2011) as writable
# static data: about 28 KB of RAM for the parameter server alone. The type
# hashes stay. Set here because micro_ros_setup's build passes no extra CMake
# arguments.
set(ROSIDL_GENERATOR_C_DISABLE_TYPE_DESCRIPTION_CODEGEN ON CACHE BOOL "" FORCE)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
