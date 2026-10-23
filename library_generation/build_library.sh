#!/usr/bin/env bash
# Builds libmicroros.a for the F2838x Connectivity Manager with TI Arm Clang.
#
# Usage: library_generation/build_library.sh [options]
#   --distro NAME          ROS 2 distribution (default: jazzy)
#   --profile NAME         profile from library_generation/profiles (default: default)
#   --toolchain NAME       compiler toolchain; only "tiarmclang" is supported (default)
#   --extra-packages DIR   extra ROS 2 packages (.msg/.srv) to build in; repeatable.
#                          In Docker mode DIR must be inside the repository.
#   --native               build on this Linux host (e.g. WSL2) instead of in Docker
#   --clean                drop the cached workspaces and fetch all sources again
#
# Output: microros_static_library/<distro>/<profile>/
#           libmicroros.a, include/, posix_shim/, build_info.json, sources.repos

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

TIARMCLANG_VERSION=5.1.1.LTS
TIARMCLANG_INSTALLER=ti_cgt_armllvm_${TIARMCLANG_VERSION}_linux-x64_installer.bin
TIARMCLANG_URL=https://dr-download.ti.com/software-development/ide-configuration-compiler-or-debugger/MD-ayxs93eZNN/${TIARMCLANG_VERSION}/${TIARMCLANG_INSTALLER}
TIARMCLANG_SHA256=ec4c54500b04aa3245d855038a86d3123c53c9eb14c2bfba5fcfb633dc3b3869

DOCKER_IMAGE=micro_ros_f2838x_cm_builder:tiarmclang-${TIARMCLANG_VERSION}
DOCKER_WORK_VOLUME=micro_ros_f2838x_cm_work

DISTRO=jazzy
PROFILE=default
TOOLCHAIN=tiarmclang
EXTRA_PACKAGES=()
NATIVE=0
CLEAN=0

die() { echo "error: $*" >&2; exit 1; }
step() { echo; echo "=== $*"; }

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
REPOS_DIR=$SCRIPT_DIR/repos/$DISTRO
META=$SCRIPT_DIR/profiles/$PROFILE.meta
TOOLCHAIN_FILE=$SCRIPT_DIR/toolchains/${TOOLCHAIN}_cm4.cmake
[[ -d $REPOS_DIR ]] || die "no package lists for distro '$DISTRO' in $REPOS_DIR"
[[ -f $META ]] || die "no profile '$PROFILE' ($META)"

# -------------------------------- Docker mode --------------------------------

