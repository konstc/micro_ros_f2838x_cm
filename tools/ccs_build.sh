#!/usr/bin/env bash
# Headless import and build of CCS projects (.projectspec) with CCS's ccs-server-cli.
#
# Usage: tools/ccs_build.sh [options] PROJECTSPEC...
#   --ccs DIR          CCS installation (default: $CCS_ROOT, else C:/ti/ccs2101 or /opt/ti/ccs2101)
#   --c2000ware DIR    C2000Ware installation (default: $C2000WARE_ROOT)
#   --workspace DIR    CCS workspace (default: <repo>/.ccs_workspace)
#   --config NAME      build only this configuration (repeatable; default: all in the projectspec)
#   --library D/P      link microros_static_library/D/P (e.g. lyrical/default) instead
#                      of the projectspec's MICROROS_LIB
#
# Works in Git Bash on Windows (ccs-server-cli.bat) and on Linux (ccs-server-cli.sh).

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CCS=${CCS_ROOT:-}
C2000WARE=${C2000WARE_ROOT:-}
WORKSPACE=$REPO_ROOT/.ccs_workspace
CONFIGS=()
SPECS=()
LIBRARY=

usage() { sed -n '2,/^$/s/^# \{0,1\}//p' "${BASH_SOURCE[0]}"; exit "${1:-0}"; }

while [[ $# -gt 0 ]]; do
    case "$1" in
        --ccs)        CCS="$2"; shift 2 ;;
        --c2000ware)  C2000WARE="$2"; shift 2 ;;
        --workspace)  WORKSPACE="$2"; shift 2 ;;
        --config)     CONFIGS+=("$2"); shift 2 ;;
        --library)    LIBRARY="$2"; shift 2 ;;
        -h|--help)    usage ;;
        -*)           echo "unknown option: $1" >&2; usage 1 ;;
        *)            SPECS+=("$1"); shift ;;
    esac
done
[[ ${#SPECS[@]} -gt 0 ]] || usage 1
[[ -n $C2000WARE ]] || { echo "error: pass --c2000ware or set C2000WARE_ROOT" >&2; exit 1; }
[[ -z $LIBRARY || $LIBRARY =~ ^[a-z0-9_]+/[a-z0-9_]+$ ]] ||
    { echo "error: --library takes DISTRO/PROFILE, e.g. lyrical/default" >&2; exit 1; }

if [[ $(uname -s) == MINGW* || $(uname -s) == MSYS* ]]; then
    CCS=${CCS:-C:/ti/ccs2101}
    CLI=("$CCS/ccs/eclipse/ccs-server-cli.bat")
    export MSYS_NO_PATHCONV=1
    native_path() { cygpath -m "$(realpath "$1")"; }
else
    CCS=${CCS:-/opt/ti/ccs2101}
    CLI=("$CCS/ccs/eclipse/ccs-server-cli.sh")
    native_path() { realpath "$1"; }
fi
[[ -e ${CLI[0]} ]] || { echo "error: CCS headless CLI not found: ${CLI[0]}" >&2; exit 1; }

mkdir -p "$WORKSPACE/logs"
WS=$(native_path "$WORKSPACE")
C2000WARE=$(native_path "$C2000WARE")

failed=0
for spec in "${SPECS[@]}"; do
    spec_path=$(native_path "$spec")
    # Tags may span lines: flatten the file before matching attributes.
    flat=$(tr '\n\r' '  ' < "$spec")
    project=$(grep -o '<project [^>]*' <<< "$flat" | grep -o ' name="[^"]*"' | head -1 | cut -d'"' -f2 || true)
    [[ -n $project ]] || { echo "error: no project name in $spec" >&2; failed=1; continue; }
    configs=("${CONFIGS[@]}")
    if [[ ${#configs[@]} -eq 0 ]]; then
        mapfile -t configs < <(grep -o '<configuration [^>]*' <<< "$flat" | grep -o ' name="[^"]*"' | cut -d'"' -f2)
    fi

    echo "== $project ($spec)"
    import_log=$WORKSPACE/logs/$project.import.log
    if ! "${CLI[@]}" -workspace "$WS" -application projectImport -ccs.location "$spec_path" \
            -ccs.definePathVariable COM_TI_C2000WARE_INSTALL_DIR "$C2000WARE" \
            -ccs.overwrite > "$import_log" 2>&1 || ! grep -q '^Done' "$import_log"; then
        echo "   import FAILED (see $import_log)"
        failed=1
        continue
    fi
    # Project-scope path variables beat -ccs.definePathVariable, so the imported
    # project is edited instead: MICROROS_LIB is a path variable in .project
    # (linked files) and a build macro in .cproject (include paths).
    if [[ -n $LIBRARY ]]; then
        sed -i "s#\(/microros_static_library/\)[^/<\"]*/[^/<\"]*\([<\"]\)#\1$LIBRARY\2#g" \
            "$WORKSPACE/$project/.project" "$WORKSPACE/$project/.cproject"
    fi

    for config in "${configs[@]}"; do
        log=$WORKSPACE/logs/$project.$config.log
        "${CLI[@]}" -workspace "$WS" -application projectBuild -ccs.projects "$project" \
            -ccs.configuration "$config" -ccs.buildType full -ccs.listProblems > "$log" 2>&1 || true
        summary=$(grep -o "Problem summary for project '$project': [0-9]* errors, [0-9]* warnings" "$log" \
                  | sed 's/.*: //' || true)
        echo "   $config: ${summary:-no summary (see $log)}"
        grep -E '(error|warning)( #[0-9]+[-A-Z]*)?:' "$log" | grep -v '^"' | sort -u | sed 's/^/      /' | head -20 || true
        if [[ -z $summary || ! $summary =~ ^0\ errors ]]; then
            failed=1
        fi
    done
done
exit $failed
