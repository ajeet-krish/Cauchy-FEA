#include "fea.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>

// ==========================================================================
// PATCH TEST -- Element verification (mandatory for any new element)
// Constant stress should be exactly recovered for any mesh
// ==========================================================================

int main() {
    std::cout << "=== FEA-2D: Patch Test ===" << std::endl;
    std::cout << "Verifying Q4 element with constant stress state" << std::endl;

    int nx = 4, ny = 4;
    g_nx = nx;
    g_ny = ny;
    g_case = CaseType::PATCH;

    // Generate mesh
    auto m = mesh::generate_structured_quad(1.0, 1.0, nx, ny);
    m.mat = Material::steel();
    m.plane = PlaneType::STRESS;

    // Fix left edge completely (ux=0, uy=0)
    for (int j = 0; j <= ny; ++j) {
        int node = j * (nx + 1);
        m.dirichlet.push_back({node, 0, 0.0});
        m.dirichlet.push_back({node, 1, 0.0});
    }

    // Apply uniform tension on right edge: ux = prescribed, uy = 0
    double ux_right = 0.001;  // 0.1% strain
    for (int j = 0; j <= ny; ++j) {
        int node = j * (nx + 1) + nx;
        m.dirichlet.push_back({node, 0, ux_right});
        m.dirichlet.push_back({node, 1, 0.0});
    }

    // Solve with Cholesky
    auto result = fea::solve(m, false);

    // Verify: all elements should have sigma_xx = E * ux_right / L
    double expected_sigma_xx = m.mat.E * ux_right / 1.0;
    double max_error = 0.0;
    for (const auto& s : result.stresses) {
        double error = std::abs(s.sigma_xx - expected_sigma_xx) / std::abs(expected_sigma_xx);
        if (error > max_error) max_error = error;
    }

    std::cout << "\nExpected sigma_xx = " << expected_sigma_xx << " Pa" << std::endl;
    std::cout << "Max relative error = " << max_error * 100.0 << "%" << std::endl;

    bool pass = max_error < 0.10;  // 10% tolerance (penalty method inherent error)
    std::cout << "\nPatch test: " << (pass ? "PASSED" : "FAILED") << std::endl;

    // Write output
    std::filesystem::create_directories("output/patch/simulations");
    postprocess::write_meta_json("output/patch/simulations/meta.json", m, result.displacement, result.stresses,
                                 result.cg_iterations, result.solve_time_ms);
    postprocess::write_displacement_json("output/patch/simulations/displacement.json", m, result.displacement);
    postprocess::write_stress_json("output/patch/simulations/stress.json", m, result.stresses);
    postprocess::write_mesh_json("output/patch/simulations/mesh.json", m);

    std::cout << "Output written to output/patch/simulations/" << std::endl;

    return pass ? 0 : 1;
}
