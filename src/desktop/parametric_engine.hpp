#pragma once
#include "fea.hpp"
#include <vector>
#include <cmath>

// ==========================================================================
// PARAMETRIC ENGINE -- Fast rescaling for scale-only parameter changes
// ==========================================================================

// Data captured from a baseline solve for fast parametric rescaling
struct BaselineData {
    bool valid = false;

    // Baseline material parameters
    double E = 0.0;
    double nu = 0.0;
    double t = 0.0;
    double force_scale = 1.0;

    // Baseline stiffness matrix (COO format for rescaling)
    std::vector<int> coo_rows;
    std::vector<int> coo_cols;
    std::vector<double> coo_vals;
    int coo_n = 0;

    // Baseline RHS (force vector)
    std::vector<double> f_neumann;

    // Baseline mesh (geometry and connectivity unchanged by E, force, thickness)
    Mesh mesh;

    // Original solve result
    fea::SolveResult original_result;
};

class ParametricEngine {
public:
    // Capture baseline data after a full solve
    void captureBaseline(const Mesh& m, const fea::SolveResult& result,
                         double E, double nu, double t, double forceScale);

    // Check if we can use the fast rescaling path
    // Fast path: E, force, thickness changes only (same nu)
    bool canRescale(double newE, double newNu, double newT) const;

    // Fast rescale: K_new = (E_new * t_new) / (E_base * t_base) * K_base
    // f_new = force_scale_new / force_scale_base * f_base
    // Returns rescaled result without reassembly
    fea::SolveResult rescale(double newE, double newForce, double newT);

    // Invalidate baseline (e.g., after mesh change)
    void invalidate();

    // Check if baseline is valid
    bool isValid() const { return m_baseline.valid; }

    // Access baseline parameters
    double baselineE() const { return m_baseline.E; }
    double baselineNu() const { return m_baseline.nu; }
    double baselineT() const { return m_baseline.t; }
    double baselineForceScale() const { return m_baseline.force_scale; }

private:
    BaselineData m_baseline;
};
