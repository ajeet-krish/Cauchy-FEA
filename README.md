# Cauchy: 2D Finite Element Structural Solver

A header-only C++20 2D finite element solver implementing bar, bilinear quad (Q4),
and serendipity quad (Q8) elements with COO/CSR sparse assembly, Cholesky direct
and Conjugate Gradient iterative solvers, Von Mises stress recovery, and ZZ error
estimator-driven adaptive h-refinement. Validated against 6 analytical benchmarks
with 22 Google Test cases.

Built as a mechanical/aerospace portfolio piece demonstrating FEA competency
(C++20, sparse linear algebra, element formulation), structural mechanics
fundamentals (shape functions, Gauss quadrature, stress recovery, adaptive
mesh refinement), and engineering communication skills (interactive web results
with Three.js visualization and per-case dedicated pages).

## Quick Start

```bash
cmake -B build && cmake --build build -j$(sysctl -n hw.ncpu)

# Validation cases
./build/FEA_Cantilever 32         # Cantilever beam (32x8 mesh)
./build/FEA_Cantilever 32 --q8    # Cantilever with Q8 elements
./build/FEA_Michell               # Michell truss
./build/FEA_Cook 32               # Cook's membrane (32x32 mesh)
./build/FEA_Cook 32 --q8          # Cook's with Q8 elements
./build/FEA_LBracket 32           # L-bracket (32x32 mesh)
./build/FEA_Patch                 # Patch test (4x4 mesh)
./build/FEA_PlateHole 16          # Plate with hole (16x16 mesh)
./build/FEA_PlateHole 16 --q8     # Plate with hole, Q8 elements

# Adaptive refinement
./build/FEA_AdaptHole 8 --iters 4 # Adaptive refinement on plate-with-hole

# Run with CG solver (auto-switches for large meshes)
./build/FEA_LBracket 64 --cg

# Run convergence study
./build/FEA_Cantilever 32 --convergence
./build/FEA_Cook 32 --convergence
./build/FEA_PlateHole 16 --convergence

# Run tests (22 tests)
./build/FEA_Tests

# Post-process (matplotlib PNGs)
python3 scripts/postprocess.py output/cantilever_32/ --all
python3 scripts/postprocess.py output/ --all-cases

# Preview website
python3 -m http.server -d docs 8765
open http://localhost:8765
```

## Validation Coverage

| Case | Analytical Reference | Key Metric | What It Proves |
|------|---------------------|-----------|----------------|
| **Cantilever beam** | delta = PL^3/(3EI), Timoshenko | Tip deflection, max stress | Q4 shear locking, Q8 quadratic completeness |
| **Michell truss** | Published truss solutions | Nodal displacements | Assembly for bar elements |
| **Cook's membrane** | Tip displacement ~13.68 mm | Tip displacement | Bending + shear, Q4 vs Q8 comparison |
| **L-bracket** | Stress concentration factor ~2-3 | Max stress at fillet | Stress singularity, mesh refinement |
| **Patch test** | Exact for linear elements | Constant stress recovery | Element verification (mandatory for Q4 + Q8) |
| **Plate with hole** | Kirsch: sigma_max = 3*sigma_inf | Max stress at hole edge | Stress concentration, adaptive refinement |

## Key Features

- **Bar + Q4 + Q8 elements**: Truss structures (bar), 2D plane stress/strain (bilinear quad), and bending-optimized serendipity quad (8-node, 3x3 Gauss).
- **COO-to-CSR assembly**: Natural append-only COO for element assembly, compressed CSR for fast SpMV in solvers.
- **Dual solvers**: Cholesky direct (small/medium systems, verification) + Conjugate Gradient iterative (large sparse, scalability).
- **Plane stress + plane strain**: Configurable constitutive law via global flag.
- **Stress recovery**: Element-centered Q4/Q8 stress, node-averaged smooth contours, Von Mises and principal stresses. Q8 uses 2x2 Gauss averaging.
- **ZZ error estimator**: Zienkiewicz-Zhu superconvergent patch recovery (SPR) for a posteriori error estimation.
- **Adaptive h-refinement**: Red-green refinement with "largest first" marking strategy, concentrating DOFs where error is highest.
- **Structured quad mesher**: Built-in mesher with x/y grading for mesh refinement toward edges. Q8 mesh generators for rectangular and trapezoidal domains.
- **JSON mesh input**: Hand-crafted node/element/boundary JSON for patch test and debugging.
- **OpenMP parallel**: Element assembly and stress recovery loops.
- **JSON output pipeline**: Per-case meta.json, displacement/stress fields, convergence data, mesh connectivity, adaptive convergence data.
- **Interactive portfolio site**: Per-case pages with Three.js contour viewer, deformed shape, stress field, convergence plot.
- **Scientific colormaps**: Turbo, viridis, RdBu_r for professional FEA visualization.
- **Mesh convergence studies**: h-refinement with GCI (Grid Convergence Index) on log-log scale.
- **Production-grade**: Google Test suite (22 tests), GitHub Actions CI on ubuntu + macos.

