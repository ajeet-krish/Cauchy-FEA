# Cauchy: 2D Finite Element Structural Solver

A header-only C++20 2D finite element solver implementing bar and bilinear quad
(Q4) elements with COO/CSR sparse assembly, Cholesky direct and Conjugate
Gradient iterative solvers, and Von Mises stress recovery. Validated against
analytical solutions for cantilever beam, Michell truss, Cook's membrane,
L-bracket stress concentration, patch test, and plate with hole (Kirsch).

Built as a mechanical/aerospace portfolio piece demonstrating FEA competency
(C++20, sparse linear algebra, element formulation), structural mechanics
fundamentals (shape functions, Gauss quadrature, stress recovery), and
engineering communication skills (interactive web results with Three.js
visualization and per-case dedicated pages).

## Quick Start

```bash
cmake -B build && cmake --build build -j$(sysctl -n hw.ncpu)

# Validation cases
./build/FEA_Cantilever 32         # Cantilever beam (32x8 mesh)
./build/FEA_Michell              # Michell truss
./build/FEA_Cook 32              # Cook's membrane (32x32 mesh)
./build/FEA_LBracket 32          # L-bracket (32x32 mesh)
./build/FEA_Patch                # Patch test (4x4 mesh)
./build/FEA_PlateHole 16         # Plate with hole (16x16 mesh)

# Run with CG solver (auto-switches for large meshes)
./build/FEA_LBracket 64 --cg

# Run convergence study
./build/FEA_Cantilever 32 --convergence
./build/FEA_Cook 32 --convergence
./build/FEA_PlateHole 16 --convergence

# Run tests
./build/FEA_Tests

# Post-process (matplotlib PNGs)
python3 scripts/postprocess.py output/cantilever_32/ --all
python3 scripts/postprocess.py output/ --all-cases  # Generate PNGs for all cases

# Generate browser-ready visualization data
python3 scripts/convert_for_web.py output/cantilever_32/ docs/assets/data/cantilever_32.json

# Generate all browser data at once
python3 scripts/convert_for_web.py --all

# Generate thumbnails for landing page
python3 scripts/postprocess.py output/ --thumbnails

# Preview website
python3 -m http.server -d docs 8765
open http://localhost:8765
```

## Validation Coverage

| Case | Analytical Reference | Key Metric | What It Proves |
|------|---------------------|-----------|----------------|
| **Cantilever beam** | delta = PL^3/(3EI), sigma = My/I | Tip deflection, max stress | Basic correctness, element formulation |
| **Michell truss** | Published truss solutions | Nodal displacements | Assembly for bar elements |
| **Cook's membrane** | Tip displacement ~13.68 mm | Tip displacement | Q4 bending, shear locking test |
| **L-bracket** | Stress concentration factor ~2-3 | Max stress at fillet | Stress singularity, mesh refinement |
| **Patch test** | Exact for linear elements | Constant stress recovery | Element verification (mandatory) |
| **Plate with hole** | Kirsch: sigma_max = 3*sigma_inf | Max stress at hole edge | Stress concentration, symmetry |

## Key Features

- **Bar + Q4 elements**: Truss structures (bar) and 2D plane stress/strain (bilinear quad).
- **COO-to-CSR assembly**: Natural append-only COO for element assembly, compressed CSR for fast SpMV in solvers.
- **Dual solvers**: Cholesky direct (small/medium systems, verification) + Conjugate Gradient iterative (large sparse, scalability).
- **Plane stress + plane strain**: Configurable constitutive law via global flag.
- **Stress recovery**: Element-centered Q4 stress, node-averaged smooth contours, Von Mises and principal stresses.
- **Structured quad mesher**: Built-in mesher with x/y grading for mesh refinement toward edges.
- **JSON mesh input**: Hand-crafted node/element/boundary JSON for patch test and debugging.
- **OpenMP parallel**: Element assembly and stress recovery loops.
- **JSON output pipeline**: Per-case meta.json, displacement/stress fields, convergence data, mesh connectivity.
- **Interactive portfolio site**: Per-case pages with Three.js contour viewer, deformed shape, stress field, convergence plot.
- **Scientific colormaps**: Turbo, viridis, coolwarm for professional FEA visualization.
- **Mesh convergence studies**: h-refinement with GCI (Grid Convergence Index) on log-log scale.
- **Production-grade**: Google Test suite (19 tests), GitHub Actions CI on ubuntu + macos.

## Visualization Pipeline

The solver outputs JSON data that feeds both Python matplotlib postprocessing
and an interactive Three.js browser viewer:

```
C++ Solver
  |
  +--> output/case/meta.json           (mesh stats, material, max values)
  +--> output/case/displacement.json   (nodal ux, uy)
  +--> output/case/stress.json         (element sigma_xx, yy, xy, VM, principal)
  +--> output/case/mesh.json           (nodes, element connectivity)
  +--> output/case/convergence.json    (h-refinement study, GCI)
  |
  +--> scripts/postprocess.py          (matplotlib PNGs for README/docs)
  |      +--> displacement_contour.png
  |      +--> stress_contour.png
  |      +--> deformed_mesh.png
  |      +--> convergence.png
  |      +--> thumbnail_stress.png    (for landing page)
  |
  +--> scripts/convert_for_web.py      (browser-optimized JSON)
         +--> docs/assets/data/*.json  (combined mesh+results for Three.js)
```

