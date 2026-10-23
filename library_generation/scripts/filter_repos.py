#!/usr/bin/env python3
"""Keeps only the listed repositories of a vcstool .repos file.

Usage: filter_repos.py KEEP_LIST < ros2.repos > filtered.repos

KEEP_LIST is a YAML file with a whitespace-separated "keep" entry, the format
used by micro_ros_setup (e.g. repos/jazzy/client_ros2_packages.txt).
"""
import sys
import yaml

def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)

    with open(sys.argv[1]) as f:
        keep = yaml.safe_load(f)['keep'].split()
    repos = (yaml.safe_load(sys.stdin) or {}).get('repositories', {})

    missing = [name for name in keep if name not in repos]
    if missing:
        sys.exit('repositories not found in input: ' + ', '.join(missing))

    kept = {name: repos[name] for name in keep}
    yaml.safe_dump({'repositories': kept}, sys.stdout, sort_keys=True)

if __name__ == '__main__':
    main()