## Element Formulation

### Q4 Bilinear Quad (4 nodes, 2x2 Gauss)
- Shape functions: N_i = (1/4)(1 + xi_i*xi)(1 + eta_i*eta)
- Suitable for general 2D solid mechanics
- Exhibits shear locking in bending (documented limitation)

### Q8 Serendipity Quad (8 nodes, 3x3 Gauss)
- Shape functions: quadratic with corner + midside nodes
- Eliminates shear locking in bending-dominated problems
- Converges to Timoshenko beam solution (not stiffer Euler-Bernoulli)
- Node ordering: corners 0-3 (CCW), horizontal midside 4-7, vertical midside 8-11

## Visualization Pipeline

```
C++ Solver
  |
  +--> output/case/meta.json           (mesh stats, material, max values)
  +--> output/case/displacement.json   (nodal ux, uy)
  +--> output/case/stress.json         (element sigma_xx, yy, xy, VM, principal)
  +--> output/case/mesh.json           (nodes, element connectivity)
  +--> output/case/convergence.json    (h-refinement study, GCI)
  +--> output/case/adaptive_convergence.json  (adaptive refinement history)
  |
  +--> scripts/postprocess.py          (matplotlib PNGs for README/docs)
  |
  +--> scripts/convert_for_web.py      (browser-optimized JSON)
         +--> docs/assets/data/*.json  (combined mesh+results for Three.js)
```

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
│   ├── elements.hpp               # Element stiffness (Bar + Q4 + Q8 + T3)
│   ├── sparse.hpp                 # COO/CSR sparse matrix
│   ├── solver.hpp                 # Cholesky + Conjugate Gradient
│   ├── postprocess.hpp            # Stress recovery, Von Mises, JSON output
│   ├── mesh.hpp                   # Structured quad mesher + Q8 generators
│   ├── adaptivity.hpp             # ZZ SPR, error indicators, red-green refinement
│   ├── convergence.hpp            # GCI computation, h-refinement wrapper
│   │
│   ├── main_cantilever.cpp        # Case 1: Cantilever beam (--q8, --convergence)
│   ├── main_michell.cpp           # Case 2: Michell truss
│   ├── main_cook.cpp              # Case 3: Cook's membrane (--q8, --convergence)
│   ├── main_lbracket.cpp          # Case 4: L-bracket stress concentration
│   ├── main_patch.cpp             # Case 5: Patch test
│   ├── main_plate_hole.cpp        # Case 6: Plate with hole (--q8, --convergence)
│   ├── main_adapt_hole.cpp        # Adaptive refinement on plate-with-hole
│   │
│   └── fea_test.cpp               # Google Test suite (22 tests)
│
├── scripts/
│   ├── postprocess.py             # JSON -> matplotlib PNG contour plots
│   ├── convert_for_web.py         # JSON -> browser-optimized Three.js data
│   └── run_all_sims.py            # Batch runner for all cases
│
├── docs/
│   ├── index.html                 # Landing page with case thumbnails
│   ├── cantilever.html            # Case pages with interactive viewers
│   ├── cook.html
│   ├── lbracket.html
│   ├── patch.html
│   ├── plate_hole.html
│   ├── michell.html
│   ├── thermal_cylinder.html
│   ├── pinn.html
│   ├── theory.html                # FEA theory (Q4 + Q8 shape functions, ZZ estimator)
│   ├── implementation.html        # Code architecture
│   ├── css/style.css              # Dark terminal theme
│   └── assets/
│       ├── js/                    # Three.js viewer components
│       ├── data/                  # Pre-processed browser JSON (gitignored)
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
