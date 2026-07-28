#include "fea.hpp"
#include <iostream>
#include <filesystem>

// ==========================================================================
// L-BRACKET -- Stress concentration at re-entrant corner
// Demonstrates mesh refinement toward high-stress region
// ==========================================================================

int main(int argc, char* argv[]) {
    int nx = 32, ny = 32;
    bool use_cg = false;
    if (argc > 1) nx = std::atoi(argv[1]);
    ny = nx;
    for (int i = 2; i < argc; ++i) {
        if (std::string(argv[i]) == "--cg") use_cg = true;
    }

    std::cout << "=== FEA-2D: L-Bracket ===" << std::endl;
    std::cout << "Mesh: " << nx << "x" << ny << std::endl;

    g_nx = nx;
    g_ny = ny;
    g_case = CaseType::LBRACKET;

    // L-bracket: full domain [0, 2] x [0, 2]
    // Cutout: [1, 2] x [0, 1] (bottom-right removed)
    double Lx = 2.0, Ly = 2.0;
    double cx = 1.0, cy = 1.0;

    auto m = mesh::generate_lbracket(Lx, Ly, cx, cy, nx, ny);
    m.mat = Material::steel();
    m.mat.t = 0.01;
    m.plane = PlaneType::STRESS;

    std::cout << "Nodes: " << m.num_nodes() << ", Elements: " << m.num_quads() << std::endl;

    // Fix top edge (y=2): uy=0, and left edge (x=0): ux=0
    double tol = 1e-10;
    for (int i = 0; i < m.num_nodes(); ++i) {
        if (std::abs(m.nodes[i].y - Ly) < tol) {
            m.dirichlet.push_back({i, 1, 0.0});
        }
        if (std::abs(m.nodes[i].x) < tol) {
            m.dirichlet.push_back({i, 0, 0.0});
        }
    }

    // Apply downward load at bottom of vertical leg
    double P = -1000.0;
    int load_nodes = 0;
    for (int i = 0; i < m.num_nodes(); ++i) {
        if (std::abs(m.nodes[i].x) < tol && m.nodes[i].y < cy) {
            load_nodes++;
        }
    }
    for (int i = 0; i < m.num_nodes(); ++i) {
        if (std::abs(m.nodes[i].x) < tol && m.nodes[i].y < cy) {
            m.neumann.push_back({i, 1, P / load_nodes});
        }
    }

    // Solve
    auto result = fea::solve(m, use_cg);

    std::filesystem::create_directories("output/lbracket");
    postprocess::write_meta_json("output/lbracket/meta.json", m, result.displacement, result.stresses,
                                 result.cg_iterations, result.solve_time_ms);
    postprocess::write_displacement_json("output/lbracket/displacement.json", m, result.displacement);
    postprocess::write_stress_json("output/lbracket/stress.json", m, result.stresses);

    std::cout << "Output written to output/lbracket/" << std::endl;
    return 0;
}
