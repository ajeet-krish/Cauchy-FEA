#!/usr/bin/env python3
"""Generate accuracy study plots: FEA vs PINN vs Error maps.

Creates side-by-side comparison plots for each case:
  - FEA displacement/stress field
  - PINN predicted field
  - Absolute error map

Usage:
    python -m pinn.evaluate --case cantilever
    python -m pinn.evaluate --all-cases
"""

import os
import sys
import json
import argparse
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.tri import Triangulation

import torch
from pinn.models.pinn import FEAParametricPINN, build_input_tensor

OUTPUT_DIR = os.path.join(os.path.dirname(__file__), '..', 'output')
RESULTS_DIR = os.path.join(os.path.dirname(__file__), 'results')


def load_fea_data(outdir):
    """Load FEA solver output (mesh, displacement, stress)."""
    with open(os.path.join(outdir, 'mesh.json')) as f:
        mesh = json.load(f)
    with open(os.path.join(outdir, 'displacement.json')) as f:
        disp = json.load(f)
    with open(os.path.join(outdir, 'stress.json')) as f:
        stress = json.load(f)

    nodes = mesh['nodes']
    x = np.array([n['x'] for n in nodes])
    y = np.array([n['y'] for n in nodes])
    ux = np.array([n['ux'] for n in disp['nodes']])
    uy = np.array([n['uy'] for n in disp['nodes']])

    quad_elems = mesh.get('quad_elements', [])
    connectivity = np.array([[e['n0'], e['n1'], e['n2'], e['n3']] for e in quad_elems])

    sigma_xx = np.array([e['sigma_xx'] for e in stress['elements']])
    sigma_yy = np.array([e['sigma_yy'] for e in stress['elements']])
    von_mises = np.array([e['von_mises'] for e in stress['elements']])

    return {
        'x': x, 'y': y,
        'ux': ux, 'uy': uy,
        'connectivity': connectivity,
        'sigma_xx': sigma_xx, 'sigma_yy': sigma_yy,
        'von_mises': von_mises,
        'num_nodes': len(nodes),
        'num_elements': len(quad_elems),
    }


def load_pinn_model(case_name):
    """Load trained PINN model."""
    model_path = os.path.join(OUTPUT_DIR, case_name, 'pinn', 'model.pt')
    if not os.path.exists(model_path):
        return None

    ckpt = torch.load(model_path, map_location='cpu')
    model = FEAParametricPINN(
        hidden=int(ckpt['hidden']),
        n_layers=int(ckpt['n_layers']),
        n_freqs=int(ckpt['n_freqs']),
        sigma=float(ckpt['sigma']),
    )
    model.load_state_dict(ckpt['state_dict'])
    model.eval()
    return model, ckpt


def pinn_predict(model, x, y, E_norm, nu_norm, P_norm):
    """Run PINN inference on full grid."""
    x_norm = torch.from_numpy(x).float()
    y_norm = torch.from_numpy(y).float()
    xyt = build_input_tensor(x_norm, y_norm,
                             torch.tensor(E_norm, dtype=torch.float32),
                             torch.tensor(nu_norm, dtype=torch.float32),
                             torch.tensor(P_norm, dtype=torch.float32))
    with torch.no_grad():
        out = model(xyt)
    return {
        'ux': out[:, 0].numpy(),
        'uy': out[:, 1].numpy(),
        'sigma_xx': out[:, 2].numpy(),
        'sigma_yy': out[:, 3].numpy(),
        'von_mises': out[:, 5].numpy(),
    }


def make_triangulation(x, y, connectivity):
    """Create matplotlib Triangulation from quad connectivity."""
    triangles = []
    for elem in connectivity:
        n0, n1, n2, n3 = elem
        triangles.append([n0, n1, n2])
        triangles.append([n0, n2, n3])
    return Triangulation(x, y, np.array(triangles))


def nodal_average(element_values, connectivity, num_nodes):
    """Convert element-centered values to nodal values via averaging."""
    nodal_sum = np.zeros(num_nodes)
    nodal_count = np.zeros(num_nodes)
    for i, elem in enumerate(connectivity):
        val = element_values[i]
        for node_idx in elem[:3]:  # n0, n1, n2 (triangles)
            nodal_sum[node_idx] += val
            nodal_count[node_idx] += 1
        # For quads, also include n3
        if len(elem) > 3:
            node_idx = elem[3]
            nodal_sum[node_idx] += val
            nodal_count[node_idx] += 1
    nodal_count[nodal_count == 0] = 1
    return nodal_sum / nodal_count


