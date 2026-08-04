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
// DYNAMICS -- Mass matrix, Newmark-beta, Modal analysis
// ==========================================================================
//
// Time-domain structural dynamics:
//   M * u'' + C * u' + K * u = f(t)
//
// where M = mass matrix, C = damping matrix, K = stiffness matrix
//
// Consistent mass matrix for Q4:
//   M = integral(rho * N^T * N * detJ * t * dxi * deta)
//
// Newmark-beta method:
//   u_{n+1} = u_n + dt*v_n + (dt^2/2)*((1-2*beta)*a_n + 2*beta*a_{n+1})
//   v_{n+1} = v_n + dt*((1-gamma)*a_n + gamma*a_{n+1})
//   where beta and gamma control stability and accuracy:
//     beta=1/4, gamma=1/2: trapezoidal (unconditionally stable, 2nd order)
//     beta=1/6, gamma=1/2: linear acceleration (conditionally stable)
//     beta=0, gamma=1/2: central difference (explicit, conditionally stable)
//
// Modal analysis:
//   K * phi = omega^2 * M * phi
//   Solved via subspace iteration or inverse iteration

namespace dynamics {

// ------------------------------------------------------------------
// Consistent mass matrix for Q4 element (8x8)
// M_ij = integral(rho * N_i * N_j * detJ * t * dxi * deta)
//
// Analytical integration for bilinear Q4:
//   M_consistent = (rho * t * detJ / 4) * [[2,0,1,0,1,0,1,0],
//                                            [0,2,0,1,0,1,0,1],
//                                            [1,0,2,0,1,0,1,0],
//                                            ...]
// ------------------------------------------------------------------
struct Q4MassMatrix {
    static std::array<std::array<double, 8>, 8> consistent(
        const std::array<Node, 4>& elem_nodes,
        double rho, double t) {

        std::array<std::array<double, 8>, 8> M{};

        const double GP = 1.0 / std::sqrt(3.0);
        const double gp2[2] = { -GP, GP };
        const double gw2[2] = { 1.0, 1.0 };

        static const double xi_pts[4]  = { -1.0,  1.0,  1.0, -1.0 };
        static const double eta_pts[4] = { -1.0, -1.0,  1.0,  1.0 };

        for (int gi = 0; gi < 2; ++gi) {
            for (int gj = 0; gj < 2; ++gj) {
                double xi = gp2[gi], eta = gp2[gj];

                // Compute Jacobian at this Gauss point
                double J11 = 0.0, J12 = 0.0, J21 = 0.0, J22 = 0.0;
                for (int i = 0; i < 4; ++i) {
                    double dN_dxi  = 0.25 * xi_pts[i]  * (1.0 + eta_pts[i] * eta);
                    double dN_deta = 0.25 * (1.0 + xi_pts[i] * xi) * eta_pts[i];
                    J11 += dN_dxi  * elem_nodes[i].x;
                    J12 += dN_dxi  * elem_nodes[i].y;
                    J21 += dN_deta * elem_nodes[i].x;
                    J22 += dN_deta * elem_nodes[i].y;
                }
                double detJ = std::abs(J11 * J22 - J12 * J21);

                // Shape functions at this Gauss point
                double N[4];
                for (int i = 0; i < 4; ++i) {
                    N[i] = 0.25 * (1.0 + xi_pts[i] * xi) * (1.0 + eta_pts[i] * eta);
                }

                double factor = rho * t * detJ * gw2[gi] * gw2[gj];

                // M += N^T * N * factor (block diagonal: x-x and y-y only)
                for (int i = 0; i < 4; ++i) {
                    for (int j = 0; j < 4; ++j) {
                        double val = N[i] * N[j] * factor;
                        M[2*i][2*j]       += val;   // x-x coupling
                        M[2*i+1][2*j+1]   += val;  // y-y coupling
                    }
                }
            }
        }

        return M;
    }

