#include "fea.hpp"
#include <iostream>
#include <filesystem>
#include <cmath>

// ==========================================================================
// PLATE WITH HOLE -- Kirsch solution for stress concentration
// Analytical: sigma_max = 3 * sigma_inf at hole edge
// ==========================================================================

int main(int argc, char* argv[]) {
    int nx = 64, ny = 64;
    if (argc > 1) nx = std::atoi(argv[1]);
    ny = nx;

    std::cout << "=== FEA-2D: Plate with Hole ===" << std::endl;
    std::cout << "Mesh: " << nx << "x" << ny << std::endl;

    g_nx = nx;
    g_ny = ny;
    g_case = CaseType::PLATE_HOLE;

    // Plate: [0, 4] x [0, 4], hole radius R = 0.5 at center (2, 2)
    double Lx = 4.0, Ly = 4.0;
    double cx = 2.0, cy = 2.0, R = 0.5;

    // Generate structured mesh
    auto m = mesh::generate_structured_quad(Lx, Ly, nx, ny);
    m.mat = Material::steel();
    m.mat.t = 0.01;
    m.plane = PlaneType::STRESS;

    // Remove elements inside the hole
    std::vector<bool> node_active(m.num_nodes(), true);
    for (int i = 0; i < m.num_nodes(); ++i) {
        double dx = m.nodes[i].x - cx;
        double dy = m.nodes[i].y - cy;
        if (std::sqrt(dx * dx + dy * dy) < R) {
            node_active[i] = false;
        }
    }

    // Filter elements
    std::vector<std::array<int, 4>> active_quads;
    for (const auto& elem : m.quad_elements) {
        bool all_active = true;
        for (int n : elem) {
            if (!node_active[n]) { all_active = false; break; }
        }
        if (all_active) active_quads.push_back(elem);
    }
    m.quad_elements = active_quads;

    // Renumber nodes
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

    // BCs: symmetry (fix bottom edge ux=0, left edge uy=0)
    // Apply tension sigma_inf = 1.0 on right edge (ux direction)
    double sigma_inf = 1.0e6;  // 1 MPa

    // Fix bottom edge (y=0): ux=0
    // Fix left edge (x=0): uy=0
    // Fix centerline nodes (y=2) for anti-symmetry: uy=0
    double tol = 1e-10;
    for (int i = 0; i < m.num_nodes(); ++i) {
        if (std::abs(m.nodes[i].y) < tol) {
            m.dirichlet.push_back({i, 1, 0.0});  // uy=0 on bottom
        }
        if (std::abs(m.nodes[i].x) < tol) {
            m.dirichlet.push_back({i, 0, 0.0});  // ux=0 on left
        }
    }

    // Apply uniform tension on right edge
    for (int i = 0; i < m.num_nodes(); ++i) {
        if (std::abs(m.nodes[i].x - Lx) < tol) {
            m.neumann.push_back({i, 0, sigma_inf * m.mat.t * Ly / ny});
        }
    }

    // Solve
    auto result = fea::solve(m, false);

    // Find max stress near hole edge
    double max_stress = 0.0;
    for (const auto& s : result.stresses) {
        if (s.von_mises > max_stress) max_stress = s.von_mises;
    }

    std::cout << "\nMax von Mises stress: " << max_stress << " Pa" << std::endl;
    std::cout << "Expected (Kirsch): ~" << 3.0 * sigma_inf << " Pa" << std::endl;
    std::cout << "SCF: " << max_stress / sigma_inf << " (expected ~3.0)" << std::endl;

    std::filesystem::create_directories("output/plate_hole");
    postprocess::write_meta_json("output/plate_hole/meta.json", m, result.displacement, result.stresses,
                                 result.cg_iterations, result.solve_time_ms);
    postprocess::write_displacement_json("output/plate_hole/displacement.json", m, result.displacement);
    postprocess::write_stress_json("output/plate_hole/stress.json", m, result.stresses);

    std::cout << "Output written to output/plate_hole/" << std::endl;
    return 0;
}
