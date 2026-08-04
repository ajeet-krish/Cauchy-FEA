#!/usr/bin/env python3
"""
FEA-2D Web Data Converter: JSON -> browser-optimized Three.js data
Combines mesh, displacement, and stress into a single JSON file for the
interactive Three.js viewer.

Handles 2D (Q4, Q8, T3) and 3D (H8 hexahedral) elements.

Usage:
    python3 scripts/convert_for_web.py output/cantilever_32/ docs/assets/data/cantilever_32.json
    python3 scripts/convert_for_web.py --all
"""

import json
import sys
import os
import argparse
import numpy as np


def load_json(outdir, filename):
    filepath = os.path.join(outdir, filename)
    if not os.path.exists(filepath):
        return None
    with open(filepath) as f:
        return json.load(f)


def detect_3d(mesh):
    """Detect if the mesh contains 3D hexahedral elements."""
    return mesh.get('hex_elements') is not None and len(mesh.get('hex_elements', [])) > 0


def compute_element_centroids(nodes, elements, is_3d=False):
    """Compute element centroids for 2D quad/tri or 3D hex elements."""
    centroids = []
    for elem in elements:
        if is_3d:
            n_ids = [elem['n0'], elem['n1'], elem['n2'], elem['n3'],
                     elem['n4'], elem['n5'], elem['n6'], elem['n7']]
            cx = sum(nodes[n]['x'] for n in n_ids) / 8.0
            cy = sum(nodes[n]['y'] for n in n_ids) / 8.0
            cz = sum(nodes[n]['z'] for n in n_ids) / 8.0
            centroids.append({'x': cx, 'y': cy, 'z': cz})
        else:
            n0, n1, n2, n3 = elem['n0'], elem['n1'], elem['n2'], elem['n3']
            cx = (nodes[n0]['x'] + nodes[n1]['x'] + nodes[n2]['x'] + nodes[n3]['x']) / 4.0
            cy = (nodes[n0]['y'] + nodes[n1]['y'] + nodes[n2]['y'] + nodes[n3]['y']) / 4.0
            centroids.append({'x': cx, 'y': cy})
    return centroids


def compute_nodal_averaged_stresses(nodes, elements, elem_stresses, is_3d=False):
    """Compute nodally-averaged stresses by accumulating element contributions."""
    n_nodes = len(nodes)

    if is_3d:
        node_stress = [{'sum_vm': 0.0, 'sum_xx': 0.0, 'sum_yy': 0.0,
                        'sum_zz': 0.0, 'sum_xy': 0.0, 'sum_yz': 0.0,
                        'sum_xz': 0.0, 'sum_s1': 0.0, 'sum_s2': 0.0,
                        'sum_s3': 0.0, 'count': 0} for _ in range(n_nodes)]

        for i, elem in enumerate(elements):
            if i >= len(elem_stresses):
                break
            s = elem_stresses[i]
            for n in [elem['n0'], elem['n1'], elem['n2'], elem['n3'],
                      elem['n4'], elem['n5'], elem['n6'], elem['n7']]:
                node_stress[n]['sum_vm'] += s['von_mises']
                node_stress[n]['sum_xx'] += s['sigma_xx']
                node_stress[n]['sum_yy'] += s['sigma_yy']
                node_stress[n]['sum_zz'] += s.get('sigma_zz', 0.0)
                node_stress[n]['sum_xy'] += s['sigma_xy']
                node_stress[n]['sum_yz'] += s.get('sigma_yz', 0.0)
                node_stress[n]['sum_xz'] += s.get('sigma_xz', 0.0)
                node_stress[n]['sum_s1'] += s.get('sigma_1', 0.0)
                node_stress[n]['sum_s2'] += s.get('sigma_2', 0.0)
                node_stress[n]['sum_s3'] += s.get('sigma_3', 0.0)
                node_stress[n]['count'] += 1

        result = []
        for ns in node_stress:
            c = max(ns['count'], 1)
            result.append({
                'von_mises': ns['sum_vm'] / c,
                'sigma_xx': ns['sum_xx'] / c,
                'sigma_yy': ns['sum_yy'] / c,
                'sigma_zz': ns['sum_zz'] / c,
                'sigma_xy': ns['sum_xy'] / c,
                'sigma_yz': ns['sum_yz'] / c,
                'sigma_xz': ns['sum_xz'] / c,
                'sigma_1': ns['sum_s1'] / c,
                'sigma_2': ns['sum_s2'] / c,
                'sigma_3': ns['sum_s3'] / c
            })
        return result
    else:
        node_stress = [{'sum_vm': 0.0, 'sum_xx': 0.0, 'sum_yy': 0.0,
                        'sum_xy': 0.0, 'count': 0} for _ in range(n_nodes)]

        for i, elem in enumerate(elements):
            if i >= len(elem_stresses):
                break
            s = elem_stresses[i]
            for n in [elem['n0'], elem['n1'], elem['n2'], elem['n3']]:
                node_stress[n]['sum_vm'] += s['von_mises']
                node_stress[n]['sum_xx'] += s['sigma_xx']
                node_stress[n]['sum_yy'] += s['sigma_yy']
                node_stress[n]['sum_xy'] += s['sigma_xy']
                node_stress[n]['count'] += 1

        result = []
        for ns in node_stress:
            c = max(ns['count'], 1)
            result.append({
                'von_mises': ns['sum_vm'] / c,
                'sigma_xx': ns['sum_xx'] / c,
                'sigma_yy': ns['sum_yy'] / c,
                'sigma_xy': ns['sum_xy'] / c
            })
        return result


