#!/usr/bin/env bash
# Builds libmicroros.a for the F2838x Connectivity Manager with TI Arm Clang.
#
# Usage: library_generation/build_library.sh [options]
#   --distro NAME          ROS 2 distribution (default: jazzy)
#   --profile NAME         profile from library_generation/profiles (default: default)
#   --toolchain NAME       compiler toolchain; only "tiarmclang" is supported (default)
#   --extra-packages DIR   extra ROS 2 packages (.msg/.srv) to build in; repeatable.
#                          In Docker mode DIR must be inside the repository.
#   --native               build on this Linux host (e.g. WSL2) instead of in Docker;
#                          Jazzy only
#   --clean                native mode: drop the cached workspaces and fetch all
#                          sources again (a Docker build always starts afresh)
#
# Docker mode runs library_generation.sh in micro-ROS's static library builder
# image, microros/micro_ros_static_library_builder:<distro>, which builds with
# micro_ros_setup. Lyrical has no such image yet (micro-ROS/docker#117) and
# the Rolling image's micro_ros_setup predates fixes the build needs: both
# install micro_ros_setup from its distro branch first, Lyrical in ros:lyrical.
# Native mode takes the same steps without a ROS 2 installation, from the
# package lists in repos/<distro>/.
#
# Environment:
#   MICROROS_DOCKER_CACHE  Docker mode: volume name or absolute path that keeps
#                          TI Arm Clang (default: volume micro_ros_f2838x_cm_cache)
#   MICROROS_WORK_DIR      native mode: toolchain, sources and workspaces
#                          (default: ~/.cache/micro_ros_f2838x_cm)
#
# Output: microros_static_library/<distro>/<profile>/
#           libmicroros.a, include/, posix_shim/, build_info.json, sources.repos

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
# shellcheck source=library_generation/common.sh
. "$SCRIPT_DIR/common.sh"

BUILDER_IMAGE=microros/micro_ros_static_library_builder
DOCKER_CACHE=${MICROROS_DOCKER_CACHE:-micro_ros_f2838x_cm_cache}

DISTRO=jazzy
PROFILE=default
TOOLCHAIN=tiarmclang
EXTRA_PACKAGES=()
NATIVE=0
CLEAN=0

usage() { sed -n '2,/^$/s/^# \{0,1\}//p' "${BASH_SOURCE[0]}"; exit "${1:-0}"; }

while [[ $# -gt 0 ]]; do
    case "$1" in
        --distro)         DISTRO="$2"; shift 2 ;;
        --profile)        PROFILE="$2"; shift 2 ;;
        --toolchain)      TOOLCHAIN="$2"; shift 2 ;;
        --extra-packages) EXTRA_PACKAGES+=("$(realpath "$2")"); shift 2 ;;
        --native)         NATIVE=1; shift ;;
        --clean)          CLEAN=1; shift ;;
        -h|--help)        usage ;;
        *)                echo "unknown option: $1" >&2; usage 1 ;;
    esac
done

[[ $TOOLCHAIN == tiarmclang ]] || die "unsupported toolchain '$TOOLCHAIN' (only tiarmclang)"
META=$SCRIPT_DIR/profiles/$PROFILE.meta
TOOLCHAIN_FILE=$SCRIPT_DIR/toolchains/${TOOLCHAIN}_cm4.cmake
[[ -f $META ]] || die "no profile '$PROFILE' ($META)"
OUT=$REPO_ROOT/microros_static_library/$DISTRO/$PROFILE

# -------------------------------- Docker mode --------------------------------

run_in_docker() {
    command -v docker >/dev/null || die "docker not found; install Docker or use --native"
    local image=$BUILDER_IMAGE:$DISTRO
    local args=(--profile "$PROFILE") pkg
    case $DISTRO in
        lyrical) image=ros:lyrical; args+=(--setup-from-branch) ;;
        rolling) args+=(--setup-from-branch) ;;
    esac
    for pkg in "${EXTRA_PACKAGES[@]}"; do
        [[ $pkg == "$REPO_ROOT"/* ]] || die "in Docker mode --extra-packages must be inside the repository: $pkg"
        args+=(--extra-packages "/project/${pkg#"$REPO_ROOT"/}")
    done

    local host_repo=$REPO_ROOT cache=$DOCKER_CACHE run_opts=()
    if [[ $(uname -s) == MINGW* || $(uname -s) == MSYS* ]]; then
        # Git Bash on Windows: pass Windows paths to docker and stop MSYS from
        # rewriting container paths (/project, /cache).
        export MSYS_NO_PATHCONV=1
        host_repo=$(cygpath -m "$REPO_ROOT")
        [[ $cache != */* ]] || cache=$(cygpath -m "$cache")
    else
        # Linux: hand output files back to the calling user.
        run_opts+=(-e HOST_UID="$(id -u)" -e HOST_GID="$(id -g)")
    fi
    [[ -t 1 ]] && run_opts+=(-t)

    step "Pulling $image"
    docker pull "$image" || docker image inspect "$image" >/dev/null ||
        die "image $image is neither pullable nor present"

    # bash as the entrypoint: the image's own entrypoint needs the script to be
    # executable, which a checkout on Windows does not guarantee.
    step "Running library_generation.sh in $image"
    docker run --rm "${run_opts[@]}" \
        -v "$host_repo:/project" \
        -v "$cache:/cache" \
        -e MICROROS_CACHE_DIR=/cache \
        --entrypoint bash \
        "$image" \
        /project/library_generation/library_generation.sh "${args[@]}"
}

