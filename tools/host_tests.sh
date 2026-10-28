#!/usr/bin/env bash
# Builds and runs the port's host unit tests (tests/port).
#
# Usage: tools/host_tests.sh [--native] [--sanitize]
#   default     in micro-ROS's library builder image, which build_library.sh
#               also uses (gcc and cmake from ROS 2's base image)
#   --native    with the host's gcc and cmake
#   --sanitize  with AddressSanitizer and UndefinedBehaviorSanitizer; any
#               finding fails the test
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DOCKER_IMAGE=microros/micro_ros_static_library_builder:jazzy
NATIVE=0
SANITIZE=0

usage() { sed -n '2,/^$/s/^# \{0,1\}//p' "${BASH_SOURCE[0]}"; exit "${1:-0}"; }

while [[ $# -gt 0 ]]; do
    case "$1" in
        --native)   NATIVE=1; shift ;;
        --sanitize) SANITIZE=1; shift ;;
        -h|--help)  usage ;;
        *)          echo "unknown option: $1" >&2; usage 1 ;;
    esac
done

# Separate build directories, so that a cached configuration never mixes the variants.
BUILD_DIR=/tmp/port-tests
CMAKE_ARGS="-DCMAKE_BUILD_TYPE=Debug"
if [[ $SANITIZE == 1 ]]; then
    BUILD_DIR=/tmp/port-tests-sanitize
    SAN_FLAGS="-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer"
    CMAKE_ARGS+=" -DCMAKE_C_FLAGS='$SAN_FLAGS' -DCMAKE_EXE_LINKER_FLAGS='$SAN_FLAGS'"
fi
CMDS="cmake -S tests/port -B $BUILD_DIR $CMAKE_ARGS &&
      cmake --build $BUILD_DIR &&
      ctest --test-dir $BUILD_DIR --output-on-failure"

if [[ $NATIVE == 1 ]]; then
    cd "$REPO_ROOT"
    bash -c "$CMDS"
    exit
fi

host_repo=$REPO_ROOT
if [[ $(uname -s) == MINGW* || $(uname -s) == MSYS* ]]; then
    export MSYS_NO_PATHCONV=1
    host_repo=$(cygpath -m "$REPO_ROOT")
fi
docker run --rm -v "$host_repo:/project" -w /project --entrypoint bash "$DOCKER_IMAGE" -c "$CMDS"
