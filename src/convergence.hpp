#pragma once
#include "fea_types.hpp"
#include "fea.hpp"
#include <vector>
#include <cmath>
#include <functional>
#include <fstream>
#include <iostream>
#include <iomanip>

// ==========================================================================
// MESH CONVERGENCE -- h-refinement wrapper, GCI computation (ASME V&V 20)
// ==========================================================================

namespace convergence {

// ------------------------------------------------------------------
// One sample point in a convergence study
// ------------------------------------------------------------------
struct ConvergenceSample {
    int nx, ny;
    int num_nodes;
    int num_elements;
    double h;                       // characteristic element size (1/nx)
    double value;                   // quantity of interest
    double solve_time_ms;
    int cg_iterations;
};

// ------------------------------------------------------------------
// GCI result from Richardson extrapolation
// ------------------------------------------------------------------
struct GCIResult {
    double gci_fine;                // GCI on finest mesh
    double gci_medium;              // GCI on medium mesh
    double observed_order;          // apparent order of convergence (p)
    double extrapolated_value;      // Richardson extrapolation to h=0
    bool is_oscillatory;            // convergence is non-monotonic
};

// ------------------------------------------------------------------
// Compute GCI per ASME V&V 20-2009
// f1=coarse, f2=medium, f3=fine; h1>h2>h3; r=refinement ratio
// ------------------------------------------------------------------
inline GCIResult compute_gci(
    double f1, double f2, double f3,
    double h1, double h2, double h3)
{
    GCIResult result{};

    double e21 = f2 - f1;
    double e32 = f3 - f2;

    // Refinement ratio (should be constant, but compute from actual h)
    double r = h1 / h2;

    // Check for oscillatory convergence
    result.is_oscillatory = (e21 * e32 < 0);

    if (std::abs(e21) < 1e-30 || std::abs(e32) < 1e-30) {
        result.observed_order = 0.0;
        result.gci_fine = 0.0;
        result.gci_medium = 0.0;
        result.extrapolated_value = f3;
        return result;
    }

    // Apparent order of convergence
    double p = std::log(std::abs(e32 / e21)) / std::log(r);
    result.observed_order = p;

    // GCI on fine mesh (compared to Richardson extrapolation)
    result.gci_fine = 1.25 * std::abs(e21) / (std::pow(r, p) - 1.0);
    result.gci_medium = r * r * result.gci_fine;  // extrapolate backward

    // Richardson extrapolation to h=0
    result.extrapolated_value = f3 + (f3 - f2) / (std::pow(r, p) - 1.0);

    return result;
}

// ------------------------------------------------------------------
// Run a convergence study for a single case
// setup_fn: takes nx, returns a configured Mesh
// extract_qoi: takes SolveResult + Mesh, returns the quantity of interest
// ------------------------------------------------------------------
template<typename SetupFn, typename ExtractFn>
std::vector<ConvergenceSample> run_study(
    SetupFn setup_fn,
    ExtractFn extract_qoi,
    const std::vector<int>& resolutions)
{
    std::vector<ConvergenceSample> samples;

    for (int nx : resolutions) {
        auto m = setup_fn(nx);
        int ny = nx;

        auto result = fea::solve(m, true);  // use CG for convergence study
        double qoi = extract_qoi(result, m);
        double h = 1.0 / nx;

        samples.push_back({
            nx, ny,
            m.num_nodes(),
            m.num_quads(),
            h, qoi,
            result.solve_time_ms,
            result.cg_iterations
        });

        std::cout << "  nx=" << nx << "  nodes=" << m.num_nodes()
                  << "  qoi=" << std::scientific << std::setprecision(6) << qoi
                  << "  h=" << h << std::endl;
    }

    return samples;
}

// ------------------------------------------------------------------
// Write convergence.json
// ------------------------------------------------------------------
inline void write_json(
    const std::string& filepath,
    const std::string& case_name,
    const std::string& quantity_name,
    double analytical_value,
    const std::vector<ConvergenceSample>& samples,
    const GCIResult& gci)
{
    std::ofstream f(filepath);
    f << "{\n";
    f << "  \"case\": \"" << case_name << "\",\n";
    f << "  \"quantity\": \"" << quantity_name << "\",\n";
    f << "  \"analytical\": " << analytical_value << ",\n";

    f << "  \"samples\": [\n";
    for (size_t i = 0; i < samples.size(); ++i) {
        const auto& s = samples[i];
        f << "    {\"nx\": " << s.nx
          << ", \"ny\": " << s.ny
          << ", \"num_nodes\": " << s.num_nodes
          << ", \"num_elements\": " << s.num_elements
          << ", \"h\": " << std::scientific << std::setprecision(6) << s.h
          << ", \"value\": " << s.value
          << ", \"solve_ms\": " << std::fixed << std::setprecision(1) << s.solve_time_ms
          << ", \"cg_iters\": " << s.cg_iterations
          << "}" << (i + 1 < samples.size() ? "," : "") << "\n";
    }
    f << "  ],\n";

    f << "  \"gci\": {\n";
    f << "    \"gci_fine\": " << std::scientific << std::setprecision(6) << gci.gci_fine << ",\n";
    f << "    \"gci_medium\": " << gci.gci_medium << ",\n";
    f << "    \"observed_order\": " << std::fixed << std::setprecision(2) << gci.observed_order << ",\n";
    f << "    \"extrapolated_value\": " << std::scientific << std::setprecision(6) << gci.extrapolated_value << ",\n";
    f << "    \"is_oscillatory\": " << (gci.is_oscillatory ? "true" : "false") << "\n";
    f << "  },\n";

    f << "  \"convergence_type\": \"" << (gci.is_oscillatory ? "oscillatory" : "monotonic") << "\"\n";
    f << "}\n";

    std::cout << "Convergence data written to " << filepath << std::endl;
}

}  // namespace convergence
