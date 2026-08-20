#include "parametric_engine.hpp"
#include <iostream>
#include <chrono>
#include <algorithm>
#include <cmath>

// ==========================================================================
// PARAMETRIC ENGINE -- Fast rescaling for scale-only parameter changes
// ==========================================================================

void ParametricEngine::captureBaseline(const Mesh& m, const fea::SolveResult& result,
                                       double E, double nu, double t, double forceScale) {
    m_baseline.valid = true;
    m_baseline.E = E;
    m_baseline.nu = nu;
    m_baseline.t = t;
    m_baseline.force_scale = forceScale;
    m_baseline.mesh = m;
    m_baseline.original_result = result;

    // Reassemble to capture the raw COO matrix (pre-penalty)
    // This is a one-time cost after the full solve
    auto K_coo = fea::assemble(m);
    m_baseline.coo_rows = std::move(K_coo.row);
    m_baseline.coo_cols = std::move(K_coo.col);
    m_baseline.coo_vals = std::move(K_coo.val);
    m_baseline.coo_n = K_coo.nrows;

    // Capture the baseline force vector (pre-Dirichlet penalty)
    m_baseline.f_neumann = fea::build_rhs(m);

    std::cout << "[ParametricEngine] Baseline captured: E=" << E
              << ", nu=" << nu << ", t=" << t
              << ", force_scale=" << forceScale
              << ", COO entries=" << m_baseline.coo_vals.size() << std::endl;
}

bool ParametricEngine::canRescale(double newE, double newNu, double newT) const {
    if (!m_baseline.valid) return false;
    // Fast path only when nu is unchanged
    return std::abs(newNu - m_baseline.nu) < 1e-12;
}

fea::SolveResult ParametricEngine::rescale(double newE, double newForce, double newT) {
    fea::SolveResult result;

    if (!m_baseline.valid) {
        std::cerr << "[ParametricEngine] No baseline data; cannot rescale." << std::endl;
        return result;
    }

    auto t_start = std::chrono::high_resolution_clock::now();

    // Compute scale factor for stiffness matrix
    // K depends on E and t (thickness) linearly for 2D plane stress/strain
    double E_ratio = newE / m_baseline.E;
    double t_ratio = newT / m_baseline.t;
    double K_scale = E_ratio * t_ratio;

    // Compute scale factor for force vector
    double f_scale = newForce / m_baseline.force_scale;

    // 1. Create scaled COO matrix
    int n = m_baseline.coo_n;
    COOMatrix K_coo(n, n);
    K_coo.row = m_baseline.coo_rows;
    K_coo.col = m_baseline.coo_cols;
    K_coo.val.resize(m_baseline.coo_vals.size());
    for (size_t k = 0; k < m_baseline.coo_vals.size(); ++k) {
        K_coo.val[k] = m_baseline.coo_vals[k] * K_scale;
    }

    // 2. Compute penalty from scaled matrix
    double K_max = 0.0;
    for (size_t k = 0; k < K_coo.val.size(); ++k) {
        if (K_coo.row[k] == K_coo.col[k]) {
            K_max = std::max(K_max, std::abs(K_coo.val[k]));
        }
    }
    double penalty = std::max(K_max * 1e4, 1e8);

    // 3. Apply Dirichlet BCs (penalty method)
    fea::apply_dirichlet_penalty(K_coo, m_baseline.mesh, penalty);

    // 4. Convert to CSR
    auto K_csr = K_coo.to_csr();

    // 5. Build scaled RHS
    int ndof = n;
    std::vector<double> f(ndof, 0.0);
    for (int k = 0; k < ndof && k < static_cast<int>(m_baseline.f_neumann.size()); ++k) {
        f[k] = m_baseline.f_neumann[k] * f_scale;
    }

    // Apply Dirichlet penalty to RHS
    fea::modify_rhs_dirichlet(f, m_baseline.mesh, penalty);

    // 6. Solve with CG (fast for rescaled systems)
    std::vector<double> u;
    int cg_iters = 0;
    bool cg_conv = false;

    preconditioners::Jacobi M;
    M.setup(K_csr);
    CGSolver cg(10000, 1e-10);
    auto cg_result = cg.solve(K_csr, f, M);
    u = cg_result.x;
    cg_iters = cg_result.iterations;
    cg_conv = cg_result.converged;

    // 7. Post-process: scale stresses from baseline
    // sigma = D * B * u; D scales with E (same nu), u scales with f_scale/K_scale
    // So sigma_new = E_ratio * (f_scale / K_scale) * sigma_base
    double disp_scale = f_scale / K_scale;
    double stress_scale = E_ratio * disp_scale;

    std::vector<postprocess::ElementStress> stresses;
    stresses.reserve(m_baseline.original_result.stresses.size());
    for (const auto& s : m_baseline.original_result.stresses) {
        postprocess::ElementStress scaled;
        scaled.sigma_xx = s.sigma_xx * stress_scale;
        scaled.sigma_yy = s.sigma_yy * stress_scale;
        scaled.sigma_xy = s.sigma_xy * stress_scale;
        scaled.sigma_zz = s.sigma_zz * stress_scale;
        scaled.sigma_yz = s.sigma_yz * stress_scale;
        scaled.sigma_xz = s.sigma_xz * stress_scale;
        scaled.sigma_1 = s.sigma_1 * stress_scale;
        scaled.sigma_2 = s.sigma_2 * stress_scale;
        scaled.sigma_3 = s.sigma_3 * stress_scale;
        scaled.von_mises = s.von_mises * stress_scale;
        stresses.push_back(scaled);
    }

    auto t_end = std::chrono::high_resolution_clock::now();
    double solve_time = std::chrono::duration<double, std::milli>(t_end - t_start).count();

    result.displacement = std::move(u);
    result.stresses = std::move(stresses);
    result.K_csr = std::move(K_csr);
    result.f = std::move(f);
    result.cg_iterations = cg_iters;
    result.solve_time_ms = solve_time;
    result.cg_converged = cg_conv;

    std::cout << "[ParametricEngine] Rescale: K_scale=" << K_scale
              << ", f_scale=" << f_scale << ", disp_scale=" << disp_scale
              << ", stress_scale=" << stress_scale
              << ", solve_time=" << solve_time << " ms" << std::endl;

    return result;
}

void ParametricEngine::invalidate() {
    m_baseline.valid = false;
    m_baseline.coo_rows.clear();
    m_baseline.coo_cols.clear();
    m_baseline.coo_vals.clear();
    m_baseline.f_neumann.clear();
}
