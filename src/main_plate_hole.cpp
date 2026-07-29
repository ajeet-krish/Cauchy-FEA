#include "fea.hpp"
#include <iostream>
#include <filesystem>
#include <cmath>

// ==========================================================================
// PLATE WITH HOLE -- Kirsch solution for stress concentration
// Analytical: sigma_max = 3 * sigma_inf at hole edge
//
// Quarter-symmetry model:
//   Domain: [0, 2] x [0, 2], hole at (0, 0) with R = 0.5
//   Left (x=0): ux=0 (symmetry)
//   Bottom (y=0): uy=0 (symmetry)
//   Right (x=2): sigma_inf in x-direction
//   Top (y=2): free
// ==========================================================================

int main(int argc, char* argv[]) {
    int nx = 16, ny = 16;
    bool use_cg = false;
    bool convergence_mode = false;
    if (argc > 1) nx = std::atoi(argv[1]);
    ny = nx;
    for (int i = 2; i < argc; ++i) {
        if (std::string(argv[i]) == "--cg") use_cg = true;
        if (std::string(argv[i]) == "--convergence") convergence_mode = true;
    }

    std::cout << "=== FEA-2D: Plate with Hole ===" << std::endl;
    std::cout << "Mesh: " << nx << "x" << ny << std::endl;

    g_nx = nx;
    g_ny = ny;
    g_case = CaseType::PLATE_HOLE;

    // Quarter-symmetry: domain [0, 2] x [0, 2], hole at (0, 0) with R = 0.5
    double Lx = 2.0, Ly = 2.0;
    double cx = 0.0, cy = 0.0, R = 0.5;

    // Generate structured mesh
    auto m = mesh::generate_structured_quad(Lx, Ly, nx, ny);
    m.mat = Material::steel();
    m.mat.t = 0.01;
    m.plane = PlaneType::STRESS;

    // Remove nodes inside the hole
    std::vector<bool> node_active(m.num_nodes(), true);
    for (int i = 0; i < m.num_nodes(); ++i) {
        double dx = m.nodes[i].x - cx;
        double dy = m.nodes[i].y - cy;
        if (std::sqrt(dx * dx + dy * dy) < R) {
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

    // Boundary conditions for quarter-symmetry
    double sigma_inf = 1.0e6;  // 1 MPa
    double tol = 1e-10;

    // Left edge (x=0): ux=0 (symmetry)
    // Bottom edge (y=0): uy=0 (symmetry)
    for (int i = 0; i < m.num_nodes(); ++i) {
        if (std::abs(m.nodes[i].x) < tol) {
            m.dirichlet.push_back({i, 0, 0.0});  // ux=0 on left
        }
        if (std::abs(m.nodes[i].y) < tol) {
            m.dirichlet.push_back({i, 1, 0.0});  // uy=0 on bottom
        }
    }

    // Apply uniform tension on right edge (x=Lx)
    for (int i = 0; i < m.num_nodes(); ++i) {
        if (std::abs(m.nodes[i].x - Lx) < tol) {
            m.neumann.push_back({i, 0, sigma_inf * m.mat.t * Ly / ny});
        }
    }

    // Solve
    auto result = fea::solve(m, use_cg);

    // Find max stress near hole edge
    double max_stress = 0.0;
    for (const auto& s : result.stresses) {
        if (s.von_mises > max_stress) max_stress = s.von_mises;
    }

    std::cout << "\nMax von Mises stress: " << max_stress << " Pa" << std::endl;
    std::cout << "Expected (Kirsch): ~" << 3.0 * sigma_inf << " Pa" << std::endl;
    std::cout << "SCF: " << max_stress / sigma_inf << " (expected ~3.0)" << std::endl;

    // Energy balance
    double U = fea::compute_strain_energy(result.K_csr, result.displacement);
    double W = fea::compute_work_done(result.f, result.displacement);
    std::cout << "Energy balance: U=" << U << ", W=" << W
              << ", error=" << std::abs(U - W) / (std::abs(W) + 1e-30) * 100.0 << "%" << std::endl;

    std::filesystem::create_directories("output/plate_hole");
    postprocess::write_meta_json("output/plate_hole/meta.json", m, result.displacement, result.stresses,
                                 result.cg_iterations, result.solve_time_ms);
    postprocess::write_displacement_json("output/plate_hole/displacement.json", m, result.displacement);
    postprocess::write_stress_json("output/plate_hole/stress.json", m, result.stresses);
    postprocess::write_mesh_json("output/plate_hole/mesh.json", m);

    std::cout << "Output written to output/plate_hole/" << std::endl;

    // Convergence study
    if (convergence_mode) {
        std::cout << "\n=== Convergence Study ===" << std::endl;
        std::cout << "Reference: Kirsch solution - sigma_max = 3 * sigma_inf" << std::endl;
        
        std::vector<int> meshes = {8, 16, 32, 64};
        std::vector<double> max_stresses;
        std::vector<double> h_values;
        
        for (int mesh_size : meshes) {
            int nx_test = mesh_size;
            int ny_test = mesh_size;
            
            // Create mesh
            auto m_test = mesh::generate_structured_quad(Lx, Ly, nx_test, ny_test);
            m_test.mat = Material::steel();
            m_test.mat.t = 0.01;
            m_test.plane = PlaneType::STRESS;
            
            // Remove nodes inside the hole
            std::vector<bool> node_active_test(m_test.num_nodes(), true);
            for (int i = 0; i < m_test.num_nodes(); ++i) {
                double dx = m_test.nodes[i].x - cx;
                double dy = m_test.nodes[i].y - cy;
                if (std::sqrt(dx * dx + dy * dy) < R) {
                    node_active_test[i] = false;
                }
            }
            
            // Filter elements
            std::vector<std::array<int, 4>> active_quads_test;
            for (const auto& elem : m_test.quad_elements) {
                bool all_active = true;
                for (int n : elem) {
                    if (!node_active_test[n]) { all_active = false; break; }
                }
                if (all_active) active_quads_test.push_back(elem);
            }
            m_test.quad_elements = active_quads_test;
            
            // Renumber nodes
            std::vector<int> node_map_test(m_test.num_nodes(), -1);
            std::vector<Node> new_nodes_test;
            for (int i = 0; i < m_test.num_nodes(); ++i) {
                if (node_active_test[i]) {
                    node_map_test[i] = static_cast<int>(new_nodes_test.size());
                    new_nodes_test.push_back(m_test.nodes[i]);
                }
            }
            m_test.nodes = new_nodes_test;
            for (auto& elem : m_test.quad_elements) {
                for (int& n : elem) n = node_map_test[n];
            }
            
            // Apply BCs
            for (int i = 0; i < m_test.num_nodes(); ++i) {
                if (std::abs(m_test.nodes[i].x) < tol) {
                    m_test.dirichlet.push_back({i, 0, 0.0});
                }
                if (std::abs(m_test.nodes[i].y) < tol) {
                    m_test.dirichlet.push_back({i, 1, 0.0});
                }
            }
            
            // Apply load
            for (int i = 0; i < m_test.num_nodes(); ++i) {
                if (std::abs(m_test.nodes[i].x - Lx) < tol) {
                    m_test.neumann.push_back({i, 0, sigma_inf * m_test.mat.t * Ly / ny_test});
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
        
        // Compute stress concentration factors
        std::cout << "\nConvergence Results (Stress Concentration Factor):" << std::endl;
        for (size_t i = 0; i < meshes.size(); ++i) {
            double scf = max_stresses[i] / sigma_inf;
            std::cout << "  " << meshes[i] << "x" << meshes[i] 
                      << ": SCF=" << scf << " (expected ~3.0)" << std::endl;
        }
        
        // Compute GCI
        if (max_stresses.size() >= 2) {
            double f1 = max_stresses.back();
            double f2 = max_stresses[max_stresses.size() - 2];
            double h1 = h_values.back();
            double h2 = h_values[h_values.size() - 2];
            double r = h2 / h1;
            
            // Assume second-order convergence
            double p = 2.0;
            double gci = std::abs(f1 - f2) / (std::pow(r, p) - 1.0);
            double extrapolated = f1 + gci;
            
            std::cout << "\nRichardson Extrapolation:" << std::endl;
            std::cout << "  Extrapolated max stress: " << extrapolated << " Pa" << std::endl;
            std::cout << "  Extrapolated SCF: " << extrapolated / sigma_inf << std::endl;
        }
    }

    return 0;
}
