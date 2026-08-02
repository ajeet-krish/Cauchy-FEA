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


def extract_case_name(outdir):
    """Extract case name from output directory path.
    
    Supports both old structure (output/cantilever_32/) and 
    new structure (output/cantilever/simulations/32/).
    """
    parts = outdir.rstrip('/').split(os.sep)
    # Find 'simulations' in path -> case name is two levels up
    if 'simulations' in parts:
        sim_idx = parts.index('simulations')
        if sim_idx >= 2:
            return parts[sim_idx - 1]
    # Fallback: use basename and strip mesh size suffixes
    name = os.path.basename(outdir)
    for suffix in ['_32', '_64', '_16', '_8', '_4', '_q8']:
        name = name.replace(suffix, '')
    return name


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


def load_triangulation(outdir):
    """Load mesh and create a matplotlib Triangulation using actual element connectivity.
    
    Splits quad elements into 2 triangles each, uses T3 elements directly.
    Returns (triang, x, y) or (None, None, None) if mesh.json not found or no elements.
    """
    mesh = load_mesh(outdir)
    if not mesh:
        return None, None, None
    
    nodes = mesh['nodes']
    
    # Handle Q4, Q8, and T3 element formats
    quad_elems = mesh.get('quad_elements', [])
    quad8_elems = mesh.get('quad8_elements', [])
    tri_elems = mesh.get('tri_elements', [])
    
    # Skip if no elements (e.g., bar elements only like michell)
    if not quad_elems and not quad8_elems and not tri_elems:
        return None, None, None
    
    x = np.array([n['x'] for n in nodes])
    y = np.array([n['y'] for n in nodes])
    
    triangles = []
    
    # Split each Q4 into 2 triangles: (n0,n1,n2) and (n0,n2,n3)
    for elem in quad_elems:
        n0, n1, n2, n3 = elem['n0'], elem['n1'], elem['n2'], elem['n3']
        triangles.append([n0, n1, n2])
        triangles.append([n0, n2, n3])
    
    # Split each Q8 into 2 triangles using corner nodes only
    for elem in quad8_elems:
        n0, n1, n2, n3 = elem['n0'], elem['n1'], elem['n2'], elem['n3']
        triangles.append([n0, n1, n2])
        triangles.append([n0, n2, n3])
    
    # Add T3 elements directly
    for elem in tri_elems:
        n0, n1, n2 = elem['n0'], elem['n1'], elem['n2']
        triangles.append([n0, n1, n2])
    
    triangles = np.array(triangles)
    if triangles.shape[0] == 0:
        return None, None, None
    
    triang = Triangulation(x, y, triangles=triangles)
    return triang, x, y


def nodal_average(element_values, mesh):
    """Convert element-centered values to nodal values via averaging."""
    num_nodes = mesh['num_nodes']
    
    # Handle Q4, Q8, and T3 element formats
    quad_elems = mesh.get('quad_elements', [])
    quad8_elems = mesh.get('quad8_elements', [])
    tri_elems = mesh.get('tri_elements', [])
    
    nodal_sum = np.zeros(num_nodes)
    nodal_count = np.zeros(num_nodes)
    
    # Average Q4 element values
    for i, elem in enumerate(quad_elems):
        val = element_values[i]
        for node_idx in [elem['n0'], elem['n1'], elem['n2'], elem['n3']]:
            nodal_sum[node_idx] += val
            nodal_count[node_idx] += 1
    
    # Average Q8 element values (use corner nodes only for averaging)
    offset = len(quad_elems)
    for i, elem in enumerate(quad8_elems):
        val = element_values[offset + i]
        for node_idx in [elem['n0'], elem['n1'], elem['n2'], elem['n3']]:
            nodal_sum[node_idx] += val
            nodal_count[node_idx] += 1
    
    # Average T3 element values
    offset = len(quad_elems) + len(quad8_elems)
    for i, elem in enumerate(tri_elems):
        val = element_values[offset + i]
        for node_idx in [elem['n0'], elem['n1'], elem['n2']]:
            nodal_sum[node_idx] += val
            nodal_count[node_idx] += 1
    
    # Avoid division by zero
    nodal_count[nodal_count == 0] = 1
    return nodal_sum / nodal_count


