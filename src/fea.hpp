#pragma once
#include "fea_types.hpp"
#include "elements.hpp"
#include "elements_3d.hpp"
#include "locking_mitigation.hpp"
#include "contact.hpp"
#include "sparse.hpp"
#include "solver.hpp"
#include "mesh.hpp"
#include "postprocess.hpp"
#include <vector>
#include <chrono>
#include <iostream>
#include <iomanip>

// ==========================================================================
// FEA SOLVER CORE -- Assembly, solve, post-process
// ==========================================================================

namespace fea {

// ------------------------------------------------------------------
// Assemble global stiffness matrix (COO format)
// ------------------------------------------------------------------
inline COOMatrix assemble(const Mesh& m) {
    int ndof = m.num_dofs();
    COOMatrix K(ndof, ndof);

    // Assemble Q4 elements
    #pragma omp parallel
    {
        COOMatrix K_local(ndof, ndof);

        #pragma omp for nowait
        for (int e = 0; e < m.num_quads(); ++e) {
            const auto& elem = m.quad_elements[e];
            std::array<Node, 4> elem_nodes;
            for (int i = 0; i < 4; ++i) {
                elem_nodes[i] = m.nodes[elem[i]];
            }

            // Select Q4 element formulation based on integration type
            std::array<std::array<double, 8>, 8> Ke;
            if (g_integration == IntegrationType::SRI) {
                Ke = locking::Q4SRIElement::stiffness(elem_nodes, m.mat, m.plane);
            } else if (g_integration == IntegrationType::BBAR) {
                Ke = locking::Q4BBarElement::stiffness(elem_nodes, m.mat, m.plane);
            } else {
                Ke = elements::Q4Element::stiffness(elem_nodes, m.mat, m.plane);
            }
            auto dof_idx = elements::Q4Element::dof_indices(elem);

            for (int i = 0; i < 8; ++i) {
                for (int j = 0; j < 8; ++j) {
                    K_local.add(dof_idx[i], dof_idx[j], Ke[i][j]);
                }
            }
        }

        // Assemble Q8 elements
        #pragma omp for nowait
        for (int e = 0; e < m.num_quad8s(); ++e) {
            const auto& elem = m.quad8_elements[e];
            std::vector<Node> elem_nodes(8);
            for (int i = 0; i < 8; ++i) {
                elem_nodes[i] = m.nodes[elem[i]];
            }

            auto Ke = elements::Q8Element::stiffness(elem_nodes, m.mat, m.plane);
            auto dof_idx = elements::Q8Element::dof_indices(elem);

            for (int i = 0; i < 16; ++i) {
                for (int j = 0; j < 16; ++j) {
                    K_local.add(dof_idx[i], dof_idx[j], Ke[i][j]);
                }
            }
        }

        // Assemble T3 elements
        #pragma omp for nowait
        for (int e = 0; e < m.num_tris(); ++e) {
            const auto& elem = m.tri_elements[e];
            std::array<Node, 3> elem_nodes;
            for (int i = 0; i < 3; ++i) {
                elem_nodes[i] = m.nodes[elem[i]];
            }

            auto Ke = elements::T3Element::stiffness(elem_nodes, m.mat, m.plane);
            auto dof_idx = elements::T3Element::dof_indices(elem);

            for (int i = 0; i < 6; ++i) {
                for (int j = 0; j < 6; ++j) {
                    K_local.add(dof_idx[i], dof_idx[j], Ke[i][j]);
                }
            }
        }

        // Assemble bar elements
        #pragma omp for nowait
        for (int e = 0; e < m.num_bars(); ++e) {
            const auto& bar = m.bar_elements[e];
            const auto& n1 = m.nodes[bar[0]];
            const auto& n2 = m.nodes[bar[1]];
            double A = m.bar_areas[e];

            auto Ke = elements::BarElement::stiffness(n1, n2, A, m.mat);
            auto dof_idx = elements::BarElement::dof_indices(bar[0], bar[1]);

            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 4; ++j) {
                    K_local.add(dof_idx[i], dof_idx[j], Ke[i][j]);
                }
            }
        }

