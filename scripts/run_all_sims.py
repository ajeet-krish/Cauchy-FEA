#!/usr/bin/env python3
"""
Run all FEA-2D simulation cases and generate output.
"""

import subprocess
import sys
import os

CASES = [
    ("FEA_Patch", [], "patch"),
    ("FEA_Cantilever", ["32"], "cantilever_32"),
    ("FEA_Michell", [], "michell"),
    ("FEA_Cook", ["32"], "cook_32"),
    ("FEA_LBracket", ["16"], "lbracket_16"),
    ("FEA_PlateHole", ["16"], "plate_hole_16"),
]


def main():
    build_dir = os.path.join(os.path.dirname(__file__), '..', 'build')

    for exe, args, outdir in CASES:
        exe_path = os.path.join(build_dir, exe)
        if not os.path.exists(exe_path):
            print(f'SKIP: {exe} not found (build first)')
            continue

        print(f'\n{"="*60}')
        print(f'Running {exe} {" ".join(args)}')
        print(f'{"="*60}')

        cmd = [exe_path] + args
        result = subprocess.run(cmd, capture_output=False, text=True)

        if result.returncode != 0:
            print(f'  WARNING: {exe} exited with code {result.returncode}')

    print(f'\n{"="*60}')
    print('All cases complete.')
    print(f'{"="*60}')


if __name__ == '__main__':
    main()
