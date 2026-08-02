#include "fea.hpp"
#include "elements_3d.hpp"
#include <iostream>
#include <filesystem>
#include <cmath>

int main(int argc, char* argv[]) {
    int nx = 16, ny = 4, nz = 4;
    bool use_cg = false;
    bool convergence_mode = false;
    if (argc > 1) nx = std::atoi(argv[1]);
    ny = nx / 4; if (ny < 2) ny = 2;
    nz = ny;
    for (int i = 2; i < argc; ++i) {
        if (std::string(argv[i]) == "--cg") use_cg = true;
        if (std::string(argv[i]) == "--convergence") convergence_mode = true;
    }

    g_dim = 3;
    g_case = CaseType::CANTILEVER_3D;

    double L = 1.0, H = 0.1, t = 0.1;
    auto m = mesh::generate_structured_hex(L, H, t, nx, ny, nz);
    m.mat = Material::steel();
    m.plane = PlaneType::STRESS;

    // Fix left face (x=0): ux=uy=uz=0
    for (int k = 0; k <= nz; ++k) {
        for (int j = 0; j <= ny; ++j) {
            int node = k * (ny + 1) * (nx + 1) + j * (nx + 1);
            m.dirichlet.push_back({node, 0, 0.0});
            m.dirichlet.push_back({node, 1, 0.0});
            m.dirichlet.push_back({node, 2, 0.0});
        }
    }

    // Point load at tip centroid (top-right-front node)
    int tip_node = nz * (ny + 1) * (nx + 1) + ny * (nx + 1) + nx;
    m.neumann.push_back({tip_node, 1, -1000.0});

    auto result = fea::solve(m, use_cg);

    double I = t * H * H * H / 12.0;
    double delta_exact = -1000.0 * L * L * L / (3.0 * m.mat.E * I);
    double delta_fea = result.displacement[dof_index(tip_node, 1)];

    std::cout << "=== FEA-2D: 3D Cantilever Beam (H8) ===" << std::endl;
    std::cout << "Mesh: " << nx << "x" << ny << "x" << nz << " H8 hex elements" << std::endl;
    std::cout << "Nodes: " << m.num_nodes() << ", Elements: " << m.num_hexes() << ", DOFs: " << m.num_dofs() << std::endl;
    std::cout << "Tip deflection: FEA = " << delta_fea << ", Analytical = " << delta_exact << std::endl;
    std::cout << "Relative error: " << std::abs(delta_fea - delta_exact) / std::abs(delta_exact) * 100.0 << "%" << std::endl;

    double U = fea::compute_strain_energy(result.K_csr, result.displacement);
    double W = fea::compute_work_done(result.f, result.displacement);
    std::cout << "Energy balance: U=" << U << ", W=" << W
              << ", error=" << std::abs(U - W) / (std::abs(W) + 1e-30) * 100.0 << "%" << std::endl;

    std::string outdir = "output/cantilever_3d/simulations/" + std::to_string(nx);
    std::filesystem::create_directories(outdir);
    postprocess::write_meta_json(outdir + "/meta.json", m, result.displacement, result.stresses,
                                 result.cg_iterations, result.solve_time_ms);
    postprocess::write_displacement_json(outdir + "/displacement.json", m, result.displacement);
    postprocess::write_stress_json(outdir + "/stress.json", m, result.stresses);
    postprocess::write_mesh_json(outdir + "/mesh.json", m);

    std::cout << "Output written to " << outdir << "/" << std::endl;
    return 0;
}