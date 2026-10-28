#!/usr/bin/env bash
# Builds the CCS image for CI's firmware job (Dockerfile next to this script)
# and optionally pushes it to GHCR.
#
# Usage: tools/ci/ccs/build_image.sh --c2000ware DIR [--push]
#   --c2000ware DIR   C2000Ware installation
#   --push            push the image after building; log in first with
#                     "docker login ghcr.io" and a token with write:packages
#
# Environment: CCS_IMAGE  image name (default: ghcr.io/konstc/micro_ros_f2838x_cm/ccs)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IMAGE=${CCS_IMAGE:-ghcr.io/konstc/micro_ros_f2838x_cm/ccs}

# Also in .github/workflows/ci.yml (firmware job).
TAG=ccs21.0.1-c2000ware26.02.00.00
C2000WARE_VERSION=26.02.00.00
C2000WARE=
PUSH=0

usage() { sed -n '2,/^$/s/^# \{0,1\}//p' "${BASH_SOURCE[0]}"; exit "${1:-0}"; }
die() { echo "error: $*" >&2; exit 1; }

while [[ $# -gt 0 ]]; do
    case "$1" in
        --c2000ware) C2000WARE="$2"; shift 2 ;;
        --push)      PUSH=1; shift ;;
        -h|--help)   usage ;;
        *)           echo "unknown option: $1" >&2; usage 1 ;;
    esac
done

[[ -n $C2000WARE ]] || usage 1
grep -q "\"version\": \"$C2000WARE_VERSION\"" "$C2000WARE/.metadata/sdk.json" 2>/dev/null ||
    die "not a C2000Ware $C2000WARE_VERSION installation: $C2000WARE"

context=$SCRIPT_DIR
if [[ $(uname -s) == MINGW* || $(uname -s) == MSYS* ]]; then
    C2000WARE=$(cygpath -m "$C2000WARE")
    context=$(cygpath -m "$SCRIPT_DIR")
fi

docker build --build-context c2000ware="$C2000WARE" -t "$IMAGE:$TAG" "$context"
docker image ls "$IMAGE:$TAG"
if [[ $PUSH == 1 ]]; then
    docker push "$IMAGE:$TAG"
fi
