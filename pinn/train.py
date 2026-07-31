#!/usr/bin/env python3
"""Train PINN surrogate for 2D linear elasticity.

Uses Adam + L-BFGS training pipeline (same as LBM-2D).

Usage:
    python -m pinn.train --case cantilever --epochs-adam 5000
    python -m pinn.train --all-cases --epochs-adam 10000
"""

import os
import sys
import time
import json
import argparse
import numpy as np
import torch
import torch.nn.functional as F

from pinn.models.pinn import FEAParametricPINN, predict, build_input_tensor
from pinn.models.losses import total_loss
from pinn.data.loader import FEADataset, sample_collocation, sample_sensors, make_boundary_tensors


OUTPUT_DIR = os.path.join(os.path.dirname(__file__), '..', 'output')
DATA_DIR = os.path.join(os.path.dirname(__file__), 'data', 'training')


def train_single_case(args, case_name, model, device):
    """Train PINN for a single case.

    Args:
        args: argparse namespace.
        case_name: Name of the case (e.g., 'cantilever').
        model: FEAParametricPINN instance (will be re-initialized).
        device: Torch device.

    Returns:
        Dict with training history and metrics.
    """
    print(f'\n{"="*60}')
    print(f'Training PINN for: {case_name}')
    print(f'{"="*60}')

    # Load dataset
    dataset = FEADataset(DATA_DIR, case_names=[case_name])
    if len(dataset) == 0:
        print(f'  No training data found for {case_name}')
        return None

    # Use a single sample for now (all samples have same mesh)
    sample = dataset[0]
    print(f'  Nodes: {sample["num_nodes"]}, Elements: {sample["num_elements"]}')

    # Build full input tensor for all nodes
    x_norm = torch.from_numpy(sample['x']).float()
    y_norm = torch.from_numpy(sample['y']).float()
    E_norm = torch.tensor(sample['E_norm'], dtype=torch.float32)
    nu_norm = torch.tensor(sample['nu_norm'], dtype=torch.float32)
    P_norm = torch.tensor(sample['P_norm'], dtype=torch.float32)

    xyt_all = build_input_tensor(x_norm, y_norm, E_norm, nu_norm, P_norm).to(device)

    # Sample collocation points
    rng = np.random.default_rng(42)
    n_colloc = min(args.n_colloc, sample['num_nodes'])
    colloc_idx = rng.choice(sample['num_nodes'], size=n_colloc, replace=False)
    xyt_colloc = xyt_all[colloc_idx].clone().detach().requires_grad_(True)

    # Sample sensor points
    n_sensors = min(args.n_sensors, sample['num_nodes'])
    sensor_idx = rng.choice(sample['num_nodes'], size=n_sensors, replace=False)
    xyt_sensors = xyt_all[sensor_idx].clone()
    ux_target = torch.from_numpy(sample['ux'][sensor_idx]).float().to(device)
    uy_target = torch.from_numpy(sample['uy'][sensor_idx]).float().to(device)

    # Boundary conditions
    bc = make_boundary_tensors(sample)
    dir_nodes = bc['dir_nodes'].to(device)
    dir_dofs = bc['dir_dofs'].to(device)
    dir_values = bc['dir_values'].to(device)
    neu_nodes = bc['neu_nodes'].to(device)
    neu_dofs = bc['neu_dofs'].to(device)
    neu_values = bc['neu_values'].to(device)

    # Initialize model
    model = FEAParametricPINN(
        hidden=args.hidden,
        n_layers=args.n_layers,
        n_freqs=args.n_freqs,
        sigma=args.sigma,
    ).to(device)

    n_params = sum(p.numel() for p in model.parameters())
    print(f'  Model parameters: {n_params:,}')

    # Optimizer: Adam phase
    optimizer = torch.optim.Adam(model.parameters(), lr=args.lr)
    scheduler = torch.optim.lr_scheduler.ReduceLROnPlateau(
        optimizer, factor=0.5, patience=500, min_lr=1e-6)

    # Output directory
    out_dir = os.path.join(OUTPUT_DIR, case_name, 'pinn')
    os.makedirs(out_dir, exist_ok=True)

    # Training history
    history = {'epoch': [], 'loss': [], 'pde': [], 'data': [], 'bc': [], 'const': []}

    # Adam phase
    t0 = time.time()
    print(f'\n--- Adam training ({args.epochs_adam} epochs) ---')

    for ep in range(1, args.epochs_adam + 1):
        optimizer.zero_grad()

        L, L_pde, L_data, L_bc, L_const = total_loss(
            model, xyt_colloc, xyt_sensors, ux_target, uy_target,
            E_norm, nu_norm, P_norm,
            dir_nodes, dir_dofs, dir_values,
            neu_nodes, neu_dofs, neu_values,
            w_pde=args.w_pde, w_data=args.w_data, w_bc=args.w_bc, w_const=args.w_const,
        )

        L.backward()
        optimizer.step()
        scheduler.step(L.item())

        if ep % 500 == 0 or ep == 1:
            lr = optimizer.param_groups[0]['lr']
            print(f'  epoch {ep:5d}  loss={L.item():.6f}  '
                  f'pde={L_pde.item():.6f}  data={L_data.item():.6f}  '
                  f'bc={L_bc.item():.6f}  const={L_const.item():.6f}  lr={lr:.2e}')
            history['epoch'].append(ep)
            history['loss'].append(L.item())
            history['pde'].append(L_pde.item())
            history['data'].append(L_data.item())
            history['bc'].append(L_bc.item())
            history['const'].append(L_const.item())

    adam_time = time.time() - t0
    print(f'  Adam phase done in {adam_time:.1f}s')

    # L-BFGS fine-tune
    if args.epochs_lbfgs > 0:
        print(f'\n--- L-BFGS fine-tune ({args.epochs_lbfgs} steps) ---')
        t1 = time.time()
        optimizer_lbfgs = torch.optim.LBFGS(
            model.parameters(), lr=1.0,
            max_iter=args.epochs_lbfgs, history_size=100,
            line_search_fn='strong_wolfe')

        def closure():
            optimizer_lbfgs.zero_grad()
            L, _, _, _, _ = total_loss(
                model, xyt_colloc, xyt_sensors, ux_target, uy_target,
                E_norm, nu_norm, P_norm,
                dir_nodes, dir_dofs, dir_values,
                neu_nodes, neu_dofs, neu_values,
                w_pde=args.w_pde, w_data=args.w_data, w_bc=args.w_bc, w_const=args.w_const,
            )
            L.backward()
            return L

        L_lbfgs = optimizer_lbfgs.step(closure)
        lbfgs_time = time.time() - t1
        print(f'  L-BFGS done in {lbfgs_time:.1f}s  final loss={L_lbfgs.item():.6f}')

    total_time = time.time() - t0

    # Save model
    model_path = os.path.join(out_dir, 'model.pt')
    torch.save({
        'state_dict': model.state_dict(),
        'hidden': args.hidden,
        'n_layers': args.n_layers,
        'n_freqs': args.n_freqs,
        'sigma': args.sigma,
        'case_name': case_name,
    }, model_path)
    print(f'\n  Model saved: {model_path}')

    # Evaluate on full grid
    model.eval()
    with torch.no_grad():
        out = model(xyt_all)
    ux_pred = out[:, 0].cpu().numpy()
    uy_pred = out[:, 1].cpu().numpy()

    # L2 relative error
    l2_ux = np.linalg.norm(ux_pred - sample['ux']) / (np.linalg.norm(sample['ux']) + 1e-12)
    l2_uy = np.linalg.norm(uy_pred - sample['uy']) / (np.linalg.norm(sample['uy']) + 1e-12)

    print(f'\n  L2 relative error (reference sample):')
    print(f'    ux: {l2_ux:.6f}')
    print(f'    uy: {l2_uy:.6f}')

    # Save loss history
    np.savez(os.path.join(out_dir, 'loss_history.npz'), **history)

    return {
        'case_name': case_name,
        'l2_ux': float(l2_ux),
        'l2_uy': float(l2_uy),
        'total_time': total_time,
        'n_params': n_params,
    }


