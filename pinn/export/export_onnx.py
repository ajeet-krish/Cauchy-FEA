#!/usr/bin/env python3
"""Export trained PINN model to ONNX for browser deployment.

Usage:
    python -m pinn.export.export_onnx --case cantilever
    python -m pinn.export.export_onnx --all-cases
"""

import os
import sys
import argparse
import torch

from pinn.models.pinn import FEAParametricPINN

OUTPUT_DIR = os.path.join(os.path.dirname(__file__), '..', '..', 'output')


def export_onnx(model_path, out_path):
    """Export a trained PINN model to ONNX format.

    Args:
        model_path: Path to .pt checkpoint.
        out_path: Path for output .onnx file.
    """
    ckpt = torch.load(model_path, map_location='cpu')
    model = FEAParametricPINN(
        hidden=int(ckpt['hidden']),
        n_layers=int(ckpt['n_layers']),
        n_freqs=int(ckpt['n_freqs']),
        sigma=float(ckpt['sigma']),
    )
    model.load_state_dict(ckpt['state_dict'])
    model.eval()

    # Dummy input: (N, 5) -- (x, y, E_norm, nu_norm, P_norm)
    dummy = torch.randn(100, 5)

    torch.onnx.export(
        model,
        dummy,
        out_path,
        input_names=['input'],
        output_names=['output'],
        dynamic_axes={'input': {0: 'N'}, 'output': {0: 'N'}},
        opset_version=13,
        do_constant_folding=True,
    )

    size = os.path.getsize(out_path)
    print(f'  Exported: {out_path} ({size / 1e6:.2f} MB)')
    return out_path


def export_case(case_name):
    """Export PINN model for a single case."""
    model_path = os.path.join(OUTPUT_DIR, case_name, 'pinn', 'model.pt')
    if not os.path.exists(model_path):
        print(f'  Skipping {case_name}: {model_path} not found')
        return None

    out_dir = os.path.join(OUTPUT_DIR, case_name, 'pinn')
    os.makedirs(out_dir, exist_ok=True)
    out_path = os.path.join(out_dir, 'pinn_model.onnx')

    return export_onnx(model_path, out_path)


def main():
    parser = argparse.ArgumentParser(description='Export PINN to ONNX')
    parser.add_argument('--case', type=str, help='Case name to export')
    parser.add_argument('--all-cases', action='store_true', help='Export all cases')
    args = parser.parse_args()

    if not args.case and not args.all_cases:
        parser.print_help()
        sys.exit(1)

    if args.all_cases:
        cases = ['cantilever', 'cook', 'patch']
        for case in cases:
            print(f'\nExporting {case}:')
            export_case(case)
    else:
        print(f'\nExporting {args.case}:')
        export_case(args.case)


if __name__ == '__main__':
    main()