def compute_principal_stresses(sigma_xx, sigma_yy, sigma_xy):
    """Compute 2D principal stresses and their angles."""
    sigma_1 = []
    sigma_2 = []
    theta1 = []
    theta2 = []

    for i in range(len(sigma_xx)):
        sxx = sigma_xx[i]
        syy = sigma_yy[i]
        sxy = sigma_xy[i]

        # Principal stresses
        avg = (sxx + syy) / 2
        diff = (sxx - syy) / 2
        R = np.sqrt(diff**2 + sxy**2)

        s1 = avg + R
        s2 = avg - R

        # Principal angles
        if abs(sxx - syy) < 1e-10 and abs(sxy) < 1e-10:
            theta_1 = 0.0
            theta_2 = np.pi / 2
        else:
            theta_1 = 0.5 * np.arctan2(2 * sxy, sxx - syy)
            theta_2 = theta_1 + np.pi / 2

        sigma_1.append(s1)
        sigma_2.append(s2)
        theta1.append(theta_1)
        theta2.append(theta_2)

    return sigma_1, sigma_2, theta1, theta2


def compute_principal_stresses_3d(sigma_xx, sigma_yy, sigma_zz,
                                   sigma_xy, sigma_yz, sigma_xz):
    """Compute 3 principal stresses from 6-component stress tensor.

    Uses Cardano's formula for eigenvalues of a 3x3 symmetric tensor.
    Returns (sigma_1, sigma_2, sigma_3) sorted descending (s1 >= s2 >= s3).
    """
    sigma_1 = []
    sigma_2 = []
    sigma_3 = []

    for i in range(len(sigma_xx)):
        sxx = sigma_xx[i]
        syy = sigma_yy[i]
        szz = sigma_zz[i]
        sxy = sigma_xy[i]
        syz = sigma_yz[i]
        sxz = sigma_xz[i]

        # Invariants of the stress tensor
        I1 = sxx + syy + szz
        I2 = (sxx * syy + syy * szz + szz * sxx
              - sxy**2 - syz**2 - sxz**2)
        I3 = (sxx * (syy * szz - syz**2)
              - sxy * (sxy * szz - syz * sxz)
              + sxz * (sxy * syz - syy * sxz))

        # Characteristic equation: lambda^3 - I1*lambda^2 + I2*lambda - I3 = 0
        # Depressed cubic: t^3 + p*t + q = 0
        p = I2 - I1**2 / 3.0
        q = (2.0 * I1**3 / 27.0 - I1 * I2 / 3.0 + I3)

        # Discriminant
        disc = q**2 / 4.0 + p**3 / 27.0

        if disc > 1e-12:
            # One real root, two complex conjugate
            sqrt_disc = np.sqrt(disc)
            u = np.cbrt(-q / 2.0 + sqrt_disc)
            v = np.cbrt(-q / 2.0 - sqrt_disc)
            roots = [u + v]
            # Complex roots give equal real parts
            real_part = -(u + v) / 2.0
            imag_part = np.sqrt(3.0) / 2.0 * (u - v)
            roots.extend([real_part + imag_part * 1j,
                         real_part - imag_part * 1j])
            # Take real parts and sort
            lam = sorted([r.real if np.isreal(r) else r.real for r in roots],
                        reverse=True)
        else:
            # All real roots
            if abs(p) < 1e-15:
                lam = [I1 / 3.0, I1 / 3.0, I1 / 3.0]
            else:
                phi = np.arccos(np.clip(-q / 2.0 / np.sqrt(-p**3 / 27.0),
                                       -1.0, 1.0))
                lam = sorted([
                    2.0 * np.sqrt(-p / 3.0) * np.cos(phi / 3.0),
                    2.0 * np.sqrt(-p / 3.0) * np.cos((phi - 2.0 * np.pi) / 3.0),
                    2.0 * np.sqrt(-p / 3.0) * np.cos((phi - 4.0 * np.pi) / 3.0)
                ], reverse=True)

        sigma_1.append(lam[0] + I1 / 3.0)
        sigma_2.append(lam[1] + I1 / 3.0)
        sigma_3.append(lam[2] + I1 / 3.0)

    return sigma_1, sigma_2, sigma_3


