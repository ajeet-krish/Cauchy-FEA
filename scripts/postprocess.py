#!/usr/bin/env python3
"""
FEA-2D Postprocessor: JSON -> matplotlib contour plots
Generates displacement, stress, and convergence plots from simulation output.

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

def load_mesh(outdir):
    mesh_file = os.path.join(outdir, 'mesh.json')
    if os.path.exists(mesh_file):
        with open(mesh_file) as f:
            return json.load(f)
    return None


def plot_displacement_contour(outdir, meta):
    data = load_displacement(outdir)
    nodes = data['nodes']

    x = np.array([n['x'] for n in nodes])
    y = np.array([n['y'] for n in nodes])
    ux = np.array([n['ux'] for n in nodes])
    uy = np.array([n['uy'] for n in nodes])
    disp = np.sqrt(ux**2 + uy**2)

    fig, axes = plt.subplots(1, 3, figsize=(15, 4))

    triang = Triangulation(x, y)

    # Displacement magnitude
    ax = axes[0]
    ax.tricontourf(triang, disp, levels=20, cmap='turbo')
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
    stress_file = os.path.join(outdir, 'stress.json')
    if not os.path.exists(stress_file):
        print(f'  No stress.json found in {outdir}, skipping stress contour')
        return
    
    stress_data = load_stress(outdir)
    elements = stress_data['elements']
    mesh = load_mesh(outdir)

    von_mises = np.array([e['von_mises'] for e in elements])
    sigma_xx = np.array([e['sigma_xx'] for e in elements])
    sigma_yy = np.array([e['sigma_yy'] for e in elements])

    # Compute element centroids for proper scatter plotting
    if mesh:
        nodes = mesh['nodes']
        quads = mesh['elements']
        cx = np.array([(nodes[e['n0']]['x'] + nodes[e['n1']]['x'] +
                        nodes[e['n2']]['x'] + nodes[e['n3']]['x']) / 4.0 for e in quads])
        cy = np.array([(nodes[e['n0']]['y'] + nodes[e['n1']]['y'] +
                        nodes[e['n2']]['y'] + nodes[e['n3']]['y']) / 4.0 for e in quads])
    else:
        disp_data = load_displacement(outdir)
        nodes = disp_data['nodes']
        cx = np.array([n['x'] for n in nodes])[:len(von_mises)]
        cy = np.array([n['y'] for n in nodes])[:len(von_mises)]

    fig, axes = plt.subplots(1, 2, figsize=(12, 4))

    # Von Mises stress
    ax = axes[0]
    sc = ax.scatter(cx, cy, c=von_mises, cmap='viridis', s=1, vmin=0)
    ax.set_title('Von Mises Stress (Pa)')
    ax.set_aspect('equal')
    plt.colorbar(sc, ax=ax, shrink=0.8)

    # Sigma XX
    ax = axes[1]
    sc = ax.scatter(cx, cy, c=sigma_xx, cmap='RdBu_r', s=1)
    ax.set_title('Sigma XX (Pa)')
    ax.set_aspect('equal')
    plt.colorbar(sc, ax=ax, shrink=0.8)

    fig.suptitle(f'Stress Field (max VM: {meta["max_von_mises"]:.2e} Pa)')
    plt.tight_layout()
    plt.savefig(os.path.join(outdir, 'stress_contour.png'), dpi=150, bbox_inches='tight')
    plt.close()
    print(f'  Saved stress_contour.png')


def plot_deformed_mesh(outdir, meta, scale=None):
    disp_data = load_displacement(outdir)
    mesh = load_mesh(outdir)

    if not mesh:
        print(f'  No mesh.json found, skipping deformed mesh plot')
        return

    nodes_orig = mesh['nodes']
    elements = mesh['elements']
    nodes_disp = disp_data['nodes']

    x_orig = np.array([n['x'] for n in nodes_orig])
    y_orig = np.array([n['y'] for n in nodes_orig])
    ux = np.array([n['ux'] for n in nodes_disp])
    uy = np.array([n['uy'] for n in nodes_disp])

    # Auto-scale deformation for visualization
    if scale is None:
        max_disp = np.max(np.sqrt(ux**2 + uy**2))
        if max_disp > 0:
            x_range = np.max(x_orig) - np.min(x_orig)
            y_range = np.max(y_orig) - np.min(y_orig)
            scale = 0.1 * max(x_range, y_range) / max_disp

    x_def = x_orig + ux * scale
    y_def = y_orig + uy * scale

    fig, ax = plt.subplots(figsize=(10, 6))

    # Draw undeformed mesh (wireframe)
    for elem in elements:
        n = [elem['n0'], elem['n1'], elem['n2'], elem['n3']]
        xs = list(x_orig[n]) + [x_orig[n[0]]]
        ys = list(y_orig[n]) + [y_orig[n[0]]]
        ax.plot(xs, ys, 'b-', linewidth=0.3, alpha=0.3)

    # Draw deformed mesh outline
    for elem in elements:
        n = [elem['n0'], elem['n1'], elem['n2'], elem['n3']]
        xs = list(x_def[n]) + [x_def[n[0]]]
        ys = list(y_def[n]) + [y_def[n[0]]]
        ax.plot(xs, ys, 'k-', linewidth=0.3)

    # Scatter colored by displacement magnitude
    disp_mag = np.sqrt(ux**2 + uy**2)
    sc = ax.scatter(x_def, y_def, c=disp_mag, cmap='turbo', s=2, zorder=5)
    plt.colorbar(sc, ax=ax, shrink=0.8, label='|u| (m)')

    ax.set_title(f'Deformed Mesh (scale: {scale:.0f}x)')
    ax.set_aspect('equal')
    ax.set_xlabel('x (m)')
    ax.set_ylabel('y (m)')

    plt.tight_layout()
    plt.savefig(os.path.join(outdir, 'deformed_mesh.png'), dpi=150, bbox_inches='tight')
    plt.close()
    print(f'  Saved deformed_mesh.png')


def plot_convergence(outdir):
    conv_file = os.path.join(outdir, 'convergence.json')
    if not os.path.exists(conv_file):
        print(f'  No convergence.json found in {outdir}')
        return

    with open(conv_file) as f:
        data = json.load(f)

    h = [s['h'] for s in data['samples']]
    values = [s['value'] for s in data['samples']]

    fig, ax = plt.subplots(figsize=(8, 6))
    ax.loglog(h, values, 'bo-', label='FEA', linewidth=2, markersize=8)

    if data.get('analytical'):
        ax.axhline(data['analytical'], color='r', linestyle='--',
                   label=f'Analytical ({data["analytical"]:.4e})', linewidth=1.5)

    gci = data.get('gci', {})
    if gci.get('extrapolated_value'):
        ax.axhline(gci['extrapolated_value'], color='green', linestyle=':',
                   label=f'Richardson ({gci["extrapolated_value"]:.4e})', linewidth=1.5)

    ax.set_xlabel('Element size h', fontsize=12)
    ax.set_ylabel(data.get('quantity', 'Value'), fontsize=12)
    title = f'{data["case"]} Mesh Convergence'
    if gci.get('observed_order'):
        title += f' (p={gci["observed_order"]:.2f})'
    ax.set_title(title, fontsize=14)
    ax.legend(fontsize=10)
    ax.grid(True, which='both', alpha=0.3)

    plt.tight_layout()
    plt.savefig(os.path.join(outdir, 'convergence.png'), dpi=150, bbox_inches='tight')
    plt.close()
    print(f'  Saved convergence.png')


def print_summary(outdir):
    meta = load_meta(outdir)
    print(f'\n  Nodes: {meta["num_nodes"]}, Elements: {meta["num_elements"]}, DOFs: {meta["num_dofs"]}')
    print(f'  Material: E={meta["material"]["E"]:.2e} Pa, nu={meta["material"]["nu"]:.2f}, t={meta["material"]["t"]:.4f} m')
    print(f'  Max displacement: {meta["max_displacement"]:.6e} m')
    print(f'  Max von Mises: {meta["max_von_mises"]:.6e} Pa')
    print(f'  Solve time: {meta["solve_time_ms"]:.1f} ms')
    if meta.get("cg_iterations", 0) > 0:
        print(f'  CG iterations: {meta["cg_iterations"]}')


def plot_stress_contour_thumbnail(outdir, meta, figsize=(4, 3)):
    """Generate a small thumbnail for the landing page."""
    stress_file = os.path.join(outdir, 'stress.json')
    if not os.path.exists(stress_file):
        print(f'  No stress.json found in {outdir}, skipping thumbnail')
        return
    
    stress_data = load_stress(outdir)
    elements = stress_data['elements']
    mesh = load_mesh(outdir)

    von_mises = np.array([e['von_mises'] for e in elements])

    if mesh:
        nodes = mesh['nodes']
        quads = mesh['elements']
        cx = np.array([(nodes[e['n0']]['x'] + nodes[e['n1']]['x'] +
                        nodes[e['n2']]['x'] + nodes[e['n3']]['x']) / 4.0 for e in quads])
        cy = np.array([(nodes[e['n0']]['y'] + nodes[e['n1']]['y'] +
                        nodes[e['n2']]['y'] + nodes[e['n3']]['y']) / 4.0 for e in quads])
    else:
        disp_data = load_displacement(outdir)
        nodes = disp_data['nodes']
        cx = np.array([n['x'] for n in nodes])[:len(von_mises)]
        cy = np.array([n['y'] for n in nodes])[:len(von_mises)]

    fig, ax = plt.subplots(figsize=figsize)
    sc = ax.scatter(cx, cy, c=von_mises, cmap='viridis', s=1, vmin=0)
    ax.set_aspect('equal')
    ax.axis('off')
    plt.colorbar(sc, ax=ax, shrink=0.8)

    plt.tight_layout()
    plt.savefig(os.path.join(outdir, 'thumbnail_stress.png'), dpi=100, bbox_inches='tight')
    plt.close()
    print(f'  Saved thumbnail_stress.png')


def plot_all_cases(outdir_base):
    """Generate PNGs for all 6 cases."""
    cases = ['cantilever_32', 'cook_32', 'lbracket', 'michell', 'patch', 'plate_hole']
    for case in cases:
        outdir = os.path.join(outdir_base, case)
        if os.path.exists(outdir) and os.path.exists(os.path.join(outdir, 'meta.json')):
            print(f'\nProcessing {case}:')
            meta = load_meta(outdir)
            plot_displacement_contour(outdir, meta)
            plot_stress_contour(outdir, meta)
            plot_deformed_mesh(outdir, meta)
        else:
            print(f'\nSkipping {case}: output not found')


def generate_thumbnails(outdir_base):
    """Generate small thumbnails for landing page."""
    cases = ['cantilever_32', 'cook_32', 'lbracket', 'michell', 'patch', 'plate_hole']
    for case in cases:
        outdir = os.path.join(outdir_base, case)
        if os.path.exists(outdir) and os.path.exists(os.path.join(outdir, 'meta.json')):
            print(f'\nGenerating thumbnail for {case}:')
            meta = load_meta(outdir)
            plot_stress_contour_thumbnail(outdir, meta)
        else:
            print(f'\nSkipping {case}: output not found')


def main():
    parser = argparse.ArgumentParser(description='FEA-2D Postprocessor')
    parser.add_argument('outdir', help='Output directory (or base directory for --all-cases)')
    parser.add_argument('--all', action='store_true', help='Generate all plots for a single case')
    parser.add_argument('--all-cases', action='store_true', help='Generate PNGs for all 6 cases')
    parser.add_argument('--thumbnails', action='store_true', help='Generate small thumbnails for landing page')
    parser.add_argument('--displacement', action='store_true', help='Displacement contour only')
    parser.add_argument('--stress', action='store_true', help='Stress contour only')
    parser.add_argument('--deformed', action='store_true', help='Deformed mesh plot')
    parser.add_argument('--convergence', action='store_true', help='Convergence plot')
    args = parser.parse_args()

    if args.all_cases:
        plot_all_cases(args.outdir)
        return

    if args.thumbnails:
        generate_thumbnails(args.outdir)
        return

    has_meta = os.path.exists(os.path.join(args.outdir, 'meta.json'))

    if has_meta:
        print_summary(args.outdir)

    if args.all or args.convergence:
        plot_convergence(args.outdir)
    if (args.all or args.displacement) and has_meta:
        plot_displacement_contour(args.outdir, load_meta(args.outdir))
    if (args.all or args.stress) and has_meta:
        plot_stress_contour(args.outdir, load_meta(args.outdir))
    if (args.all or args.deformed) and has_meta:
        plot_deformed_mesh(args.outdir, load_meta(args.outdir))

    if not (args.all or args.displacement or args.stress or args.deformed or args.convergence):
        print('\n  Use --all, --all-cases, --thumbnails, --displacement, --stress, --deformed, or --convergence to generate plots')


if __name__ == '__main__':
    main()