        // Assemble H8 (hexahedron) elements
        #pragma omp for nowait
        for (int e = 0; e < m.num_hexes(); ++e) {
            const auto& elem = m.hex_elements[e];
            std::array<Node, 8> elem_nodes;
            for (int i = 0; i < 8; ++i) elem_nodes[i] = m.nodes[elem[i]];

            auto Ke = elements::H8Element::stiffness(elem_nodes, m.mat);
            auto dof_idx = elements::H8Element::dof_indices(elem);

            for (int i = 0; i < 24; ++i)
                for (int j = 0; j < 24; ++j)
                    K_local.add(dof_idx[i], dof_idx[j], Ke[i][j]);
        }

        // Assemble T4 (tetrahedron) elements
        #pragma omp for nowait
        for (int e = 0; e < m.num_tets(); ++e) {
            const auto& elem = m.tet_elements[e];
            std::array<Node, 4> elem_nodes;
            for (int i = 0; i < 4; ++i) elem_nodes[i] = m.nodes[elem[i]];

            auto Ke = elements::T4Element::stiffness(elem_nodes, m.mat);
            auto dof_idx = elements::T4Element::dof_indices(elem);

            for (int i = 0; i < 12; ++i)
                for (int j = 0; j < 12; ++j)
                    K_local.add(dof_idx[i], dof_idx[j], Ke[i][j]);
        }

        #pragma omp critical
        {
            for (size_t k = 0; k < K_local.val.size(); ++k) {
                K.add(K_local.row[k], K_local.col[k], K_local.val[k]);
            }
        }
    }

    return K;
}

// ------------------------------------------------------------------
// Compute penalty value from assembled stiffness matrix
// Must be called BEFORE adding penalty to the matrix
// ------------------------------------------------------------------
inline double compute_penalty(const COOMatrix& K) {
    double K_max = 0.0;
    for (size_t k = 0; k < K.val.size(); ++k) {
        if (K.row[k] == K.col[k]) {
            K_max = std::max(K_max, std::abs(K.val[k]));
        }
    }
    // 1e4 * K_max -- large enough to enforce BC, small enough to avoid conditioning issues
    double penalty = K_max * 1e4;
    if (penalty < 1e8) penalty = 1e8;
    return penalty;
}

// ------------------------------------------------------------------
// Apply penalty method for Dirichlet BCs to stiffness matrix
// ------------------------------------------------------------------
inline void apply_dirichlet_penalty(COOMatrix& K, const Mesh& m, double penalty) {
    for (const auto& bc : m.dirichlet) {
        int dof = dof_index(bc.node, bc.dof);
        K.add(dof, dof, penalty);
    }
}

// ------------------------------------------------------------------
// Build RHS vector from Neumann BCs and thermal loads
// ------------------------------------------------------------------
inline std::vector<double> build_rhs(const Mesh& m) {
    std::vector<double> f(m.num_dofs(), 0.0);
    for (const auto& bc : m.neumann) {
        f[dof_index(bc.node, bc.dof)] += bc.value;
    }

    // Assemble thermal load if temperature field is defined
    if (m.temperature.size() == static_cast<size_t>(m.num_nodes()) && m.mat.alpha != 0.0) {
        // Q4 elements
        for (int e = 0; e < m.num_quads(); ++e) {
            const auto& elem = m.quad_elements[e];
            std::array<Node, 4> elem_nodes;
            std::array<double, 4> temps;
            for (int i = 0; i < 4; ++i) {
                elem_nodes[i] = m.nodes[elem[i]];
                temps[i] = m.temperature[elem[i]];
            }
            auto fe_th = elements::Q4Element::thermal_load(elem_nodes, m.mat, temps, m.T_ref, m.plane);
            auto dof_idx = elements::Q4Element::dof_indices(elem);
            for (int i = 0; i < 8; ++i) {
                f[dof_idx[i]] += fe_th[i];
            }
        }

        // T3 elements
        for (int e = 0; e < m.num_tris(); ++e) {
            const auto& elem = m.tri_elements[e];
            std::array<Node, 3> elem_nodes;
            std::array<double, 3> temps;
            for (int i = 0; i < 3; ++i) {
                elem_nodes[i] = m.nodes[elem[i]];
                temps[i] = m.temperature[elem[i]];
            }
            auto fe_th = elements::T3Element::thermal_load(elem_nodes, m.mat, temps, m.T_ref, m.plane);
            auto dof_idx = elements::T3Element::dof_indices(elem);
            for (int i = 0; i < 6; ++i) {
                f[dof_idx[i]] += fe_th[i];
            }
        }
    }

    return f;
}