if [[ $NATIVE == 0 ]]; then
    run_in_docker
    exit 0
fi

# -------------------------------- Native mode --------------------------------

REPOS_DIR=$SCRIPT_DIR/repos/$DISTRO
[[ -d $REPOS_DIR ]] || die "no package lists for '$DISTRO' in $REPOS_DIR; build it in Docker mode"
WORK_ROOT=${MICROROS_WORK_DIR:-$HOME/.cache/micro_ros_f2838x_cm}
WORK=$WORK_ROOT/$DISTRO
FW=$WORK/firmware

setup_python() {
    # Tools already on PATH (e.g. a ROS 2 environment) are used; otherwise our own venv.
    if command -v colcon >/dev/null && command -v vcs >/dev/null; then
        return
    fi
    local venv=$WORK_ROOT/venv
    step "Python tools ($venv)"
    [[ -x $venv/bin/python ]] || python3 -m venv "$venv"
    "$venv/bin/pip" install --quiet --disable-pip-version-check -r "$SCRIPT_DIR/requirements.txt"
    export PATH="$venv/bin:$PATH"
}

fetch_sources() {
    if [[ $CLEAN == 1 ]]; then
        rm -rf "$FW"
    fi
    if [[ -f $FW/.sources_ready ]]; then
        return
    fi
    step "Fetching sources into $FW"
    rm -rf "$FW"
    mkdir -p "$FW/dev_ws" "$FW/mcu_ws"
    curl -fsSL -o "$FW/ros2.repos" "https://raw.githubusercontent.com/ros2/ros2/$DISTRO/ros2.repos"

    python3 "$SCRIPT_DIR/scripts/filter_repos.py" "$REPOS_DIR/dev_ros2_packages.txt" \
        < "$FW/ros2.repos" > "$FW/dev_ws/ros2.repos"
    (cd "$FW/dev_ws" && vcs import --input ros2.repos --skip-existing)

    python3 "$SCRIPT_DIR/scripts/filter_repos.py" "$REPOS_DIR/client_ros2_packages.txt" \
        < "$FW/ros2.repos" > "$FW/mcu_ws/ros2.repos"
    (cd "$FW/mcu_ws" && vcs import --input ros2.repos --skip-existing \
                     && vcs import --input "$REPOS_DIR/client_uros_packages.repos" --skip-existing)

    local path
    while read -r path; do
        [[ -z $path || $path == \#* ]] && continue
        touch "$FW/mcu_ws/$path/COLCON_IGNORE"
    done < "$REPOS_DIR/colcon_ignore.txt"
    touch "$FW/.sources_ready"
}

build_dev_ws() {
    if [[ ! -f $FW/dev_ws/install/setup.bash ]]; then
        step "Building host tools (dev_ws)"
        (cd "$FW/dev_ws" && colcon build --event-handlers console_cohesion+)
    fi
    set +u
    # shellcheck disable=SC1091
    source "$FW/dev_ws/install/setup.bash"
    set -u
}

# The same colcon invocation as micro_ros_setup's generate_lib target.
build_mcu_ws() {
    step "Cross-compiling micro-ROS (profile $PROFILE)"
    export MICROROS_SHIM_DIR=$SCRIPT_DIR/posix_shim
    cd "$FW/mcu_ws"
    rm -rf build install log
    colcon build \
        --merge-install \
        --packages-ignore-regex='.*_cpp' \
        --event-handlers console_cohesion+ \
        --metas "$META" \
        --cmake-args \
            --no-warn-unused-cli \
            -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=OFF \
            -DTHIRDPARTY=ON \
            -DBUILD_SHARED_LIBS=OFF \
            -DBUILD_TESTING=OFF \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE"
}

mkdir -p "$WORK"
setup_python
setup_toolchain "$WORK_ROOT/toolchain"
fetch_sources
build_dev_ws
prepare_mcu_ws "$FW/mcu_ws"
build_mcu_ws
package "$FW/mcu_ws" "$WORK/package"
