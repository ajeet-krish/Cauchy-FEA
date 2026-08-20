#pragma once
#include "fea.hpp"
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <limits>

// ==========================================================================
// ANALYTICAL FORMULAS -- Compare FEA results against closed-form solutions
// ==========================================================================

struct AnalyticalRow {
    std::string quantity;
    double analytical_value = 0.0;
    double fea_value = 0.0;
    double error_pct = 0.0;
    bool passed = false;
    std::string formula;
};

// ------------------------------------------------------------------
// Helper: mesh bounding box
// ------------------------------------------------------------------
struct MeshBounds {
    double min_x, max_x, min_y, max_y;
};

inline MeshBounds compute_bounds(const Mesh& m) {
    MeshBounds b;
    b.min_x = b.min_y = std::numeric_limits<double>::max();
    b.max_x = b.max_y = std::numeric_limits<double>::lowest();
    for (const auto& node : m.nodes) {
        b.min_x = std::min(b.min_x, node.x);
        b.max_x = std::max(b.max_x, node.x);
        b.min_y = std::min(b.min_y, node.y);
        b.max_y = std::max(b.max_y, node.y);
    }
    return b;
}

// ------------------------------------------------------------------
// Helper: find max absolute value in stress component
// ------------------------------------------------------------------
inline double max_abs_stress(const std::vector<postprocess::ElementStress>& stresses,
                             const std::string& field) {
    double val = 0.0;
    for (const auto& s : stresses) {
        double v = 0.0;
        if (field == "sigma_xx") v = std::abs(s.sigma_xx);
        else if (field == "sigma_yy") v = std::abs(s.sigma_yy);
        else if (field == "sigma_xy") v = std::abs(s.sigma_xy);
        else if (field == "von_mises") v = s.von_mises;
        val = std::max(val, v);
    }
    return val;
}

// ------------------------------------------------------------------
// Helper: find tip node (rightmost node with applied Neumann BC)
// ------------------------------------------------------------------
inline int find_tip_node(const Mesh& m) {
    int tip = -1;
    double max_x = std::numeric_limits<double>::lowest();
    for (const auto& bc : m.neumann) {
        if (m.nodes[bc.node].x > max_x) {
            max_x = m.nodes[bc.node].x;
            tip = bc.node;
        }
    }
    return tip;
}

// ------------------------------------------------------------------
// Helper: find right-edge max displacement from dirichlet BCs
// ------------------------------------------------------------------
inline double find_prescribed_right_displacement(const Mesh& m) {
    MeshBounds b = compute_bounds(m);
    double tol = (b.max_x - b.min_x) * 0.01 + 1e-10;
    for (const auto& bc : m.dirichlet) {
        if (std::abs(m.nodes[bc.node].x - b.max_x) < tol && bc.dof == 0) {
            return bc.value;
        }
    }
    return 0.0;
}

// ------------------------------------------------------------------
// Helper: total applied force magnitude
// ------------------------------------------------------------------
inline double total_applied_force(const Mesh& m) {
    double total = 0.0;
    for (const auto& bc : m.neumann) {
        total += std::abs(bc.value);
    }
    return total;
}

// ------------------------------------------------------------------
// CANTILEVER -- PL^3/(3EI), sigma = PH/(2I)
// ------------------------------------------------------------------
inline std::vector<AnalyticalRow> analytical_cantilever(
    const Mesh& m, const fea::SolveResult& result) {

    std::vector<AnalyticalRow> rows;

    MeshBounds bounds = compute_bounds(m);
    double L = bounds.max_x - bounds.min_x;
    double H = bounds.max_y - bounds.min_y;
    double t = m.mat.t;
    double E = m.mat.E;
    double I = t * H * H * H / 12.0;

    double P = 0.0;
    for (const auto& bc : m.neumann) {
        P += bc.value;
    }

    // Tip deflection
    double delta_exact = P * L * L * L / (3.0 * E * I);
    int tip = find_tip_node(m);
    double delta_fea = 0.0;
    if (tip >= 0) {
        delta_fea = result.displacement[dof_index(tip, 1)];
    }
    double delta_err = (std::abs(delta_exact) > 1e-30)
        ? std::abs(delta_fea - delta_exact) / std::abs(delta_exact) * 100.0
        : 0.0;

    rows.push_back({
        "Tip Deflection uy",
        delta_exact,
        delta_fea,
        delta_err,
        delta_err < 5.0,
        "PL^3/(3EI)"
    });

    // Max sigma_xx
    double sigma_exact = std::abs(P) * H / (2.0 * I);
    double sigma_fea = max_abs_stress(result.stresses, "sigma_xx");
    double sigma_err = (sigma_exact > 1e-30)
        ? std::abs(sigma_fea - sigma_exact) / sigma_exact * 100.0
        : 0.0;

    rows.push_back({
        "Max |sigma_xx|",
        sigma_exact,
        sigma_fea,
        sigma_err,
        sigma_err < 5.0,
        "|P|*H/(2I)"
    });

    // Energy balance
    double U = fea::compute_strain_energy(result.K_csr, result.displacement);
    double W = fea::compute_work_done(result.f, result.displacement);
    double energy_err = (std::abs(W) > 1e-30)
        ? std::abs(U - W) / std::abs(W) * 100.0
        : 0.0;

    rows.push_back({
        "Energy Balance (U vs W)",
        W,
        U,
        energy_err,
        energy_err < 1.0,
        "0.5*u^T*K*u == 0.5*f^T*u"
    });

    return rows;
}

