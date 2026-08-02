#include "fea.hpp"
#include <iostream>
#include <filesystem>
#include <cmath>

int main(int argc, char* argv[]) {
    int nx = 16, ny = 16, nz = 4;
    bool use_cg = false;
    bool convergence_mode = false;
    if (argc > 1) nx = std::atoi(argv[1]);
    ny = nx; nz = std::max(2, nx / 8);
    for (int i = 2; i < argc; ++i) {
        if (std::string(argv[i]) == "--cg") use_cg = true;
        if (std::string(argv[i]) == "--convergence") convergence_mode = true;
    }

    g_dim = 3;
    g_case = CaseType::LAME_3D;

    double a = 1.0, b = 2.0, L = 1.0;
    double p_i = 1.0e6;

    auto m = mesh::generate_structured_hex(b, b, L, nx, ny, nz);
    m.mat = Material::steel();
    m.plane = PlaneType::STRAIN;

    // Remove nodes inside inner circle (r < a)
    int num_nodes = m.num_nodes();
    std::vector<bool> node_active(num_nodes, true);
    for (int i = 0; i < num_nodes; ++i) {
        double r = std::sqrt(m.nodes[i].x * m.nodes[i].x + m.nodes[i].y * m.nodes[i].y);
        if (r < a) {
            node_active[i] = false;
        }
    }

    // Filter elements (remove any element with inactive nodes)
    std::vector<std::array<int, 8>> active_hexes;
    for (const auto& elem : m.hex_elements) {
        bool all_active = true;
        for (int n : elem) {
            if (!node_active[n]) { all_active = false; break; }
        }
        if (all_active) active_hexes.push_back(elem);
    }
    m.hex_elements = active_hexes;

    // Remove orphaned nodes and renumber
    std::vector<int> node_map(num_nodes, -1);
    int new_idx = 0;
    for (int i = 0; i < num_nodes; ++i) {
        if (node_active[i]) node_map[i] = new_idx++;
    }
    for (auto& elem : m.hex_elements) {
        for (int& n : elem) n = node_map[n];
    }
    std::vector<Node> new_nodes;
    for (int i = 0; i < num_nodes; ++i) {
        if (node_active[i]) new_nodes.push_back(m.nodes[i]);
    }
    m.nodes = new_nodes;

    std::cout << "3D Lame Problem: " << m.num_nodes() << " nodes, "
              << m.num_hexes() << " H8 elements, " << m.num_dofs() << " DOFs" << std::endl;

    // Symmetry BCs on x=0 and y=0 planes
    double tol = 1e-10;
    for (int i = 0; i < m.num_nodes(); ++i) {
        if (std::abs(m.nodes[i].x) < tol) {
            m.dirichlet.push_back({i, 0, 0.0});
        }
        if (std::abs(m.nodes[i].y) < tol) {
            m.dirichlet.push_back({i, 1, 0.0});
        }
        if (std::abs(m.nodes[i].z) < tol) {
            m.dirichlet.push_back({i, 2, 0.0});
        }
    }

    // Internal pressure on inner hole surface (r = a)
    for (int i = 0; i < m.num_nodes(); ++i) {
        double r = std::sqrt(m.nodes[i].x * m.nodes[i].x + m.nodes[i].y * m.nodes[i].y);
        if (std::abs(r - a) < tol) {
            double theta = std::atan2(m.nodes[i].y, m.nodes[i].x);
            double nx = std::cos(theta);
            double ny = std::sin(theta);
            m.neumann.push_back({i, 0, -p_i * nx});
            m.neumann.push_back({i, 1, -p_i * ny});
        }
    }

    auto result = fea::solve(m, use_cg);

    // Compute Lame analytical solution at inner surface
    double C = p_i * a * a / (b * b - a * a);
    double sigma_theta_analytical = C * (1.0 + b * b / (a * a));
    double sigma_r_analytical = -p_i;

    std::cout << "Lame analytical (at r=a):" << std::endl;
    std::cout << "  sigma_theta = " << sigma_theta_analytical << " Pa" << std::endl;
    std::cout << "  sigma_r = " << sigma_r_analytical << " Pa" << std::endl;

    // Find max von Mises and compare
    double max_vm = 0.0;
    for (const auto& s : result.stresses) {
        if (s.von_mises > max_vm) max_vm = s.von_mises;
    }
    std::cout << "Max von Mises: " << max_vm << " Pa" << std::endl;

    double U = fea::compute_strain_energy(result.K_csr, result.displacement);
    double W = fea::compute_work_done(result.f, result.displacement);
    std::cout << "Energy balance: U=" << U << ", W=" << W
              << ", error=" << std::abs(U - W) / (std::abs(W) + 1e-30) * 100.0 << "%" << std::endl;

    std::string outdir = "output/lame_3d/simulations/" + std::to_string(nx);
    std::filesystem::create_directories(outdir);
    postprocess::write_meta_json(outdir + "/meta.json", m, result.displacement, result.stresses,
                                     result.cg_iterations, result.solve_time_ms);
    postprocess::write_displacement_json(outdir + "/displacement.json", m, result.displacement);
    postprocess::write_stress_json(outdir + "/stress.json", m, result.stresses);
    postprocess::write_mesh_json(outdir + "/mesh.json", m);

    std::cout << "Output written to " << outdir << "/" << std::endl;
    return 0;
}