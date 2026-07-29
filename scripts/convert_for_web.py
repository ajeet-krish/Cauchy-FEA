#!/usr/bin/env python3
"""
FEA-2D Web Data Converter: JSON -> browser-optimized Three.js data
Combines mesh, displacement, and stress into a single JSON file for the
interactive Three.js viewer.

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


def compute_element_centroids(nodes, elements):
    centroids = []
    for elem in elements:
        n0, n1, n2, n3 = elem['n0'], elem['n1'], elem['n2'], elem['n3']
        cx = (nodes[n0]['x'] + nodes[n1]['x'] + nodes[n2]['x'] + nodes[n3]['x']) / 4.0
        cy = (nodes[n0]['y'] + nodes[n1]['y'] + nodes[n2]['y'] + nodes[n3]['y']) / 4.0
        centroids.append({'x': cx, 'y': cy})
    return centroids


def compute_nodal_averaged_stresses(nodes, elements, elem_stresses):
    n_nodes = len(nodes)
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
    """Compute principal stresses and their angles."""
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


def extract_boundary_conditions(outdir):
    """Extract boundary conditions from meta.json or mesh.json."""
    meta = load_json(outdir, 'meta.json')
    mesh = load_json(outdir, 'mesh.json')
    
    boundary = {
        'dirichlet': [],
        'neumann': []
    }
    
    # Try to extract from meta.json if available
    if meta and 'boundary_conditions' in meta:
        bc = meta['boundary_conditions']
        if 'dirichlet' in bc:
            boundary['dirichlet'] = bc['dirichlet']
        if 'neumann' in bc:
            boundary['neumann'] = bc['neumann']
    
    return boundary


def compute_camera_preset(mesh):
    """Compute camera preset based on mesh bounds."""
    if not mesh or 'nodes' not in mesh:
        return {'position': [0.5, 0.5, 2.0], 'target': [0.5, 0.5, 0.0]}
    
    nodes = mesh['nodes']
    xs = [n['x'] for n in nodes]
    ys = [n['y'] for n in nodes]
    
    x_center = (min(xs) + max(xs)) / 2
    y_center = (min(ys) + max(ys)) / 2
    x_range = max(xs) - min(xs)
    y_range = max(ys) - min(ys)
    max_range = max(x_range, y_range)
    
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
    elements = mesh['elements']

    # Build node array for Three.js
    node_array = []
    for n in nodes:
        node_array.append({'x': n['x'], 'y': n['y'], 'z': 0.0})

    # Build element array for Three.js
    elem_array = []
    for e in elements:
        elem_array.append({'n0': e['n0'], 'n1': e['n1'], 'n2': e['n2'], 'n3': e['n3']})

    # Displacement data
    disp_array = []
    if disp:
        for n in disp['nodes']:
            disp_array.append({'ux': n['ux'], 'uy': n['uy']})

    # Stress data
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
        sigma_xx = [e['sigma_xx'] for e in stress['elements']]
        sigma_yy = [e['sigma_yy'] for e in stress['elements']]
        sigma_xy = [e['sigma_xy'] for e in stress['elements']]
        
        # Compute principal stresses
        sigma_1, sigma_2, theta1, theta2 = compute_principal_stresses(sigma_xx, sigma_yy, sigma_xy)
        
        stress_data['von_mises'] = [e['von_mises'] for e in stress['elements']]
        stress_data['sigma_xx'] = sigma_xx
        stress_data['sigma_yy'] = sigma_yy
        stress_data['sigma_xy'] = sigma_xy
        stress_data['sigma_1'] = sigma_1
        stress_data['sigma_2'] = sigma_2
        stress_data['theta1'] = theta1
        stress_data['theta2'] = theta2

    # Compute nodally-averaged stresses for smooth contours
    node_stress_avg = compute_nodal_averaged_stresses(nodes, elements, stress['elements'] if stress else [])
    
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
    output['bounds'] = {
        'xmin': min(xs), 'xmax': max(xs),
        'ymin': min(ys), 'ymax': max(ys)
    }

    os.makedirs(os.path.dirname(outfile) if os.path.dirname(outfile) else '.', exist_ok=True)
    with open(outfile, 'w') as f:
        json.dump(output, f)

    print(f'  Converted: {os.path.basename(outfile)} ({len(nodes)} nodes, {len(elements)} elements)')
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
}


def main():
    parser = argparse.ArgumentParser(description='FEA-2D Web Data Converter')
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
