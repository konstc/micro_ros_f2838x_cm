# Shared by build_library.sh (native mode) and library_generation.sh (micro-ROS
# static library builder image): TI Arm Clang, preparation of the mcu workspace
# and packaging of the result. Sourced, not run.
#
# The caller sets SCRIPT_DIR (library_generation/), DISTRO, PROFILE, META (the
# profile's .meta file), EXTRA_PACKAGES (array of package directories) and OUT
# (the output folder); library_generation.sh also sets MICRO_ROS_SETUP (the
# micro_ros_setup that built the library, recorded in build_info.json).

# shellcheck shell=bash

TIARMCLANG_VERSION=5.1.1.LTS
TIARMCLANG_INSTALLER=ti_cgt_armllvm_${TIARMCLANG_VERSION}_linux-x64_installer.bin
TIARMCLANG_URL=https://dr-download.ti.com/software-development/ide-configuration-compiler-or-debugger/MD-ayxs93eZNN/${TIARMCLANG_VERSION}/${TIARMCLANG_INSTALLER}
TIARMCLANG_SHA256=ec4c54500b04aa3245d855038a86d3123c53c9eb14c2bfba5fcfb633dc3b3869

die() { echo "error: $*" >&2; exit 1; }
step() { echo; echo "=== $*"; }

# Installs TI Arm Clang into DIR, unless TIARMCLANG_ROOT already names an
# installation; exports TIARMCLANG_ROOT.
setup_toolchain() {
    local dir=$1
    if [[ -n ${TIARMCLANG_ROOT:-} ]]; then
        [[ -x $TIARMCLANG_ROOT/bin/tiarmclang ]] || die "TIARMCLANG_ROOT has no bin/tiarmclang: $TIARMCLANG_ROOT"
        return
    fi
    local root=$dir/ti-cgt-armllvm_$TIARMCLANG_VERSION
    if [[ ! -x $root/bin/tiarmclang ]]; then
        step "Installing TI Arm Clang $TIARMCLANG_VERSION into $dir"
        mkdir -p "$dir"
        local installer=$dir/$TIARMCLANG_INSTALLER
        if [[ ! -f $installer ]]; then
            if command -v curl >/dev/null; then
                curl -fL -o "$installer.part" "$TIARMCLANG_URL"
            else
                wget -q -O "$installer.part" "$TIARMCLANG_URL"
            fi
            mv "$installer.part" "$installer"
        fi
        echo "$TIARMCLANG_SHA256  $installer" | sha256sum -c -
        chmod +x "$installer"
        "$installer" --mode unattended --prefix "$dir"
    fi
    export TIARMCLANG_ROOT=$root
}

# Adds the extra packages and applies the patches in the mcu workspace MCU_WS.
prepare_mcu_ws() {
    local mcu_ws=$1
    step "Preparing mcu_ws: extra packages and patches"
    local extra=$mcu_ws/extra_packages
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
        repo=$mcu_ws/$(dirname "$rel")
        if git -C "$repo" apply --reverse --check "$patch" 2>/dev/null; then
            echo "already applied: $rel"
        else
            echo "applying: $rel"
            git -C "$repo" apply "$patch"
        fi
    done < <(find "$pdir" -name '*.patch' | sort)
}

# Packs the built mcu workspace MCU_WS into OUT, staging in STAGE: one archive
# made with tiarmar from the per-package archives, the headers, posix_shim/,
# sources.repos and build_info.json.
package() {
    local mcu_ws=$1 stage=$2
    step "Packaging into $OUT"
    local ar=$TIARMCLANG_ROOT/bin/tiarmar lib name obj dir
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
    done < <(find "$mcu_ws/install/lib" -name '*.a' | sort)
    (cd "$stage/objs" && "$ar" rcs "$result/libmicroros.a" ./*)

    cp -R "$mcu_ws/install/include" "$result/include"
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

    (cd "$mcu_ws" && vcs export --exact --nested) > "$result/sources.repos"
    python3 "$SCRIPT_DIR/scripts/write_build_info.py" \
        --distro "$DISTRO" --profile "$PROFILE" --meta "$META" \
        --compiler "$TIARMCLANG_ROOT/bin/tiarmclang" \
        --cmake-cache "$mcu_ws/build/rcutils/CMakeCache.txt" \
        --sources "$result/sources.repos" \
        ${MICRO_ROS_SETUP:+--micro-ros-setup "$MICRO_ROS_SETUP"} \
        --json "$result/build_info.json" \
        --header "$result/include/microros_f2838x_build_info.h"

    # Replace the contents of $OUT (the folder itself may be open in CCS).
    mkdir -p "$OUT"
    find "$OUT" -mindepth 1 -delete
    cp -R "$result/." "$OUT/"

    if [[ -n ${HOST_UID:-} ]]; then
        chown -R "$HOST_UID:${HOST_GID:-$HOST_UID}" "$(dirname "$(dirname "$OUT")")"
    fi
    echo
    ls -l "$OUT/libmicroros.a"
    echo "Done: $OUT"
}