def plot_comparison(feas, pinn, case_name, outdir):
    """Generate side-by-side FEA vs PINN vs Error plots."""
    os.makedirs(outdir, exist_ok=True)

    triang = make_triangulation(feas['x'], feas['y'], feas['connectivity'])

    # Nodal fields (displacement)
    nodal_fields = [
        ('ux', 'Displacement X', 'RdBu_r'),
        ('uy', 'Displacement Y', 'RdBu_r'),
    ]

    for field_key, title, cmap in nodal_fields:
        feas_vals = feas[field_key]
        pinn_vals = pinn[field_key]
        error = np.abs(pinn_vals - feas_vals)

        fig, axes = plt.subplots(1, 3, figsize=(18, 5))

        for ax_idx, (vals, lbl) in enumerate([(feas_vals, 'FEA'), (pinn_vals, 'PINN'), (error, 'Error')]):
            ax = axes[ax_idx]
            cmap_use = 'viridis' if ax_idx == 2 else cmap
            tc = ax.tricontourf(triang, vals, levels=20, cmap=cmap_use)
            plt.colorbar(tc, ax=ax, shrink=0.8)
            ax.set_aspect('equal')
            ax.set_title(f'{lbl} {title}')

        l2 = np.linalg.norm(pinn_vals - feas_vals) / (np.linalg.norm(feas_vals) + 1e-12)
        fig.suptitle(f'{case_name}: {title} (L2 rel error: {l2:.4f})', fontsize=14)
        plt.tight_layout()
        fname = f'pinn_{field_key}.png'
        plt.savefig(os.path.join(outdir, fname), dpi=150, bbox_inches='tight')
        plt.close()
        print(f'  Saved {fname} (L2={l2:.4f})')

    # Element-centered fields (stress) -- nodalize for plotting
    elem_fields = [
        ('sigma_xx', 'Stress XX', 'hot'),
        ('sigma_yy', 'Stress YY', 'hot'),
        ('von_mises', 'Von Mises Stress', 'hot'),
    ]

    for field_key, title, cmap in elem_fields:
        feas_elem = feas[field_key]
        pinn_elem = pinn[field_key]

        # Nodalize both for plotting
        feas_nodal = nodal_average(feas_elem, feas['connectivity'], feas['num_nodes'])
        pinn_nodal = nodal_average(pinn_elem, feas['connectivity'], feas['num_nodes'])
        error_nodal = np.abs(pinn_nodal - feas_nodal)

        fig, axes = plt.subplots(1, 3, figsize=(18, 5))

        for ax_idx, (vals, lbl) in enumerate([(feas_nodal, 'FEA'), (pinn_nodal, 'PINN'), (error_nodal, 'Error')]):
            ax = axes[ax_idx]
            cmap_use = 'viridis' if ax_idx == 2 else cmap
            tc = ax.tricontourf(triang, vals, levels=20, cmap=cmap_use)
            plt.colorbar(tc, ax=ax, shrink=0.8)
            ax.set_aspect('equal')
            ax.set_title(f'{lbl} {title}')

        l2 = np.linalg.norm(pinn_nodal - feas_nodal) / (np.linalg.norm(feas_nodal) + 1e-12)
        fig.suptitle(f'{case_name}: {title} (L2 rel error: {l2:.4f})', fontsize=14)
        plt.tight_layout()
        fname = f'pinn_{field_key}.png'
        plt.savefig(os.path.join(outdir, fname), dpi=150, bbox_inches='tight')
        plt.close()
        print(f'  Saved {fname} (L2={l2:.4f})')


def plot_loss_convergence(case_name, outdir):
    """Plot training loss convergence."""
    history_path = os.path.join(OUTPUT_DIR, case_name, 'pinn', 'loss_history.npz')
    if not os.path.exists(history_path):
        return

    data = np.load(history_path)
    epochs = data['epoch']
    loss = data['loss']

    fig, ax = plt.subplots(figsize=(8, 5))
    ax.semilogy(epochs, loss, 'b-', linewidth=2)
    ax.set_xlabel('Epoch')
    ax.set_ylabel('Total Loss')
    ax.set_title(f'{case_name}: Training Loss Convergence')
    ax.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(os.path.join(outdir, 'pinn_loss_convergence.png'), dpi=150, bbox_inches='tight')
    plt.close()
    print(f'  Saved pinn_loss_convergence.png')


def evaluate_case(case_name, outdir):
    """Full evaluation for one case."""
    print(f'\n--- Evaluating {case_name} ---')

    # Map case names to output directories
    case_dirs = {
        'cantilever': 'cantilever_32',
        'cook': 'cook_32',
        'patch': 'patch',
    }
    fea_dir = os.path.join(OUTPUT_DIR, case_dirs.get(case_name, case_name))

    if not os.path.exists(os.path.join(fea_dir, 'mesh.json')):
        print(f'  FEA output not found: {fea_dir}')
        return

    feas = load_fea_data(fea_dir)

    result = load_pinn_model(case_name)
    if result is None:
        print(f'  PINN model not found for {case_name}')
        return
    model, ckpt = result

    # Get parameters from checkpoint
    meta_path = os.path.join(fea_dir, 'meta.json')
    with open(meta_path) as f:
        meta = json.load(f)

    E_ref = meta['material']['E']
    nu_ref = meta['material']['nu']

    from pinn.data.loader import E_MIN, E_MAX, NU_MIN, NU_MAX, P_MIN, P_MAX, normalize_param
    E_norm = normalize_param(E_ref, E_MIN, E_MAX)
    nu_norm = normalize_param(nu_ref, NU_MIN, NU_MAX)
    P_norm = normalize_param(1000.0, P_MIN, P_MAX)  # Reference load

    pinn = pinn_predict(model, feas['x'], feas['y'], E_norm, nu_norm, P_norm)

    plot_comparison(feas, pinn, case_name, outdir)
    plot_loss_convergence(case_name, outdir)


def main():
    parser = argparse.ArgumentParser(description='PINN Accuracy Study')
    parser.add_argument('--case', type=str, help='Case name to evaluate')
    parser.add_argument('--all-cases', action='store_true', help='Evaluate all cases')
    args = parser.parse_args()

    os.makedirs(RESULTS_DIR, exist_ok=True)

    if args.all_cases:
        cases = ['cantilever', 'cook', 'patch']
        for case in cases:
            case_outdir = os.path.join(RESULTS_DIR, case)
            evaluate_case(case, case_outdir)
    elif args.case:
        case_outdir = os.path.join(RESULTS_DIR, args.case)
        evaluate_case(args.case, case_outdir)
    else:
        parser.print_help()
        sys.exit(1)


if __name__ == '__main__':
    main()
