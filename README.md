# Crucible-FEA: 2D/3D Finite Element Structural Solver with Desktop Application

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg?logo=cplusplus&logoColor=white)](https://en.cppreference.com/w/cpp/20)
[![Platform](https://img.shields.io/badge/Platform-macOS%20%7C%20Linux-lightgrey.svg)]()
[![Tests](https://img.shields.io/badge/Tests-86-brightgreen.svg)]()

A C++20 finite element solver with a native Qt 6 desktop application for interactive structural analysis. Solves plane stress/strain and 3D solid mechanics problems with adaptive mesh refinement, geometric nonlinearity, transient dynamics, and contact mechanics. Validated against 10 analytical benchmarks.

**[View Portfolio Site](https://ajeet-krish.github.io/Crucible-FEA/)** | **[Download Desktop App](#build--install)** | **[GitHub](https://github.com/ajeet-krish/Crucible-FEA)**

---

## Demo

![Crucible-FEA Desktop Application Demo](docs/assets/images/desktop_app_demo.gif)

*Crucible-FEA Desktop: mesh generation, boundary condition assignment, solver execution, and stress visualization in a native Qt 6 application.*

<table>
  <tr>
    <td align="center">
      <img src="docs/assets/images/desktop_screenshots/mesh_editor.png" alt="Mesh Editor" width="400">
      <br><em>Interactive mesh editor with nx/ny controls and quality metrics</em>
    </td>
    <td align="center">
      <img src="docs/assets/images/desktop_screenshots/bc_editor.png" alt="Boundary Condition Editor" width="400">
      <br><em>Boundary condition assignment with visual feedback</em>
    </td>
    <td align="center">
      <img src="docs/assets/images/desktop_screenshots/stress_contour.png" alt="Stress Contour" width="400">
      <br><em>Von Mises stress contour with scientific colormap</em>
    </td>
  </tr>
  <tr>
    <td align="center">
      <img src="docs/assets/images/desktop_screenshots/principal_arrows.png" alt="Principal Stress Arrows" width="400">
      <br><em>Principal stress arrows at element centroids</em>
    </td>
    <td align="center">
      <img src="docs/assets/images/desktop_screenshots/analysis_plots.png" alt="Analysis Plots" width="400">
      <br><em>Six analysis tabs: stress, energy, displacement, error, convergence</em>
    </td>
    <td align="center">
      <img src="docs/assets/images/desktop_screenshots/mesh_quality.png" alt="Mesh Quality Overlay" width="400">
      <br><em>Mesh quality heatmap overlay for pre-solve validation</em>
    </td>
  </tr>
  <tr>
    <td align="center">
      <img src="docs/assets/images/desktop_screenshots/probe_tool.png" alt="Probe Tool" width="400">
      <br><em>Click-to-probe stress and displacement at any point (I key)</em>
    </td>
    <td align="center">
      <img src="docs/assets/images/desktop_screenshots/deformation_animation.gif" alt="Deformation Animation" width="400">
      <br><em>10-second smooth deformation animation with play/pause/reset</em>
    </td>
    <td align="center">
      <img src="docs/assets/images/desktop_screenshots/material_library.png" alt="Material Library" width="400">
      <br><em>Material preset dropdown: Steel, Aluminum, Titanium, Copper, Concrete</em>
    </td>
  </tr>
  <tr>
    <td align="center">
      <img src="docs/assets/images/desktop_screenshots/analytical_comparison.png" alt="Analytical Comparison" width="400">
      <br><em>Expected vs. computed values with color-coded pass/fail</em>
    </td>
    <td align="center">
      <img src="docs/assets/images/desktop_screenshots/element_inspector.png" alt="Element Inspector" width="400">
      <br><em>Double-click to inspect stress tensor, strain, and quality metrics</em>
    </td>
    <td align="center">
      <img src="docs/assets/images/desktop_screenshots/stress_streamlines.png" alt="Stress Streamlines" width="400">
      <br><em>Principal stress direction tracing (red=tension, blue=compression)</em>
    </td>
  </tr>
  <tr>
    <td align="center">
      <img src="docs/assets/images/desktop_screenshots/mode_shape_animator.gif" alt="Mode Shape Animator" width="400">
      <br><em>Async modal analysis with sinusoidal oscillation at natural frequency</em>
    </td>
    <td align="center">
      <img src="docs/assets/images/desktop_screenshots/parametric_study.png" alt="Parametric Study" width="400">
      <br><em>Real-time sliders for E, nu, force, thickness with fast rescaling</em>
    </td>
    <td align="center">
      <img src="docs/assets/images/desktop_screenshots/project_save_load.png" alt="Project Save/Load" width="400">
      <br><em>Full v2 JSON format with Ctrl+S quick-save</em>
    </td>
  </tr>
</table>

*Desktop application panels: mesh editor, boundary conditions, stress contour, principal stress arrows, analysis plots, mesh quality overlay, probe tool, deformation animation, material library, analytical comparison, element inspector, stress streamlines, mode shape animator, parametric study, and project save/load.*

---

## Why Crucible-FEA

Most FEA projects stop at a single element type or a handful of validation cases. Crucible-FEA covers the **full structural analysis pipeline** that practicing engineers use daily:

1. **Generate** structured meshes with automatic grading and quality metrics
2. **Assign** boundary conditions through an interactive editor with visual feedback
3. **Solve** static, dynamic, nonlinear, and contact problems with verified element formulations
4. **Visualize** stress contours, principal stress arrows, deformed shapes, and mesh quality overlays
5. **Analyze** convergence, energy balance, and error distributions through six built-in plot modules
6. **Probe** any point in the mesh for exact stress and displacement values
7. **Animate** deformation and mode shapes with smooth, configurable playback
8. **Compare** FEA results against analytical solutions in real time

---

## Key Capabilities

| Category | Details |
|----------|---------|
| **Elements** | Bar, Q4 (bilinear quad), Q8 (serendipity quad), T3 (linear triangle), H8 (hexahedron), T4 (tetrahedron) |
| **Solvers** | Cholesky direct + Conjugate Gradient iterative |
| **Preconditioners** | Jacobi, SSOR, Incomplete Cholesky IC(0), Block Jacobi (Additive Schwarz) |
| **Locking Mitigation** | Selective Reduced Integration (SRI), B-Bar method for Q4 bending |
| **Adaptive Refinement** | ZZ error estimator (SPR) + red-green h-refinement with GCI convergence tracking |
| **Geometric Nonlinearity** | Total Lagrangian Newton-Raphson with Green-Lagrange strain |
| **Dynamic Analysis** | Newmark-beta time integration + modal analysis (eigenvalue solver) |
| **Contact Mechanics** | Node-to-surface frictionless penalty method |
| **Thermal Loading** | Steady-state thermoelastic analysis with temperature fields |
| **Assembly** | COO natural assembly (append-only) compressed to CSR for fast SpMV |
| **Parallelization** | OpenMP for element assembly and stress recovery |
| **Material Library** | Steel, Aluminum, Titanium, Copper, Concrete presets with custom option |
| **Probe Tool** | Click-to-query stress/displacement at any mesh point |
| **Stress Streamlines** | Principal stress direction tracing with RK4 integration |
| **Analytical Comparison** | Real-time FEA vs. closed-form solution validation for all 7 cases |
| **Parametric Study** | Real-time sliders for E, nu, force, thickness with fast rescaling |
| **Mode Shape Animation** | Async modal analysis with sinusoidal oscillation playback |
| **Output** | JSON pipeline: meta, displacement, stress, mesh, convergence, adaptive convergence |

---

## Quick Start

### Prerequisites

- **C++20 compiler**: Clang 14+ (macOS) or GCC 12+ (Linux)
- **CMake**: 3.20 or later
- **Qt 6**: For desktop app only (install via `brew install qt` on macOS)
- **OpenMP**: Optional, for parallel assembly
- **Python 3.8+**: For postprocessing scripts (optional)

### Desktop Application (recommended)

```bash
# One command: build, install, and launch
./build-desktop.sh
```

This script:
1. Configures CMake with Qt 6 desktop app enabled
2. Builds the release binary
3. Installs to `/Applications/Crucible-FEA.app` (macOS)
4. Launches the application

**Manual build** (if you prefer step-by-step):

```bash
# Configure
cmake -B build-desktop -DCRUCIBLE_FEA_DESKTOP=ON -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build-desktop -j$(sysctl -n hw.ncpu)

# Install (macOS)
cp -R build-desktop/crucible-fea-desktop.app /Applications/

# Launch
open /Applications/crucible-fea-desktop.app
```

### Solver (command line)

```bash
# Build
cmake -B build && cmake --build build -j$(sysctl -n hw.ncpu)

# Run validation cases
./build/FEA_Cantilever 32              # Cantilever beam (32x8 mesh)
./build/FEA_Cantilever 32 --q8         # Cantilever with Q8 elements
./build/FEA_Cook 32                    # Cook's membrane (32x32)
./build/FEA_PlateHole 16               # Plate with hole (16x16)
./build/FEA_LBracket 32                # L-bracket (32x32)
./build/FEA_Patch                      # Patch test (4x4)
./build/FEA_Michell                    # Michell truss
./build/FEA_AdaptHole 8 --iters 4      # Adaptive refinement
./build/FEA_ThermalCylinder            # Thermal cylinder (3D)
./build/FEA_Cantilever3D               # 3D cantilever (H8)
./build/FEA_PlateHole3D                # 3D plate with hole (H8)
./build/FEA_Lame3D                     # 3D Lame problem (H8)

# Run tests (86 tests)
./build/FEA_Tests

# Post-process (matplotlib PNGs)
python3 scripts/postprocess.py output/cantilever_32/ --all
python3 scripts/postprocess.py output/ --all-cases

# Preview portfolio site
python3 -m http.server -d docs 8765
open http://localhost:8765
```

---

## Desktop Application Walkthrough

The desktop application provides an interactive GUI for every stage of the FEA workflow. The following sections describe each major feature.

### Probe Tool (I key)

Press **I** to enter probe mode, then click any point in the viewport. The status bar displays the interpolated stress tensor, displacement vector, and element ID at that location. The probe tool uses shape function interpolation for sub-element accuracy, making it useful for checking values at specific geometric features like fillets or hole edges.

<!-- GIF: probe_tool -->
![Probe Tool](docs/assets/images/desktop_screenshots/probe_tool.gif)
<!-- END GIF -->

### Deformation Animation

After solving, press the play button to watch a 10-second smooth animation of the deformed shape. The animation uses ease-in-out cubic interpolation and includes a progress bar. Play, pause, and reset controls are available in the toolbar. The displacement scale updates in real time so you can observe how each load step contributes to the total deformation.

<!-- GIF: deformation_animation -->
![Deformation Animation](docs/assets/images/desktop_screenshots/deformation_animation.gif)
<!-- END GIF -->

### Material Library

The material dropdown in the mesh editor provides five engineering presets:

| Material | E (GPa) | nu | rho (kg/m3) | alpha (1/K) |
|----------|---------|------|------------|-------------|
| Steel (A36) | 200.0 | 0.30 | 7800 | 12.0e-6 |
| Aluminum (6061-T6) | 68.9 | 0.33 | 2700 | 23.6e-6 |
| Titanium (Ti-6Al-4V) | 113.8 | 0.34 | 4430 | 8.6e-6 |
| Copper (C11000) | 117.0 | 0.34 | 8960 | 17.0e-6 |
| Concrete (C30) | 30.0 | 0.20 | 2400 | 10.0e-6 |

Selecting a preset automatically populates E, nu, density, thickness, and thermal expansion coefficient. A "Custom" option lets you enter arbitrary values.

<!-- SCREENSHOT: material_library -->
![Material Library](docs/assets/images/desktop_screenshots/material_library.png)
<!-- END SCREENSHOT -->

### Project Save/Load

Use **Ctrl+S** to quick-save the current project as a `.cauchy` JSON file. The v2 format captures the full project state: mesh geometry, boundary conditions, material properties, solver settings, and results. Use **File > Open** to reload a saved project. The save/load system uses QFileDialog for native OS file pickers.

<!-- SCREENSHOT: project_save_load -->
![Project Save/Load](docs/assets/images/desktop_screenshots/project_save_load.png)
<!-- END SCREENSHOT -->

### Analytical Comparison Panel

The right dock includes a comparison table that shows expected vs. computed values for all 7 validation cases: cantilever tip deflection, cantilever max stress, Cook's membrane tip displacement, L-bracket stress concentration factor, plate with hole max stress, patch test constant stress, and Michell truss deflection. Each row is color-coded green (pass, error < 5%) or red (fail) with the analytical formula displayed inline.

<!-- SCREENSHOT: analytical_comparison -->
![Analytical Comparison](docs/assets/images/desktop_screenshots/analytical_comparison.png)
<!-- END SCREENSHOT -->

### Element Inspector

Double-click any element in the viewport to open the element inspector panel. The inspector displays:

- **Stress tensor**: sigma_xx, sigma_yy, sigma_xy, von Mises, principal stresses sigma_1 and sigma_2
- **Strain tensor**: epsilon_xx, epsilon_yy, gamma_xy
- **Quality metrics**: aspect ratio, Jacobian determinant, skew angle

This is useful for debugging stress concentrations and identifying elements with poor quality that may need refinement.

<!-- SCREENSHOT: element_inspector -->
![Element Inspector](docs/assets/images/desktop_screenshots/element_inspector.png)
<!-- END SCREENSHOT -->

### Stress Streamlines

Enable stress streamlines via the **View > Streamlines** menu. The tracer seeds points across the mesh and integrates principal stress directions using RK4 stepping. Red streamlines indicate tension (sigma > 0), blue streamlines indicate compression (sigma < 0). Streamline density adapts to the stress field, concentrating in high-gradient regions. This visualization is standard in FEA post-processors and provides intuitive insight into load paths.

<!-- GIF: stress_streamlines -->
![Stress Streamlines](docs/assets/images/desktop_screenshots/stress_streamlines.gif)
<!-- END GIF -->

### Mode Shape Animator

The mode shape panel provides modal analysis visualization. After solving, select a natural frequency from the dropdown and press play to see sinusoidal oscillation at that frequency. The animator runs asynchronously so the UI stays responsive. Amplitude and playback speed are adjustable via sliders. Each mode shape is overlaid on the original mesh so you can see how the structure deforms at resonance.

<!-- GIF: mode_shape_animator -->
![Mode Shape Animator](docs/assets/images/desktop_screenshots/mode_shape_animator.gif)
<!-- END GIF -->

### Parametric Study

The parametric panel provides real-time sliders for Young's modulus (E), Poisson's ratio (nu), applied force, and plate thickness. When you drag a slider, the parametric engine rescales the baseline stiffness matrix and force vector without reassembling from scratch, giving near-instant feedback. This is useful for exploring design sensitivity and understanding how material or load changes affect the stress and displacement fields.

<!-- GIF: parametric_study -->
![Parametric Study](docs/assets/images/desktop_screenshots/parametric_study.gif)
<!-- END GIF -->

---

## Architecture

```
fea.hpp  (assembly + solve orchestration)
├── fea_types.hpp        (types, enums, globals)
├── elements.hpp         (Bar, Q4, Q8, T3 stiffness)
├── elements_3d.hpp      (H8, T4 stiffness)
├── locking_mitigation.hpp (SRI, B-Bar)
├── sparse.hpp           (COO/CSR sparse matrix)
├── solver.hpp           (Cholesky + CG)
├── preconditioners.hpp  (Jacobi, SSOR, IC(0), Block Jacobi)
├── mesh.hpp             (structured quad mesher)
├── postprocess.hpp      (stress recovery, Von Mises)
├── adaptivity.hpp       (ZZ SPR, error indicators, red-green refinement)
├── convergence.hpp      (GCI, mesh convergence)
├── nonlinear.hpp        (Total Lagrangian Newton-Raphson)
├── dynamics.hpp         (Newmark-beta, modal analysis)
└── contact.hpp          (node-to-surface penalty)
```

### Desktop Application Architecture

```
src/desktop/
├── main.cpp                    # Qt application entry point
├── cauchy_app.hpp/cpp          # QApplication subclass
├── main_window.hpp/cpp         # Main window (menus, docks, toolbar)
├── mesh_editor.hpp/cpp         # Left dock: mesh generation + BC editor
├── solver_panel.hpp/cpp        # Right dock: solver controls + results
├── viewport_widget.hpp/cpp     # Central: 2D QPainter mesh renderer
├── viewport_3d.hpp/cpp         # 3D OpenGL viewport
├── analytical.hpp              # Closed-form formulas for 7 validation cases
├── analytical_panel.hpp/cpp    # Expected vs. computed comparison table
├── element_inspector_panel.hpp # Double-click element detail view
├── streamline_tracer.hpp       # Principal stress direction RK4 integration
├── parametric_engine.hpp       # Fast rescaling for parameter sweeps
├── parametric_panel.hpp/cpp    # Real-time E/nu/force/thickness sliders
├── material_library.hpp        # Steel, Al, Ti, Cu, Concrete presets
├── mode_shape_panel.hpp/cpp    # Modal analysis animation controls
├── modal_runner.hpp/cpp        # Async eigenvalue solver
├── benchmark_gl.hpp            # OpenGL benchmark for GPU performance
├── probe_tool.hpp/cpp          # Click-to-probe stress/displacement
├── mesh_quality_overlay.hpp    # Aspect ratio / Jacobian heatmap
├── mesh_generator.hpp/cpp      # Structured mesh generation
├── geometry_model.hpp/cpp      # Geometric model representation
├── geometry_panel.hpp/cpp      # Geometry editing panel
├── geometry_primitive.hpp      # Basic geometry shapes
├── selection_model.hpp/cpp     # Element/node selection handling
├── editor_state.hpp/cpp        # Editor undo/redo state
├── tool_context.hpp/cpp        # Active tool context
├── bc_model.hpp/cpp            # Boundary condition data model
├── bc_panel.hpp/cpp            # Boundary condition editor panel
├── result_model.hpp/cpp        # QAbstractItemModel for tables
├── solver_runner.hpp/cpp       # QThread async solver execution
├── project_io.hpp/cpp          # Save/load .cauchy JSON files
├── convergence_chart.hpp/cpp   # Log-log convergence plot
├── stress_histogram.hpp/cpp    # Element stress distribution
├── energy_balance_chart.hpp/cpp # Strain energy vs work done
├── displacement_line_chart.hpp/cpp # Displacement profile along edge
├── load_displacement_chart.hpp/cpp # Force vs max displacement
├── error_heatmap.hpp/cpp       # ZZ error indicator overlay
├── undo_commands.hpp/cpp       # Undo/redo command stack
└── about_dialog.hpp/cpp        # About/help dialog
```

### Data Flow

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
  +--> scripts/postprocess.py          (matplotlib PNGs)
  +--> scripts/convert_for_web.py      (browser-optimized JSON)
         +--> docs/assets/data/*.json  (Three.js viewer data)
```

---

## Validation Coverage

All implementations are validated against published analytical solutions.

| Case | Dimension | Element | Analytical Reference | What It Proves |
|------|-----------|---------|---------------------|----------------|
| Cantilever beam | 2D | Q4, Q8 | delta = PL^3/(3EI), Timoshenko | Shear locking (Q4), quadratic completeness (Q8) |
| Michell truss | 2D | Bar | Published truss solutions | Bar element assembly |
| Cook's membrane | 2D | Q4, Q8 | Tip displacement ~13.68 mm | Bending + shear, Q4 vs Q8 |
| L-bracket | 2D | Q4 | SCF ~2-3 at fillet | Stress concentration, refinement |
| Patch test | 2D | Q4, Q8 | Exact for linear elements | Element verification (mandatory) |
| Plate with hole | 2D | Q4, Q8 | Kirsch: sigma_max = 3*sigma_inf | Stress concentration, adaptive refinement |
| Thermal cylinder | 3D | H8 | Steady-state thermoelastic | Thermal loading |
| Cantilever 3D | 3D | H8 | Euler-Bernoulli beam | 3D element verification |
| Plate with hole 3D | 3D | H8 | 3D stress concentration | 3D stress recovery |
| Lame problem | 3D | H8 | Analytical thick cylinder | 3D plane strain |

---

## Analysis Modules

### Element Formulations

Six element types covering truss, 2D solid, and 3D solid mechanics:

- **Bar**: Truss element for axial loading (2 DOF per node)
- **Q4**: Bilinear quadrilateral for standard 2D problems (plane stress/strain)
- **Q8**: Serendipity quadratic quad for bending-dominated problems (3x3 Gauss integration)
- **T3**: Linear triangle for complex geometries
- **H8**: Trilinear hexahedron for 3D solid mechanics
- **T4**: Linear tetrahedron for 3D geometries

### Adaptive Mesh Refinement

ZZ Superconvergent Patch Recovery (SPR) drives adaptive h-refinement:

1. Recover nodal stresses from element centroids using least-squares fit
2. Compute error indicators as the difference between recovered and raw stresses
3. Mark elements with largest error indicators (theta = 0.5 threshold)
4. Refine marked elements using red-green refinement
5. Track convergence via Generalized Courant Richardson Extrapolation (GCI)

### Geometric Nonlinearity

Total Lagrangian formulation with Newton-Raphson iteration:

- Green-Lagrange strain tensor for large deformation
- Consistent tangent stiffness from material + geometric contributions
- Line search with backtracking for robust convergence
- Validated against linear solutions at small displacements

### Dynamic Analysis

Newmark-beta time integration for transient structural response:

- Consistent and lumped mass matrices
- Modal analysis via subspace iteration (eigenvalue solver)
- Rayleigh damping for energy dissipation
- Validated against analytical spring-mass systems

### Contact Mechanics

Node-to-surface frictionless penalty method:

- Gap function computation between slave nodes and master surface
- Penalty stiffness for penetration resistance
- Contact force assembly into global system
- Hertz contact benchmark setup for validation

---

## Tech Stack

| Component | Technology | Version |
|-----------|-----------|---------|
| Language | C++20 | Header-only pattern |
| GUI | Qt 6 (QWidgets, OpenGLWidgets, WebEngineWidgets) | 6.x |
| 2D Rendering | QPainter (CPU) for main viewport | -- |
| 3D Benchmark | QOpenGLWidget for GPU performance tests | -- |
| Build | CMake + FetchContent | 3.15+ |
| Testing | Google Test | v1.15.2 |
| CPU Parallel | OpenMP | -- |
| Website | Three.js (WebGL) | CDN |
| Python | numpy, matplotlib | Post-processing |

---

## Project Structure

```
fea-2d/
├── CMakeLists.txt                  # Build config (solver + desktop)
├── build-desktop.sh                # One-command desktop build + install
├── README.md
├── CHANGELOG.md
│
├── src/                            # C++ solver (header-only)
│   ├── fea.hpp                     # Assembly + solve orchestration
│   ├── fea_types.hpp               # Types, enums, globals
│   ├── elements.hpp                # 2D element stiffness (Bar, Q4, Q8, T3)
│   ├── elements_3d.hpp             # 3D element stiffness (H8, T4)
│   ├── locking_mitigation.hpp      # SRI, B-Bar methods
│   ├── sparse.hpp                  # COO/CSR sparse matrix
│   ├── solver.hpp                  # Cholesky + CG solvers
│   ├── preconditioners.hpp         # Jacobi, SSOR, IC(0), Block Jacobi
│   ├── mesh.hpp                    # Structured quad mesher
│   ├── postprocess.hpp             # Stress recovery, Von Mises
│   ├── adaptivity.hpp              # ZZ SPR + red-green refinement
│   ├── convergence.hpp             # GCI, mesh convergence
│   ├── nonlinear.hpp               # Newton-Raphson (Total Lagrangian)
│   ├── dynamics.hpp                # Newmark-beta, modal analysis
│   ├── contact.hpp                 # Node-to-surface penalty
│   │
│   ├── main_cantilever.cpp         # Validation case entry points
│   ├── main_cook.cpp
│   ├── main_plate_hole.cpp
│   ├── main_lbracket.cpp
│   ├── main_patch.cpp
│   ├── main_michell.cpp
│   ├── main_adapt_hole.cpp
│   ├── main_thermal.cpp
│   ├── main_cantilever_3d.cpp
│   ├── main_plate_hole_3d.cpp
│   ├── main_lame_3d.cpp
│   │
│   ├── fea_test.cpp                # Google Test suite (86 cases)
│   │
│   └── desktop/                    # Qt 6 desktop application
│       ├── main.cpp                # Entry point
│       ├── cauchy_app.hpp/cpp      # QApplication subclass
│       ├── main_window.hpp/cpp     # Main window (menus, docks, toolbar)
│       ├── mesh_editor.hpp/cpp     # Left dock: mesh + BC editor
│       ├── solver_panel.hpp/cpp    # Right dock: solver controls
│       ├── viewport_widget.hpp/cpp # Central: 2D QPainter renderer
│       ├── viewport_3d.hpp/cpp     # 3D OpenGL viewport
│       ├── analytical.hpp          # Closed-form formulas for 7 cases
│       ├── analytical_panel.hpp/cpp # Expected vs. computed table
│       ├── element_inspector_panel.hpp # Double-click element detail
│       ├── streamline_tracer.hpp   # Principal stress direction tracing
│       ├── parametric_engine.hpp   # Fast rescaling for parameter sweeps
│       ├── parametric_panel.hpp/cpp # Real-time sliders
│       ├── material_library.hpp    # Steel, Al, Ti, Cu, Concrete presets
│       ├── mode_shape_panel.hpp/cpp # Modal analysis animation
│       ├── modal_runner.hpp/cpp    # Async eigenvalue solver
│       ├── benchmark_gl.hpp        # OpenGL GPU performance benchmark
│       ├── probe_tool.hpp/cpp      # Click-to-probe stress/displacement
│       ├── mesh_quality_overlay.hpp # Aspect ratio / Jacobian heatmap
│       ├── mesh_generator.hpp/cpp  # Structured mesh generation
│       ├── geometry_model.hpp/cpp  # Geometric model representation
│       ├── geometry_panel.hpp/cpp  # Geometry editing panel
│       ├── geometry_primitive.hpp  # Basic geometry shapes
│       ├── selection_model.hpp/cpp # Element/node selection handling
│       ├── editor_state.hpp/cpp    # Editor undo/redo state
│       ├── tool_context.hpp/cpp    # Active tool context
│       ├── bc_model.hpp/cpp        # Boundary condition data model
│       ├── bc_panel.hpp/cpp        # BC editor panel
│       ├── result_model.hpp/cpp    # QAbstractItemModel for tables
│       ├── solver_runner.hpp/cpp   # QThread async solver
│       ├── project_io.hpp/cpp      # Save/load .cauchy files
│       ├── convergence_chart.hpp/cpp
│       ├── stress_histogram.hpp/cpp
│       ├── energy_balance_chart.hpp/cpp
│       ├── displacement_line_chart.hpp/cpp
│       ├── load_displacement_chart.hpp/cpp
│       ├── error_heatmap.hpp/cpp
│       ├── undo_commands.hpp/cpp   # Undo/redo command stack
│       └── about_dialog.hpp/cpp
│
├── scripts/
│   ├── postprocess.py              # JSON -> matplotlib PNGs
│   ├── convert_for_web.py          # JSON -> browser-optimized Three.js data
│   └── run_all_sims.py             # Batch runner
│
├── desktop/                        # Qt resources, .desktop, Info.plist
│   ├── resources.qrc
│   ├── crucible-fea.desktop
│   ├── Info.plist
│   ├── icon.icns
│   └── icon.png
│
├── docs/                           # Portfolio website
│   ├── index.html                  # Landing page
│   ├── cantilever.html             # Per-case pages with Three.js viewers
│   ├── cook.html
│   ├── lbracket.html
│   ├── patch.html
│   ├── plate_hole.html
│   ├── michell.html
│   ├── thermal_cylinder.html
│   ├── theory.html                 # FEA theory (shape functions, ZZ estimator)
│   ├── implementation.html         # Code architecture
│   ├── css/style.css               # Dark terminal theme
│   └── assets/
│       ├── js/                     # Three.js viewer components
│       ├── data/                   # Pre-processed browser JSON
│       └── images/                 # Generated PNG plots
│           └── desktop_screenshots/ # Desktop app screenshots and GIFs
│
├── output/                         # Simulation output (gitignored)
└── build/                          # CMake build (gitignored)
```

---

## Build Requirements

- **C++20 compiler**: GCC 10+, Clang 12+, or Apple Clang 14+
- **CMake**: 3.15 or later
- **OpenMP**: required for parallel assembly
- **Google Test**: fetched automatically via CMake FetchContent
- **Python 3.8+**: for postprocessing scripts (optional)
- **NumPy + Matplotlib**: for Python postprocessor (optional)
- **Qt 6**: for desktop application only (`cmake -DCRUCIBLE_FEA_DESKTOP=ON`)

---

## What This Demonstrates

For Mechanical/Aerospace Engineering roles, this project demonstrates:

- **FEA competency**: Element formulation (Q4, Q8, H8, T4), sparse assembly (COO to CSR), solver implementation (Cholesky + CG with preconditioners)
- **Structural mechanics fundamentals**: Shape functions, Gauss quadrature, stress recovery, adaptive refinement, geometric nonlinearity, transient dynamics
- **Software engineering**: C++20 header-only design, 86 Google Test cases, CI pipeline, cross-platform build
- **Communication**: Interactive desktop GUI, portfolio website with Three.js viewers, validation documentation

---

## References

1. Cook, R.D., Malkus, D.S., Plesha, M.E., and Witt, R.J., "Concepts and Applications of Finite Element Analysis", 4th ed., Wiley, 2001.
2. Hughes, T.J.R., "The Finite Element Method: Linear Static and Dynamic Finite Element Analysis", Dover, 2000.
3. Zienkiewicz, O.C., Taylor, R.L., and Zhu, J.Z., "The Finite Element Method: Its Basis and Fundamentals", 7th ed., Butterworth-Heinemann, 2013.
4. Bathe, K.J., "Finite Element Procedures", 2nd ed., Watertown, MA, 2014.
5. Belytschko, T., Liu, W.K., Moran, B., and Elkhodary, K., "Nonlinear Finite Elements for Continua and Structures", 2nd ed., Wiley, 2013.
6. Zienkiewicz, O.C. and Zhu, J.Z., "The superconvergent patch recovery and a posteriori error estimates", International Journal for Numerical Methods in Engineering, 33(7), 1331-1364, 1992.

---

## Contributing

Contributions are welcome. Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

---

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

---

<p align="center">
  <i>Built for Mechanical/Aerospace Engineering roles at SpaceX, Lockheed Martin, Northrop Grumman, Boeing, and similar.</i>
</p>