// ------------------------------------------------------------------
// PATCH TEST -- sigma_xx = E * ux_right / L (constant stress)
// ------------------------------------------------------------------
inline std::vector<AnalyticalRow> analytical_patch(
    const Mesh& m, const fea::SolveResult& result) {

    std::vector<AnalyticalRow> rows;

    MeshBounds bounds = compute_bounds(m);
    double L = bounds.max_x - bounds.min_x;

    double ux_right = find_prescribed_right_displacement(m);
    double expected_sigma = m.mat.E * ux_right / L;

    double fea_sigma = 0.0;
    double max_err = 0.0;
    int n = static_cast<int>(result.stresses.size());
    for (const auto& s : result.stresses) {
        double err = std::abs(s.sigma_xx - expected_sigma) / std::abs(expected_sigma);
        max_err = std::max(max_err, err);
        fea_sigma += s.sigma_xx;
    }
    if (n > 0) fea_sigma /= n;

    double error_pct = max_err * 100.0;

    rows.push_back({
        "Mean sigma_xx",
        expected_sigma,
        fea_sigma,
        error_pct,
        error_pct < 5.0,
        "E * ux_right / L"
    });

    // Energy balance
    double U = fea::compute_strain_energy(result.K_csr, result.displacement);
    double W = fea::compute_work_done(result.f, result.displacement);
    double energy_err = (std::abs(W) > 1e-30)
        ? std::abs(U - W) / std::abs(W) * 100.0
        : 0.0;

    rows.push_back({
        "Energy Balance (U vs W)",
        W,
        U,
        energy_err,
        energy_err < 1.0,
        "0.5*u^T*K*u == 0.5*f^T*u"
    });

    return rows;
}

// ------------------------------------------------------------------
// COOK'S MEMBRANE -- reference tip displacement ~13.68 mm
// ------------------------------------------------------------------
inline std::vector<AnalyticalRow> analytical_cook(
    const Mesh& m, const fea::SolveResult& result) {

    std::vector<AnalyticalRow> rows;

    // Standard Cook's membrane: E=1 MPa, nu=1/3, t=1mm, total shear load=1N
    // Reference tip displacement from literature: ~13.68 mm
    double reference_disp = 13.68e-3;  // 13.68 mm in meters

    MeshBounds bounds = compute_bounds(m);
    double mid_y = (bounds.min_y + bounds.max_y) / 2.0;
    double tol_y = (bounds.max_y - bounds.min_y) * 0.05 + 1e-10;

    // Find midpoint of right edge
    double fea_disp = 0.0;
    double best_dist = std::numeric_limits<double>::max();
    for (int i = 0; i < m.num_nodes(); ++i) {
        if (std::abs(m.nodes[i].x - bounds.max_x) < (bounds.max_x - bounds.min_x) * 0.01 + 1e-10) {
            double dy = std::abs(m.nodes[i].y - mid_y);
            if (dy < best_dist) {
                best_dist = dy;
                fea_disp = result.displacement[dof_index(i, 1)];
            }
        }
    }

    double disp_err = (std::abs(reference_disp) > 1e-30)
        ? std::abs(fea_disp - reference_disp) / std::abs(reference_disp) * 100.0
        : 0.0;

    rows.push_back({
        "Tip Displacement uy",
        reference_disp,
        fea_disp,
        disp_err,
        disp_err < 10.0,
        "Ref: ~13.68 mm (Cook et al.)"
    });

    // Energy balance
    double U = fea::compute_strain_energy(result.K_csr, result.displacement);
    double W = fea::compute_work_done(result.f, result.displacement);
    double energy_err = (std::abs(W) > 1e-30)
        ? std::abs(U - W) / std::abs(W) * 100.0
        : 0.0;

    rows.push_back({
        "Energy Balance (U vs W)",
        W,
        U,
        energy_err,
        energy_err < 1.0,
        "0.5*u^T*K*u == 0.5*f^T*u"
    });

    return rows;
}

