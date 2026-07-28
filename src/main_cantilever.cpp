#include "fea.hpp"
#include <iostream>
#include <filesystem>

// ==========================================================================
// CANTILEVER BEAM -- Clamped at left end, point load at right tip
// Analytical: delta = PL^3/(3EI), sigma = My/I
// ==========================================================================

int main(int argc, char* argv[]) {
    int nx = 32, ny = 8;
    if (argc > 1) nx = std::atoi(argv[1]);
    ny = nx / 4;
    if (ny < 2) ny = 2;

    std::cout << "=== FEA-2D: Cantilever Beam ===" << std::endl;
    std::cout << "Mesh: " << nx << "x" << ny << std::endl;

    g_nx = nx;
    g_ny = ny;
    g_case = CaseType::CANTILEVER;

    double L = 1.0;     // beam length
    double H = 0.25;    // beam height (L/4)
    double P = -1000.0;  // tip load (downward)
    double t = 0.01;     // thickness

    auto m = mesh::generate_structured_quad(L, H, nx, ny);
    m.mat = Material::steel();
    m.mat.t = t;
    m.plane = PlaneType::STRESS;

    // Fix left edge (x=0): ux=0, uy=0
    for (int j = 0; j <= ny; ++j) {
        int node = j * (nx + 1);
        m.dirichlet.push_back({node, 0, 0.0});
        m.dirichlet.push_back({node, 1, 0.0});
    }

    // Apply point load at bottom-right corner
    int tip_node = ny * (nx + 1) + nx;
    m.neumann.push_back({tip_node, 1, P});

    // Solve with Cholesky
    auto result = fea::solve(m, false);

    // Analytical solution
    double I = t * H * H * H / 12.0;
    double delta_exact = P * L * L * L / (3.0 * m.mat.E * I);
    double sigma_exact = std::abs(P) * H / (2.0 * I);

    double delta_fea = result.displacement[dof_index(tip_node, 1)];

    std::cout << "\n--- Analytical vs FEA ---" << std::endl;
    std::cout << "Tip deflection: FEA = " << delta_fea
              << ", Analytical = " << delta_exact << std::endl;
    std::cout << "Relative error: " << std::abs(delta_fea - delta_exact) / std::abs(delta_exact) * 100.0 << "%" << std::endl;

    // Write output
    std::string outdir = "output/cantilever_" + std::to_string(nx);
    std::filesystem::create_directories(outdir);
    postprocess::write_meta_json(outdir + "/meta.json", m, result.displacement, result.stresses,
                                 result.cg_iterations, result.solve_time_ms);
    postprocess::write_displacement_json(outdir + "/displacement.json", m, result.displacement);
    postprocess::write_stress_json(outdir + "/stress.json", m, result.stresses);

    std::cout << "Output written to " << outdir << "/" << std::endl;
    return 0;
}
