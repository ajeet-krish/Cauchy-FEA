# Project Context, Cauchy: 2D Finite Element Structural Solver

## Style Rules
- **No em dashes** in any file. Use two regular hyphens (--) instead of ---, &mdash;, &ndash;, or literal Unicode em dash.
- **C++ code style**: 4-space indentation, K&R braces, no tabs, no trailing whitespace.
- **Variable naming**: snake_case for local variables, PascalCase for structs/classes, SCREAMING_SNAKE_CASE for constants.
- **Global variables**: `g_` prefix for inline globals (e.g., `g_case`, `g_analysis`, `g_nx`).
- **HTML/CSS**: Double quotes for attributes, 2-space indentation, semantic HTML5 elements.
- **Header-only pattern**: All core logic in `.hpp` files with `#pragma once`. Use `inline` for functions and variables to avoid ODR violations.
- **Enum classes**: All enums use `enum class` with PascalCase values.
- **JavaScript**: ES6+ modules where supported, 2-space indentation, single quotes for strings, semicolons required.
- **No em dashes in JS/HTML**: Use `--` or `&mdash;` entity.

## Goal
Build and deploy a 2D finite element structural solver in C++20 as a portfolio centrepiece for mechanical/aerospace engineering roles. Deliver a 6+ page HTML portfolio with per-case dedicated pages (interactive Three.js viewers, KaTeX theory, validation tables), and a production-grade GitHub repository with CI and unit tests.

## Target Audience
Mechanical/aerospace hiring managers at SpaceX, Lockheed Martin, Northrop Grumman, Boeing, and similar. The site must communicate: FEA competency (element formulation, sparse assembly, solver implementation), structural mechanics fundamentals (shape functions, Gauss quadrature, stress recovery), and engineering communication skills (interactive web presentation, per-case analysis narratives).

## Current Status

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Project setup (CMake, types, sparse matrix), bar element | Complete |
| 2 | Q4 element, assembly, Cholesky solver | Complete |
| 3 | CG solver, BC enforcement, all 6 validation cases | Complete |
| 4 | Python postprocessor, JSON output, mesh convergence | Complete |
| 5 | Q8 serendipity element (3x3 Gauss, 16x16 stiffness) | Complete |
| 6 | ZZ error estimator + adaptive h-refinement | Complete |
| 7 | Website update (Q8 comparison, adaptive refinement) | Complete |

## Architecture Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Element types | Bar + Q4 + Q8 | Bar for truss, Q4 for standard 2D solid, Q8 for bending-dominated problems |
| Assembly | COO -> CSR | COO natural for assembly (append-only), CSR for fast SpMV |
| Solver | Cholesky (small) + CG (large) | Cholesky for verification, CG for scalability |
| BC enforcement | Penalty method | Simple, no matrix restructuring |
| Integration | 2x2 Gauss for Q4, 3x3 for Q8 | Full integration (document shear locking as limitation) |
| Plane stress vs strain | Both via config | Plane stress for thin plates, plane strain for thick sections |
| Mesh input | Built-in structured + JSON | No external dependencies, sufficient for all 6 cases |
| Output | JSON like LBM-2D | Reuse Python postprocessor pattern |
| Browser viewer | Three.js (WebGL) | GPU-accelerated, future-proof for 3D solver upgrades |
| Data format | Separate JSON files | Clean separation, cached independently, GitHub Pages compatible |
| Colormaps | Scientific (turbo, viridis, RdBu_r) | Industry standard, not rainbow |
| Testing | Google Test, same CI pattern | Consistency with LBM-2D |
| Principal stress arrows | Element-based (centroid) | Standard in FEA post-processors (Abaqus, ANSYS) |
| Boundary symbols | Triangles (fixed), arrows (forces), circles (rollers) | Clear, standard notation |
| Animation duration | 10 seconds | Allows reader to fully understand deformation effects |
| Export resolution | 1920x1080 | Full HD for portfolio presentation |
| Colorbar position | Bottom | Does not obscure mesh, clear min/max labels |

