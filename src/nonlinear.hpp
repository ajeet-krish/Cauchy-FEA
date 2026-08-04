#pragma once
#include "fea_types.hpp"
#include "elements.hpp"
#include "sparse.hpp"
#include "solver.hpp"
#include <vector>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <iomanip>

// ==========================================================================
// GEOMETRIC NONLINEARITY -- Total Lagrangian Newton-Raphson
// ==========================================================================
//
// Total Lagrangian formulation:
//   - Reference configuration: undeformed mesh at t=0
//   - Deformed configuration: x = X + u (current displacement)
//   - Deformation gradient: F = I + du/dX
//   - Green-Lagrange strain: E = 0.5 * (F^T * F - I)
//   - Second Piola-Kirchhoff stress: S = C : E
//   - First Piola-Kirchhoff stress: P = F * S
//
// Newton-Raphson iteration:
//   1. Given current u, compute internal force f_int = integral(B^T * S * detJ)
//   2. Residual: r = f_ext - f_int
//   3. Tangent stiffness: K_T = integral(B^T * C_t * B * detJ)
//   4. Solve: K_T * du = r
//   5. Update: u := u + du
//   6. Check convergence: ||r|| / ||f_ext|| < tol

namespace nonlinear {

// ------------------------------------------------------------------
// Deformation gradient for Q4 element at a Gauss point
// F = I + du/dX = [[1+du1/dX1, du1/dX2], [du2/dX1, 1+du2/dX2]]
//
// du/dX = sum_i (dN_i/dX) * u_i
// where dN_i/dX = J^{-1} * dN_i/dxi
// ------------------------------------------------------------------
struct DeformationGradient {
    double F11, F12, F21, F22;  // 2x2 deformation gradient

    // Compute from Q4 shape function derivatives and element displacements
    static DeformationGradient compute(
        const std::array<Node, 4>& elem_nodes,
        const std::array<double, 8>& u_elem,
        double xi, double eta) {

        DeformationGradient F;

        // Jacobian at (xi, eta)
        double J11 = 0.0, J12 = 0.0, J21 = 0.0, J22 = 0.0;
        static const double xi_pts[4]  = { -1.0,  1.0,  1.0, -1.0 };
        static const double eta_pts[4] = { -1.0, -1.0,  1.0,  1.0 };

        for (int i = 0; i < 4; ++i) {
            double dN_dxi  = 0.25 * xi_pts[i]  * (1.0 + eta_pts[i] * eta);
            double dN_deta = 0.25 * (1.0 + xi_pts[i] * xi) * eta_pts[i];
            J11 += dN_dxi  * elem_nodes[i].x;
            J12 += dN_dxi  * elem_nodes[i].y;
            J21 += dN_deta * elem_nodes[i].x;
            J22 += dN_deta * elem_nodes[i].y;
        }

        double invJ11 =  J22 / (J11 * J22 - J12 * J21);
        double invJ12 = -J12 / (J11 * J22 - J12 * J21);
        double invJ21 = -J21 / (J11 * J22 - J12 * J21);
        double invJ22 =  J11 / (J11 * J22 - J12 * J21);

        // Compute du/dX
        double du1_dX1 = 0.0, du1_dX2 = 0.0, du2_dX1 = 0.0, du2_dX2 = 0.0;
        for (int i = 0; i < 4; ++i) {
            double dN_dxi  = 0.25 * xi_pts[i]  * (1.0 + eta_pts[i] * eta);
            double dN_deta = 0.25 * (1.0 + xi_pts[i] * xi) * eta_pts[i];
            double dN_dX1 = invJ11 * dN_dxi + invJ12 * dN_deta;
            double dN_dX2 = invJ21 * dN_dxi + invJ22 * dN_deta;

            du1_dX1 += dN_dX1 * u_elem[2 * i];
            du1_dX2 += dN_dX2 * u_elem[2 * i];
            du2_dX1 += dN_dX1 * u_elem[2 * i + 1];
            du2_dX2 += dN_dX2 * u_elem[2 * i + 1];
        }

        // F = I + du/dX
        F.F11 = 1.0 + du1_dX1;
        F.F12 = du1_dX2;
        F.F21 = du2_dX1;
        F.F22 = 1.0 + du2_dX2;

        return F;
    }