### Browser Viewer Features

- **Three.js WebGL rendering** with GPU-accelerated contour plots
- **Deformed mesh overlay** with configurable displacement scale factor (1x-10000x)
- **Contour type selector**: Von Mises, sigma_xx, sigma_yy, sigma_xy, sigma_1, sigma_2, |u|, ux, uy
- **Scientific colormaps**: Turbo (magnitude), RdBu_r (signed), Viridis (stress)
- **Color legend** at bottom with min/max values and units
- **Interactive controls**: OrbitControls for pan, zoom, 3D rotation
- **Principal stress vectors**: Element-based arrow visualization showing sigma_1 and sigma_2 directions
- **Boundary condition symbols**: Triangles (fixed), arrows (forces), circles (rollers)
- **10-second deformation animation**: Smooth ease-in-out for clear visualization of load effects
- **PNG export**: High-resolution 1920x1080 screenshots of current view
- **Responsive canvas** that adapts to container size
- **Static PNG fallback**: Images load before WebGL initializes

### Viewer Controls

| Control | Function |
|---------|----------|
| **Contour** | Select field to visualize (Von Mises, sigma_xx, sigma_1, etc.) |
| **Scale** | Adjust displacement scale (1x to 10000x) |
| **Undeformed** | Toggle undeformed wireframe overlay |
| **Deformed** | Toggle deformed mesh |
| **Edges** | Toggle element edge rendering |
| **Stress Arrows** | Toggle principal stress vectors |
| **Boundary** | Toggle boundary condition symbols |
| **Animate** | Play 10-second deformation animation |
| **Export PNG** | Save current view as 1920x1080 image |

## Directory Structure

```
fea-2d/
├── README.md
├── AGENTS.md
├── CMakeLists.txt
├── .gitignore
├── .github/workflows/ci.yml
│
├── src/
│   ├── fea_types.hpp              # Core types, enums, global config
│   ├── fea.hpp                    # Solver core (assemble, solve, post-process)
│   ├── elements.hpp               # Element stiffness matrices (Bar + Q4)
│   ├── sparse.hpp                 # COO/CSR sparse matrix
│   ├── solver.hpp                 # Cholesky + Conjugate Gradient
│   ├── postprocess.hpp            # Stress recovery, Von Mises, JSON output
│   ├── mesh.hpp                   # Structured quad mesher + JSON input
│   ├── convergence.hpp            # GCI computation, h-refinement wrapper
│   │
│   ├── main_cantilever.cpp        # Case 1: Cantilever beam
│   ├── main_michell.cpp           # Case 2: Michell truss
│   ├── main_cook.cpp              # Case 3: Cook's membrane
│   ├── main_lbracket.cpp          # Case 4: L-bracket stress concentration
│   ├── main_patch.cpp             # Case 5: Patch test
│   ├── main_plate_hole.cpp        # Case 6: Plate with hole
│   │
│   └── fea_test.cpp               # Google Test suite (19 tests)
│
├── scripts/
│   ├── postprocess.py             # JSON -> matplotlib PNG contour plots
│   ├── convert_for_web.py         # JSON -> browser-optimized Three.js data
│   └── run_all_sims.py            # Batch runner for all 6 cases
│
├── docs/
│   ├── index.html                 # Landing page with case thumbnails
│   ├── cantilever.html            # Case pages with interactive viewers
│   ├── cook.html
│   ├── lbracket.html
│   ├── patch.html
│   ├── plate_hole.html
│   ├── michell.html
│   ├── theory.html                # FEA theory (weak form, shape functions, GCI)
│   ├── implementation.html        # Code architecture
│   ├── css/style.css              # Dark terminal theme
│   └── assets/
│       ├── js/
│       │   ├── fea-viewer.js      # Main Three.js viewer (scene, rendering)
│       │   ├── fea-mesh.js        # BufferGeometry generation
│       │   ├── fea-contours.js    # Vertex coloring with colormaps
│       │   ├── fea-controls.js    # OrbitControls + UI toolbar
│       │   ├── fea-colorbar.js    # Colorbar component (bottom)
│       │   ├── fea-arrows.js      # Principal stress vectors
│       │   ├── fea-boundary.js    # Boundary condition visualization
│       │   ├── fea-animation.js   # 10-second deformation animation
│       │   ├── colormaps.js       # Scientific colormap functions
│       │   └── convergence-chart.js  # Canvas convergence plot
│       ├── data/
│       │   ├── cantilever_32.json  # Pre-processed mesh + results
│       │   ├── cook_32.json
│       │   ├── lbracket.json
│       │   ├── patch.json
│       │   ├── plate_hole.json
│       │   └── michell.json
│       └── images/                # Generated PNG plots
│
├── output/                        # Simulation output (gitignored)
└── build/                         # CMake build (gitignored)
```

## Build Requirements

- C++20 compiler (GCC 10+, Clang 12+, Apple Clang 14+)
- CMake 3.15+
- OpenMP (optional, for parallel assembly)
- Google Test (fetched via CMake FetchContent)
- Python 3.8+ (for postprocessing scripts)
- NumPy + Matplotlib (for Python postprocessor, optional)

## License

Portfolio project -- see repository for details.
