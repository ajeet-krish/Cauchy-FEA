#pragma once

// ==========================================================================
// C FFI LAYER FOR CAUCHY FEA SOLVER
// Provides extern "C" functions callable from Rust, Python, or any FFI
// capable language. This is the primary integration point for the Tauri
// desktop app.
// ==========================================================================

#ifdef __cplusplus
extern "C" {
#endif

// ------------------------------------------------------------------
// Generate a structured quad mesh from geometry shapes
// ------------------------------------------------------------------
// shapes_json: JSON array of shape primitives for domain definition
//   {"type":"rectangle", "x":0.0, "y":0.0, "width":1.0, "height":0.5}
//   {"type":"circle", "cx":0.5, "cy":0.5, "radius":0.25}
//   {"type":"polygon", "points":[[x1,y1],[x2,y2],...]}
//   {"type":"ibeam", "x":0.0, "y":0.0, "width":1.0, "height":1.0,
//    "flange":0.1, "web":0.05}
//   {"type":"lbracket", "x":0.0, "y":0.0, "width":1.0, "height":1.0,
//    "flange":0.1, "web":0.05}
// nx, ny: mesh density (number of elements in each direction)
// elem_type: 0=Q4, 1=Q8, 2=T3
// output_dir: directory to write mesh.json
// Returns 0 on success, non-zero on error
int fea_generate_mesh_c(
    const char* shapes_json,
    int nx, int ny,
    int elem_type,
    const char* output_dir
);

// ------------------------------------------------------------------
// Run FEA solve with given configuration
// ------------------------------------------------------------------
// mesh_json: full mesh data (nodes, connectivity, BCs, material)
//   See mesh.json output format for the expected structure.
// config_json: solver configuration
//   {
//     "plane": "stress" | "strain",
//     "solver": "cholesky" | "cg",
//     "cg_tolerance": 1e-10,
//     "cg_max_iterations": 10000,
//     "integration": "full" | "sri" | "bbar",
//     "material": {"E": 200e9, "nu": 0.3, "rho": 7800, "t": 0.01}
//   }
// output_dir: directory to write results (displacement.json, stress.json,
//   meta.json)
// Returns 0 on success, non-zero on error
int fea_solve_c(
    const char* mesh_json,
    const char* config_json,
    const char* output_dir
);

#ifdef __cplusplus
}
#endif