## Verification Checklist

Before declaring the solver "done," each must pass:
- [x] Patch test: constant stress exactly recovered for any mesh
- [x] Cantilever tip deflection matches PL^3/(3EI) within 1% (32x8 mesh)
- [x] Cantilever max stress matches My/I within 5%
- [x] Cook's membrane tip displacement matches ~13.68 mm (plane stress)
- [x] Plate with hole max stress matches 3*sigma_inf at hole edge (within 10%)
- [x] Energy balance: 0.5 * u^T * K * u == 0.5 * u^T * f (exact for linear)
- [x] CG residual drops below 1e-10 for all cases
- [x] Cholesky and CG produce identical results (within solver tolerance)
- [x] Negative Jacobian detection triggers on invalid elements
- [x] All 22 Google Test cases pass
- [x] CI builds and tests on Ubuntu + macOS
- [x] All 6 cases have static PNG visualizations
- [x] All cases with analytical references have convergence data
- [x] Three.js viewer renders at 60 FPS for typical meshes
- [x] Principal stress vectors visible at element centroids
- [x] Boundary condition symbols clear (triangles, arrows, circles)
- [x] 10-second deformation animation plays smoothly
- [x] Export PNG produces 1920x1080 images
- [x] Colorbar positioned at bottom with correct min/max
- [x] Landing page shows case thumbnails
- [x] Theory page includes all equations (strain-displacement, principal stresses, GCI, Q8 shape functions, ZZ estimator)

## Limitations to Document

1. **Shear locking**: Standard Q4 with full integration locks in bending. Document and show mesh refinement mitigates it.
2. **2D only**: Plane stress/strain. No 3D elements.
3. **Linear elastic only**: No plasticity, no geometric nonlinearity.
4. **No dynamic analysis**: Static loading only (future: implicit/explicit dynamics).

## Code Conventions

### Include Hierarchy
```
fea.hpp
├── fea_types.hpp    (types, enums, globals)
├── elements.hpp     (bar + Q4 + Q8 stiffness matrices)
├── sparse.hpp       (COO/CSR sparse matrix)
├── solver.hpp       (Cholesky + CG)
├── mesh.hpp         (structured quad mesher + Q8 generators)
├── postprocess.hpp  (stress recovery, Von Mises, mesh JSON export)
└── adaptivity.hpp   (ZZ SPR, error indicators, red-green refinement)
```

### Entry Point Pattern (matching LBM-2D)
Each `main_*.cpp` follows:
1. Set globals (`g_nx`, `g_ny`, `g_case`, `g_analysis`)
2. Generate or load mesh
3. Assemble global stiffness matrix
4. Apply boundary conditions
5. Solve (Cholesky or CG)
6. Post-process (stress recovery, output)
7. Report statistics

### Adaptive Refinement Entry Point
`main_adapt_hole.cpp` follows:
1. Set globals (`g_case = PLATE_HOLE`)
2. Generate initial Q4 mesh (baseline uniform)
3. Run uniform solve, write output, compute SCF
4. For each adaptive iteration:
   - Assemble, solve, compute stresses
   - Run ZZ SPR recovery + error indicators
   - Mark elements (largest first, theta = 0.5)
   - Red-green refine (split marked elements)
   - Re-run hole cut to remove elements inside circular hole
   - Write adaptive_convergence.json
5. Report convergence statistics

### JSON Output Format
- `meta.json`: mesh dimensions, material props, max displacement, max stress
- `displacement.json`: nodal displacement field (u_x, u_y per node)
- `stress.json`: element-centered stress field (sigma_xx, sigma_yy, sigma_xy, von_mises, sigma_1, sigma_2)
- `mesh.json`: node coordinates and element connectivity (for browser viewer)
- `convergence.json`: mesh convergence data (GCI, order of convergence)

