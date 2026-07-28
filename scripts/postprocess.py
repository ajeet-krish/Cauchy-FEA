#!/usr/bin/env python3
"""
FEA-2D Postprocessor: JSON -> matplotlib contour plots
Generates displacement and stress contour plots from simulation output.

Usage:
    python3 scripts/postprocess.py output/patch/
    python3 scripts/postprocess.py output/cantilever_32/ --all
"""

import json
import sys
import os
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.colors import Normalize
from matplotlib.tri import Triangulation
import argparse


def load_meta(outdir):
    with open(os.path.join(outdir, 'meta.json')) as f:
        return json.load(f)

def load_displacement(outdir):
    with open(os.path.join(outdir, 'displacement.json')) as f:
        return json.load(f)

def load_stress(outdir):
    with open(os.path.join(outdir, 'stress.json')) as f:
        return json.load(f)


def plot_displacement_contour(outdir, meta):
    data = load_displacement(outdir)
    nodes = data['nodes']

    x = np.array([n['x'] for n in nodes])
    y = np.array([n['y'] for n in nodes])
    ux = np.array([n['ux'] for n in nodes])
    uy = np.array([n['uy'] for n in nodes])
    disp = np.sqrt(ux**2 + uy**2)

    fig, axes = plt.subplots(1, 3, figsize=(15, 4))

    # Displacement magnitude
    ax = axes[0]
    triang = Triangulation(x, y)
    ax.tricontourf(triang, disp, levels=20, cmap='jet')
    ax.set_title('|u| (m)')
    ax.set_aspect('equal')
    plt.colorbar(ax.collections[0], ax=ax, shrink=0.8)

    # Ux
    ax = axes[1]
    ax.tricontourf(triang, ux, levels=20, cmap='RdBu_r')
    ax.set_title('ux (m)')
    ax.set_aspect('equal')
    plt.colorbar(ax.collections[0], ax=ax, shrink=0.8)

    # Uy
    ax = axes[2]
    ax.tricontourf(triang, uy, levels=20, cmap='RdBu_r')
    ax.set_title('uy (m)')
    ax.set_aspect('equal')
    plt.colorbar(ax.collections[0], ax=ax, shrink=0.8)

    fig.suptitle(f'Displacement Field ({meta["num_nodes"]} nodes, {meta["num_elements"]} elements)')
    plt.tight_layout()
    plt.savefig(os.path.join(outdir, 'displacement_contour.png'), dpi=150, bbox_inches='tight')
    plt.close()
    print(f'  Saved displacement_contour.png')


def plot_stress_contour(outdir, meta):
    data = load_stress(outdir)
    elements = data['elements']

    von_mises = np.array([e['von_mises'] for e in elements])
    sigma_xx = np.array([e['sigma_xx'] for e in elements])

    # For element-centered data, use element centroids
    disp_data = load_displacement(outdir)
    nodes = disp_data['nodes']
    x = np.array([n['x'] for n in nodes])
    y = np.array([n['y'] for n in nodes])

    fig, axes = plt.subplots(1, 2, figsize=(10, 4))

    # Von Mises stress
    ax = axes[0]
    sc = ax.scatter(x[::1], y[::1], c=von_mises[:len(x)] if len(von_mises) >= len(x)
                    else np.resize(von_mises, len(x)),
                    cmap='jet', s=1, vmin=0)
    ax.set_title('Von Mises Stress (Pa)')
    ax.set_aspect('equal')
    plt.colorbar(sc, ax=ax, shrink=0.8)

    # Sigma XX
    ax = axes[1]
    sc = ax.scatter(x[:len(sigma_xx)], y[:len(sigma_xx)], c=sigma_xx,
                    cmap='RdBu_r', s=1)
    ax.set_title('Sigma XX (Pa)')
    ax.set_aspect('equal')
    plt.colorbar(sc, ax=ax, shrink=0.8)

    fig.suptitle(f'Stress Field (max VM: {meta["max_von_mises"]:.2e} Pa)')
    plt.tight_layout()
    plt.savefig(os.path.join(outdir, 'stress_contour.png'), dpi=150, bbox_inches='tight')
    plt.close()
    print(f'  Saved stress_contour.png')


def print_summary(outdir):
    meta = load_meta(outdir)
    print(f'\n  Nodes: {meta["num_nodes"]}, Elements: {meta["num_elements"]}, DOFs: {meta["num_dofs"]}')
    print(f'  Material: E={meta["material"]["E"]:.2e} Pa, nu={meta["material"]["nu"]:.2f}, t={meta["material"]["t"]:.4f} m')
    print(f'  Max displacement: {meta["max_displacement"]:.6e} m')
    print(f'  Max von Mises: {meta["max_von_mises"]:.6e} Pa')
    print(f'  Solve time: {meta["solve_time_ms"]:.1f} ms')
    if meta.get("cg_iterations", 0) > 0:
        print(f'  CG iterations: {meta["cg_iterations"]}')


def main():
    parser = argparse.ArgumentParser(description='FEA-2D Postprocessor')
    parser.add_argument('outdir', help='Output directory')
    parser.add_argument('--all', action='store_true', help='Generate all plots')
    parser.add_argument('--displacement', action='store_true', help='Displacement contour only')
    parser.add_argument('--stress', action='store_true', help='Stress contour only')
    args = parser.parse_args()

    if not os.path.exists(os.path.join(args.outdir, 'meta.json')):
        print(f'Error: No meta.json found in {args.outdir}')
        sys.exit(1)

    print_summary(args.outdir)

    if args.all or args.displacement:
        plot_displacement_contour(args.outdir, load_meta(args.outdir))
    if args.all or args.stress:
        plot_stress_contour(args.outdir, load_meta(args.outdir))

    if not (args.all or args.displacement or args.stress):
        print('\n  Use --all, --displacement, or --stress to generate plots')


if __name__ == '__main__':
    main()
