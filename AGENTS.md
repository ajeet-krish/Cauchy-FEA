# Project Context, FEA-2D: 2D Finite Element Structural Solver

## Style Rules
- **No em dashes** in any file. Use two regular hyphens (--) instead of ---, &mdash;, &ndash;, or literal Unicode em dash.
- **C++ code style**: 4-space indentation, K&R braces, no tabs, no trailing whitespace.
- **Variable naming**: snake_case for local variables, PascalCase for structs/classes, SCREAMING_SNAKE_CASE for constants.
- **Global variables**: `g_` prefix for inline globals (e.g., `g_case`, `g_analysis`, `g_nx`).
- **HTML/CSS**: Double quotes for attributes, 2-space indentation, semantic HTML5 elements.
- **Header-only pattern**: All core logic in `.hpp` files with `#pragma once`. Use `inline` for functions and variables to avoid ODR violations.
- **Enum classes**: All enums use `enum class` with PascalCase values.

## Goal
Build and deploy a 2D finite element structural solver in C++20 as a portfolio centrepiece for mechanical/aerospace engineering roles. Deliver a 6+ page HTML portfolio with per-case dedicated pages (interactive viewers, KaTeX theory, validation tables), and a production-grade GitHub repository with CI and unit tests.

## Target Audience
Mechanical/aerospace hiring managers at SpaceX, Lockheed Martin, Northrop Grumman, Boeing, and similar. The site must communicate: FEA competency (element formulation, sparse assembly, solver implementation), structural mechanics fundamentals (shape functions, Gauss quadrature, stress recovery), and engineering communication skills (interactive web presentation, per-case analysis narratives).

## Current Status

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Project setup (CMake, types, sparse matrix), bar element | In progress |
| 2 | Q4 element, assembly, Cholesky solver | Pending |
| 3 | CG solver, BC enforcement, all 6 validation cases | Pending |
| 4 | Python postprocessor, JSON output, mesh convergence | Pending |
| 5 | Portfolio site (6 case pages + theory + implementation) | Pending |

## Architecture Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Element types | Bar + Q4 | Bar for truss, Q4 for 2D solid -- covers portfolio needs |
| Assembly | COO -> CSR | COO natural for assembly (append-only), CSR for fast SpMV |
| Solver | Cholesky (small) + CG (large) | Cholesky for verification, CG for scalability |
| BC enforcement | Penalty method | Simple, no matrix restructuring |
| Integration | 2x2 Gauss for Q4 | Full integration (document shear locking as limitation) |
| Plane stress vs strain | Both via config | Plane stress for thin plates, plane strain for thick sections |
| Mesh input | Built-in structured + JSON | No external dependencies, sufficient for all 6 cases |
| Output | JSON like LBM-2D | Reuse Python postprocessor pattern |
| Testing | Google Test, same CI pattern | Consistency with LBM-2D |

## Verification Checklist

Before declaring the solver "done," each must pass:
- [ ] Patch test: constant stress exactly recovered for any mesh
- [ ] Cantilever tip deflection matches PL^3/(3EI) within 1% (32x32 mesh)
- [ ] Cantilever max stress matches My/I within 5%
- [ ] Cook's membrane tip displacement matches ~13.68 mm (plane stress)
- [ ] Plate with hole max stress matches 3*sigma_inf at hole edge (within 10%)
- [ ] Energy balance: 0.5 * u^T * K * u == 0.5 * u^T * f (exact for linear)
- [ ] CG residual drops below 1e-10 for all cases
- [ ] Cholesky and CG produce identical results (within solver tolerance)
- [ ] Negative Jacobian detection triggers on invalid elements
- [ ] All 15+ Google Test cases pass
- [ ] CI builds and tests on Ubuntu + macOS

## Limitations to Document

1. **Shear locking**: Standard Q4 with full integration locks in bending. Document and show mesh refinement mitigates it.
2. **2D only**: Plane stress/strain. No 3D elements.
3. **Linear elastic only**: No plasticity, no geometric nonlinearity.
4. **No adaptive meshing**: Fixed mesh (future: h-adaptivity).
5. **No dynamic analysis**: Static loading only (future: implicit/explicit dynamics).

## Code Conventions

### Include Hierarchy
```
fea.hpp
├── fea_types.hpp    (types, enums, globals)
├── elements.hpp     (bar + Q4 stiffness matrices)
├── sparse.hpp       (COO/CSR sparse matrix)
├── solver.hpp       (Cholesky + CG)
├── mesh.hpp         (structured quad mesher + JSON input)
└── postprocess.hpp  (stress recovery, Von Mises)
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

### JSON Output Format
- `meta.json`: mesh dimensions, material props, max displacement, max stress
- `displacement.json`: nodal displacement field (u_x, u_y per node)
- `stress.json`: element-centered stress field (sigma_xx, sigma_yy, sigma_xy, von_mises)
- `convergence.json`: mesh convergence data (GCI, order of convergence)