// ------------------------------------------------------------------
// Modify RHS for Dirichlet BCs (penalty method)
// f[dof] += penalty * prescribed_value
// ------------------------------------------------------------------
inline void modify_rhs_dirichlet(std::vector<double>& f, const Mesh& m, double penalty) {
    for (const auto& bc : m.dirichlet) {
        int dof = dof_index(bc.node, bc.dof);
        f[dof] += penalty * bc.value;
    }
}

// ------------------------------------------------------------------
// Full solve: assemble, apply BCs, solve
// ------------------------------------------------------------------
struct SolveResult {
    std::vector<double> displacement;
    std::vector<postprocess::ElementStress> stresses;
    CSRMatrix K_csr;
    std::vector<double> f;
    int cg_iterations = 0;
    double solve_time_ms = 0.0;
    bool cg_converged = false;
    preconditioners::PreconditionerType prec_used =
        preconditioners::PreconditionerType::JACOBI;
};

// Auto-select preconditioner based on matrix properties
inline preconditioners::PreconditionerType auto_select_preconditioner(
    const CSRMatrix& K) {

    int n = K.nrows;
    double nnz_per_row = static_cast<double>(K.values.size()) / n;

    // For very small systems, Jacobi is fine
    if (n < 500) return preconditioners::PreconditionerType::JACOBI;

    // For moderate systems with moderate fill, use SSOR
    if (n < 5000 && nnz_per_row < 20) return preconditioners::PreconditionerType::SSOR;

    // For large systems, use IC(0) if fill is manageable
    if (nnz_per_row < 30) return preconditioners::PreconditionerType::IC0;

    // For very large or very sparse systems, block Jacobi
    return preconditioners::PreconditionerType::BLOCK_JACOBI;
}