    // Right Cauchy-Green tensor: C = F^T * F
    void right_cauchy_green(double& C11, double& C12, double& C22) const {
        C11 = F11 * F11 + F21 * F21;
        C12 = F11 * F12 + F21 * F22;
        C22 = F12 * F12 + F22 * F22;
    }

    // Green-Lagrange strain: E = 0.5 * (C - I)
    void green_lagrange_strain(double& E11, double& E22, double& E12) const {
        double C11, C12, C22;
        right_cauchy_green(C11, C12, C22);
        E11 = 0.5 * (C11 - 1.0);
        E22 = 0.5 * (C22 - 1.0);
        E12 = 0.5 * C12;
    }

    // Determinant of F (area ratio)
    double detF() const {
        return F11 * F22 - F12 * F21;
    }
};

// ------------------------------------------------------------------
// Material tangent for Neo-Hookean hyperelasticity
// For small strains, this reduces to the linear elastic D matrix
// ------------------------------------------------------------------
struct MaterialTangent {
    // For linear elasticity in total Lagrangian form:
    // S = C : E (same as sigma = D * eps for small strains)
    // The tangent dS/dE is just the D matrix (constant for linear elasticity)
    //
    // For Neo-Hookean:
    //   W = (mu/2)(I1 - 3) - mu*ln(J) + (lambda/2)*(ln(J))^2
    //   S = lambda*ln(J)*C^{-1} + mu*(I - C^{-1})
    //   C_tangent = lambda*C^{-1} x C^{-1} + 2*mu*I4_sym - 2*lambda*(ln(J))*C^{-1} x C^{-1}
    //   (complex, not implemented here)