def train_all_cases(args):
    """Train PINN for all foundation cases."""
    device = torch.device('mps' if torch.backends.mps.is_available() else
                          'cuda' if torch.cuda.is_available() else 'cpu')
    print(f'Device: {device}')

    cases = ['cantilever', 'cook', 'patch']
    results = []

    for case_name in cases:
        result = train_single_case(args, case_name, None, device)
        if result:
            results.append(result)

    # Summary
    print(f'\n{"="*60}')
    print(f'Training Summary')
    print(f'{"="*60}')
    for r in results:
        print(f'  {r["case_name"]:15s}  L2_ux={r["l2_ux"]:.6f}  L2_uy={r["l2_uy"]:.6f}  '
              f'time={r["total_time"]:.1f}s  params={r["n_params"]:,}')

    # Save summary
    summary_path = os.path.join(OUTPUT_DIR, 'pinn_summary.json')
    with open(summary_path, 'w') as f:
        json.dump(results, f, indent=2)
    print(f'\nSummary saved: {summary_path}')


def main():
    parser = argparse.ArgumentParser(description='Train PINN for 2D linear elasticity')
    parser.add_argument('--case', type=str, help='Case name to train (cantilever, cook, patch)')
    parser.add_argument('--all-cases', action='store_true', help='Train all foundation cases')
    parser.add_argument('--hidden', type=int, default=256, help='MLP hidden width')
    parser.add_argument('--n-layers', type=int, default=8, help='Number of MLP layers')
    parser.add_argument('--n-freqs', type=int, default=128, help='Fourier feature frequencies')
    parser.add_argument('--sigma', type=float, default=5.0, help='Fourier feature sigma')
    parser.add_argument('--n-colloc', type=int, default=2000, help='Collocation points')
    parser.add_argument('--n-sensors', type=int, default=500, help='Sensor points')
    parser.add_argument('--lr', type=float, default=1e-3, help='Adam learning rate')
    parser.add_argument('--epochs-adam', type=int, default=5000, help='Adam epochs')
    parser.add_argument('--epochs-lbfgs', type=int, default=500, help='L-BFGS steps')
    parser.add_argument('--w-pde', type=float, default=1.0, help='PDE loss weight')
    parser.add_argument('--w-data', type=float, default=10.0, help='Data loss weight')
    parser.add_argument('--w-bc', type=float, default=5.0, help='BC loss weight')
    parser.add_argument('--w-const', type=float, default=1.0, help='Constitutive loss weight')
    args = parser.parse_args()

    if not args.case and not args.all_cases:
        parser.print_help()
        sys.exit(1)

    if args.all_cases:
        train_all_cases(args)
    else:
        device = torch.device('mps' if torch.backends.mps.is_available() else
                              'cuda' if torch.cuda.is_available() else 'cpu')
        print(f'Device: {device}')
        train_single_case(args, args.case, None, device)


if __name__ == '__main__':
    main()
