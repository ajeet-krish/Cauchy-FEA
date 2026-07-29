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
    bool convergence_mode = false;
    if (argc > 1) nx = std::atoi(argv[1]);
    ny = nx;
    for (int i = 2; i < argc; ++i) {
        if (std::string(argv[i]) == "--cg") use_cg = true;
        if (std::string(argv[i]) == "--convergence") convergence_mode = true;
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
    postprocess::write_mesh_json("output/lbracket/mesh.json", m);

    std::cout << "Output written to output/lbracket/" << std::endl;

    // Convergence study
    if (convergence_mode) {
        std::cout << "\n=== Convergence Study ===" << std::endl;
        std::cout << "Reference: stress concentration factor ~2.5-3.0 at re-entrant corner" << std::endl;
        
        std::vector<int> meshes = {8, 16, 32, 64};
        std::vector<double> max_stresses;
        std::vector<double> h_values;
        
        for (int mesh_size : meshes) {
            int nx_test = mesh_size;
            int ny_test = mesh_size;
            
            // Create mesh
            auto m_test = mesh::generate_lbracket(Lx, Ly, cx, cy, nx_test, ny_test);
            m_test.mat = Material::steel();
            m_test.mat.t = 0.01;
            m_test.plane = PlaneType::STRESS;
            
            // Apply BCs
            for (int i = 0; i < m_test.num_nodes(); ++i) {
                if (std::abs(m_test.nodes[i].y - Ly) < tol) {
                    m_test.dirichlet.push_back({i, 1, 0.0});
                }
                if (std::abs(m_test.nodes[i].x) < tol) {
                    m_test.dirichlet.push_back({i, 0, 0.0});
                }
            }
            
            // Apply load
            double P_test = -1000.0;
            int load_nodes_test = 0;
            for (int i = 0; i < m_test.num_nodes(); ++i) {
                if (std::abs(m_test.nodes[i].x) < tol && m_test.nodes[i].y < cy) {
                    load_nodes_test++;
                }
            }
            for (int i = 0; i < m_test.num_nodes(); ++i) {
                if (std::abs(m_test.nodes[i].x) < tol && m_test.nodes[i].y < cy) {
                    m_test.neumann.push_back({i, 1, P_test / load_nodes_test});
                }
            }
            
            // Solve
            auto result_test = fea::solve(m_test, use_cg);
            
            // Find max stress
            double max_stress_test = 0.0;
            for (const auto& s : result_test.stresses) {
                if (s.von_mises > max_stress_test) max_stress_test = s.von_mises;
            }
            
            // Compute h (element size)
            double h = Lx / nx_test;
            
            max_stresses.push_back(max_stress_test);
            h_values.push_back(h);
            
            std::cout << "Mesh " << nx_test << "x" << ny_test 
                      << ": max_stress=" << max_stress_test << " Pa, h=" << h << std::endl;
        }
        
        // Compute stress concentration factor
        double P_total = 1000.0;
        double t_val = 0.01;
        double Ly_val = 2.0;
        double nominal_stress = P_total / (t_val * Ly_val);
        
        std::cout << "\nConvergence Results (Stress Concentration Factor):" << std::endl;
        for (size_t i = 0; i < meshes.size(); ++i) {
            double scf = max_stresses[i] / nominal_stress;
            std::cout << "  " << meshes[i] << "x" << meshes[i] 
                      << ": SCF=" << scf << std::endl;
        }
    }

    return 0;
}
