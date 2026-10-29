#!/usr/bin/env bash
# Repository lint, as CI runs it: repository checks (whitespace, line endings,
# SPDX headers, Markdown links), shellcheck, ruff, and well-formed projectspecs
# and target configurations. ruff and shellcheck are installed into a virtual
# environment on first use.
#
# Usage: tools/lint.sh
#
# Environment: MICROROS_LINT_VENV  the virtual environment
#              (default: ~/.cache/micro_ros_f2838x_cm/lint-venv)

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VENV=${MICROROS_LINT_VENV:-$HOME/.cache/micro_ros_f2838x_cm/lint-venv}
TOOLS=(ruff==0.17.2 shellcheck-py==0.11.0.1)

usage() { sed -n '2,/^$/s/^# \{0,1\}//p' "${BASH_SOURCE[0]}"; exit "${1:-0}"; }
step() { echo; echo "=== $*"; }

case "${1:-}" in
    "")        ;;
    -h|--help) usage ;;
    *)         echo "unknown option: $1" >&2; usage 1 ;;
esac

# On Windows, "python3" may be the Microsoft Store placeholder.
if [[ $(uname -s) == MINGW* || $(uname -s) == MSYS* ]]; then
    python=python
    bin=$VENV/Scripts
else
    python=python3
    bin=$VENV/bin
fi
if [[ ! -d $bin ]]; then
    step "Creating $VENV"
    "$python" -m venv "$VENV"
fi
"$bin/python" -m pip install --quiet --disable-pip-version-check "${TOOLS[@]}"
export PATH="$bin:$PATH"

cd "$REPO_ROOT"
step "Repository checks"
python tools/check_repo.py
step "Shell scripts"
shellcheck -x library_generation/*.sh tools/*.sh tools/ci/ccs/*.sh
step "Python tools"
ruff check tools library_generation/scripts
step "Projectspecs and target configurations"
git ls-files '*.projectspec' '*.ccxml' |
    xargs python -c 'import sys, xml.dom.minidom; [xml.dom.minidom.parse(p) for p in sys.argv[1:]]'
echo "well-formed"