    // Lumped mass matrix (diagonal, simpler but less accurate)
    // M_lumped = (total_mass / num_nodes) * I
    static std::array<std::array<double, 8>, 8> lumped(
        const std::array<Node, 4>& elem_nodes,
        double rho, double t) {

        // Element area
        double J11 = elem_nodes[1].x - elem_nodes[0].x;
        double J12 = elem_nodes[1].y - elem_nodes[0].y;
        double J21 = elem_nodes[3].x - elem_nodes[0].x;
        double J22 = elem_nodes[3].y - elem_nodes[0].y;
        double area = 0.5 * std::abs(J11 * J22 - J12 * J21);

        double total_mass = rho * area * t;
        double mass_per_node = total_mass / 4.0;

        std::array<std::array<double, 8>, 8> M{};
        for (int i = 0; i < 8; ++i) {
            M[i][i] = mass_per_node;
        }
        return M;
    }
};

// ------------------------------------------------------------------
// Assemble global mass matrix
// ------------------------------------------------------------------
inline CSRMatrix assemble_mass(
    const Mesh& m,
    bool use_lumped = false) {

    int ndof = m.num_dofs();
    COOMatrix M_coo(ndof, ndof);

    for (int e = 0; e < m.num_quads(); ++e) {
        const auto& elem = m.quad_elements[e];
        std::array<Node, 4> elem_nodes;
        for (int i = 0; i < 4; ++i) {
            elem_nodes[i] = m.nodes[elem[i]];
        }

        auto Me = use_lumped ?
            Q4MassMatrix::lumped(elem_nodes, m.mat.rho, m.mat.t) :
            Q4MassMatrix::consistent(elem_nodes, m.mat.rho, m.mat.t);

        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                M_coo.add(dof_index(elem[i], 0), dof_index(elem[j], 0), Me[2*i][2*j]);
                M_coo.add(dof_index(elem[i], 0), dof_index(elem[j], 1), Me[2*i][2*j+1]);
                M_coo.add(dof_index(elem[i], 1), dof_index(elem[j], 0), Me[2*i+1][2*j]);
                M_coo.add(dof_index(elem[i], 1), dof_index(elem[j], 1), Me[2*i+1][2*j+1]);
            }
        }
    }

    return M_coo.to_csr();
}

// ------------------------------------------------------------------
// Rayleigh damping: C = alpha * M + beta * K
// ------------------------------------------------------------------
inline CSRMatrix assemble_damping(
    const CSRMatrix& M,
    const CSRMatrix& K,
    double alpha = 0.0,   // mass-proportional damping
    double beta = 0.0) {  // stiffness-proportional damping

    int n = M.nrows;
    COOMatrix C_coo(n, n);

    for (int i = 0; i < n; ++i) {
        for (int k = M.row_ptr[i]; k < M.row_ptr[i + 1]; ++k) {
            C_coo.add(i, M.col_ind[k], alpha * M.values[k]);
        }
        for (int k = K.row_ptr[i]; k < K.row_ptr[i + 1]; ++k) {
            C_coo.add(i, K.col_ind[k], beta * K.values[k]);
        }
    }

    return C_coo.to_csr();
}

// ------------------------------------------------------------------
// Newmark-beta time integration
//
// At each time step:
//   1. Predict: u_pred = u_n + dt*v_n + (dt^2/2)*((1-2*beta)*a_n)
//   2. Predict: v_pred = v_n + dt*(1-gamma)*a_n
//   3. Solve: (M/dt^2*beta + gamma/dt*beta*C + K) * u_{n+1} = f_{n+1} + ...
//   4. Update: a_{n+1} = (u_{n+1} - u_pred) / (dt^2*beta)
//   5. Update: v_{n+1} = v_pred + dt*gamma*a_{n+1}
// ------------------------------------------------------------------
struct NewmarkResult {
    std::vector<std::vector<double>> displacement_history;  // u at each time step
    std::vector<std::vector<double>> velocity_history;      // v at each time step
    std::vector<std::vector<double>> acceleration_history;  // a at each time step
    std::vector<double> time;
    int num_steps;
};

