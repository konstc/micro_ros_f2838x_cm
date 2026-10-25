#!/usr/bin/env python3
"""Repository checks, run by CI and handy before a commit.

Checks every file git knows about (tracked, or untracked and not ignored):
  - no trailing whitespace in text files;
  - LF line endings in the git index (what a checkout produces with
    .gitattributes);
  - an SPDX-License-Identifier in the header of this project's C sources,
    headers and linker command files;
  - relative links in Markdown files point to existing files.
Third-party files kept as upstream ships them are exempt (THIRD_PARTY).

Usage: check_repo.py    (exit code 1 and a list of problems if a check fails)
"""
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent

THIRD_PARTY = {
    '3rd-party-licenses.txt',
    'LICENSE',
    'boards/tmdscncd28388d/targetConfigs/f28388d_cpu1_attach.gel',
    'boards/tmdscncd28388d/targetConfigs/f28388x_cm_attach.gel',
    'device/f2838x/cm/freertos_port/port.c',
    'device/f2838x/cm/freertos_port/portmacro.h',
}
SPDX_SUFFIXES = {'.c', '.h', '.cmd'}
SPDX_LINES = 40     # the identifier must appear in the first lines (the header comment)
LINK = re.compile(r'\]\(([^)\s]+)\)')

def git(*args):
    return subprocess.run(['git', *args], cwd=REPO, check=True, capture_output=True,
                          text=True).stdout.splitlines()

def read_text(path):
    try:
        return (REPO / path).read_text(encoding='utf-8')
    except (UnicodeDecodeError, FileNotFoundError, IsADirectoryError):
        return None     # binary, or deleted but still in the index

def check_file(path, text, problems):
    for number, line in enumerate(text.splitlines(), 1):
        if line != line.rstrip(' \t'):
            problems.append(f'{path}:{number}: trailing whitespace')

    header = '\n'.join(text.splitlines()[:SPDX_LINES])
    if Path(path).suffix in SPDX_SUFFIXES and 'SPDX-License-Identifier:' not in header:
        problems.append(f'{path}: no SPDX-License-Identifier in the header')

    if path.endswith('.md'):
        for target in LINK.findall(text):
            if target.startswith(('http://', 'https://', 'mailto:', '#')):
                continue
            if not (REPO / path).parent.joinpath(target.split('#')[0]).exists():
                problems.append(f'{path}: broken link {target}')

def check_line_endings(problems):
    # "i/lf    w/crlf  attr/text            <TAB>path": i/ is the index content.
    for line in git('ls-files', '--eol'):
        info, path = line.split('\t', 1)
        index_eol = info.split()[0]
        if path not in THIRD_PARTY and index_eol in ('i/crlf', 'i/mixed'):
            problems.append(f'{path}: {index_eol[2:].upper()} line endings in the index')

def main():
    problems = []
    for path in sorted(set(git('ls-files', '--cached', '--others', '--exclude-standard'))):
        if path in THIRD_PARTY:
            continue
        text = read_text(path)
        if text is not None:
            check_file(path, text, problems)
    check_line_endings(problems)

    for problem in problems:
        print(problem)
    print(f'check_repo: {len(problems)} problem(s)' if problems else 'check_repo: OK')
    return 1 if problems else 0

if __name__ == '__main__':
    sys.exit(main())