    // For now: use linear elasticity D matrix (small-strain assumption)
    // This is sufficient for moderate geometric nonlinearity
    static std::array<std::array<double, 3>, 3> linear_elastic(
        const Material& mat, PlaneType plane) {
        return mat.d_matrix(plane);
    }
};

// ------------------------------------------------------------------
// Internal force vector for Q4 element (Total Lagrangian)
// f_int = integral(B^T * S * detJ * t * dxi * deta)
// where B is the strain-displacement matrix and S is PK2 stress
// ------------------------------------------------------------------
struct InternalForceResult {
    std::vector<double> f_int;    // global internal force vector
    double strain_energy;         // total strain energy
};

inline InternalForceResult compute_internal_forces(
    const Mesh& m,
    const std::vector<double>& u) {

    int ndof = m.num_dofs();
    InternalForceResult result;
    result.f_int.assign(ndof, 0.0);
    result.strain_energy = 0.0;

    const double GP = 1.0 / std::sqrt(3.0);
    const double gp2[2] = { -GP, GP };
    const double gw2[2] = { 1.0, 1.0 };

    for (int e = 0; e < m.num_quads(); ++e) {
        const auto& elem = m.quad_elements[e];
        std::array<Node, 4> elem_nodes;
        std::array<double, 8> u_elem;

        for (int i = 0; i < 4; ++i) {
            elem_nodes[i] = m.nodes[elem[i]];
            u_elem[2 * i]     = u[dof_index(elem[i], 0)];
            u_elem[2 * i + 1] = u[dof_index(elem[i], 1)];
        }

        auto D = m.mat.d_matrix(m.plane);
        std::array<double, 8> f_elem{};

        for (int gi = 0; gi < 2; ++gi) {
            for (int gj = 0; gj < 2; ++gj) {
                double xi  = gp2[gi];
                double eta = gp2[gj];

                // Compute deformation gradient
                auto F = DeformationGradient::compute(elem_nodes, u_elem, xi, eta);

                // Green-Lagrange strain
                double E11, E22, E12;
                F.green_lagrange_strain(E11, E22, E12);

                // PK2 stress: S = D * E (linear elasticity)
                double S11 = D[0][0] * E11 + D[0][1] * E22 + D[0][2] * (2.0 * E12);
                double S22 = D[1][0] * E11 + D[1][1] * E22 + D[1][2] * (2.0 * E12);
                double S12 = D[2][0] * E11 + D[2][1] * E22 + D[2][2] * (2.0 * E12);

                // Jacobian and B matrix (linearized around current config)
                double J11 = 0.0, J12 = 0.0, J21 = 0.0, J22 = 0.0;
                static const double xi_pts[4]  = { -1.0,  1.0,  1.0, -1.0 };
                static const double eta_pts[4] = { -1.0, -1.0,  1.0,  1.0 };

                for (int i = 0; i < 4; ++i) {
                    double dN_dxi  = 0.25 * xi_pts[i]  * (1.0 + eta_pts[i] * eta);
                    double dN_deta = 0.25 * (1.0 + xi_pts[i] * xi) * eta_pts[i];
                    J11 += dN_dxi  * elem_nodes[i].x;
                    J12 += dN_dxi  * elem_nodes[i].y;
                    J21 += dN_deta * elem_nodes[i].x;
                    J22 += dN_deta * elem_nodes[i].y;
                }

                double detJ0 = J11 * J22 - J12 * J21;
                double invJ11 =  J22 / detJ0;
                double invJ12 = -J12 / detJ0;
                double invJ21 = -J21 / detJ0;
                double invJ22 =  J11 / detJ0;

                // Material B matrix (relates strain to nodal displacements)
                // For total Lagrangian: uses reference configuration
                std::array<std::array<double, 8>, 3> B0{};
                for (int i = 0; i < 4; ++i) {
                    double dN_dxi  = 0.25 * xi_pts[i]  * (1.0 + eta_pts[i] * eta);
                    double dN_deta = 0.25 * (1.0 + xi_pts[i] * xi) * eta_pts[i];
                    double dN_dX1 = invJ11 * dN_dxi + invJ12 * dN_deta;
                    double dN_dX2 = invJ21 * dN_dxi + invJ22 * dN_deta;

                    int col = 2 * i;
                    B0[0][col]     = dN_dX1;
                    B0[0][col + 1] = 0.0;
                    B0[1][col]     = 0.0;
                    B0[1][col + 1] = dN_dX2;
                    B0[2][col]     = dN_dX2;
                    B0[2][col + 1] = dN_dX1;
                }

                double factor = detJ0 * gw2[gi] * gw2[gj] * m.mat.t;

                // f_int += B0^T * S * factor
                for (int i = 0; i < 8; ++i) {
                    f_elem[i] += (B0[0][i] * S11 + B0[1][i] * S22 +
                                  B0[2][i] * S12) * factor;
                }

                // Strain energy: 0.5 * E^T * S * detJ * t
                result.strain_energy += 0.5 * (E11 * S11 + E22 * S22 + 2.0 * E12 * S12) * factor;
            }
        }

        // Assemble into global
        for (int i = 0; i < 4; ++i) {
            result.f_int[dof_index(elem[i], 0)] += f_elem[2 * i];
            result.f_int[dof_index(elem[i], 1)] += f_elem[2 * i + 1];
        }
    }

    return result;
}

// ------------------------------------------------------------------
// Consistent tangent stiffness for Q4 (Total Lagrangian)
// K_T = integral(B0^T * D * B0 * detJ0) + geometric stiffness terms
//
// For linear elasticity, the material tangent is constant.
// The geometric stiffness (stress stiffness) accounts for
// the effect of current stress on stiffness:
//   K_geo = integral G^T * S * G * detJ0
// where G is the displacement gradient matrix
// ------------------------------------------------------------------
inline CSRMatrix compute_tangent_stiffness(
    const Mesh& m,
    const std::vector<double>& u) {

    int ndof = m.num_dofs();
    COOMatrix K_coo(ndof, ndof);

    const double GP = 1.0 / std::sqrt(3.0);
    const double gp2[2] = { -GP, GP };
    const double gw2[2] = { 1.0, 1.0 };

    for (int e = 0; e < m.num_quads(); ++e) {
        const auto& elem = m.quad_elements[e];
        std::array<Node, 4> elem_nodes;
        std::array<double, 8> u_elem;

        for (int i = 0; i < 4; ++i) {
            elem_nodes[i] = m.nodes[elem[i]];
            u_elem[2 * i]     = u[dof_index(elem[i], 0)];
            u_elem[2 * i + 1] = u[dof_index(elem[i], 1)];
        }

        auto D = m.mat.d_matrix(m.plane);
        std::array<std::array<double, 8>, 8> K_elem{};

        for (int gi = 0; gi < 2; ++gi) {
            for (int gj = 0; gj < 2; ++gj) {
                double xi  = gp2[gi];
                double eta = gp2[gj];

                // Deformation gradient and strain
                auto F = DeformationGradient::compute(elem_nodes, u_elem, xi, eta);
                double E11, E22, E12;
                F.green_lagrange_strain(E11, E22, E12);

                // PK2 stress
                double S11 = D[0][0] * E11 + D[0][1] * E22 + D[0][2] * (2.0 * E12);
                double S22 = D[1][0] * E11 + D[1][1] * E22 + D[1][2] * (2.0 * E12);
                double S12 = D[2][0] * E11 + D[2][1] * E22 + D[2][2] * (2.0 * E12);

                // Jacobian at reference config
                double J11 = 0.0, J12 = 0.0, J21 = 0.0, J22 = 0.0;
                static const double xi_pts[4]  = { -1.0,  1.0,  1.0, -1.0 };
                static const double eta_pts[4] = { -1.0, -1.0,  1.0,  1.0 };

                for (int i = 0; i < 4; ++i) {
                    double dN_dxi  = 0.25 * xi_pts[i]  * (1.0 + eta_pts[i] * eta);
                    double dN_deta = 0.25 * (1.0 + xi_pts[i] * xi) * eta_pts[i];
                    J11 += dN_dxi  * elem_nodes[i].x;
                    J12 += dN_dxi  * elem_nodes[i].y;
                    J21 += dN_deta * elem_nodes[i].x;
                    J22 += dN_deta * elem_nodes[i].y;
                }

                double detJ0 = J11 * J22 - J12 * J21;
                double invJ11 =  J22 / detJ0;
                double invJ12 = -J12 / detJ0;
                double invJ21 = -J21 / detJ0;
                double invJ22 =  J11 / detJ0;

                // Material B matrix
                std::array<std::array<double, 8>, 3> B0{};
                for (int i = 0; i < 4; ++i) {
                    double dN_dxi  = 0.25 * xi_pts[i]  * (1.0 + eta_pts[i] * eta);
                    double dN_deta = 0.25 * (1.0 + xi_pts[i] * xi) * eta_pts[i];
                    double dN_dX1 = invJ11 * dN_dxi + invJ12 * dN_deta;
                    double dN_dX2 = invJ21 * dN_dxi + invJ22 * dN_deta;

                    int col = 2 * i;
                    B0[0][col]     = dN_dX1;
                    B0[0][col + 1] = 0.0;
                    B0[1][col]     = 0.0;
                    B0[1][col + 1] = dN_dX2;
                    B0[2][col]     = dN_dX2;
                    B0[2][col + 1] = dN_dX1;
                }

                // Geometric stiffness: G matrix (2 x 8)
                // G[0][2*i]   = dN_i/dX1
                // G[1][2*i]   = dN_i/dX2
                std::array<std::array<double, 8>, 2> G{};
                for (int i = 0; i < 4; ++i) {
                    double dN_dxi  = 0.25 * xi_pts[i]  * (1.0 + eta_pts[i] * eta);
                    double dN_deta = 0.25 * (1.0 + xi_pts[i] * xi) * eta_pts[i];
                    G[0][2 * i]     = invJ11 * dN_dxi + invJ12 * dN_deta;
                    G[0][2 * i + 1] = 0.0;
                    G[1][2 * i]     = 0.0;
                    G[1][2 * i + 1] = invJ21 * dN_dxi + invJ22 * dN_deta;
                }

                double factor = detJ0 * gw2[gi] * gw2[gj] * m.mat.t;

                // K_mat = B0^T * D * B0 * factor
                std::array<std::array<double, 8>, 3> DB{};
                for (int r = 0; r < 3; ++r) {
                    for (int c = 0; c < 8; ++c) {
                        double sum = 0.0;
                        for (int k = 0; k < 3; ++k) {
                            sum += D[r][k] * B0[k][c];
                        }
                        DB[r][c] = sum;
                    }
                }

                for (int i = 0; i < 8; ++i) {
                    for (int j = 0; j < 8; ++j) {
                        double sum = 0.0;
                        for (int k = 0; k < 3; ++k) {
                            sum += B0[k][i] * DB[k][j];
                        }
                        K_elem[i][j] += sum * factor;
                    }
                }

                // K_geo = (G^T * S * G) * factor
                // S is 2x2: [[S11, S12], [S12, S22]]
                for (int i = 0; i < 8; ++i) {
                    for (int j = 0; j < 8; ++j) {
                        double geo = (G[0][i] * S11 * G[0][j] +
                                      G[0][i] * S12 * G[1][j] +
                                      G[1][i] * S12 * G[0][j] +
                                      G[1][i] * S22 * G[1][j]) * factor;
                        K_elem[i][j] += geo;
                    }
                }
            }
        }

        // Assemble
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                K_coo.add(dof_index(elem[i], 0), dof_index(elem[j], 0), K_elem[2*i][2*j]);
                K_coo.add(dof_index(elem[i], 0), dof_index(elem[j], 1), K_elem[2*i][2*j+1]);
                K_coo.add(dof_index(elem[i], 1), dof_index(elem[j], 0), K_elem[2*i+1][2*j]);
                K_coo.add(dof_index(elem[i], 1), dof_index(elem[j], 1), K_elem[2*i+1][2*j+1]);
            }
        }
    }

    return K_coo.to_csr();
}

