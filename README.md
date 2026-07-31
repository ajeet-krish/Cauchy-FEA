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
- **Desktop application**: Qt 6 GUI for mesh generation, BC assignment, solver execution, and interactive results visualization (optional build).

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
│   ├── fea_types.hpp
│   ├── fea.hpp
│   ├── elements.hpp
│   ├── sparse.hpp
│   ├── solver.hpp
│   ├── mesh.hpp
│   ├── postprocess.hpp
│   ├── adaptivity.hpp
│   ├── convergence.hpp
│   │
│   ├── main_cantilever.cpp
│   ├── main_michell.cpp
│   ├── main_cook.cpp
│   ├── main_lbracket.cpp
│   ├── main_patch.cpp
│   ├── main_plate_hole.cpp
│   ├── main_adapt_hole.cpp
│   ├── main_thermal.cpp
│   │
│   └── fea_test.cpp
│
├── src/desktop/                  # Qt 6 desktop application (optional)
│   ├── main.cpp
│   ├── cauchy_app.hpp/cpp
│   ├── main_window.hpp/cpp
│   ├── mesh_editor.hpp/cpp
│   ├── solver_panel.hpp/cpp
│   ├── viewport_widget.hpp/cpp
│   ├── result_model.hpp/cpp
│   ├── solver_runner.hpp/cpp
│   ├── project_io.hpp/cpp
│   ├── convergence_chart.hpp/cpp
│   ├── probe_tool.hpp/cpp
│   ├── mesh_quality_overlay.hpp/cpp
│   └── about_dialog.hpp/cpp
│
├── desktop/                      # Qt resources, .desktop file, Info.plist
│   ├── resources.qrc
│   ├── cauchy.desktop
│   └── Info.plist
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
- Qt 6 (optional, for desktop application: `cmake -DCAUCHY_DESKTOP=ON`)

## License

Portfolio project -- see repository for details.

## Desktop Application

Cauchy also ships as a native desktop FEA tool built with Qt 6. The desktop
application provides an interactive GUI for mesh generation, boundary condition
assignment, solver execution, and results visualization -- similar to commercial
FEA tools like Abaqus CAE or ANSYS Mechanical, but focused on 2D plane
stress/strain problems.

The portfolio website (`docs/`) remains the project deliverable for hiring
managers. The desktop application is the actual working tool used to run and
analyze simulations.

### Architecture

```
Cauchy Desktop (Qt 6)
├── Application Shell (QMainWindow)
│   ├── Menu bar (File, Edit, Solve, View, Help)
│   ├── Toolbar (solver controls, view toggles)
│   ├── Status bar (solver progress, mesh stats)
│   ├── Left dock: Mesh & BC editor
│   ├── Right dock: Properties & results
│   └── Central: 3D viewport (QOpenGLWidget / QWebEngineView)
│
├── Solver Bridge (native C++ link, no IPC)
│   ├── Mesh model (mirrors fea::Mesh)
│   ├── Material model (mirrors fea::Material)
│   ├── BC/Load model (QAbstractItemModel)
│   ├── Solver runner (QThread, async)
│   └── Result model (QAbstractItemModel for tables)
│
├── Visualization Engine
│   ├── 3D viewport (reuse Three.js via QWebEngineView, or native OpenGL)
│   ├── Contour mapping (same colormap JS logic)
│   ├── Mesh quality overlay
│   ├── Probe tool (click to read values)
│   └── Animation controller
│
├── File I/O
│   ├── Project file (.cauchy) -- JSON-based archive
│   ├── Mesh import (JSON, Gmsh .msh, VTK .vtu)
│   ├── Results export (JSON, CSV, VTU)
│   └── Screenshot/PNG export
│
└── Solver Backend (existing C++ headers, linked directly)
    ├── fea_types.hpp
    ├── elements.hpp
    ├── sparse.hpp
    ├── solver.hpp
    ├── mesh.hpp
    ├── postprocess.hpp
    ├── adaptivity.hpp
    └── fea.hpp
```

