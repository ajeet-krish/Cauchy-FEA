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
| 8 | Desktop FEA application (Qt 6) | In Progress |

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
| Desktop plotting | Custom QPainter widgets | No external charting dependency; matches dark theme |
| Desktop packaging | macOS .app bundle (MACOSX_BUNDLE) | Native double-click launch; install to /Applications |

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

## Desktop Application Plan

### Overview
A native Qt 6 desktop FEA tool that provides an interactive GUI for mesh
generation, boundary condition assignment, solver execution, and results
visualization. The portfolio website (`docs/`) remains the project deliverable
for hiring managers. The desktop application is the actual working tool.

### Architecture Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| GUI framework | Qt 6 (QWidgets) | Direct C++ solver link, industry recognition, cross-platform .app bundle |
| 2D rendering | QPainter (CPU) | Sufficient for 2D mesh + plots; no GPU dependency |
| Solver integration | Direct C++ link (no IPC) | Eliminates JSON serialization overhead; direct access to data structures |
| Async execution | QThread with progress signals | Keeps UI responsive during solver runs |
| Project format | JSON (.cauchy) | Same format as existing pipeline; human-readable |
| Plotting | Custom QPainter widgets | No external charting dependency; matches dark theme |
| Packaging | MACOSX_BUNDLE + install target | Native .app launch; `cmake --install` copies to /Applications |
| License | MIT (same as solver) | Consistent with existing project |

### Why Not ImGui / Tauri / Flutter

| Framework | Why Not for Cauchy |
|-----------|-------------------|
| Dear ImGui | No native file dialogs, no dock widgets, custom-only look, less industry recognition |
| Rust + Tauri | C++/Rust FFI overhead makes no sense for solver-heavy app; two languages to maintain |
| Flutter | Dart/C++ FFI adds complexity; Material Design looks like a phone app on desktop |
| wxWidgets | Sparse documentation, no built-in plotting, manual packaging on macOS |
| GTK | Poor macOS/Windows support, C API is painful from C++, tiny scientific ecosystem |

### Desktop App File Structure

```
src/
├── desktop/
│   ├── main.cpp                  # Qt application entry point
│   ├── cauchy_app.hpp            # QApplication subclass
│   ├── cauchy_app.cpp
│   ├── main_window.hpp           # QMainWindow with menu/toolbar/docks
│   ├── main_window.cpp
│   ├── mesh_editor.hpp           # Left dock: mesh generation + BC editor
│   ├── mesh_editor.cpp
│   ├── solver_panel.hpp          # Right dock: solver controls + results
│   ├── solver_panel.cpp
│   ├── viewport_widget.hpp       # Central: 2D mesh viewport (QPainter)
│   ├── viewport_widget.cpp
│   ├── result_model.hpp          # QAbstractItemModel for stress/displacement tables
│   ├── result_model.cpp
│   ├── solver_runner.hpp         # QThread wrapper for async solver execution
│   ├── solver_runner.cpp
│   ├── project_io.hpp            # Save/load .cauchy project files
│   ├── project_io.cpp
│   ├── convergence_chart.hpp     # Log-log convergence plot widget
│   ├── convergence_chart.cpp
│   ├── stress_histogram.hpp      # Element stress distribution histogram
│   ├── stress_histogram.cpp
│   ├── energy_balance_chart.hpp  # Strain energy vs work done bar chart
│   ├── energy_balance_chart.cpp
│   ├── displacement_line_chart.hpp # Displacement profile along mesh edge
│   ├── displacement_line_chart.cpp
│   ├── load_displacement_chart.hpp # Applied force vs max displacement
│   ├── load_displacement_chart.cpp
│   ├── error_heatmap.hpp         # ZZ error indicator per-element heatmap
│   ├── error_heatmap.cpp
│   ├── probe_tool.hpp            # Click-to-probe stress/displacement at a point
│   ├── probe_tool.cpp
│   ├── mesh_quality_overlay.hpp  # Aspect ratio / Jacobian heatmap overlay
│   ├── mesh_quality_overlay.cpp
│   └── about_dialog.hpp          # About/help dialog
│
└── desktop/
    ├── resources.qrc             # Qt resource system (icons, styles)
    ├── cauchy.desktop            # Linux .desktop file
    └── Info.plist                # macOS .plist file

docs/                             # Portfolio website (unchanged)
scripts/                          # Python postprocessing (unchanged)
```

### Bottom Plot Panel

