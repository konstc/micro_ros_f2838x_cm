#!/bin/bash
# Builds libmicroros.a in micro-ROS's static library builder image
# (microros/micro_ros_static_library_builder:<distro>), with micro_ros_setup's
# generate_lib target and TI Arm Clang. From the repository root:
#
#   docker run --rm -v "$(pwd)":/project -e MICROROS_LIBRARY_FOLDER=. \
#       microros/micro_ros_static_library_builder:jazzy [options]
#
# The image's entrypoint runs this script; build_library.sh does the same with
# a cache volume for the compiler and the output owned by the calling user.
#
# Options:
#   --profile NAME         profile from library_generation/profiles (default: default)
#   --extra-packages DIR   extra ROS 2 packages (.msg/.srv) to build in, as a path
#                          in the container (/project/...); repeatable
#   --setup-from-branch    first install micro_ros_setup from the head of its
#                          <distro> branch instead of using the image's copy; also
#                          runs in a plain ros:<distro> image (as root)
#
# The distribution is the image's ROS_DISTRO. TI Arm Clang is installed into
# $MICROROS_CACHE_DIR (default /tmp/micro_ros_f2838x_cm); mount a volume there
# to keep it between runs.
#
# Output: microros_static_library/<distro>/<profile>/
#           libmicroros.a, include/, posix_shim/, build_info.json, sources.repos

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
# shellcheck source=library_generation/common.sh
. "$SCRIPT_DIR/common.sh"

UROS_WS=/uros_ws    # the image's micro_ros_setup workspace
PROFILE=default
EXTRA_PACKAGES=()
SETUP_FROM_BRANCH=0

usage() { sed -n '2,/^$/s/^# \{0,1\}//p' "${BASH_SOURCE[0]}"; exit "${1:-0}"; }

while [[ $# -gt 0 ]]; do
    case "$1" in
        --profile)        PROFILE="$2"; shift 2 ;;
        --extra-packages) EXTRA_PACKAGES+=("$(realpath "$2")"); shift 2 ;;
        --setup-from-branch) SETUP_FROM_BRANCH=1; shift ;;
        -h|--help)        usage ;;
        *)                echo "unknown option: $1" >&2; usage 1 ;;
    esac
done

[[ -n ${ROS_DISTRO:-} ]] && [[ $SETUP_FROM_BRANCH == 1 || -f $UROS_WS/install/local_setup.bash ]] ||
    die "run this in microros/micro_ros_static_library_builder:<distro> (see build_library.sh)"
DISTRO=$ROS_DISTRO
META=$SCRIPT_DIR/profiles/$PROFILE.meta
[[ -f $META ]] || die "no profile '$PROFILE' ($META)"
OUT=$REPO_ROOT/microros_static_library/$DISTRO/$PROFILE
CACHE=${MICROROS_CACHE_DIR:-/tmp/micro_ros_f2838x_cm}

# Replaces the image's micro_ros_setup, or provides one in ros:<distro>. The
# images delete the apt lists, which rosdep needs to install dependencies.
install_micro_ros_setup() {
    step "Installing micro_ros_setup from its $DISTRO branch"
    mkdir -p "$UROS_WS/src"
    cd "$UROS_WS"
    rm -rf src/micro_ros_setup
    git clone --quiet --depth 1 -b "$DISTRO" https://github.com/micro-ROS/micro_ros_setup.git src/micro_ros_setup
    apt-get update -qq
    rosdep update --rosdistro "$DISTRO" -q
    rosdep install --from-paths src --ignore-src -y -q
    colcon build
}

# ROS setup scripts read unset variables.
ros_source() {
    set +u
    # shellcheck disable=SC1090
    source "$1"
    set -u
}

ros_source "/opt/ros/$ROS_DISTRO/setup.bash"
if [[ $SETUP_FROM_BRANCH == 1 ]]; then
    install_micro_ros_setup
fi
ros_source "$UROS_WS/install/local_setup.bash"

# Recorded in build_info.json.
MICRO_ROS_SETUP=$(sed -n 's:.*<version>\(.*\)</version>.*:\1:p' \
    "$UROS_WS/install/micro_ros_setup/share/micro_ros_setup/package.xml")
if [[ $SETUP_FROM_BRANCH == 1 ]]; then
    MICRO_ROS_SETUP+=" ($DISTRO branch, $(git -C "$UROS_WS/src/micro_ros_setup" rev-parse --short=12 HEAD))"
else
    MICRO_ROS_SETUP+=" (builder image)"
fi

setup_toolchain "$CACHE/toolchain"

step "Creating the generate_lib workspace (micro_ros_setup, $DISTRO)"
cd "$UROS_WS"
rm -rf firmware
ros2 run micro_ros_setup create_firmware_ws.sh generate_lib

prepare_mcu_ws "$UROS_WS/firmware/mcu_ws"

step "Cross-compiling micro-ROS (micro_ros_setup, profile $PROFILE)"
export MICROROS_SHIM_DIR=$SCRIPT_DIR/posix_shim
ros2 run micro_ros_setup build_firmware.sh "$SCRIPT_DIR/toolchains/tiarmclang_cm4.cmake" "$META"

# micro_ros_setup also packs firmware/build/libmicroros.a, with the host's ar;
# the package is built from the same per-package archives with tiarmar.
package "$UROS_WS/firmware/mcu_ws" "$UROS_WS/firmware/package"