inline SolveResult solve(Mesh& m, bool use_cg = false,
    preconditioners::PreconditionerType prec_type =
        preconditioners::PreconditionerType::JACOBI) {

    auto t_start = std::chrono::high_resolution_clock::now();

    // Auto-switch to CG for large systems (Cholesky is O(n^3) with dense matrix)
    if (!use_cg && m.num_dofs() > 2000) {
        std::cout << "Auto-switching to CG (DOFs=" << m.num_dofs() << " > 2000)" << std::endl;
        use_cg = true;
    }

    // 1. Assemble
    std::cout << "Assembling global stiffness matrix..." << std::endl;
    auto K_coo = assemble(m);
    std::cout << "  COO entries: " << K_coo.val.size() << std::endl;

    // 2. Compute penalty BEFORE modifying matrix
    double penalty = compute_penalty(K_coo);

    // 3. Apply Dirichlet BCs (penalty method)
    apply_dirichlet_penalty(K_coo, m, penalty);

    // 4. Convert to CSR
    auto K_csr = K_coo.to_csr();
    std::cout << "  CSR non-zeros: " << K_csr.values.size() << std::endl;

    // 5. Build RHS and apply Dirichlet penalty
    auto f = build_rhs(m);
    modify_rhs_dirichlet(f, m, penalty);

    // 6. Solve
    std::vector<double> u;
    int cg_iters = 0;
    bool cg_conv = false;
    auto final_prec = prec_type;

    if (use_cg) {
        // Auto-select preconditioner if using default Jacobi on large systems
        if (prec_type == preconditioners::PreconditionerType::JACOBI && m.num_dofs() > 5000) {
            prec_type = auto_select_preconditioner(K_csr);
            std::cout << "  Auto-selected preconditioner: "
                      << preconditioners::preconditioner_name(prec_type) << std::endl;
        }

        std::string prec_label = preconditioners::preconditioner_name(prec_type);

        if (prec_type == preconditioners::PreconditionerType::IC0) {
            preconditioners::IncompleteCholesky M;
            M.setup(K_csr);
            CGSolver cg(10000, 1e-10);
            auto result = cg.solve(K_csr, f, M);
            u = result.x;
            cg_iters = result.iterations;
            cg_conv = result.converged;
            final_prec = result.prec_type;
            std::cout << "  CG iterations: " << cg_iters
                      << ", residual: " << std::scientific << std::setprecision(2)
                      << result.residual_norm
                      << (cg_conv ? " (converged)" : " (DID NOT converge)")
                      << std::endl;
        } else if (prec_type == preconditioners::PreconditionerType::SSOR) {
            preconditioners::SSOR M(1.0);
            M.setup(K_csr);
            CGSolver cg(10000, 1e-10);
            auto result = cg.solve(K_csr, f, M);
            u = result.x;
            cg_iters = result.iterations;
            cg_conv = result.converged;
            std::cout << "  CG iterations: " << cg_iters
                      << ", residual: " << std::scientific << std::setprecision(2)
                      << result.residual_norm
                      << (cg_conv ? " (converged)" : " (DID NOT converge)")
                      << std::endl;
        } else if (prec_type == preconditioners::PreconditionerType::BLOCK_JACOBI) {
            preconditioners::BlockJacobi M(2);  // block_size = DOF_PER_NODE
            M.setup(K_csr);
            CGSolver cg(10000, 1e-10);
            auto result = cg.solve(K_csr, f, M);
            u = result.x;
            cg_iters = result.iterations;
            cg_conv = result.converged;
            std::cout << "  CG iterations: " << cg_iters
                      << ", residual: " << std::scientific << std::setprecision(2)
                      << result.residual_norm
                      << (cg_conv ? " (converged)" : " (DID NOT converge)")
                      << std::endl;
        } else {
            // Jacobi (default)
            preconditioners::Jacobi M;
            M.setup(K_csr);
            CGSolver cg(10000, 1e-10);
            auto result = cg.solve(K_csr, f, M);
            u = result.x;
            cg_iters = result.iterations;
            cg_conv = result.converged;
            std::cout << "  CG iterations: " << cg_iters
                      << ", residual: " << std::scientific << std::setprecision(2)
                      << result.residual_norm
                      << (cg_conv ? " (converged)" : " (DID NOT converge)")
                      << std::endl;
        }

        std::cout << "  Preconditioner: " << preconditioners::preconditioner_name(final_prec) << std::endl;
    } else {
        std::cout << "Solving with Cholesky..." << std::endl;
        auto K_dense = DenseMatrix::from_csr(K_csr);
        CholeskySolver chol;
        chol.factor(K_dense);
        u = chol.solve(f);
    }

    // 7. Post-process
    std::cout << "Computing stresses..." << std::endl;
    auto stresses = postprocess::compute_all_stresses(m, u);

    auto t_end = std::chrono::high_resolution_clock::now();
    double solve_time = std::chrono::duration<double, std::milli>(t_end - t_start).count();

    // Report
    double max_disp = 0.0;
    for (int i = 0; i < m.num_nodes(); ++i) {
        double ux = u[dof_index(i, 0)];
        double uy = u[dof_index(i, 1)];
        double uz = (g_dim == 3) ? u[dof_index(i, 2)] : 0.0;
        double d = std::sqrt(ux * ux + uy * uy + uz * uz);
        if (d > max_disp) max_disp = d;
    }

    double max_stress = 0.0;
    for (const auto& s : stresses) {
        if (s.von_mises > max_stress) max_stress = s.von_mises;
    }

    std::cout << "  Max displacement: " << std::scientific << std::setprecision(4)
              << max_disp << " m" << std::endl;
    std::cout << "  Max von Mises stress: " << max_stress << " Pa" << std::endl;
    std::cout << "  Solve time: " << std::fixed << std::setprecision(1)
              << solve_time << " ms" << std::endl;

    return { u, stresses, K_csr, f, cg_iters, solve_time, cg_conv, final_prec };
}