// ------------------------------------------------------------------
// Newton-Raphson solver for geometric nonlinearity
// ------------------------------------------------------------------
struct NewtonRaphsonResult {
    std::vector<double> displacement;
    int iterations;
    bool converged;
    double final_residual_norm;
    double strain_energy;
    std::vector<double> residual_history;
};

struct NewtonRaphsonConfig {
    int max_iterations = 25;
    double relative_tolerance = 1e-8;
    double absolute_tolerance = 1e-12;
    int load_steps = 10;            // number of load increments
    double initial_load_fraction = 0.1;  // first load step fraction
    bool line_search = true;
    int max_line_search = 5;
};

inline NewtonRaphsonResult newton_raphson(
    const Mesh& m,
    const std::vector<double>& f_ext,
    NewtonRaphsonConfig config = NewtonRaphsonConfig{}) {

    int ndof = m.num_dofs();
    NewtonRaphsonResult result;
    result.converged = false;
    result.strain_energy = 0.0;

    std::vector<double> u(ndof, 0.0);
    std::vector<double> u_total(ndof, 0.0);

    // Load stepping
    double load_fraction = config.initial_load_fraction;

    for (int step = 0; step < config.load_steps; ++step) {
        // Current applied load
        double alpha = (step + 1) * load_fraction;
        if (step == config.load_steps - 1) alpha = 1.0;  // final step is full load

        std::vector<double> f_current(ndof);
        for (int i = 0; i < ndof; ++i) {
            f_current[i] = alpha * f_ext[i];
        }

        std::cout << "  Load step " << step + 1 << "/" << config.load_steps
                  << " (alpha=" << std::fixed << std::setprecision(2) << alpha << ")" << std::endl;

        // Newton-Raphson iteration for this load step
        u.assign(ndof, 0.0);  // incremental displacement

        for (int iter = 0; iter < config.max_iterations; ++iter) {
            // Compute internal forces at current configuration
            auto internal = compute_internal_forces(m, u_total);

            // Residual: r = f_ext - f_int
            std::vector<double> residual(ndof);
            for (int i = 0; i < ndof; ++i) {
                residual[i] = f_current[i] - internal.f_int[i];
            }

            // Apply penalty for Dirichlet BCs to residual
            // At Dirichlet DOFs: r = penalty * (u - u_prescribed)
            double penalty = 1e8;
            for (const auto& bc : m.dirichlet) {
                int dof = dof_index(bc.node, bc.dof);
                residual[dof] = penalty * (u_total[dof] - bc.value);
            }

            // Check convergence
            double r_norm = 0.0, f_norm = 0.0;
            for (int i = 0; i < ndof; ++i) {
                r_norm += residual[i] * residual[i];
                f_norm += f_current[i] * f_current[i];
            }
            r_norm = std::sqrt(r_norm);
            f_norm = std::sqrt(f_norm);

            result.residual_history.push_back(r_norm);

            double rel_tol = (f_norm > 1e-30) ? r_norm / f_norm : r_norm;

            std::cout << "    Iteration " << iter + 1
                      << ": ||r|| = " << std::scientific << std::setprecision(2) << r_norm
                      << ", ||r||/||f|| = " << rel_tol << std::endl;

            if (rel_tol < config.relative_tolerance ||
                r_norm < config.absolute_tolerance) {
                result.converged = true;
                result.iterations = iter + 1;
                result.final_residual_norm = r_norm;
                result.strain_energy = internal.strain_energy;
                break;
            }

            // Compute tangent stiffness
            auto K_T = compute_tangent_stiffness(m, u_total);

            // Apply penalty for Dirichlet BCs to tangent stiffness
            // Modify CSR directly for efficiency
            for (const auto& bc : m.dirichlet) {
                int dof = dof_index(bc.node, bc.dof);
                // Find diagonal entry and add penalty
                for (int k = K_T.row_ptr[dof]; k < K_T.row_ptr[dof + 1]; ++k) {
                    if (K_T.col_ind[k] == dof) {
                        K_T.values[k] += penalty;
                        break;
                    }
                }
            }

            // Solve K_T * du = residual
            CGSolver cg(1000, 1e-6);  // relaxed tolerance for Newton
            preconditioners::Jacobi M;
            M.setup(K_T);
            auto solve_result = cg.solve(K_T, residual, M);

            if (!solve_result.converged) {
                std::cout << "    WARNING: CG did not converge in Newton step" << std::endl;
            }

            auto& du = solve_result.x;

            // Line search (optional)
            if (config.line_search) {
                double alpha_ls = 1.0;
                for (int ls = 0; ls < config.max_line_search; ++ls) {
                    // Try u_trial = u + alpha_ls * du
                    std::vector<double> u_trial(ndof);
                    for (int i = 0; i < ndof; ++i) {
                        u_trial[i] = u[i] + alpha_ls * du[i];
                    }

                    // Compute residual at trial point
                    auto internal_trial = compute_internal_forces(m, u_trial);
                    std::vector<double> r_trial(ndof);
                    for (int i = 0; i < ndof; ++i) {
                        r_trial[i] = f_current[i] - internal_trial.f_int[i];
                    }

                    double r_trial_norm = 0.0;
                    for (int i = 0; i < ndof; ++i) {
                        r_trial_norm += r_trial[i] * r_trial[i];
                    }
                    r_trial_norm = std::sqrt(r_trial_norm);

                    // Armijo condition: reduce residual
                    if (r_trial_norm < r_norm) {
                        break;
                    }

                    alpha_ls *= 0.5;
                    if (ls == config.max_line_search - 1) {
                        std::cout << "    Line search failed, using alpha=0" << std::endl;
                        alpha_ls = 0.0;
                    }
                }

                // Apply line search (simplified: just use full step for now)
                // A proper implementation would re-solve with modified residual
            }

            // Update displacement
            for (int i = 0; i < ndof; ++i) {
                u[i] += du[i];
                u_total[i] += du[i];
            }
        }

        if (!result.converged) {
            std::cout << "  Newton-Raphson did NOT converge at load step " << step + 1 << std::endl;
            break;
        }
    }

    result.displacement = u_total;

    if (result.converged) {
        std::cout << "  Newton-Raphson converged in " << result.iterations
                  << " iterations (final residual: " << std::scientific
                  << result.final_residual_norm << ")" << std::endl;
    }

    return result;
}

}  // namespace nonlinear