struct NewmarkConfig {
    double beta = 0.25;   // trapezoidal rule (default)
    double gamma = 0.5;   // trapezoidal rule (default)
    double dt = 0.01;     // time step
    double t_final = 1.0; // final time
    bool use_lumped_mass = true;  // lumped mass for efficiency
};

inline NewmarkResult newmark_beta(
    const Mesh& m,
    const CSRMatrix& K,
    std::vector<double> f_ext,  // constant external force
    NewmarkConfig config = NewmarkConfig{}) {

    int ndof = m.num_dofs();
    int num_steps = static_cast<int>(config.t_final / config.dt) + 1;

    // Assemble mass matrix
    std::cout << "Assembling mass matrix..." << std::endl;
    auto M = assemble_mass(m, config.use_lumped_mass);

    // Apply penalty for Dirichlet BCs to M, K
    double penalty = 1e8;
    COOMatrix M_coo(ndof, ndof);
    COOMatrix K_coo(ndof, ndof);

    for (int i = 0; i < ndof; ++i) {
        for (int k = M.row_ptr[i]; k < M.row_ptr[i + 1]; ++k) {
            M_coo.add(i, M.col_ind[k], M.values[k]);
        }
        for (int k = K.row_ptr[i]; k < K.row_ptr[i + 1]; ++k) {
            K_coo.add(i, K.col_ind[k], K.values[k]);
        }
    }

    for (const auto& bc : m.dirichlet) {
        int dof = dof_index(bc.node, bc.dof);
        M_coo.add(dof, dof, penalty);
        K_coo.add(dof, dof, penalty);
        f_ext[dof] = penalty * bc.value;
    }

    auto M_csr = M_coo.to_csr();
    auto K_csr = K_coo.to_csr();

    // Effective stiffness: K_eff = K + (1/(beta*dt^2)) * M
    // For simplicity, assemble K_eff = K + M / (beta * dt^2)
    double dt = config.dt;
    double beta = config.beta;
    double gamma = config.gamma;

    COOMatrix K_eff_coo(ndof, ndof);
    double mass_factor = 1.0 / (beta * dt * dt);

    for (int i = 0; i < ndof; ++i) {
        // K_eff = K + mass_factor * M
        for (int k = K_csr.row_ptr[i]; k < K_csr.row_ptr[i + 1]; ++k) {
            K_eff_coo.add(i, K_csr.col_ind[k], K_csr.values[k]);
        }
        for (int k = M_csr.row_ptr[i]; k < M_csr.row_ptr[i + 1]; ++k) {
            K_eff_coo.add(i, M_csr.col_ind[k], mass_factor * M_csr.values[k]);
        }
    }

    auto K_eff = K_eff_coo.to_csr();

    // Initialize
    NewmarkResult result;
    result.num_steps = num_steps;
    result.displacement_history.resize(num_steps + 1, std::vector<double>(ndof, 0.0));
    result.velocity_history.resize(num_steps + 1, std::vector<double>(ndof, 0.0));
    result.acceleration_history.resize(num_steps + 1, std::vector<double>(ndof, 0.0));
    result.time.resize(num_steps + 1);

    // Initial acceleration: a_0 = M^{-1} * (f - K*u_0 - C*v_0)
    // For simplicity: a_0 = 0 (starts from rest)
    std::vector<double> u(ndof, 0.0);
    std::vector<double> v(ndof, 0.0);
    std::vector<double> a(ndof, 0.0);

    result.time[0] = 0.0;

    std::cout << "Running Newmark-beta integration (" << num_steps << " steps)..." << std::endl;

    for (int step = 0; step < num_steps; ++step) {
        double t = (step + 1) * dt;
        result.time[step + 1] = t;

        // Predict
        std::vector<double> u_pred(ndof), v_pred(ndof);
        for (int i = 0; i < ndof; ++i) {
            u_pred[i] = u[i] + dt * v[i] + 0.5 * dt * dt * (1.0 - 2.0 * beta) * a[i];
            v_pred[i] = v[i] + dt * (1.0 - gamma) * a[i];
        }

        // Effective RHS: f_eff = f_ext + M * (u_pred / (beta*dt^2))
        std::vector<double> f_eff(ndof, 0.0);
        for (int i = 0; i < ndof; ++i) {
            f_eff[i] = f_ext[i];
        }

        // Add mass contribution: M * u_pred * mass_factor
        for (int i = 0; i < ndof; ++i) {
            double mass_contrib = 0.0;
            for (int k = M_csr.row_ptr[i]; k < M_csr.row_ptr[i + 1]; ++k) {
                mass_contrib += M_csr.values[k] * u_pred[M_csr.col_ind[k]];
            }
            f_eff[i] += mass_contrib * mass_factor;
        }

        // Apply Dirichlet BCs to RHS
        for (const auto& bc : m.dirichlet) {
            int dof = dof_index(bc.node, bc.dof);
            f_eff[dof] = penalty * bc.value;
        }

        // Solve K_eff * u_{n+1} = f_eff
        CGSolver cg(1000, 1e-8);
        preconditioners::Jacobi M_prec;
        M_prec.setup(K_eff);
        auto solve_result = cg.solve(K_eff, f_eff, M_prec);

        if (!solve_result.converged && step % 100 == 0) {
            std::cout << "  Step " << step << ": CG did not converge" << std::endl;
        }

        u = solve_result.x;

        // Update acceleration and velocity
        for (int i = 0; i < ndof; ++i) {
            a[i] = (u[i] - u_pred[i]) / (beta * dt * dt);
            v[i] = v_pred[i] + gamma * dt * a[i];
        }

        // Store results
        result.displacement_history[step + 1] = u;
        result.velocity_history[step + 1] = v;
        result.acceleration_history[step + 1] = a;

        if (step % 100 == 0) {
            double max_disp = 0.0;
            for (int i = 0; i < ndof; ++i) {
                max_disp = std::max(max_disp, std::abs(u[i]));
            }
            std::cout << "  Step " << step + 1 << "/" << num_steps
                      << " t=" << std::fixed << std::setprecision(3) << t
                      << " max_disp=" << std::scientific << max_disp << std::endl;
        }
    }

    std::cout << "Newmark-beta complete." << std::endl;
    return result;
}