// ------------------------------------------------------------------
// Solve with contact: assembles contact forces into RHS
// Uses penalty method for contact constraint enforcement
// ------------------------------------------------------------------
struct ContactSolveResult {
    SolveResult base_result;
    int contact_iterations;
    double max_penetration;
};

inline ContactSolveResult solve_with_contact(
    Mesh& m,
    const std::vector<int>& slave_nodes,
    const contact::ContactSurface& master,
    double contact_penalty = 1e6,
    bool use_cg = false,
    preconditioners::PreconditionerType prec_type =
        preconditioners::PreconditionerType::JACOBI) {

    auto t_start = std::chrono::high_resolution_clock::now();

    // Auto-switch to CG for large systems
    if (!use_cg && m.num_dofs() > 2000) {
        std::cout << "Auto-switching to CG (DOFs=" << m.num_dofs() << " > 2000)" << std::endl;
        use_cg = true;
    }

    // 1. Assemble
    std::cout << "Assembling global stiffness matrix..." << std::endl;
    auto K_coo = assemble(m);
    std::cout << "  COO entries: " << K_coo.val.size() << std::endl;

    // 2. Compute penalty BEFORE modifying matrix
    double penalty = compute_penalty(K_coo);

    // 3. Apply Dirichlet BCs (penalty method)
    apply_dirichlet_penalty(K_coo, m, penalty);

    // 4. Convert to CSR
    auto K_csr = K_coo.to_csr();
    std::cout << "  CSR non-zeros: " << K_csr.values.size() << std::endl;

    // 5. Build RHS
    auto f = build_rhs(m);

    // 6. Detect contact and assemble contact forces
    std::cout << "Detecting contact..." << std::endl;
    auto contact_result = contact::assemble_contact_forces(
        m.nodes, slave_nodes, master, m.num_dofs(), contact_penalty);

    // Add contact forces to RHS
    for (int i = 0; i < m.num_dofs(); ++i) {
        f[i] += contact_result.f_contact[i];
    }

    // Add contact diagonal stiffness to K
    // (This is needed for the penalty method to work correctly)
    for (const auto& [dof, val] : contact_result.K_diag_additions) {
        // We need to add to the CSR diagonal
        // Find the diagonal entry and add to it
        for (int k = K_csr.row_ptr[dof]; k < K_csr.row_ptr[dof + 1]; ++k) {
            if (K_csr.col_ind[k] == dof) {
                K_csr.values[k] += val;
                break;
            }
        }
    }

    modify_rhs_dirichlet(f, m, penalty);

    // 7. Solve
    std::vector<double> u;
    int cg_iters = 0;
    bool cg_conv = false;
    auto final_prec = prec_type;

    if (use_cg) {
        if (prec_type == preconditioners::PreconditionerType::JACOBI && m.num_dofs() > 5000) {
            prec_type = auto_select_preconditioner(K_csr);
        }

        if (prec_type == preconditioners::PreconditionerType::IC0) {
            preconditioners::IncompleteCholesky M;
            M.setup(K_csr);
            CGSolver cg(10000, 1e-10);
            auto result = cg.solve(K_csr, f, M);
            u = result.x;
            cg_iters = result.iterations;
            cg_conv = result.converged;
            final_prec = result.prec_type;
        } else if (prec_type == preconditioners::PreconditionerType::SSOR) {
            preconditioners::SSOR M(1.0);
            M.setup(K_csr);
            CGSolver cg(10000, 1e-10);
            auto result = cg.solve(K_csr, f, M);
            u = result.x;
            cg_iters = result.iterations;
            cg_conv = result.converged;
        } else if (prec_type == preconditioners::PreconditionerType::BLOCK_JACOBI) {
            preconditioners::BlockJacobi M(2);
            M.setup(K_csr);
            CGSolver cg(10000, 1e-10);
            auto result = cg.solve(K_csr, f, M);
            u = result.x;
            cg_iters = result.iterations;
            cg_conv = result.converged;
        } else {
            preconditioners::Jacobi M;
            M.setup(K_csr);
            CGSolver cg(10000, 1e-10);
            auto result = cg.solve(K_csr, f, M);
            u = result.x;
            cg_iters = result.iterations;
            cg_conv = result.converged;
        }

        std::cout << "  CG iterations: " << cg_iters << std::endl;
    } else {
        std::cout << "Solving with Cholesky..." << std::endl;
        auto K_dense = DenseMatrix::from_csr(K_csr);
        CholeskySolver chol;
        chol.factor(K_dense);
        u = chol.solve(f);
    }

    // 8. Compute penetration
    double max_penetration = 0.0;
    for (int sn : slave_nodes) {
        double gap, nx, ny, xi;
        contact::find_nearest_segment(sn, m.nodes, master, gap, nx, ny, xi);
        // Apply displacement to get deformed position
        double deformed_x = m.nodes[sn].x + u[dof_index(sn, 0)];
        double deformed_y = m.nodes[sn].y + u[dof_index(sn, 1)];
        // Recompute gap with deformed configuration
        // (simplified: just check original gap + displacement in normal direction)
        double disp_normal = u[dof_index(sn, 0)] * nx + u[dof_index(sn, 1)] * ny;
        double final_gap = gap + disp_normal;
        if (final_gap < max_penetration) {
            max_penetration = final_gap;
        }
    }

    // 9. Post-process
    std::cout << "Computing stresses..." << std::endl;
    auto stresses = postprocess::compute_all_stresses(m, u);

    auto t_end = std::chrono::high_resolution_clock::now();
    double solve_time = std::chrono::duration<double, std::milli>(t_end - t_start).count();

    // Report
    double max_disp = 0.0;
    for (int i = 0; i < m.num_nodes(); ++i) {
        double ux = u[dof_index(i, 0)];
        double uy = u[dof_index(i, 1)];
        double uz = (g_dim == 3) ? u[dof_index(i, 2)] : 0.0;
        double d = std::sqrt(ux * ux + uy * uy + uz * uz);
        if (d > max_disp) max_disp = d;
    }

    double max_stress = 0.0;
    for (const auto& s : stresses) {
        if (s.von_mises > max_stress) max_stress = s.von_mises;
    }

    std::cout << "  Max displacement: " << std::scientific << std::setprecision(4)
              << max_disp << " m" << std::endl;
    std::cout << "  Max von Mises stress: " << max_stress << " Pa" << std::endl;
    std::cout << "  Max penetration: " << max_penetration << " m" << std::endl;
    std::cout << "  Solve time: " << std::fixed << std::setprecision(1)
              << solve_time << " ms" << std::endl;

    SolveResult base{ u, stresses, K_csr, f, cg_iters, solve_time, cg_conv, final_prec };
    return { base, 1, max_penetration };
}

// ------------------------------------------------------------------
// Energy balance check: 0.5 * u^T * K * u == 0.5 * u^T * f
// ------------------------------------------------------------------
inline double compute_strain_energy(const CSRMatrix& K, const std::vector<double>& u) {
    auto Ku = K * u;
    double energy = 0.0;
    for (size_t i = 0; i < u.size(); ++i) {
        energy += u[i] * Ku[i];
    }
    return 0.5 * energy;
}

inline double compute_work_done(const std::vector<double>& f, const std::vector<double>& u) {
    double work = 0.0;
    for (size_t i = 0; i < u.size(); ++i) {
        work += f[i] * u[i];
    }
    return 0.5 * work;
}

}  // namespace fea
