#include "fea.hpp"
#include "convergence.hpp"
#include <iostream>
#include <filesystem>

// ==========================================================================
// CANTILEVER BEAM -- Clamped at left end, point load at right tip
// Analytical: delta = PL^3/(3EI), sigma = My/I
// ==========================================================================

// Reusable setup function for convergence studies
inline fea::SolveResult setup_and_solve_cantilever(int nx, bool use_cg = false) {
    int ny = nx / 4;
    if (ny < 2) ny = 2;

    g_nx = nx;
    g_ny = ny;
    g_case = CaseType::CANTILEVER;

    double L = 1.0;
    double H = 0.25;
    double P = -1000.0;
    double t = 0.01;

    auto m = mesh::generate_structured_quad(L, H, nx, ny);
    m.mat = Material::steel();
    m.mat.t = t;
    m.plane = PlaneType::STRESS;

    for (int j = 0; j <= ny; ++j) {
        int node = j * (nx + 1);
        m.dirichlet.push_back({node, 0, 0.0});
        m.dirichlet.push_back({node, 1, 0.0});
    }

    int tip_node = ny * (nx + 1) + nx;
    m.neumann.push_back({tip_node, 1, P});

    return fea::solve(m, use_cg);
}

int main(int argc, char* argv[]) {
    int nx = 32, ny = 8;
    bool use_cg = false;
    bool run_convergence = false;
    if (argc > 1) nx = std::atoi(argv[1]);
    ny = nx / 4;
    if (ny < 2) ny = 2;
    for (int i = 2; i < argc; ++i) {
        if (std::string(argv[i]) == "--cg") use_cg = true;
        if (std::string(argv[i]) == "--convergence") run_convergence = true;
    }

    g_nx = nx;
    g_ny = ny;
    g_case = CaseType::CANTILEVER;

    if (run_convergence) {
        std::cout << "=== FEA-2D: Cantilever Beam -- Convergence Study ===" << std::endl;

        double L = 1.0, H = 0.25, P = -1000.0, t = 0.01;
        auto mat = Material::steel();
        double I = t * H * H * H / 12.0;
        double delta_exact = P * L * L * L / (3.0 * mat.E * I);

        auto setup = [](int nx) {
            int ny = nx / 4;
            if (ny < 2) ny = 2;
            g_nx = nx;
            g_ny = ny;
            auto m = mesh::generate_structured_quad(1.0, 0.25, nx, ny);
            m.mat = Material::steel();
            m.mat.t = 0.01;
            m.plane = PlaneType::STRESS;
            for (int j = 0; j <= ny; ++j) {
                int node = j * (nx + 1);
                m.dirichlet.push_back({node, 0, 0.0});
                m.dirichlet.push_back({node, 1, 0.0});
            }
            int tip_node = ny * (nx + 1) + nx;
            m.neumann.push_back({tip_node, 1, -1000.0});
            return m;
        };

        auto extract = [](const fea::SolveResult& r, const Mesh& m) {
            int ny = g_ny;
            int nx = g_nx;
            int tip = ny * (nx + 1) + nx;
            return r.displacement[dof_index(tip, 1)];
        };

        std::vector<int> resolutions = {4, 8, 16, 32, 64};
        auto samples = convergence::run_study(setup, extract, resolutions);

        if (samples.size() >= 3) {
            auto gci = convergence::compute_gci(
                samples[samples.size()-3].value,
                samples[samples.size()-2].value,
                samples[samples.size()-1].value,
                samples[samples.size()-3].h,
                samples[samples.size()-2].h,
                samples[samples.size()-1].h);

            std::cout << "\nGCI Results:" << std::endl;
            std::cout << "  Observed order: " << gci.observed_order << std::endl;
            std::cout << "  Extrapolated: " << gci.extrapolated_value << std::endl;
            std::cout << "  GCI fine: " << gci.gci_fine << std::endl;

            std::filesystem::create_directories("output/cantilever");
            convergence::write_json("output/cantilever/convergence.json",
                "cantilever", "tip_displacement", delta_exact, samples, gci);
        }
        return 0;
    }

    std::cout << "=== FEA-2D: Cantilever Beam ===" << std::endl;
    std::cout << "Mesh: " << nx << "x" << ny << std::endl;

    double L = 1.0;
    double H = 0.25;
    double P = -1000.0;
    double t = 0.01;

    auto m = mesh::generate_structured_quad(L, H, nx, ny);
    m.mat = Material::steel();
    m.mat.t = t;
    m.plane = PlaneType::STRESS;

    for (int j = 0; j <= ny; ++j) {
        int node = j * (nx + 1);
        m.dirichlet.push_back({node, 0, 0.0});
        m.dirichlet.push_back({node, 1, 0.0});
    }

    int tip_node = ny * (nx + 1) + nx;
    m.neumann.push_back({tip_node, 1, P});

    auto result = fea::solve(m, use_cg);

    double I = t * H * H * H / 12.0;
    double delta_exact = P * L * L * L / (3.0 * m.mat.E * I);
    double sigma_exact = std::abs(P) * H / (2.0 * I);

    double delta_fea = result.displacement[dof_index(tip_node, 1)];

    std::cout << "\n--- Analytical vs FEA ---" << std::endl;
    std::cout << "Tip deflection: FEA = " << delta_fea
              << ", Analytical = " << delta_exact << std::endl;
    std::cout << "Relative error: " << std::abs(delta_fea - delta_exact) / std::abs(delta_exact) * 100.0 << "%" << std::endl;

    double max_sigma = 0.0;
    for (const auto& s : result.stresses) {
        if (std::abs(s.sigma_xx) > max_sigma) max_sigma = std::abs(s.sigma_xx);
    }
    std::cout << "Max sigma_xx: FEA = " << max_sigma
              << ", Analytical = " << sigma_exact << std::endl;
    std::cout << "Stress error: " << std::abs(max_sigma - sigma_exact) / sigma_exact * 100.0 << "%" << std::endl;

    double U = fea::compute_strain_energy(result.K_csr, result.displacement);
    double W = fea::compute_work_done(result.f, result.displacement);
    std::cout << "Energy balance: U=" << U << ", W=" << W
              << ", error=" << std::abs(U - W) / (std::abs(W) + 1e-30) * 100.0 << "%" << std::endl;

    std::string outdir = "output/cantilever_" + std::to_string(nx);
    std::filesystem::create_directories(outdir);
    postprocess::write_meta_json(outdir + "/meta.json", m, result.displacement, result.stresses,
                                 result.cg_iterations, result.solve_time_ms);
    postprocess::write_displacement_json(outdir + "/displacement.json", m, result.displacement);
    postprocess::write_stress_json(outdir + "/stress.json", m, result.stresses);

    std::cout << "Output written to " << outdir << "/" << std::endl;
    return 0;
}
