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


def nodal_average(element_values, mesh):
    """Convert element-centered values to nodal values via averaging."""
    num_nodes = mesh['num_nodes']
    elements = mesh['elements']
    nodal_sum = np.zeros(num_nodes)
    nodal_count = np.zeros(num_nodes)
    
    for i, elem in enumerate(elements):
        val = element_values[i]
        for node_idx in [elem['n0'], elem['n1'], elem['n2'], elem['n3']]:
            nodal_sum[node_idx] += val
            nodal_count[node_idx] += 1
    
    # Avoid division by zero
    nodal_count[nodal_count == 0] = 1
    return nodal_sum / nodal_count


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
    ax.tricontourf(triang, disp, levels=20, cmap='hot')
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
    sigma_1 = np.array([e['sigma_1'] for e in elements])
    sigma_2 = np.array([e['sigma_2'] for e in elements])
    sigma_xy = np.array([e['sigma_xy'] for e in elements])

    if not mesh:
        print(f'  No mesh.json found, skipping stress contour')
        return

    nodes = mesh['nodes']
    x = np.array([n['x'] for n in nodes])
    y = np.array([n['y'] for n in nodes])
    
    # Nodal averaging for smooth contours
    vm_nodal = nodal_average(von_mises, mesh)
    s1_nodal = nodal_average(sigma_1, mesh)
    s2_nodal = nodal_average(sigma_2, mesh)
    sxy_nodal = nodal_average(sigma_xy, mesh)

    triang = Triangulation(x, y)

    fig, axes = plt.subplots(2, 2, figsize=(14, 10))

    # Von Mises stress
    ax = axes[0, 0]
    cf = ax.tricontourf(triang, vm_nodal, levels=20, cmap='hot')
    ax.set_title(f'Von Mises Stress (max: {meta["max_von_mises"]:.2e} Pa)')
    ax.set_aspect('equal')
    plt.colorbar(cf, ax=ax, shrink=0.8)

    # Sigma 1 (max principal)
    ax = axes[0, 1]
    cf = ax.tricontourf(triang, s1_nodal, levels=20, cmap='hot')
    ax.set_title('Sigma 1 (Max Principal)')
    ax.set_aspect('equal')
    plt.colorbar(cf, ax=ax, shrink=0.8)

    # Sigma 2 (min principal)
    ax = axes[1, 0]
    cf = ax.tricontourf(triang, s2_nodal, levels=20, cmap='hot')
    ax.set_title('Sigma 2 (Min Principal)')
    ax.set_aspect('equal')
    plt.colorbar(cf, ax=ax, shrink=0.8)

    # Sigma XY (shear)
    ax = axes[1, 1]
    cf = ax.tricontourf(triang, sxy_nodal, levels=20, cmap='RdBu_r')
    ax.set_title('Sigma XY (Shear)')
    ax.set_aspect('equal')
    plt.colorbar(cf, ax=ax, shrink=0.8)

    fig.suptitle(f'Stress Field ({meta["num_nodes"]} nodes, {meta["num_elements"]} elements)', fontsize=14)
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

    # Draw undeformed mesh (gray dashed)
    for elem in elements:
        n = [elem['n0'], elem['n1'], elem['n2'], elem['n3']]
        xs = list(x_orig[n]) + [x_orig[n[0]]]
        ys = list(y_orig[n]) + [y_orig[n[0]]]
        ax.plot(xs, ys, color='#aaaaaa', linestyle='--', linewidth=0.5, alpha=0.6)

    # Draw deformed mesh (bold cyan)
    for elem in elements:
        n = [elem['n0'], elem['n1'], elem['n2'], elem['n3']]
        xs = list(x_def[n]) + [x_def[n[0]]]
        ys = list(y_def[n]) + [y_def[n[0]]]
        ax.plot(xs, ys, color='#00d4ff', linewidth=0.8)

    # Scatter colored by displacement magnitude (hot colormap)
    disp_mag = np.sqrt(ux**2 + uy**2)
    sc = ax.scatter(x_def, y_def, c=disp_mag, cmap='hot', s=3, zorder=5, edgecolors='none')
    cb = plt.colorbar(sc, ax=ax, shrink=0.8, label='|u| (m)')

    # Add displacement vectors at sampled nodes (10% of nodes)
    num_nodes = len(x_orig)
    step = max(1, num_nodes // 20)
    indices = np.arange(0, num_nodes, step)
    
    # Scale arrows for visibility (normalize to mesh extent)
    arrow_scale = 0.05 * max(x_range, y_range) / max_disp if max_disp > 0 else 1.0
    ax.quiver(x_orig[indices], y_orig[indices], 
              ux[indices] * scale, uy[indices] * scale,
              angles='xy', scale_units='xy', scale=1, 
              color='#333333', alpha=0.5, width=0.003, zorder=4)

    # Add max displacement annotation
    max_idx = np.argmax(disp_mag)
    max_x, max_y = x_def[max_idx], y_def[max_idx]
    max_val = disp_mag[max_idx]
    ax.annotate(f'Max |u| = {max_val:.4e} m',
                xy=(max_x, max_y), xytext=(10, 10),
                textcoords='offset points',
                fontsize=10, color='#333333',
                bbox=dict(boxstyle='round,pad=0.3', facecolor='white', alpha=0.8, edgecolor='#cccccc'),
                arrowprops=dict(arrowstyle='->', color='#333333'))

    ax.set_title(f'Deformed Mesh (scale: {scale:.0f}x)', fontsize=12)
    ax.set_aspect('equal')
    ax.set_xlabel('x (m)')
    ax.set_ylabel('y (m)')
    ax.grid(True, alpha=0.2)

    plt.tight_layout()
    plt.savefig(os.path.join(outdir, 'deformed_mesh.png'), dpi=150, bbox_inches='tight')
    plt.close()
    print(f'  Saved deformed_mesh.png')


def plot_principal_stress_arrows(outdir, meta):
    stress_file = os.path.join(outdir, 'stress.json')
    if not os.path.exists(stress_file):
        print(f'  No stress.json found in {outdir}, skipping principal stress arrows')
        return
    
    mesh = load_mesh(outdir)
    if not mesh:
        print(f'  No mesh.json found, skipping principal stress arrows')
        return

    stress_data = load_stress(outdir)
    elements = stress_data['elements']
    
    sigma_1 = np.array([e['sigma_1'] for e in elements])
    sigma_2 = np.array([e['sigma_2'] for e in elements])
    
    # Check if theta1/theta2 exist (principal stress angles)
    has_angles = 'theta1' in elements[0] and 'theta2' in elements[0]
    
    if has_angles:
        theta1 = np.array([e['theta1'] for e in elements])
        theta2 = np.array([e['theta2'] for e in elements])
    else:
        # Compute from sigma_xy and sigma_xx, sigma_yy
        sigma_xx = np.array([e['sigma_xx'] for e in elements])
        sigma_yy = np.array([e['sigma_yy'] for e in elements])
        sigma_xy = np.array([e['sigma_xy'] for e in elements])
        theta1 = 0.5 * np.arctan2(2 * sigma_xy, sigma_xx - sigma_yy)
        theta2 = theta1 + np.pi / 2

    # Compute element centroids
    nodes = mesh['nodes']
    quads = mesh['elements']
    cx = np.array([(nodes[e['n0']]['x'] + nodes[e['n1']]['x'] +
                    nodes[e['n2']]['x'] + nodes[e['n3']]['x']) / 4.0 for e in quads])
    cy = np.array([(nodes[e['n0']]['y'] + nodes[e['n1']]['y'] +
                    nodes[e['n2']]['y'] + nodes[e['n3']]['y']) / 4.0 for e in quads])

    fig, ax = plt.subplots(figsize=(10, 6))

    # Draw mesh wireframe for reference
    for elem in quads:
        n = [elem['n0'], elem['n1'], elem['n2'], elem['n3']]
        xs = list(np.array([nodes[i]['x'] for i in n])) + [nodes[n[0]]['x']]
        ys = list(np.array([nodes[i]['y'] for i in n])) + [nodes[n[0]]['y']]
        ax.plot(xs, ys, color='#dddddd', linewidth=0.3, alpha=0.5)

    # Normalize arrow lengths
    max_s1 = np.max(np.abs(sigma_1))
    max_s2 = np.max(np.abs(sigma_2))
    max_stress = max(max_s1, max_s2)
    if max_stress == 0:
        max_stress = 1.0

    # Compute element size for arrow scaling
    x_range = np.max(cx) - np.min(cx)
    y_range = np.max(cy) - np.min(cy)
    arrow_len = 0.03 * max(x_range, y_range)

    # Draw sigma_1 arrows (red = tension, blue = compression)
    for i in range(len(cx)):
        dx = arrow_len * (sigma_1[i] / max_stress) * np.cos(theta1[i])
        dy = arrow_len * (sigma_1[i] / max_stress) * np.sin(theta1[i])
        color = '#ff4757' if sigma_1[i] >= 0 else '#00d4ff'
        ax.arrow(cx[i], cy[i], dx, dy, 
                head_width=arrow_len * 0.15, head_length=arrow_len * 0.1,
                fc=color, ec=color, alpha=0.7, linewidth=0.5)

    # Draw sigma_2 arrows (smaller, different shade)
    for i in range(len(cx)):
        dx = arrow_len * 0.7 * (sigma_2[i] / max_stress) * np.cos(theta2[i])
        dy = arrow_len * 0.7 * (sigma_2[i] / max_stress) * np.sin(theta2[i])
        color = '#ff6b81' if sigma_2[i] >= 0 else '#70a1ff'
        ax.arrow(cx[i], cy[i], dx, dy, 
                head_width=arrow_len * 0.1, head_length=arrow_len * 0.07,
                fc=color, ec=color, alpha=0.5, linewidth=0.3)

    # Legend
    from matplotlib.lines import Line2D
    legend_elements = [
        Line2D([0], [0], color='#ff4757', linewidth=2, label='Sigma 1 (tension)'),
        Line2D([0], [0], color='#00d4ff', linewidth=2, label='Sigma 1 (compression)'),
        Line2D([0], [0], color='#ff6b81', linewidth=1.5, label='Sigma 2 (tension)'),
        Line2D([0], [0], color='#70a1ff', linewidth=1.5, label='Sigma 2 (compression)'),
    ]
    ax.legend(handles=legend_elements, loc='upper right', fontsize=9, framealpha=0.9)

    ax.set_title(f'Principal Stress Directions ({meta["num_elements"]} elements)', fontsize=12)
    ax.set_aspect('equal')
    ax.set_xlabel('x (m)')
    ax.set_ylabel('y (m)')
    ax.grid(True, alpha=0.2)

    plt.tight_layout()
    plt.savefig(os.path.join(outdir, 'principal_stress.png'), dpi=150, bbox_inches='tight')
    plt.close()
    print(f'  Saved principal_stress.png')


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
    sc = ax.scatter(cx, cy, c=von_mises, cmap='hot', s=1, vmin=0)
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
            plot_principal_stress_arrows(outdir, meta)
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
    parser.add_argument('--principal', action='store_true', help='Principal stress arrows')
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
    if (args.all or args.principal) and has_meta:
        plot_principal_stress_arrows(args.outdir, load_meta(args.outdir))

    if not (args.all or args.displacement or args.stress or args.deformed or args.principal or args.convergence):
        print('\n  Use --all, --all-cases, --thumbnails, --displacement, --stress, --deformed, --principal, or --convergence to generate plots')


if __name__ == '__main__':
    main()