### Browser Visualization Pipeline
1. C++ solver writes `mesh.json` with node coords + element connectivity
2. `scripts/convert_for_web.py` combines mesh + displacement + stress into single browser-optimized JSON
3. `docs/assets/js/fea-viewer.js` loads JSON and renders via Three.js WebGL
4. Each case page embeds viewer with `<div id="fea-viewer">` and initializes with case data

### Enhanced Browser Data Format
```json
{
  "meta": {
    "case": "cantilever",
    "nodes": 297,
    "elements": 256,
    "dof": 594,
    "maxDisplacement": 0.000137,
    "maxStress": 8110000
  },
  "nodes": [{"x": 0.0, "y": 0.0, "z": 0.0}, ...],
  "elements": [{"n0": 0, "n1": 1, "n2": 17, "n3": 16}, ...],
  "displacement": [{"ux": 0.0, "uy": 0.0}, ...],
  "stress": {
    "von_mises": [...],
    "sigma_xx": [...],
    "sigma_yy": [...],
    "sigma_xy": [...],
    "sigma_1": [...],
    "sigma_2": [...],
    "theta1": [...],
    "theta2": [...]
  },
  "nodalStress": {
    "von_mises": [...],
    "sigma_1": [...],
    "sigma_2": [...]
  },
  "boundary": {
    "dirichlet": [{"node": 0, "dof": 0, "value": 0.0}, ...],
    "neumann": [{"node": 100, "dof": 1, "value": -1000.0}, ...]
  },
  "camera": {
    "position": [0.5, 0.5, 2.0],
    "target": [0.5, 0.5, 0.0]
  }
}
```

### JavaScript Conventions
- Use `const`/`let`, never `var`
- Class-based organization for viewer components
- ES6 template literals for HTML generation
- `requestAnimationFrame` for render loops
- No external dependencies except Three.js (loaded from CDN)
- All viewer code in `docs/assets/js/` -- no build step required

### JavaScript File Structure
```
docs/assets/js/
├── fea-viewer.js          # Main viewer class (Three.js scene setup)
├── fea-mesh.js            # BufferGeometry generation from JSON
├── fea-contours.js        # Vertex coloring with scientific colormaps
├── fea-controls.js        # OrbitControls + UI toolbar
├── fea-colorbar.js        # Colorbar component at bottom
├── fea-arrows.js          # Principal stress vectors (element-based)
├── fea-boundary.js        # Boundary condition symbols
├── fea-animation.js       # 10-second deformation animation
├── colormaps.js           # Scientific colormap functions (turbo, viridis, RdBu_r)
└── convergence-chart.js   # Canvas convergence plot
```

### Principal Stress Arrow Implementation
- Element-based (centroid) approach: one arrow pair per element
- Red arrows = tension (sigma > 0), Blue arrows = compression (sigma < 0)
- Arrow length proportional to stress magnitude
- Toggle on/off via UI control

### Boundary Condition Symbol Implementation
- **Fixed supports**: Yellow triangles pointing down
- **Forces**: Green/red arrows (positive/negative direction)
- **Rollers**: Yellow circles
- Grouped by node to avoid duplicate symbols

### Animation Implementation
- 10-second duration for clear visualization
- Smooth ease-in-out cubic function
- Progress bar showing animation position
- Play/pause/reset controls
- Updates displacement scale in real-time

### Python Script Conventions
- Use `numpy` for array operations
- `matplotlib` with `Agg` backend for headless rendering
- Scientific colormaps: `turbo`, `viridis`, `RdBu_r` (never `jet` for new code)
- Command-line interface via `argparse`
- Output PNGs at 150 DPI with `bbox_inches='tight'`

### Postprocessing Script Flags
- `--all`: Generate all plot types for a single case
- `--all-cases`: Generate PNGs for all 6 cases
- `--thumbnails`: Generate small thumbnails for landing page
- `--case NAME`: Specify individual case to process
- `--displacement`: Generate displacement contour only
- `--stress`: Generate stress contour only
- `--deformed`: Generate deformed mesh only
- `--convergence`: Generate convergence plot only
