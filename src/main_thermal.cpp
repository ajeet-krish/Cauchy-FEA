#include "fea.hpp"
#include <iostream>
#include <filesystem>
#include <cmath>

// ==========================================================================
// THICK CYLINDER (Lame) -- Internal pressure on annular cylinder
// Analytical (Lame solution, plane strain):
//   sigma_r = C * (1 - b^2/r^2)
//   sigma_theta = C * (1 + b^2/r^2)
// where C = p_i * a^2 / (b^2 - a^2)
// Domain: annulus a <= r <= b, quarter-symmetry [0, b] x [0, b]
// ==========================================================================

int main(int argc, char* argv[]) {
    int nx = 32, ny = 32;
    bool use_cg = false;
    bool run_convergence = false;
    if (argc > 1) nx = std::atoi(argv[1]);
    ny = nx;
    for (int i = 2; i < argc; ++i) {
        if (std::string(argv[i]) == "--cg") use_cg = true;
        if (std::string(argv[i]) == "--convergence") run_convergence = true;
    }

    std::cout << "=== FEA-2D: Thick Cylinder (Lame) ===" << std::endl;
    std::cout << "Mesh: " << nx << "x" << ny << std::endl;

    g_nx = nx;
    g_ny = ny;
    g_case = CaseType::THERMAL_CYLINDER;

    double a = 1.0;
    double b = 2.0;
    double p_i = 1.0e6;
    double t = 0.01;

    auto m = mesh::generate_structured_quad(b, b, nx, ny);
    m.mat = Material::steel();
    m.mat.t = t;
    m.plane = PlaneType::STRAIN;

    // Remove nodes inside inner circle (r < a)
    std::vector<bool> node_active(m.num_nodes(), true);
    for (int i = 0; i < m.num_nodes(); ++i) {
        double r = std::sqrt(m.nodes[i].x * m.nodes[i].x + m.nodes[i].y * m.nodes[i].y);
        if (r < a) {
            node_active[i] = false;
        }
    }

    // Filter elements (remove any element with inactive nodes)
    std::vector<std::array<int, 4>> active_quads;
    for (const auto& elem : m.quad_elements) {
        bool all_active = true;
        for (int n : elem) {
            if (!node_active[n]) { all_active = false; break; }
        }
        if (all_active) active_quads.push_back(elem);
    }
    m.quad_elements = active_quads;

    // Renumber nodes to remove gaps
    std::vector<int> node_map(m.num_nodes(), -1);
    std::vector<Node> new_nodes;
    for (int i = 0; i < m.num_nodes(); ++i) {
        if (node_active[i]) {
            node_map[i] = static_cast<int>(new_nodes.size());
            new_nodes.push_back(m.nodes[i]);
        }
    }
    m.nodes = new_nodes;
    for (auto& elem : m.quad_elements) {
        for (int& n : elem) n = node_map[n];
    }

    std::cout << "Nodes: " << m.num_nodes() << ", Elements: " << m.num_quads() << std::endl;

    // Boundary conditions: symmetry on x=0 and y=0
    double tol = 1e-10;
    for (int i = 0; i < m.num_nodes(); ++i) {
        if (std::abs(m.nodes[i].x) < tol) {
            m.dirichlet.push_back({i, 0, 0.0});
        }
        if (std::abs(m.nodes[i].y) < tol) {
            m.dirichlet.push_back({i, 1, 0.0});
        }
    }

    // Internal pressure on inner hole boundary (r = a)
    // Use element-size-based tolerance to capture nodes near r = a
    double dx = b / nx;
    double tol_inner = dx * 0.5;

    // Find inner boundary nodes and compute total tributary arc length
    std::vector<int> inner_nodes;
    double total_arc = 0.0;
    for (int i = 0; i < m.num_nodes(); ++i) {
        double r = std::sqrt(m.nodes[i].x * m.nodes[i].x + m.nodes[i].y * m.nodes[i].y);
        if (std::abs(r - a) < tol_inner) {
            inner_nodes.push_back(i);
            // Approximate tributary arc length based on spacing to neighbors
            total_arc += dx;
        }
    }
    int n_inner = static_cast<int>(inner_nodes.size());
    if (n_inner == 0) {
        std::cerr << "ERROR: No nodes found on inner boundary (r=a)" << std::endl;
        return 1;
    }

    std::cout << "Inner boundary nodes: " << n_inner << std::endl;

    // Apply internal pressure traction to each inner boundary node
    // Total force = p_i * t * (pi*a/2) for quarter symmetry
    // Distributed proportionally to tributary arc length
    for (int idx : inner_nodes) {
        double nx = m.nodes[idx].x / a;
        double ny2 = m.nodes[idx].y / a;
        double tributary = dx;  // uniform tributary for simplicity
        double force_mag = p_i * t * tributary;
        m.neumann.push_back({idx, 0, force_mag * nx});
        m.neumann.push_back({idx, 1, force_mag * ny2});
    }

    std::cout << "Inner boundary nodes: " << n_inner << std::endl;

    auto result = fea::solve(m, use_cg);

    // Find max von Mises stress
    double max_stress = 0.0;
    for (const auto& s : result.stresses) {
        if (s.von_mises > max_stress) max_stress = s.von_mises;
    }

    // Analytical Lame solution (mechanical only)
    // At r=a: sigma_r = -p_i, sigma_theta = p_i*(b^2+a^2)/(b^2-a^2)
    double sigma_r_analytical = -p_i;
    double sigma_theta_analytical = p_i * (b*b + a*a) / (b*b - a*a);

    // Find max sigma_r at inner boundary for comparison
    double max_sigma_r = 0.0;
    for (const auto& s : result.stresses) {
        if (std::abs(s.sigma_xx) > max_sigma_r) max_sigma_r = std::abs(s.sigma_xx);
    }

    std::cout << "\n--- Analytical vs FEA (mechanical only) ---" << std::endl;
    std::cout << "sigma_r at r=a (FEA max |sigma_xx|): " << max_sigma_r << " Pa" << std::endl;
    std::cout << "Analytical sigma_r at r=a: " << sigma_r_analytical << " Pa" << std::endl;
    std::cout << "Analytical sigma_theta at r=a: " << sigma_theta_analytical << " Pa" << std::endl;
    std::cout << "Max von Mises stress: " << max_stress << " Pa" << std::endl;

    // Energy balance
    double U = fea::compute_strain_energy(result.K_csr, result.displacement);
    double W = fea::compute_work_done(result.f, result.displacement);
    std::cout << "Energy balance: U=" << U << ", W=" << W
              << ", error=" << std::abs(U - W) / (std::abs(W) + 1e-30) * 100.0 << "%" << std::endl;

    std::filesystem::create_directories("output/thermal_cylinder/simulations");
    postprocess::write_meta_json("output/thermal_cylinder/simulations/meta.json", m, result.displacement, result.stresses,
                                     result.cg_iterations, result.solve_time_ms);
    postprocess::write_displacement_json("output/thermal_cylinder/simulations/displacement.json", m, result.displacement);
    postprocess::write_stress_json("output/thermal_cylinder/simulations/stress.json", m, result.stresses);
    postprocess::write_mesh_json("output/thermal_cylinder/simulations/mesh.json", m);

    std::cout << "Output written to output/thermal_cylinder/simulations/" << std::endl;

    if (run_convergence) {
        std::cout << "\n=== Convergence Study ===" << std::endl;
        std::cout << "Reference: Lame solution -- sigma_r(r=a) = -p*a^2/(b^2-a^2) * (1 - b^2/a^2)" << std::endl;
    }

    return 0;
}