def plot_displacement_contour(outdir, meta, image_dir):
    # Extract case name from outdir (e.g., 'output/cantilever_32' -> 'cantilever')
    case_name = extract_case_name(outdir)
    data = load_displacement(outdir)
    nodes = data['nodes']

    x = np.array([n['x'] for n in nodes])
    y = np.array([n['y'] for n in nodes])
    ux = np.array([n['ux'] for n in nodes])
    uy = np.array([n['uy'] for n in nodes])
    disp = np.sqrt(ux**2 + uy**2)

    fig, axes = plt.subplots(1, 3, figsize=(15, 4))

    # Use mesh-aware triangulation (respects hole/cutout geometry)
    triang, _, _ = load_triangulation(outdir)
    if triang is None:
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
    plt.savefig(os.path.join(image_dir, f'{case_name}_displacement_contour.png'), dpi=150, bbox_inches='tight')
    plt.close()
    print(f'  Saved displacement_contour.png')


def plot_stress_contour(outdir, meta, image_dir):
    # Extract case name from outdir
    case_name = extract_case_name(outdir)
    stress_file = os.path.join(outdir, 'stress.json')
    if not os.path.exists(stress_file):
        print(f'  No stress.json found in {outdir}, skipping stress contour')
        return
    
    stress_data = load_stress(outdir)
    elements = stress_data['elements']
    
    if not elements:
        print(f'  No stress elements found, skipping stress contour')
        return
    
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

    # Use mesh-aware triangulation (respects hole/cutout geometry)
    triang, _, _ = load_triangulation(outdir)
    if triang is None:
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
    plt.savefig(os.path.join(image_dir, f'{case_name}_stress_contour.png'), dpi=150, bbox_inches='tight')
    plt.close()
    print(f'  Saved stress_contour.png')


