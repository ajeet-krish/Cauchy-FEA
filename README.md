# Cauchy: 2D Finite Element Structural Solver

A header-only C++20 2D finite element solver implementing bar and bilinear quad
(Q4) elements with COO/CSR sparse assembly, Cholesky direct and Conjugate
Gradient iterative solvers, and Von Mises stress recovery. Validated against
analytical solutions for cantilever beam, Michell truss, Cook's membrane,
L-bracket stress concentration, patch test, and plate with hole (Kirsch).

Built as a mechanical/aerospace portfolio piece demonstrating FEA competency
(C++20, sparse linear algebra, element formulation), structural mechanics
fundamentals (shape functions, Gauss quadrature, stress recovery), and
engineering communication skills (interactive web results with per-case
dedicated pages and comparison sliders).

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

# Run tests
./build/FEA_Tests

# Post-process
python3 scripts/postprocess.py output/cantilever/ --all

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
- **JSON output pipeline**: Per-case meta.json, displacement/stress fields, convergence data.
- **Interactive portfolio site**: Per-case pages with mesh viewer, deformed shape, stress contour, convergence plot.
- **Mesh convergence studies**: h-refinement with GCI (Grid Convergence Index) on log-log scale.
- **Production-grade**: Google Test suite (15+ tests), GitHub Actions CI on ubuntu + macos.

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
│   ├── postprocess.hpp            # Stress recovery, Von Mises, contour data
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
│   └── fea_test.cpp               # Google Test suite
│
├── scripts/
│   ├── postprocess.py             # JSON -> matplotlib PNG
│   └── run_all_sims.py            # Batch runner
│
├── docs/
│   ├── index.html                 # Landing page
│   ├── cantilever.html            # Case pages
│   ├── cook.html
│   ├── lbracket.html
│   ├── plate_hole.html
│   ├── theory.html                # FEA theory (weak form, shape functions)
│   ├── implementation.html        # Code architecture
│   ├── css/style.css              # Dark theme
│   └── assets/
│       ├── js/                    # mesh-viewer.js, contour-viewer.js
│       ├── images/                # Contour/deformed renders
│       └── data/                  # Simulation JSON data
│
├── output/                        # Simulation output (gitignored)
└── build/                         # CMake build (gitignored)
```

## Build Requirements

- C++20 compiler (GCC 10+, Clang 12+, Apple Clang 14+)
- CMake 3.15+
- OpenMP (optional, for parallel assembly)
- Google Test (fetched via CMake FetchContent)

## License

Portfolio project -- see repository for details.