def extract_boundary_conditions(outdir):
    """Extract boundary conditions from mesh.json."""
    mesh = load_json(outdir, 'mesh.json')

    boundary = {
        'dirichlet': [],
        'neumann': []
    }

    if mesh:
        boundary['dirichlet'] = mesh.get('dirichlet', [])
        boundary['neumann'] = mesh.get('neumann', [])

    return boundary


def compute_camera_preset(mesh):
    """Compute camera preset based on mesh bounds.

    For 3D meshes, positions the camera at a diagonal viewpoint
    that shows all three axes.
    """
    if not mesh or 'nodes' not in mesh:
        return {'position': [0.5, 0.5, 2.0], 'target': [0.5, 0.5, 0.0]}

    nodes = mesh['nodes']
    xs = [n['x'] for n in nodes]
    ys = [n['y'] for n in nodes]
    zs = [n['z'] for n in nodes]

    x_center = (min(xs) + max(xs)) / 2
    y_center = (min(ys) + max(ys)) / 2
    z_center = (min(zs) + max(zs)) / 2
    x_range = max(xs) - min(xs)
    y_range = max(ys) - min(ys)
    z_range = max(zs) - min(zs)
    max_range = max(x_range, y_range, z_range)

    # Check if this is a 3D mesh (non-zero z extent)
    if z_range > 1e-12:
        # 3D: diagonal viewpoint showing all three axes
        return {
            'position': [
                x_center + max_range * 0.8,
                y_center + max_range * 0.6,
                z_center + max_range * 1.2
            ],
            'target': [x_center, y_center, z_center]
        }
    else:
        # 2D: top-down view
        return {
            'position': [x_center, y_center, max_range * 2],
            'target': [x_center, y_center, 0.0]
        }