def plot_deformed_mesh(outdir, meta, image_dir, scale=None):
    # Extract case name from outdir
    case_name = extract_case_name(outdir)
    disp_data = load_displacement(outdir)
    mesh = load_mesh(outdir)

    if not mesh:
        print(f'  No mesh.json found, skipping deformed mesh plot')
        return

    nodes_orig = mesh['nodes']
    quad_elems = mesh.get('quad_elements', [])
    quad8_elems = mesh.get('quad8_elements', [])
    tri_elems = mesh.get('tri_elements', [])
    nodes_disp = disp_data['nodes']

    x_orig = np.array([n['x'] for n in nodes_orig])
    y_orig = np.array([n['y'] for n in nodes_orig])
    ux = np.array([n['ux'] for n in nodes_disp])
    uy = np.array([n['uy'] for n in nodes_disp])
    disp_mag = np.sqrt(ux**2 + uy**2)

    # Auto-scale deformation for visualization
    if scale is None:
        max_disp = np.max(disp_mag)
        if max_disp > 0:
            x_range = np.max(x_orig) - np.min(x_orig)
            y_range = np.max(y_orig) - np.min(y_orig)
            scale = 0.1 * max(x_range, y_range) / max_disp

    x_def = x_orig + ux * scale
    y_def = y_orig + uy * scale

    # Setup colormap for mesh edges
    vmin = 0
    vmax = np.max(disp_mag)
    cmap = plt.cm.turbo
    norm = plt.Normalize(vmin=vmin, vmax=vmax)

    fig, ax = plt.subplots(figsize=(10, 6))

    # Draw undeformed mesh (light gray dashed) - Q4 elements
    for elem in quad_elems:
        n = [elem['n0'], elem['n1'], elem['n2'], elem['n3']]
        xs = list(x_orig[n]) + [x_orig[n[0]]]
        ys = list(y_orig[n]) + [y_orig[n[0]]]
        ax.plot(xs, ys, color='#cccccc', linestyle='--', linewidth=0.4, alpha=0.5, zorder=1)

    # Draw undeformed mesh - Q8 elements (use corner nodes only)
    for elem in quad8_elems:
        n = [elem['n0'], elem['n1'], elem['n2'], elem['n3']]
        xs = list(x_orig[n]) + [x_orig[n[0]]]
        ys = list(y_orig[n]) + [y_orig[n[0]]]
        ax.plot(xs, ys, color='#cccccc', linestyle='--', linewidth=0.4, alpha=0.5, zorder=1)

    # Draw undeformed mesh - T3 elements
    for elem in tri_elems:
        n = [elem['n0'], elem['n1'], elem['n2']]
        xs = list(x_orig[n]) + [x_orig[n[0]]]
        ys = list(y_orig[n]) + [y_orig[n[0]]]
        ax.plot(xs, ys, color='#cccccc', linestyle='--', linewidth=0.4, alpha=0.5, zorder=1)

    # Draw deformed mesh with colormap edges - Q4 elements
    for elem in quad_elems:
        n = [elem['n0'], elem['n1'], elem['n2'], elem['n3']]
        # Close the quad
        n_closed = n + [n[0]]
        for i in range(4):
            i0, i1 = n_closed[i], n_closed[i + 1]
            avg_disp = (disp_mag[i0] + disp_mag[i1]) / 2.0
            color = cmap(norm(avg_disp))
            ax.plot([x_def[i0], x_def[i1]], [y_def[i0], y_def[i1]],
                    color=color, linewidth=0.9, solid_capstyle='round', zorder=2)

    # Draw deformed mesh with colormap edges - Q8 elements (use corner nodes only)
    for elem in quad8_elems:
        n = [elem['n0'], elem['n1'], elem['n2'], elem['n3']]
        n_closed = n + [n[0]]
        for i in range(4):
            i0, i1 = n_closed[i], n_closed[i + 1]
            avg_disp = (disp_mag[i0] + disp_mag[i1]) / 2.0
            color = cmap(norm(avg_disp))
            ax.plot([x_def[i0], x_def[i1]], [y_def[i0], y_def[i1]],
                    color=color, linewidth=0.9, solid_capstyle='round', zorder=2)

    # Draw deformed mesh with colormap edges - T3 elements
    for elem in tri_elems:
        n = [elem['n0'], elem['n1'], elem['n2']]
        n_closed = n + [n[0]]
        for i in range(3):
            i0, i1 = n_closed[i], n_closed[i + 1]
            avg_disp = (disp_mag[i0] + disp_mag[i1]) / 2.0
            color = cmap(norm(avg_disp))
            ax.plot([x_def[i0], x_def[i1]], [y_def[i0], y_def[i1]],
                    color=color, linewidth=0.9, solid_capstyle='round', zorder=2)

    # Scatter colored by displacement magnitude for node dots
    sc = ax.scatter(x_def, y_def, c=disp_mag, cmap='turbo', s=2, zorder=5, 
                    edgecolors='none', vmin=vmin, vmax=vmax)
    cb = plt.colorbar(sc, ax=ax, shrink=0.8, label='|u| (m)', pad=0.02)

    # Add displacement vectors at sampled nodes (bold black arrows)
    num_nodes = len(x_orig)
    step = max(1, num_nodes // 25)
    indices = np.arange(0, num_nodes, step)
    
    ax.quiver(x_orig[indices], y_orig[indices], 
              ux[indices] * scale, uy[indices] * scale,
              angles='xy', scale_units='xy', scale=1, 
              color='#111111', alpha=0.85, width=0.004, zorder=6)

    # Add max displacement annotation (positioned to avoid overlap with colorbar)
    max_idx = np.argmax(disp_mag)
    max_x, max_y = x_def[max_idx], y_def[max_idx]
    max_val = disp_mag[max_idx]
    
    # Choose annotation position based on where the max point is
    x_mid = (np.min(x_def) + np.max(x_def)) / 2
    if max_x > x_mid:
        # Max is on right side, annotate to the left
        xytext = (-80, 30)
    else:
        # Max is on left side, annotate to the right
        xytext = (30, 30)
    
    ax.annotate(f'Max |u| = {max_val:.4e} m',
                xy=(max_x, max_y), xytext=xytext,
                textcoords='offset points',
                fontsize=9, color='#111111',
                bbox=dict(boxstyle='round,pad=0.4', facecolor='white', alpha=0.9, edgecolor='#666666'),
                arrowprops=dict(arrowstyle='->', color='#111111', lw=1.2),
                zorder=10)

    ax.set_title('Deformed Mesh', fontsize=12)
    ax.set_aspect('equal')
    ax.set_xlabel('x (m)')
    ax.set_ylabel('y (m)')
    ax.grid(True, alpha=0.15, linestyle='-', color='#dddddd')

    plt.tight_layout()
    plt.savefig(os.path.join(image_dir, f'{case_name}_deformed_mesh.png'), dpi=150, bbox_inches='tight')
    plt.close()
    print(f'  Saved deformed_mesh.png')


def plot_principal_stress_arrows(outdir, meta, image_dir):
    # Extract case name from outdir
    case_name = extract_case_name(outdir)
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
    
    if not elements:
        print(f'  No stress elements found, skipping principal stress arrows')
        return
    
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
    # Handle Q8 (quad8_elements) and Q4 (quad_elements) formats
    quads = mesh.get('quad_elements', [])
    quad8s = mesh.get('quad8_elements', [])
    tri_elems = mesh.get('tri_elements', [])
    
    # For Q8 elements, use corner nodes (n0-n3) for centroid
    cx_list = []
    cy_list = []
    for e in quads:
        cx_list.append((nodes[e['n0']]['x'] + nodes[e['n1']]['x'] +
                        nodes[e['n2']]['x'] + nodes[e['n3']]['x']) / 4.0)
        cy_list.append((nodes[e['n0']]['y'] + nodes[e['n1']]['y'] +
                        nodes[e['n2']]['y'] + nodes[e['n3']]['y']) / 4.0)
    for e in quad8s:
        cx_list.append((nodes[e['n0']]['x'] + nodes[e['n1']]['x'] +
                        nodes[e['n2']]['x'] + nodes[e['n3']]['x']) / 4.0)
        cy_list.append((nodes[e['n0']]['y'] + nodes[e['n1']]['y'] +
                        nodes[e['n2']]['y'] + nodes[e['n3']]['y']) / 4.0)
    cx = np.array(cx_list)
    cy = np.array(cy_list)

    fig, ax = plt.subplots(figsize=(10, 6))

    # Draw mesh wireframe for reference
    for elem in quads:
        n = [elem['n0'], elem['n1'], elem['n2'], elem['n3']]
        xs = list(np.array([nodes[i]['x'] for i in n])) + [nodes[n[0]]['x']]
        ys = list(np.array([nodes[i]['y'] for i in n])) + [nodes[n[0]]['y']]
        ax.plot(xs, ys, color='#dddddd', linewidth=0.3, alpha=0.5)
    for elem in quad8s:
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
    plt.savefig(os.path.join(image_dir, f'{case_name}_principal_stress.png'), dpi=150, bbox_inches='tight')
    plt.close()
    print(f'  Saved principal_stress.png')


def plot_convergence(outdir, image_dir):
    # Extract case name from outdir
    case_name = extract_case_name(outdir)
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
    plt.savefig(os.path.join(image_dir, f'{case_name}_convergence.png'), dpi=150, bbox_inches='tight')
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


def plot_stress_contour_thumbnail(outdir, meta, image_dir, figsize=(4, 3)):
    """Generate a small thumbnail for the landing page."""
    # Extract case name from outdir
    case_name = extract_case_name(outdir)
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
        # Handle new format (quad_elements + tri_elements) or legacy format (elements)
        quads = mesh.get('quad_elements', mesh.get('elements', []))
        tri_elems = mesh.get('tri_elements', [])
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
    plt.savefig(os.path.join(image_dir, f'{case_name}_thumbnail_stress.png'), dpi=100, bbox_inches='tight')
    plt.close()
    print(f'  Saved thumbnail_stress.png')


def plot_deformed_mesh_thumbnail(outdir, meta, image_dir, figsize=(4, 3)):
    """Generate a small deformed mesh thumbnail for the landing page."""
    # Extract case name from outdir
    case_name = extract_case_name(outdir)
    disp_data = load_displacement(outdir)
    mesh = load_mesh(outdir)

    if not mesh:
        print(f'  No mesh.json found, skipping deformed mesh thumbnail')
        return

    nodes_orig = mesh['nodes']
    quad_elems = mesh.get('quad_elements', mesh.get('elements', []))
    tri_elems = mesh.get('tri_elements', [])
    nodes_disp = disp_data['nodes']

    x_orig = np.array([n['x'] for n in nodes_orig])
    y_orig = np.array([n['y'] for n in nodes_orig])
    ux = np.array([n['ux'] for n in nodes_disp])
    uy = np.array([n['uy'] for n in nodes_disp])
    disp_mag = np.sqrt(ux**2 + uy**2)

    max_disp = np.max(disp_mag)
    if max_disp > 0:
        x_range = np.max(x_orig) - np.min(x_orig)
        y_range = np.max(y_orig) - np.min(y_orig)
        scale = 0.1 * max(x_range, y_range) / max_disp
    else:
        scale = 1.0

    x_def = x_orig + ux * scale
    y_def = y_orig + uy * scale

    vmin = 0
    vmax = np.max(disp_mag)
    cmap = plt.cm.turbo
    norm = plt.Normalize(vmin=vmin, vmax=vmax)

    fig, ax = plt.subplots(figsize=figsize)

    # Draw deformed mesh with colormap edges - Q4 elements
    for elem in quad_elems:
        n = [elem['n0'], elem['n1'], elem['n2'], elem['n3']]
        n_closed = n + [n[0]]
        for i in range(4):
            i0, i1 = n_closed[i], n_closed[i + 1]
            avg_disp = (disp_mag[i0] + disp_mag[i1]) / 2.0
            color = cmap(norm(avg_disp))
            ax.plot([x_def[i0], x_def[i1]], [y_def[i0], y_def[i1]],
                    color=color, linewidth=0.5, solid_capstyle='round')

    # Draw deformed mesh - T3 elements
    for elem in tri_elems:
        n = [elem['n0'], elem['n1'], elem['n2']]
        n_closed = n + [n[0]]
        for i in range(3):
            i0, i1 = n_closed[i], n_closed[i + 1]
            avg_disp = (disp_mag[i0] + disp_mag[i1]) / 2.0
            color = cmap(norm(avg_disp))
            ax.plot([x_def[i0], x_def[i1]], [y_def[i0], y_def[i1]],
                    color=color, linewidth=0.5, solid_capstyle='round')

    ax.set_aspect('equal')
    ax.axis('off')

    plt.tight_layout()
    plt.savefig(os.path.join(image_dir, f'{case_name}_thumbnail_deformed.png'), dpi=100, bbox_inches='tight')
    plt.close()
    print(f'  Saved thumbnail_deformed.png')


def plot_mesh_quality(outdir, meta, image_dir):
    """Mesh wireframe with boundary condition symbols.

    Single-panel figure showing:
    1. Element edges as black wireframe lines
    2. Yellow triangles at fully fixed nodes (ux=0 AND uy=0)
    3. Yellow circles at roller nodes (single-DOF constraint)
    4. Green/red arrows at force application points
    """
    # Extract case name from outdir
    case_name = extract_case_name(outdir)
    mesh = load_mesh(outdir)
    if not mesh:
        print(f'  No mesh.json found, skipping mesh quality')
        return

    nodes = mesh['nodes']
    quad_elems = mesh.get('quad_elements', [])
    quad8_elems = mesh.get('quad8_elements', [])
    tri_elems = mesh.get('tri_elements', [])
    dirichlet = mesh.get('dirichlet', [])
    neumann = mesh.get('neumann', [])

    if not quad_elems and not quad8_elems and not tri_elems:
        print(f'  No elements found, skipping mesh quality')
        return

    x = np.array([n['x'] for n in nodes])
    y = np.array([n['y'] for n in nodes])

    fig, ax = plt.subplots(figsize=(10, 8))

    # 1. Draw wireframe (element edges) - use corner nodes for Q8
    for elem in quad_elems:
        n = [elem['n0'], elem['n1'], elem['n2'], elem['n3'], elem['n0']]
        ax.plot([x[i] for i in n], [y[i] for i in n], 'k-', linewidth=0.5)
    for elem in quad8_elems:
        n = [elem['n0'], elem['n1'], elem['n2'], elem['n3'], elem['n0']]
        ax.plot([x[i] for i in n], [y[i] for i in n], 'k-', linewidth=0.5)
    for elem in tri_elems:
        n = [elem['n0'], elem['n1'], elem['n2'], elem['n0']]
        ax.plot([x[i] for i in n], [y[i] for i in n], 'k-', linewidth=0.5)

    # 2. Classify Dirichlet BCs
    ux_fixed = set()
    uy_fixed = set()
    for bc in dirichlet:
        if bc['dof'] == 0 and bc['value'] == 0.0:
            ux_fixed.add(bc['node'])
        elif bc['dof'] == 1 and bc['value'] == 0.0:
            uy_fixed.add(bc['node'])

    fixed_nodes = ux_fixed & uy_fixed
    roller_x_only = ux_fixed - uy_fixed
    roller_y_only = uy_fixed - ux_fixed

    # 3. Compute triangle size based on mesh dimensions
    x_range = x.max() - x.min()
    y_range = y.max() - y.min()
    tri_size = min(x_range, y_range) * 0.03

    # 4. Draw fixed support triangles (fully constrained)
    for node in fixed_nodes:
        cx, cy = x[node], y[node]
        triangle = np.array([
            [cx, cy + tri_size * 0.3],
            [cx - tri_size * 0.5, cy - tri_size * 0.7],
            [cx + tri_size * 0.5, cy - tri_size * 0.7],
            [cx, cy + tri_size * 0.3]
        ])
        ax.fill(triangle[:, 0], triangle[:, 1],
                facecolor='#FFD700', edgecolor='black', linewidth=0.8, zorder=5)

    # 5. Draw roller circles (single-DOF constraint)
    for node in roller_x_only | roller_y_only:
        cx, cy = x[node], y[node]
        circle = plt.Circle((cx, cy), tri_size * 0.35,
                            facecolor='#FFD700', edgecolor='black', linewidth=0.8, zorder=5)
        ax.add_patch(circle)

    # 6. Draw force arrows
    for bc in neumann:
        node = bc['node']
        dof = bc['dof']
        value = bc['value']
        if value == 0.0:
            continue
        cx, cy = x[node], y[node]
        arrow_len = tri_size * 3.0
        if dof == 0:
            dx = arrow_len if value > 0 else -arrow_len
            dy = 0.0
        else:
            dx = 0.0
            dy = arrow_len if value > 0 else -arrow_len
        color = '#228B22' if value > 0 else '#CC0000'
        ax.annotate('', xy=(cx + dx, cy + dy), xytext=(cx, cy),
                    arrowprops=dict(arrowstyle='->', color=color, lw=2.0), zorder=6)

    ax.set_aspect('equal')
    ax.set_xlabel('x (m)')
    ax.set_ylabel('y (m)')
    ax.set_title(f'Mesh: {len(nodes)} nodes, {len(quad_elems) + len(quad8_elems) + len(tri_elems)} elements')

    # Legend
    from matplotlib.patches import Patch
    from matplotlib.lines import Line2D
    legend_elements = [
        Patch(facecolor='#FFD700', edgecolor='black', label='Fixed support'),
        Line2D([0], [0], marker='o', color='w', markerfacecolor='#FFD700',
               markeredgecolor='black', markersize=8, label='Roller'),
        Line2D([0], [0], marker='>', color='w', markerfacecolor='#228B22',
               markeredgecolor='#228B22', markersize=8, label='Force (+)'),
        Line2D([0], [0], marker='>', color='w', markerfacecolor='#CC0000',
               markeredgecolor='#CC0000', markersize=8, label='Force (-)'),
    ]
    ax.legend(handles=legend_elements, loc='lower left', fontsize=8)

    plt.tight_layout()
    plt.savefig(os.path.join(image_dir, f'{case_name}_mesh_quality.png'), dpi=150, bbox_inches='tight')
    plt.close()
    print(f'  Saved {case_name}_mesh_quality.png')


def plot_all_cases(outdir_base, image_dir):
    """Generate PNGs for all cases."""
    cases = {
        'cantilever': '32',
        'cook': '32',
        'lbracket': '',
        'michell': '',
        'patch': '',
        'plate_hole': '',
        'thermal_cylinder': '',
    }
    for case_name, mesh_size in cases.items():
        # New structure: output/{case}/simulations/{mesh_size}/
        if mesh_size:
            outdir = os.path.join(outdir_base, case_name, 'simulations', mesh_size)
        else:
            outdir = os.path.join(outdir_base, case_name, 'simulations')
        
        # Per-case image directory: docs/assets/images/{case}/simulations/
        case_image_dir = os.path.join(image_dir, case_name, 'simulations')
        os.makedirs(case_image_dir, exist_ok=True)
        
        if os.path.exists(outdir) and os.path.exists(os.path.join(outdir, 'meta.json')):
            print(f'\nProcessing {case_name}:')
            meta = load_meta(outdir)
            plot_mesh_quality(outdir, meta, case_image_dir)
            plot_displacement_contour(outdir, meta, case_image_dir)
            plot_stress_contour(outdir, meta, case_image_dir)
            plot_deformed_mesh(outdir, meta, case_image_dir)
            plot_principal_stress_arrows(outdir, meta, case_image_dir)
        else:
            print(f'\nSkipping {case_name}: output not found at {outdir}')


def generate_thumbnails(outdir_base, image_dir):
    """Generate small thumbnails for landing page."""
    cases = {
        'cantilever': '32',
        'cook': '32',
        'lbracket': '',
        'michell': '',
        'patch': '',
        'plate_hole': '',
    }
    for case_name, mesh_size in cases.items():
        if mesh_size:
            outdir = os.path.join(outdir_base, case_name, 'simulations', mesh_size)
        else:
            outdir = os.path.join(outdir_base, case_name, 'simulations')
        
        # Thumbnails go to docs/assets/images/{case}/simulations/
        case_image_dir = os.path.join(image_dir, case_name, 'simulations')
        os.makedirs(case_image_dir, exist_ok=True)
        
        if os.path.exists(outdir) and os.path.exists(os.path.join(outdir, 'meta.json')):
            print(f'\nGenerating thumbnail for {case_name}:')
            meta = load_meta(outdir)
            plot_deformed_mesh_thumbnail(outdir, meta, case_image_dir)
        else:
            print(f'\nSkipping {case_name}: output not found at {outdir}')


def main():
    parser = argparse.ArgumentParser(description='FEA-2D Postprocessor')
    parser.add_argument('outdir', help='Output directory (or base directory for --all-cases)')
    parser.add_argument('--image-dir', default='docs/assets/images', help='Directory to save PNG plots (default: docs/assets/images)')
    parser.add_argument('--all', action='store_true', help='Generate all plots for a single case')
    parser.add_argument('--all-cases', action='store_true', help='Generate PNGs for all 6 cases')
    parser.add_argument('--thumbnails', action='store_true', help='Generate small thumbnails for landing page')
    parser.add_argument('--displacement', action='store_true', help='Displacement contour only')
    parser.add_argument('--stress', action='store_true', help='Stress contour only')
    parser.add_argument('--deformed', action='store_true', help='Deformed mesh plot')
    parser.add_argument('--principal', action='store_true', help='Principal stress arrows')
    parser.add_argument('--mesh-quality', action='store_true', help='Mesh quality analysis')
    parser.add_argument('--convergence', action='store_true', help='Convergence plot')
    args = parser.parse_args()

    if args.all_cases:
        plot_all_cases(args.outdir, args.image_dir)
        return

    if args.thumbnails:
        generate_thumbnails(args.outdir, args.image_dir)
        return

    has_meta = os.path.exists(os.path.join(args.outdir, 'meta.json'))

    if has_meta:
        print_summary(args.outdir)

    # Ensure image directory exists
    os.makedirs(args.image_dir, exist_ok=True)

    if args.all or args.convergence:
        plot_convergence(args.outdir, args.image_dir)
    if (args.all or args.displacement) and has_meta:
        plot_displacement_contour(args.outdir, load_meta(args.outdir), args.image_dir)
    if (args.all or args.stress) and has_meta:
        plot_stress_contour(args.outdir, load_meta(args.outdir), args.image_dir)
    if (args.all or args.deformed) and has_meta:
        plot_deformed_mesh(args.outdir, load_meta(args.outdir), args.image_dir)
    if (args.all or args.principal) and has_meta:
        plot_principal_stress_arrows(args.outdir, load_meta(args.outdir), args.image_dir)
    if (args.all or args.mesh_quality) and has_meta:
        plot_mesh_quality(args.outdir, load_meta(args.outdir), args.image_dir)

    if not (args.all or args.displacement or args.stress or args.deformed or args.principal or args.mesh_quality or args.convergence):
        print('\n  Use --all, --all-cases, --thumbnails, --displacement, --stress, --deformed, --principal, --mesh-quality, or --convergence to generate plots')


if __name__ == '__main__':
    main()