// ------------------------------------------------------------------
// PLATE WITH HOLE -- SCF = 3.0 (Kirsch solution)
// ------------------------------------------------------------------
inline std::vector<AnalyticalRow> analytical_plate_hole(
    const Mesh& m, const fea::SolveResult& result) {

    std::vector<AnalyticalRow> rows;

    // sigma_inf: extract from applied Neumann BCs
    // The total applied force on right edge: F = sigma_inf * t * Ly
    MeshBounds bounds = compute_bounds(m);
    double Ly = bounds.max_y - bounds.min_y;
    double t = m.mat.t;
    double total_force = total_applied_force(m);
    double sigma_inf = (t * Ly > 1e-30) ? total_force / (t * Ly) : 1.0e6;

    // Analytical SCF from Kirsch solution
    double scf_analytical = 3.0;
    double sigma_max_analytical = scf_analytical * sigma_inf;

    // FEA: max von Mises stress
    double max_stress_fea = max_abs_stress(result.stresses, "von_mises");

    // Compare SCF
    double scf_fea = (sigma_inf > 1e-30) ? max_stress_fea / sigma_inf : 0.0;
    double scf_err = std::abs(scf_fea - scf_analytical) / scf_analytical * 100.0;

    rows.push_back({
        "Stress Concentration Factor",
        scf_analytical,
        scf_fea,
        scf_err,
        scf_err < 10.0,
        "Kirsch: SCF = 3.0"
    });

    rows.push_back({
        "Max sigma (von Mises)",
        sigma_max_analytical,
        max_stress_fea,
        scf_err,
        scf_err < 10.0,
        "3 * sigma_inf"
    });

    // Energy balance
    double U = fea::compute_strain_energy(result.K_csr, result.displacement);
    double W = fea::compute_work_done(result.f, result.displacement);
    double energy_err = (std::abs(W) > 1e-30)
        ? std::abs(U - W) / std::abs(W) * 100.0
        : 0.0;

    rows.push_back({
        "Energy Balance (U vs W)",
        W,
        U,
        energy_err,
        energy_err < 1.0,
        "0.5*u^T*K*u == 0.5*f^T*u"
    });

    return rows;
}

// ------------------------------------------------------------------
// THERMAL CYLINDER -- Lame solution for thick cylinder
// ------------------------------------------------------------------
inline std::vector<AnalyticalRow> analytical_thermal_cylinder(
    const Mesh& m, const fea::SolveResult& result) {

    std::vector<AnalyticalRow> rows;

    // Extract inner/outer radii from mesh nodes near axes
    // We look for the minimum and maximum radial distances
    double r_min = std::numeric_limits<double>::max();
    double r_max = 0.0;
    for (const auto& node : m.nodes) {
        double r = std::sqrt(node.x * node.x + node.y * node.y);
        if (r > 1e-10) {
            r_min = std::min(r_min, r);
            r_max = std::max(r_max, r);
        }
    }

    // Lame solution constants (assuming internal pressure p_i from Neumann BCs)
    // sigma_r = -p_i * a^2/r^2 * (b^2-r^2)/(b^2-a^2)
    // sigma_theta = p_i * a^2/r^2 * (b^2+r^2)/(b^2-a^2)
    double a = r_min;  // inner radius
    double b = r_max;  // outer radius

    // Estimate p_i from Neumann BCs
    double p_i = 0.0;
    for (const auto& bc : m.neumann) {
        if (bc.dof == 0 || bc.dof == 1) {
            double r_node = std::sqrt(m.nodes[bc.node].x * m.nodes[bc.node].x +
                                      m.nodes[bc.node].y * m.nodes[bc.node].y);
            if (std::abs(r_node - a) < (b - a) * 0.1 + 1e-10) {
                p_i = std::max(p_i, std::abs(bc.value));
            }
        }
    }

    // Analytical: sigma_theta at r=a = p_i * (b^2 + a^2) / (b^2 - a^2)
    double sigma_theta_exact = 0.0;
    if (b > a && b * b - a * a > 1e-30) {
        sigma_theta_exact = p_i * (b * b + a * a) / (b * b - a * a);
    }

    // FEA: approximate sigma_theta as max sigma_xx at inner boundary
    double max_fea_stress = max_abs_stress(result.stresses, "sigma_xx");

    double error_pct = 0.0;
    if (std::abs(sigma_theta_exact) > 1e-30) {
        error_pct = std::abs(max_fea_stress - sigma_theta_exact) /
                    std::abs(sigma_theta_exact) * 100.0;
    }

    rows.push_back({
        "sigma_theta at r=a",
        sigma_theta_exact,
        max_fea_stress,
        error_pct,
        error_pct < 15.0,
        "p_i*(b^2+a^2)/(b^2-a^2)"
    });

    // Energy balance
    double U = fea::compute_strain_energy(result.K_csr, result.displacement);
    double W = fea::compute_work_done(result.f, result.displacement);
    double energy_err = (std::abs(W) > 1e-30)
        ? std::abs(U - W) / std::abs(W) * 100.0
        : 0.0;

    rows.push_back({
        "Energy Balance (U vs W)",
        W,
        U,
        energy_err,
        energy_err < 1.0,
        "0.5*u^T*K*u == 0.5*f^T*u"
    });

    return rows;
}

