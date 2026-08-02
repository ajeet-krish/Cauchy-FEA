#!/usr/bin/env python3
"""
Run all FEA-2D simulation cases and generate output.

Phase 2: Finer Q8 meshes for production-grade convergence data.
"""

import subprocess
import sys
import os

# Primary meshes (Q8 as the main element type)
PRIMARY_CASES = [
    ("FEA_Cantilever", ["32", "--q8"], "cantilever"),
    ("FEA_Cook", ["32", "--q8"], "cook"),
    ("FEA_PlateHole", ["16", "--q8"], "plate_hole"),
    ("FEA_Patch", [], "patch"),
    ("FEA_Michell", [], "michell"),
    ("FEA_LBracket", ["16"], "lbracket"),
    ("FEA_ThermalCylinder", ["32"], "thermal_cylinder"),
]

# Finer convergence meshes (Q8 where supported, Q4 otherwise)
CONVERGENCE_CASES = [
    # Cantilever: nx x nx/4 aspect ratio
    ("FEA_Cantilever", ["8", "--q8"], "cantilever"),
    ("FEA_Cantilever", ["16", "--q8"], "cantilever"),
    ("FEA_Cantilever", ["32", "--q8"], "cantilever"),
    ("FEA_Cantilever", ["64", "--q8"], "cantilever"),
    ("FEA_Cantilever", ["128", "--q8"], "cantilever"),
    # Cook: nx x nx
    ("FEA_Cook", ["8", "--q8"], "cook"),
    ("FEA_Cook", ["16", "--q8"], "cook"),
    ("FEA_Cook", ["32", "--q8"], "cook"),
    ("FEA_Cook", ["64", "--q8"], "cook"),
    ("FEA_Cook", ["128", "--q8"], "cook"),
    # Plate hole: nx x nx
    ("FEA_PlateHole", ["8", "--q8"], "plate_hole"),
    ("FEA_PlateHole", ["16", "--q8"], "plate_hole"),
    ("FEA_PlateHole", ["32", "--q8"], "plate_hole"),
    ("FEA_PlateHole", ["64", "--q8"], "plate_hole"),
    ("FEA_PlateHole", ["128", "--q8"], "plate_hole"),
    # L-bracket: Q4 only (no Q8 mesh generator)
    ("FEA_LBracket", ["8"], "lbracket"),
    ("FEA_LBracket", ["16"], "lbracket"),
    ("FEA_LBracket", ["32"], "lbracket"),
    ("FEA_LBracket", ["64"], "lbracket"),
    ("FEA_LBracket", ["128"], "lbracket"),
    # Thermal cylinder: Q4 only
    ("FEA_ThermalCylinder", ["8"], "thermal_cylinder"),
    ("FEA_ThermalCylinder", ["16"], "thermal_cylinder"),
    ("FEA_ThermalCylinder", ["32"], "thermal_cylinder"),
    ("FEA_ThermalCylinder", ["64"], "thermal_cylinder"),
    ("FEA_ThermalCylinder", ["128"], "thermal_cylinder"),
]


def run_case(build_dir, exe, args, label=""):
    exe_path = os.path.join(build_dir, exe)
    if not os.path.exists(exe_path):
        print(f'SKIP: {exe} not found (build first)')
        return False

    desc = f'{exe} {" ".join(args)}' if args else exe
    print(f'\n{"="*60}')
    print(f'Running {desc}{(" [" + label + "]") if label else ""}')
    print(f'{"="*60}')

    cmd = [exe_path] + args
    result = subprocess.run(cmd, capture_output=False, text=True)

    if result.returncode != 0:
        print(f'  WARNING: {exe} exited with code {result.returncode}')
        return False
    return True


def main():
    build_dir = os.path.join(os.path.dirname(__file__), '..', 'build')
    mode = sys.argv[1] if len(sys.argv) > 1 else "primary"

    if mode == "primary":
        print("=== Running primary case meshes ===")
        for exe, args, outdir in PRIMARY_CASES:
            run_case(build_dir, exe, args, "primary")

    elif mode == "convergence":
        print("=== Running convergence study meshes ===")
        for exe, args, outdir in CONVERGENCE_CASES:
            run_case(build_dir, exe, args, "convergence")

    elif mode == "all":
        print("=== Running all cases ===")
        for exe, args, outdir in PRIMARY_CASES:
            run_case(build_dir, exe, args, "primary")
        for exe, args, outdir in CONVERGENCE_CASES:
            run_case(build_dir, exe, args, "convergence")

    else:
        print(f"Usage: {sys.argv[0]} [primary|convergence|all]")
        return 1

    print(f'\n{"="*60}')
    print('All cases complete.')
    print(f'{"="*60}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