### Key Design Decision: Native Link

The solver is linked directly into the Qt application (not run as a subprocess).
This eliminates JSON serialization overhead for internal data transfer and
provides direct access to mesh/material/result data structures. The solver runs
in a `QThread` to keep the UI responsive.

### GUI Layout

```
+----------------------------------------------------------+
|  File  Edit  Solve  View  Help                           |  <- Menu bar
+------+-------------------------------------------+-------+
| Mesh |  Node: 297    Elem: 256    DOF: 594         | Props|
| Edit |  Material: Steel (E=200GPa, nu=0.3)       |      |
|      |                                           |      |
| BC   |  [Dirichlet]  Fixed left edge (x=0)       |      |
|      |  [Neumann]    Force right edge: -1000 N   |      |
|      |                                           |      |
| Mesh |  [Generate]  [Refine]  [Coarsen]          |      |
| Ops  |  [Import...]  [Export...]                 |      |
|      |                                           |      |
| Solve|  [Run Cholesky]  [Run CG]  [Convergence]  |      |
|      |  [Adaptive]  [Reset]                     |      |
+------+-------------------------------------------+-------+
|                                                          |
|              3D Viewport (Three.js / OpenGL)            |
|                                                          |
|  [Undeformed] [Deformed] [Edges] [Arrows] [Boundary]   |
|  Contour: [Von Mises v]  Scale: [====|====]  100x     |
|  [Animate 10s]  [Export PNG]                            |
+----------------------------------------------------------+
|  Status: Ready | Mesh: 297 nodes | Solver: Cholesky OK  |
+----------------------------------------------------------+
```

### Development Phases

| Phase | Duration | Deliverable |
|-------|----------|-------------|
| 1 | Week 1-2 | Native solver link + async execution + basic viewport |
| 2 | Week 3-4 | Full results visualization (contours, arrows, animation) |
| 3 | Week 5-6 | Mesh generation/editing + quality metrics |
| 4 | Week 7-8 | BC editor + material editor + project files |
| 5 | Week 9-10 | Convergence study + energy balance + export |
| 6 | Week 11-12 | Polish, packaging, CI for desktop, user guide |
| 7+ | Ongoing | 3D elements, nonlinear, thermal, scripting |

### Production-Grade Considerations

- **Error handling**: Solver result struct with success/error/warning fields; user-facing toast notifications, not console-only errors
- **Performance**: Async solver via QThread; progress signals every N elements assembled and every N CG iterations
- **Memory**: Streaming results to disk for large meshes (>1M elements)
- **Cross-platform**: Qt handles macOS, Linux, Windows from the same CMake tree
- **Packaging**: CPack for DEB/RPM/DMG/NSIS installers; CI builds all platforms
- **Extensibility**: Header-only solver backend allows plugin-style additions (new element types, material models)

### Comparison with Commercial Tools

| Capability | Cauchy Desktop | Abaqus CAE | ANSYS Mechanical | CalculiX + CGX |
|-----------|----------------|------------|------------------|-----------------|
| 2D elements | Q4, Q8, Bar | Full 3D | Full 3D | 2D + 3D |
| Static linear | Yes | Yes | Yes | Yes |
| Adaptive refinement | Yes (ZZ estimator) | Yes (h-adaptive) | Yes | No |
| GUI framework | Qt (custom) | Qt (proprietary) | Proprietary | GTK (CGX) |
| Solver | Cholesky + CG | Direct + Iterative | Direct + Iterative | Sparse direct |
| Cost | Free (MIT) | ~$20K/license | ~$50K/license | Free (GPL) |
| Transparency | Full source | Black box | Black box | Partial |

### Build (Desktop App)

```bash
# Requires Qt 6 installed
cmake -B build -DCAUCHY_DESKTOP=ON
cmake --build build -j$(sysctl -n hw.ncpu)
./build/cauchy-desktop
```

The desktop app is an optional build target. The default build (`cmake -B build`)
produces only the CLI solvers and tests.