def convert_single(outdir, outfile):
    mesh = load_json(outdir, 'mesh.json')
    disp = load_json(outdir, 'displacement.json')
    stress = load_json(outdir, 'stress.json')
    meta = load_json(outdir, 'meta.json')

    if not mesh:
        print(f'  Error: No mesh.json in {outdir}')
        return False

    nodes = mesh['nodes']
    is_3d = detect_3d(mesh)

    if is_3d:
        hex_elems = mesh.get('hex_elements', [])
        all_elems = hex_elems
        elem_key = 'hex_elements'
    else:
        quad_elems = mesh.get('quad_elements', [])
        quad8_elems = mesh.get('quad8_elements', [])
        tri_elems = mesh.get('tri_elements', [])
        all_elems = quad_elems + quad8_elems + tri_elems
        elem_key = 'elements'

    # Build node array for Three.js
    node_array = []
    for n in nodes:
        if is_3d:
            node_array.append({'x': n['x'], 'y': n['y'], 'z': n.get('z', 0.0)})
        else:
            node_array.append({'x': n['x'], 'y': n['y'], 'z': 0.0})

    # Build element array for Three.js
    elem_array = []
    if is_3d:
        for e in hex_elems:
            elem_array.append({
                'n0': e['n0'], 'n1': e['n1'], 'n2': e['n2'], 'n3': e['n3'],
                'n4': e['n4'], 'n5': e['n5'], 'n6': e['n6'], 'n7': e['n7']
            })
    else:
        for e in quad_elems:
            elem_array.append({'n0': e['n0'], 'n1': e['n1'], 'n2': e['n2'], 'n3': e['n3']})
        for e in quad8_elems:
            elem_array.append({'n0': e['n0'], 'n1': e['n1'], 'n2': e['n2'], 'n3': e['n3']})
        for e in tri_elems:
            # Convert T3 to T4 (degenerate quad) for Three.js compatibility
            elem_array.append({'n0': e['n0'], 'n1': e['n1'], 'n2': e['n2'], 'n3': e['n2']})

    # Displacement data
    disp_array = []
    if disp:
        for n in disp['nodes']:
            if is_3d:
                disp_array.append({'ux': n['ux'], 'uy': n['uy'], 'uz': n.get('uz', 0.0)})
            else:
                disp_array.append({'ux': n['ux'], 'uy': n['uy']})

    # Stress data
    if is_3d:
        stress_data = {
            'von_mises': [],
            'sigma_xx': [],
            'sigma_yy': [],
            'sigma_zz': [],
            'sigma_xy': [],
            'sigma_yz': [],
            'sigma_xz': [],
            'sigma_1': [],
            'sigma_2': [],
            'sigma_3': [],
        }
    else:
        stress_data = {
            'von_mises': [],
            'sigma_xx': [],
            'sigma_yy': [],
            'sigma_xy': [],
            'sigma_1': [],
            'sigma_2': [],
            'theta1': [],
            'theta2': []
        }

    if stress:
        if is_3d:
            sigma_xx = [e['sigma_xx'] for e in stress['elements']]
            sigma_yy = [e['sigma_yy'] for e in stress['elements']]
            sigma_zz = [e.get('sigma_zz', 0.0) for e in stress['elements']]
            sigma_xy = [e['sigma_xy'] for e in stress['elements']]
            sigma_yz = [e.get('sigma_yz', 0.0) for e in stress['elements']]
            sigma_xz = [e.get('sigma_xz', 0.0) for e in stress['elements']]

            # Compute 3D principal stresses
            s1, s2, s3 = compute_principal_stresses_3d(
                sigma_xx, sigma_yy, sigma_zz, sigma_xy, sigma_yz, sigma_xz)

            stress_data['von_mises'] = [e['von_mises'] for e in stress['elements']]
            stress_data['sigma_xx'] = sigma_xx
            stress_data['sigma_yy'] = sigma_yy
            stress_data['sigma_zz'] = sigma_zz
            stress_data['sigma_xy'] = sigma_xy
            stress_data['sigma_yz'] = sigma_yz
            stress_data['sigma_xz'] = sigma_xz
            stress_data['sigma_1'] = s1
            stress_data['sigma_2'] = s2
            stress_data['sigma_3'] = s3
        else:
            sigma_xx = [e['sigma_xx'] for e in stress['elements']]
            sigma_yy = [e['sigma_yy'] for e in stress['elements']]
            sigma_xy = [e['sigma_xy'] for e in stress['elements']]

            # Compute 2D principal stresses
            s1, s2, theta1, theta2 = compute_principal_stresses(sigma_xx, sigma_yy, sigma_xy)

            stress_data['von_mises'] = [e['von_mises'] for e in stress['elements']]
            stress_data['sigma_xx'] = sigma_xx
            stress_data['sigma_yy'] = sigma_yy
            stress_data['sigma_xy'] = sigma_xy
            stress_data['sigma_1'] = s1
            stress_data['sigma_2'] = s2
            stress_data['theta1'] = theta1
            stress_data['theta2'] = theta2

    # Compute nodally-averaged stresses for smooth contours
    node_stress_avg = compute_nodal_averaged_stresses(
        nodes, all_elems,
        stress['elements'] if stress else [],
        is_3d=is_3d)

    if is_3d:
        nodal_stress = {
            'von_mises': [s['von_mises'] for s in node_stress_avg],
            'sigma_1': [s.get('sigma_1', 0.0) for s in node_stress_avg],
            'sigma_2': [s.get('sigma_2', 0.0) for s in node_stress_avg],
            'sigma_3': [s.get('sigma_3', 0.0) for s in node_stress_avg]
        }
    else:
        nodal_stress = {
            'von_mises': [s['von_mises'] for s in node_stress_avg],
            'sigma_1': [s.get('sigma_1', 0.0) for s in node_stress_avg],
            'sigma_2': [s.get('sigma_2', 0.0) for s in node_stress_avg]
        }

    # Extract boundary conditions
    boundary = extract_boundary_conditions(outdir)

    # Compute camera preset
    camera = compute_camera_preset(mesh)

    # Build output
    output = {
        'meta': meta or {},
        'nodes': node_array,
        'elements': elem_array,
        'displacement': disp_array,
        'stress': stress_data,
        'nodalStress': nodal_stress,
        'boundary': boundary,
        'camera': camera
    }

    # Add bounds for quick camera setup
    xs = [n['x'] for n in nodes]
    ys = [n['y'] for n in nodes]
    zs = [n['z'] for n in nodes]
    output['bounds'] = {
        'xmin': min(xs), 'xmax': max(xs),
        'ymin': min(ys), 'ymax': max(ys),
        'zmin': min(zs), 'zmax': max(zs)
    }

    os.makedirs(os.path.dirname(outfile) if os.path.dirname(outfile) else '.', exist_ok=True)
    with open(outfile, 'w') as f:
        json.dump(output, f)

    dim_label = '3D' if is_3d else '2D'
    print(f'  Converted ({dim_label}): {os.path.basename(outfile)} '
          f'({len(nodes)} nodes, {len(all_elems)} elements)')
    return True