// ------------------------------------------------------------------
// Modal analysis via subspace iteration
//
// Solves: K * phi = omega^2 * M * phi
//
// Algorithm:
//   1. Initialize random vectors X (n x n_modes)
//   2. Iterate:
//      a. Solve K * Y = M * X
//      b. Orthogonalize Y (Gram-Schmidt with M)
//      c. X = Y / ||Y||_M
//   3. Eigenvalues: omega_i^2 = (X^T * K * X) / (X^T * M * X)
// ------------------------------------------------------------------
struct ModalResult {
    std::vector<double> natural_frequencies;   // omega (rad/s)
    std::vector<double> frequencies_hz;        // f = omega / (2*pi)
    std::vector<std::vector<double>> mode_shapes;  // phi_i (displacement vectors)
    int num_modes;
};

inline ModalResult modal_analysis(
    const Mesh& m,
    const CSRMatrix& K,
    int num_modes = 10,
    int max_iterations = 100,
    double tolerance = 1e-8) {

    int ndof = m.num_dofs();
    num_modes = std::min(num_modes, ndof);

    std::cout << "Modal analysis: " << num_modes << " modes, " << ndof << " DOFs" << std::endl;

    // Assemble mass matrix
    auto M = assemble_mass(m, true);  // lumped mass for efficiency

    // Apply penalty for Dirichlet BCs
    COOMatrix K_coo(ndof, ndof);
    COOMatrix M_coo(ndof, ndof);
    for (int i = 0; i < ndof; ++i) {
        for (int k = K.row_ptr[i]; k < K.row_ptr[i + 1]; ++k) {
            K_coo.add(i, K.col_ind[k], K.values[k]);
        }
        for (int k = M.row_ptr[i]; k < M.row_ptr[i + 1]; ++k) {
            M_coo.add(i, M.col_ind[k], M.values[k]);
        }
    }

    double penalty = 1e8;
    for (const auto& bc : m.dirichlet) {
        int dof = dof_index(bc.node, bc.dof);
        K_coo.add(dof, dof, penalty);
        M_coo.add(dof, dof, penalty);
    }

    auto K_csr = K_coo.to_csr();
    auto M_csr = M_coo.to_csr();

    // Subspace iteration
    // X is n x num_modes (column-major stored as vector of vectors)
    std::vector<std::vector<double>> X(ndof, std::vector<double>(num_modes, 0.0));

    // Initialize with random values (simplified: use identity-like)
    for (int j = 0; j < num_modes; ++j) {
        // Set mode j to have support near node j*ndof/num_modes
        int start_dof = (j * ndof) / num_modes;
        int end_dof = std::min(start_dof + ndof / num_modes, ndof);
        for (int i = start_dof; i < end_dof; ++i) {
            X[i][j] = 1.0;
        }
    }

    // M-orthonormalize initial vectors
    for (int j = 0; j < num_modes; ++j) {
        // Compute M-norm
        double norm_M = 0.0;
        for (int i = 0; i < ndof; ++i) {
            double mx = 0.0;
            for (int k = M_csr.row_ptr[i]; k < M_csr.row_ptr[i + 1]; ++k) {
                mx += M_csr.values[k] * X[M_csr.col_ind[k]][j];
            }
            norm_M += X[i][j] * mx;
        }
        norm_M = std::sqrt(std::abs(norm_M));

        if (norm_M > 1e-15) {
            for (int i = 0; i < ndof; ++i) X[i][j] /= norm_M;
        }

        // M-orthogonalize against previous modes
        for (int k = 0; k < j; ++k) {
            double dot = 0.0;
            for (int i = 0; i < ndof; ++i) {
                double mx = 0.0;
                for (int p = M_csr.row_ptr[i]; p < M_csr.row_ptr[i + 1]; ++p) {
                    mx += M_csr.values[p] * X[M_csr.col_ind[p]][k];
                }
                dot += X[i][j] * mx;
            }
            for (int i = 0; i < ndof; ++i) X[i][j] -= dot * X[i][k];
        }
    }

    // Iteration
    for (int iter = 0; iter < max_iterations; ++iter) {
        // Y = K^{-1} * M * X (solve K * Y = M * X for each mode)
        std::vector<std::vector<double>> Y(ndof, std::vector<double>(num_modes, 0.0));

        for (int j = 0; j < num_modes; ++j) {
            // Compute M * X_j
            std::vector<double> rhs(ndof, 0.0);
            for (int i = 0; i < ndof; ++i) {
                double sum = 0.0;
                for (int k = M_csr.row_ptr[i]; k < M_csr.row_ptr[i + 1]; ++k) {
                    sum += M_csr.values[k] * X[M_csr.col_ind[k]][j];
                }
                rhs[i] = sum;
            }

            // Solve K * y = rhs
            CGSolver cg(500, 1e-6);
            preconditioners::Jacobi M_prec;
            M_prec.setup(K_csr);
            auto result = cg.solve(K_csr, rhs, M_prec);

            for (int i = 0; i < ndof; ++i) Y[i][j] = result.x[i];
        }

        // M-orthonormalize Y using modified Gram-Schmidt
        for (int j = 0; j < num_modes; ++j) {
            // M-orthogonalize against all previous modes
            for (int k = 0; k < j; ++k) {
                double dot = 0.0;
                for (int i = 0; i < ndof; ++i) {
                    double my = 0.0;
                    for (int p = M_csr.row_ptr[i]; p < M_csr.row_ptr[i + 1]; ++p) {
                        my += M_csr.values[p] * Y[M_csr.col_ind[p]][k];
                    }
                    dot += Y[i][j] * my;
                }
                for (int i = 0; i < ndof; ++i) Y[i][j] -= dot * Y[i][k];
            }

            // Compute M-norm
            double norm_M = 0.0;
            for (int i = 0; i < ndof; ++i) {
                double my = 0.0;
                for (int k = M_csr.row_ptr[i]; k < M_csr.row_ptr[i + 1]; ++k) {
                    my += M_csr.values[k] * Y[M_csr.col_ind[k]][j];
                }
                norm_M += Y[i][j] * my;
            }
            norm_M = std::sqrt(std::abs(norm_M));

            if (norm_M > 1e-15) {
                for (int i = 0; i < ndof; ++i) Y[i][j] /= norm_M;
            }
        }

        // Check convergence: ||Y - X||_M
        double diff_norm = 0.0;
        for (int j = 0; j < num_modes; ++j) {
            for (int i = 0; i < ndof; ++i) {
                double d = Y[i][j] - X[i][j];
                diff_norm += d * d;
            }
        }
        diff_norm = std::sqrt(diff_norm);

        X = Y;

        if (diff_norm < tolerance) {
            std::cout << "  Subspace iteration converged in " << iter + 1 << " iterations" << std::endl;
            break;
        }
    }

    // Compute eigenvalues: omega_i^2 = (X^T * K * X) / (X^T * M * X)
    ModalResult result;
    result.num_modes = num_modes;
    result.natural_frequencies.resize(num_modes);
    result.frequencies_hz.resize(num_modes);
    result.mode_shapes.resize(num_modes, std::vector<double>(ndof));

    for (int j = 0; j < num_modes; ++j) {
        // K * X_j
        std::vector<double> Kx(ndof, 0.0);
        for (int i = 0; i < ndof; ++i) {
            double sum = 0.0;
            for (int k = K_csr.row_ptr[i]; k < K_csr.row_ptr[i + 1]; ++k) {
                sum += K_csr.values[k] * X[K_csr.col_ind[k]][j];
            }
            Kx[i] = sum;
        }

        // M * X_j
        std::vector<double> Mx(ndof, 0.0);
        for (int i = 0; i < ndof; ++i) {
            double sum = 0.0;
            for (int k = M_csr.row_ptr[i]; k < M_csr.row_ptr[i + 1]; ++k) {
                sum += M_csr.values[k] * X[M_csr.col_ind[k]][j];
            }
            Mx[i] = sum;
        }

        double xKx = 0.0, xMx = 0.0;
        for (int i = 0; i < ndof; ++i) {
            xKx += X[i][j] * Kx[i];
            xMx += X[i][j] * Mx[i];
        }

        double omega_sq = (xMx > 1e-30) ? xKx / xMx : 0.0;
        result.natural_frequencies[j] = std::sqrt(std::abs(omega_sq));
        result.frequencies_hz[j] = result.natural_frequencies[j] / (2.0 * M_PI);

        // Store mode shape
        for (int i = 0; i < ndof; ++i) {
            result.mode_shapes[j][i] = X[i][j];
        }

        std::cout << "  Mode " << j + 1
                  << ": f=" << std::fixed << std::setprecision(2) << result.frequencies_hz[j] << " Hz"
                  << ", omega=" << std::scientific << result.natural_frequencies[j] << " rad/s"
                  << std::endl;
    }

    return result;
}

}  // namespace dynamics