run_in_docker() {
    command -v docker >/dev/null || die "docker not found; install Docker or use --native"
    local args=(--distro "$DISTRO" --profile "$PROFILE" --toolchain "$TOOLCHAIN") pkg
    [[ $CLEAN == 1 ]] && args+=(--clean)
    for pkg in "${EXTRA_PACKAGES[@]}"; do
        [[ $pkg == "$REPO_ROOT"/* ]] || die "in Docker mode --extra-packages must be inside the repository: $pkg"
        args+=(--extra-packages "/project/${pkg#"$REPO_ROOT"/}")
    done

    local host_repo=$REPO_ROOT host_ctx=$SCRIPT_DIR run_opts=()
    if [[ $(uname -s) == MINGW* || $(uname -s) == MSYS* ]]; then
        # Git Bash on Windows: pass Windows paths to docker and stop MSYS from
        # rewriting container paths (/project, /work).
        export MSYS_NO_PATHCONV=1
        host_repo=$(cygpath -m "$REPO_ROOT")
        host_ctx=$(cygpath -m "$SCRIPT_DIR")
    else
        # Linux: hand output files back to the calling user.
        run_opts+=(-e HOST_UID="$(id -u)" -e HOST_GID="$(id -g)")
    fi
    [[ -t 1 ]] && run_opts+=(-t)

    step "Building Docker image $DOCKER_IMAGE"
    docker build \
        --build-arg TIARMCLANG_VERSION="$TIARMCLANG_VERSION" \
        --build-arg TIARMCLANG_URL="$TIARMCLANG_URL" \
        --build-arg TIARMCLANG_SHA256="$TIARMCLANG_SHA256" \
        -t "$DOCKER_IMAGE" "$host_ctx"

    step "Running the build in Docker"
    docker run --rm "${run_opts[@]}" \
        -v "$host_repo:/project" \
        -v "$DOCKER_WORK_VOLUME:/work" \
        -e MICROROS_WORK_DIR=/work \
        "$DOCKER_IMAGE" \
        bash /project/library_generation/build_library.sh --native "${args[@]}"
}

if [[ $NATIVE == 0 ]]; then
    run_in_docker
    exit 0
fi

# -------------------------------- Native mode -------------------------------- 

WORK_ROOT=${MICROROS_WORK_DIR:-$HOME/.cache/micro_ros_f2838x_cm}
WORK=$WORK_ROOT/$DISTRO
FW=$WORK/firmware
OUT=$REPO_ROOT/microros_static_library/$DISTRO/$PROFILE

setup_python() {
    # The Docker image provides the tools on PATH; elsewhere use our own venv.
    if command -v colcon >/dev/null && command -v vcs >/dev/null; then
        return
    fi
    local venv=$WORK_ROOT/venv
    step "Python tools ($venv)"
    [[ -x $venv/bin/python ]] || python3 -m venv "$venv"
    "$venv/bin/pip" install --quiet --disable-pip-version-check -r "$SCRIPT_DIR/requirements.txt"
    export PATH="$venv/bin:$PATH"
}

setup_toolchain() {
    if [[ -n ${TIARMCLANG_ROOT:-} ]]; then
        [[ -x $TIARMCLANG_ROOT/bin/tiarmclang ]] || die "TIARMCLANG_ROOT has no bin/tiarmclang: $TIARMCLANG_ROOT"
        return
    fi
    local dir=$WORK_ROOT/toolchain
    local root=$dir/ti-cgt-armllvm_$TIARMCLANG_VERSION
    if [[ ! -x $root/bin/tiarmclang ]]; then
        step "Installing TI Arm Clang $TIARMCLANG_VERSION into $dir"
        mkdir -p "$dir"
        local installer=$dir/$TIARMCLANG_INSTALLER
        if [[ ! -f $installer ]]; then
            curl -fL -o "$installer.part" "$TIARMCLANG_URL"
            mv "$installer.part" "$installer"
        fi
        echo "$TIARMCLANG_SHA256  $installer" | sha256sum -c -
        chmod +x "$installer"
        "$installer" --mode unattended --prefix "$dir"
    fi
    export TIARMCLANG_ROOT=$root
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

prepare_mcu_ws() {
    step "Preparing mcu_ws: extra packages and patches"
    local extra=$FW/mcu_ws/extra_packages
    rm -rf "$extra"
    mkdir -p "$extra"
    cp -R "$SCRIPT_DIR/extra_packages/." "$extra/"
    if python3 -c 'import sys, yaml; sys.exit(not (yaml.safe_load(open(sys.argv[1])) or {}).get("repositories"))' \
            "$extra/extra_packages.repos"; then
        (cd "$extra" && vcs import --input extra_packages.repos)
    fi
    local pkg
    for pkg in "${EXTRA_PACKAGES[@]}"; do
        [[ -d $pkg ]] || die "extra package directory not found: $pkg"
        cp -R "$pkg" "$extra/"
    done

    # patches/<distro>/<repository path in mcu_ws>/*.patch, applied in name order.
    local pdir=$SCRIPT_DIR/patches/$DISTRO patch rel repo
    [[ -d $pdir ]] || return 0
    while IFS= read -r patch; do
        rel=${patch#"$pdir"/}
        repo=$FW/mcu_ws/$(dirname "$rel")
        if git -C "$repo" apply --reverse --check "$patch" 2>/dev/null; then
            echo "already applied: $rel"
        else
            echo "applying: $rel"
            git -C "$repo" apply "$patch"
        fi
    done < <(find "$pdir" -name '*.patch' | sort)
}

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
            -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
            -DROSIDL_GENERATOR_C_DISABLE_TYPE_DESCRIPTION_CODEGEN=ON
}

package() {
    step "Packaging into $OUT"
    local ar=$TIARMCLANG_ROOT/bin/tiarmar
    local stage=$WORK/package lib name obj dir
    local result=$stage/result
    rm -rf "$stage"
    mkdir -p "$stage/objs" "$result"

    while IFS= read -r lib; do
        name=$(basename "$lib" .a)
        mkdir -p "$stage/x/$name"
        (cd "$stage/x/$name" && "$ar" x "$lib")
        for obj in "$stage/x/$name"/*; do
            mv "$obj" "$stage/objs/$name-$(basename "$obj")"
        done
    done < <(find "$FW/mcu_ws/install/lib" -name '*.a' | sort)
    (cd "$stage/objs" && "$ar" rcs "$result/libmicroros.a" ./*)

    cp -R "$FW/mcu_ws/install/include" "$result/include"
    find "$result/include" -type f ! -name '*.h' ! -name '*.hpp' -delete
    find "$result/include" -type d -empty -delete
    for dir in "$result/include"/*/; do
        name=$(basename "$dir")
        if [[ -d $dir$name ]]; then
            cp -R "$dir$name/." "$dir"
            rm -rf "${dir:?}$name"
        fi
    done
    cp -R "$SCRIPT_DIR/posix_shim" "$result/posix_shim"

    (cd "$FW/mcu_ws" && vcs export --exact --nested) > "$result/sources.repos"
    python3 "$SCRIPT_DIR/scripts/write_build_info.py" \
        --distro "$DISTRO" --profile "$PROFILE" --meta "$META" \
        --compiler "$TIARMCLANG_ROOT/bin/tiarmclang" \
        --cmake-cache "$FW/mcu_ws/build/rcutils/CMakeCache.txt" \
        --sources "$result/sources.repos" \
        --json "$result/build_info.json" \
        --header "$result/include/microros_f2838x_build_info.h"

    # Replace the contents of $OUT (the folder itself may be open in CCS).
    mkdir -p "$OUT"
    find "$OUT" -mindepth 1 -delete
    cp -R "$result/." "$OUT/"

    if [[ -n ${HOST_UID:-} ]]; then
        chown -R "$HOST_UID:${HOST_GID:-$HOST_UID}" "$REPO_ROOT/microros_static_library"
    fi
    echo
    ls -l "$OUT/libmicroros.a"
    echo "Done: $OUT"
}

mkdir -p "$WORK"
setup_python
setup_toolchain
fetch_sources
build_dev_ws
prepare_mcu_ws
build_mcu_ws
package