# Map output directory names to web filenames
CASE_MAP = {
    'cantilever_32': 'cantilever_32',
    'cantilever_16': 'cantilever_16',
    'cook_32': 'cook_32',
    'cook_64': 'cook_64',
    'lbracket': 'lbracket',
    'patch': 'patch',
    'plate_hole': 'plate_hole',
    'michell': 'michell',
    'cantilever_3d': 'cantilever_3d',
    'plate_hole_3d': 'plate_hole_3d',
    'lame_3d': 'lame_3d',
}


def main():
    parser = argparse.ArgumentParser(description='FEA-2D/3D Web Data Converter')
    parser.add_argument('indir', nargs='?', help='Input directory (output/case_name/)')
    parser.add_argument('outfile', nargs='?', help='Output JSON file')
    parser.add_argument('--all', action='store_true', help='Convert all available cases')
    args = parser.parse_args()

    if args.all:
        base_dir = 'output'
        out_dir = 'docs/assets/data'
        os.makedirs(out_dir, exist_ok=True)

        converted = 0
        for case_name, web_name in CASE_MAP.items():
            # New structure: output/{case}/simulations/{mesh_size}/
            # Try common mesh sizes for each case
            sim_dir = os.path.join(base_dir, case_name, 'simulations')
            if os.path.exists(sim_dir):
                # Find subdirectories (mesh sizes) and pick the largest
                subdirs = [d for d in os.listdir(sim_dir)
                          if os.path.isdir(os.path.join(sim_dir, d)) and not d.startswith('adapt')]
                if subdirs:
                    # Sort numerically to get the largest mesh
                    subdirs.sort(key=lambda x: int(x.replace('_q8', '').replace('q8', '') or '0'))
                    indir = os.path.join(sim_dir, subdirs[-1])
                else:
                    indir = sim_dir
            else:
                # Fallback to old structure
                indir = os.path.join(base_dir, case_name)

            if os.path.exists(os.path.join(indir, 'mesh.json')):
                outfile = os.path.join(out_dir, f'{web_name}.json')
                if convert_single(indir, outfile):
                    converted += 1

        print(f'\nConverted {converted} cases to {out_dir}/')
        return

    if not args.indir:
        print('Usage: python3 convert_for_web.py output/cantilever_32/ docs/assets/data/cantilever_32.json')
        print('       python3 convert_for_web.py --all')
        sys.exit(1)

    outfile = args.outfile
    if not outfile:
        case_name = os.path.basename(args.indir.rstrip('/'))
        outfile = f'docs/assets/data/{case_name}.json'

    convert_single(args.indir, outfile)


if __name__ == '__main__':
    main()