The main window includes a collapsible bottom dock with a QTabWidget holding 6 analysis plots:

| Tab | Widget | What it shows |
|-----|--------|---------------|
| Stress Distribution | StressHistogram | Histogram of sigma_xx, sigma_yy, von_mises across elements |
| Energy Balance | EnergyBalanceChart | Bar chart comparing strain energy 0.5*u^T*K*u vs work 0.5*f^T*u |
| Displacement Profile | DisplacementLineChart | uy along top edge of mesh (FEA data points + line) |
| Load-Displacement | LoadDisplacementChart | Applied force vs max displacement (accumulates across solves) |
| Error Map | ErrorHeatmap | Per-element ZZ error indicator as colored overlay |
| Convergence | ConvergenceChart | Log-log mesh refinement convergence (GCI, observed order) |

### Solver Bridge API

The desktop app links directly to the existing solver headers. A new
`fea::run_case()` function is extracted from the `main_*.cpp` entry points:

```cpp
struct SolveConfig {
    CaseType case_type;
    ElementType element_type;
    PlaneType plane_type;
    int nx;
    int ny;
    Material material;
    std::vector<BoundaryCondition> boundary_conditions;
    SolverType solver_type;       // Cholesky or CG
    double cg_tolerance;
    int cg_max_iterations;
    bool use_adaptivity;
    int adaptive_iterations;
};

struct SolveResult {
    bool success;
    std::string error_message;
    std::string warning_message;
    SolverType solver_used;
    int iterations;
    double final_residual;
    double solve_time_ms;
    Mesh mesh;
    std::vector<NodeDisplacement> displacements;
    std::vector<ElementStress> stresses;
    std::vector<NodalStress> nodal_stresses;
    ConvergenceData convergence;
    double max_displacement;
    double max_stress;
    double strain_energy;
};

SolveResult run_case(const SolveConfig& config);
```

### Desktop App Code Conventions

- C++20, same as solver backend
- Qt 6 APIs (Qt6Core, Qt6Gui, Qt6Widgets)
- 4-space indentation, K&R braces (same as solver)
- `snake_case` for locals, `PascalCase` for classes/structs
- `g_` prefix for inline globals (e.g., `g_app`, `g_main_window`)
- `Q_OBJECT` macro for all QObject-derived classes
- Signals/slots for solver-to-UI communication (not direct function calls)
- `QThread` for async solver execution, not `QtConcurrent` (explicit control)
- `QVariant` for flexible data passing in the result model
- JSON for project file format (same as existing pipeline)

### Production-Grade Features

| Feature | Implementation |
|---------|---------------|
| Async solver | QThread with `started()` signal, `finished()` signal, progress |
| Error handling | `SolveResult::success` + `error_message`; toast notifications |
| Mesh quality | Aspect ratio + Jacobian per element; color-coded overlay |
| Probe tool | Ray-cast from mouse to mesh; display node/element properties |
| Undo/redo | `QUndoCommand` for model changes (BC assignment, mesh refine) |
| Project files | JSON archive of config + results; load/save via QFileDialog |
| Progress feedback | QProgressBar in status bar; solver status in status bar |
| Memory management | Streaming results for meshes >100K elements |
| Cross-platform | Qt handles macOS/Linux/Windows; CI builds all three |
| Packaging | CPack generates DEB/RPM/DMG/NSIS from same build |

### Desktop App Verification Checklist

- [x] Solver runs asynchronously without blocking UI
- [x] Mesh generation produces correct mesh for all 6 cases
- [x] BC editor assigns and persists boundary conditions correctly
- [ ] Results visualization matches web viewer output
- [ ] Convergence study produces correct GCI and order of convergence
- [ ] Project save/load round-trips correctly
- [ ] Error handling shows user-friendly messages for invalid inputs
- [ ] Probe tool reads correct stress/displacement values at clicked points
- [ ] Mesh quality overlay correctly identifies invalid elements
- [x] Cross-platform build passes on macOS (tested)
- [ ] Installer packages build correctly for all target platforms
- [x] All existing 22 Google Test cases still pass
- [x] Stress histogram displays sigma_xx, sigma_yy, von_mises distributions
- [x] Energy balance chart shows strain energy vs work done
- [x] Displacement line chart plots uy along top edge
- [x] Load-displacement chart accumulates across multiple solves
- [x] Error heatmap renders per-element ZZ error indicators
- [x] Convergence chart wired into bottom dock
- [x] macOS .app bundle builds and launches correctly
