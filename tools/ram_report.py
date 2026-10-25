#!/usr/bin/env python3
"""RAM usage report from a TI Arm linker map file.

Prints every RAM region (name ending in "RAM" by default) with its size, use
and free space, and the output sections placed in it. The margin is the free
space summed over these regions; with --min-margin the exit code is 1 when the
margin is smaller.

Usage: ram_report.py MAPFILE [--regions CRAM,SRAM,...] [--min-margin 8K]
"""
import argparse
import re
import sys

MEMORY_LINE = re.compile(r'^\s+(\w+)\s+([0-9a-f]{8})\s+([0-9a-f]{8})\s+([0-9a-f]{8})\s+([0-9a-f]{8})\s+\S+')
SECTION_HEAD = re.compile(r'^(\S+)\s*(?:(\d+)\s+([0-9a-f]{8})\s+([0-9a-f]{8}))?')
SECTION_STAR = re.compile(r'^\*\s+(\d+)\s+([0-9a-f]{8})\s+([0-9a-f]{8})')

def parse_size(text):
    m = re.fullmatch(r'(\d+)([kK]?)', text)
    if not m:
        raise argparse.ArgumentTypeError(f'invalid size: {text}')
    return int(m.group(1)) * (1024 if m.group(2) else 1)

def parse_map(path):
    with open(path, errors='replace') as f:
        lines = f.read().splitlines()

    regions = []
    sections = []
    part = None
    pending = None
    for line in lines:
        if line.startswith('MEMORY CONFIGURATION'):
            part = 'memory'
            continue
        if line.startswith('SECTION ALLOCATION MAP'):
            part = 'sections'
            continue
        if line.startswith(('SEGMENT ALLOCATION MAP', 'MODULE SUMMARY', 'LINKER GENERATED')):
            part = None
            continue

        if part == 'memory':
            m = MEMORY_LINE.match(line)
            if m:
                name, origin, length, used, unused = m.groups()
                regions.append({'name': name, 'origin': int(origin, 16), 'length': int(length, 16),
                                'used': int(used, 16), 'unused': int(unused, 16)})
        elif part == 'sections':
            if pending is not None:
                m = SECTION_STAR.match(line)
                if m:
                    sections.append({'name': pending, 'origin': int(m.group(2), 16),
                                     'length': int(m.group(3), 16)})
                pending = None
                continue
            if line and not line[0].isspace() and not line.startswith(('-', '*', 'section', ' output')):
                m = SECTION_HEAD.match(line)
                if m and m.group(3):
                    sections.append({'name': m.group(1), 'origin': int(m.group(3), 16),
                                     'length': int(m.group(4), 16)})
                elif m:
                    pending = m.group(1)
    return regions, sections

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('mapfile')
    ap.add_argument('--regions', help='comma-separated region names (default: names ending in "RAM")')
    ap.add_argument('--min-margin', type=parse_size, help='minimum total free RAM, bytes or K')
    args = ap.parse_args()

    regions, sections = parse_map(args.mapfile)
    if not regions:
        sys.exit(f'error: no MEMORY CONFIGURATION in {args.mapfile}')

    if args.regions:
        wanted = set(args.regions.split(','))
        ram = [r for r in regions if r['name'] in wanted]
        missing = wanted - {r['name'] for r in ram}
        if missing:
            sys.exit('error: regions not in the map: ' + ', '.join(sorted(missing)))
    else:
        ram = [r for r in regions if r['name'].endswith('RAM')]

    print(f'{"region / section":<28}{"size":>10}{"used":>10}{"free":>10}')
    for region in ram:
        print(f'{region["name"]:<28}{region["length"]:>10}{region["used"]:>10}{region["unused"]:>10}')
        start, end = region['origin'], region['origin'] + region['length']
        inside = sorted((s for s in sections if start <= s['origin'] < end and s['length'] > 0),
                        key=lambda s: s['origin'])
        for s in inside:
            print(f'  {s["name"]:<26}{"":>10}{s["length"]:>10}')

    margin = sum(r['unused'] for r in ram)
    total = sum(r['length'] for r in ram)
    print(f'{"total":<28}{total:>10}{total - margin:>10}{margin:>10}')

    if args.min_margin is not None and margin < args.min_margin:
        print(f'FAIL: free RAM {margin} bytes is below the minimum of {args.min_margin} bytes')
        return 1
    if args.min_margin is not None:
        print(f'OK: free RAM {margin} bytes (minimum {args.min_margin})')
    return 0

if __name__ == '__main__':
    sys.exit(main())