// ------------------------------------------------------------------
// MICHELL -- Bar forces from equilibrium
// ------------------------------------------------------------------
inline std::vector<AnalyticalRow> analytical_michell(
    const Mesh& m, const fea::SolveResult& result) {

    std::vector<AnalyticalRow> rows;

    // Simple hand calculation for the 3-bar truss:
    // Bar 0-2: L = sqrt(1^2 + 0.25^2), Bar 1-2: same
    // Load at node 2: P = -1000 N (downward)
    // Vertical equilibrium: F * (dy/L) * 2 = 1000
    // F = 1000 * L / (2 * dy)

    // Find the two bar elements and their forces
    if (m.num_bars() >= 2) {
        double F_analytical = 0.0;
        for (int e = 0; e < m.num_bars(); ++e) {
            const auto& bar = m.bar_elements[e];
            double dx = m.nodes[bar[1]].x - m.nodes[bar[0]].x;
            double dy = m.nodes[bar[1]].y - m.nodes[bar[0]].y;
            double L_bar = std::sqrt(dx * dx + dy * dy);

            // Check if this is a diagonal bar (not the vertical one 0-1)
            if (std::abs(dx) > 1e-10 && std::abs(dy) > 1e-10) {
                double sin_alpha = std::abs(dy) / L_bar;
                double P = std::abs(total_applied_force(m));
                F_analytical = P * L_bar / (2.0 * sin_alpha);
                break;
            }
        }

        // FEA: max bar stress
        double max_bar_stress = 0.0;
        for (const auto& s : result.stresses) {
            if (s.von_mises > max_bar_stress) max_bar_stress = s.von_mises;
        }

        // Convert analytical force to stress (A = 0.001 m^2)
        double A = 0.001;
        double sigma_analytical = F_analytical / A;
        double error_pct = (sigma_analytical > 1e-30)
            ? std::abs(max_bar_stress - sigma_analytical) / sigma_analytical * 100.0
            : 0.0;

        rows.push_back({
            "Max Bar Stress",
            sigma_analytical,
            max_bar_stress,
            error_pct,
            error_pct < 5.0,
            "F/A (equilibrium)"
        });
    }

    // Energy balance
    double U = fea::compute_strain_energy(result.K_csr, result.displacement);
    double W = fea::compute_work_done(result.f, result.displacement);
    double energy_err = (std::abs(W) > 1e-30)
        ? std::abs(U - W) / std::abs(W) * 100.0
        : 0.0;

    rows.push_back({
        "Energy Balance (U vs W)",
        W,
        U,
        energy_err,
        energy_err < 1.0,
        "0.5*u^T*K*u == 0.5*f^T*u"
    });

    return rows;
}

// ------------------------------------------------------------------
// L-BRACKET -- Qualitative only (no closed-form solution)
// ------------------------------------------------------------------
inline std::vector<AnalyticalRow> analytical_lbracket(
    const Mesh& m, const fea::SolveResult& result) {

    std::vector<AnalyticalRow> rows;

    // L-bracket has no simple closed-form solution.
    // We only check energy balance as a sanity measure.

    double U = fea::compute_strain_energy(result.K_csr, result.displacement);
    double W = fea::compute_work_done(result.f, result.displacement);
    double energy_err = (std::abs(W) > 1e-30)
        ? std::abs(U - W) / std::abs(W) * 100.0
        : 0.0;

    rows.push_back({
        "Energy Balance (U vs W)",
        W,
        U,
        energy_err,
        energy_err < 1.0,
        "0.5*u^T*K*u == 0.5*f^T*u"
    });

    return rows;
}

// ------------------------------------------------------------------
// MAIN DISPATCHER
// ------------------------------------------------------------------
inline std::vector<AnalyticalRow> compute_analytical(
    CaseType case_type, const Mesh& mesh, const fea::SolveResult& result) {

    switch (case_type) {
        case CaseType::CANTILEVER:      return analytical_cantilever(mesh, result);
        case CaseType::PATCH:           return analytical_patch(mesh, result);
        case CaseType::COOK:            return analytical_cook(mesh, result);
        case CaseType::PLATE_HOLE:      return analytical_plate_hole(mesh, result);
        case CaseType::THERMAL_CYLINDER:return analytical_thermal_cylinder(mesh, result);
        case CaseType::MICHELL:         return analytical_michell(mesh, result);
        case CaseType::LBRACKET:        return analytical_lbracket(mesh, result);
        default:                        return {};
    }
}
