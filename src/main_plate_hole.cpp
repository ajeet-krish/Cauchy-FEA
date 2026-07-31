#include "fea.hpp"
#include <iostream>
#include <filesystem>
#include <cmath>

// ==========================================================================
// PLATE WITH HOLE -- Kirsch solution for stress concentration
// ==========================================================================

int main(int argc, char* argv[]) {
    int nx = 16, ny = 16;
    bool use_cg = false;
    bool convergence_mode = false;
    bool use_q8 = false;
    if (argc > 1) nx = std::atoi(argv[1]);
    ny = nx;
    for (int i = 2; i < argc; ++i) {
        if (std::string(argv[i]) == "--cg") use_cg = true;
        if (std::string(argv[i]) == "--convergence") convergence_mode = true;
        if (std::string(argv[i]) == "--q8") use_q8 = true;
    }

    std::cout << "=== FEA-2D: Plate with Hole ===" << std::endl;
    std::cout << "Mesh: " << nx << "x" << ny << (use_q8 ? " (Q8)" : " (Q4)") << std::endl;

    g_nx = nx;
    g_ny = ny;
    g_case = CaseType::PLATE_HOLE;

    double Lx = 2.0, Ly = 2.0;
    double cx = 0.0, cy = 0.0, R = 0.5;
    double sigma_inf = 1.0e6;
    double tol = 1e-10;

    // Generate mesh
    Mesh m;
    if (use_q8) {
        m = mesh::generate_structured_quad8(Lx, Ly, nx, ny);
    } else {
        m = mesh::generate_structured_quad(Lx, Ly, nx, ny);
    }
    m.mat = Material::steel();
    m.mat.t = 0.01;
    m.plane = PlaneType::STRESS;

    std::cout << "Before hole removal: " << m.num_nodes() << " nodes, "
              << m.num_quads() << " quads, " << m.num_quad8s() << " quad8s" << std::endl;

    // Remove nodes inside the hole
    int num_nodes = m.num_nodes();
    std::vector<bool> node_active(num_nodes, true);
    for (int i = 0; i < num_nodes; ++i) {
        double dx = m.nodes[i].x - cx;
        double dy = m.nodes[i].y - cy;
        if (std::sqrt(dx * dx + dy * dy) < R) {
            node_active[i] = false;
        }
    }

    int inactive = 0;
    for (bool a : node_active) if (!a) inactive++;
    std::cout << "Inactive nodes (inside hole): " << inactive << std::endl;

    // Filter elements
    if (use_q8) {
        std::vector<std::array<int, 8>> active_quads;
        int removed = 0;
        for (const auto& elem : m.quad8_elements) {
            bool all_active = true;
            for (int n : elem) {
                if (!node_active[n]) { all_active = false; break; }
            }
            if (all_active) {
                active_quads.push_back(elem);
            } else {
                removed++;
            }
        }
        std::cout << "Removed Q8 elements: " << removed << std::endl;
        m.quad8_elements = active_quads;
    } else {
        std::vector<std::array<int, 4>> active_quads;
        int removed = 0;
        for (const auto& elem : m.quad_elements) {
            bool all_active = true;
            for (int n : elem) {
                if (!node_active[n]) { all_active = false; break; }
            }
            if (all_active) {
                active_quads.push_back(elem);
            } else {
                removed++;
            }
        }
        std::cout << "Removed Q4 elements: " << removed << std::endl;
        m.quad_elements = active_quads;
    }

    // Second pass: mark only nodes referenced by surviving elements
    // (nodes outside hole but not in any element are orphaned = zero stiffness)
    std::fill(node_active.begin(), node_active.end(), false);
    for (const auto& elem : m.quad_elements) {
        for (int n : elem) node_active[n] = true;
    }
    for (const auto& elem : m.quad8_elements) {
        for (int n : elem) node_active[n] = true;
    }
    int orphaned = 0;
    for (bool a : node_active) if (!a) orphaned++;
    std::cout << "Orphaned nodes (outside hole, not in any element): " << orphaned << std::endl;

    // Renumber nodes
    std::vector<int> node_map(num_nodes, -1);
    std::vector<Node> new_nodes;
    for (int i = 0; i < num_nodes; ++i) {
        if (node_active[i]) {
            node_map[i] = static_cast<int>(new_nodes.size());
            new_nodes.push_back(m.nodes[i]);
        }
    }
    m.nodes = new_nodes;
    for (auto& elem : m.quad_elements) {
        for (int& n : elem) n = node_map[n];
    }
    for (auto& elem : m.quad8_elements) {
        for (int& n : elem) n = node_map[n];
    }

    std::cout << "After hole removal: " << m.num_nodes() << " nodes, "
              << m.num_quads() << " quads, " << m.num_quad8s() << " quad8s" << std::endl;

    // Boundary conditions
    for (int i = 0; i < m.num_nodes(); ++i) {
        if (std::abs(m.nodes[i].x) < tol) {
            m.dirichlet.push_back({i, 0, 0.0});
        }
        if (std::abs(m.nodes[i].y) < tol) {
            m.dirichlet.push_back({i, 1, 0.0});
        }
    }

    // Apply load on right edge
    for (int i = 0; i < m.num_nodes(); ++i) {
        if (std::abs(m.nodes[i].x - Lx) < tol) {
            m.neumann.push_back({i, 0, sigma_inf * m.mat.t * Ly / ny});
        }
    }

    // Solve
    auto result = fea::solve(m, use_cg);

    // Find max stress
    double max_stress = 0.0;
    for (const auto& s : result.stresses) {
        if (s.von_mises > max_stress) max_stress = s.von_mises;
    }

    std::cout << "\nMax von Mises stress: " << max_stress << " Pa" << std::endl;
    std::cout << "Expected (Kirsch): ~" << 3.0 * sigma_inf << " Pa" << std::endl;
    std::cout << "SCF: " << max_stress / sigma_inf << " (expected ~3.0)" << std::endl;

    double U = fea::compute_strain_energy(result.K_csr, result.displacement);
    double W = fea::compute_work_done(result.f, result.displacement);
    std::cout << "Energy balance: U=" << U << ", W=" << W
              << ", error=" << std::abs(U - W) / (std::abs(W) + 1e-30) * 100.0 << "%" << std::endl;

    std::string outdir = "output/plate_hole" + std::string(use_q8 ? "_q8" : "");
    std::filesystem::create_directories(outdir);
    postprocess::write_meta_json(outdir + "/meta.json", m, result.displacement, result.stresses,
                                 result.cg_iterations, result.solve_time_ms);
    postprocess::write_displacement_json(outdir + "/displacement.json", m, result.displacement);
    postprocess::write_stress_json(outdir + "/stress.json", m, result.stresses);
    postprocess::write_mesh_json(outdir + "/mesh.json", m);

    std::cout << "Output written to " << outdir << "/" << std::endl;

    // Convergence study
    if (convergence_mode) {
        std::cout << "\n=== Convergence Study ===" << std::endl;

        std::vector<int> meshes = {8, 16, 32, 64};
        std::vector<double> max_stresses;
        std::vector<double> h_values;

        for (int mesh_size : meshes) {
            int nx_test = mesh_size;
            int ny_test = mesh_size;

            Mesh m_test;
            if (use_q8) {
                m_test = mesh::generate_structured_quad8(Lx, Ly, nx_test, ny_test);
            } else {
                m_test = mesh::generate_structured_quad(Lx, Ly, nx_test, ny_test);
            }
            m_test.mat = Material::steel();
            m_test.mat.t = 0.01;
            m_test.plane = PlaneType::STRESS;

            // Hole removal
            num_nodes = m_test.num_nodes();
            std::vector<bool> node_active_test(num_nodes, true);
            for (int i = 0; i < num_nodes; ++i) {
                double dx = m_test.nodes[i].x - cx;
                double dy = m_test.nodes[i].y - cy;
                if (std::sqrt(dx * dx + dy * dy) < R) {
                    node_active_test[i] = false;
                }
            }

            if (use_q8) {
                std::vector<std::array<int, 8>> active_quads;
                for (const auto& elem : m_test.quad8_elements) {
                    bool all_active = true;
                    for (int n : elem) {
                        if (!node_active_test[n]) { all_active = false; break; }
                    }
                    if (all_active) active_quads.push_back(elem);
                }
                m_test.quad8_elements = active_quads;
            } else {
                std::vector<std::array<int, 4>> active_quads;
                for (const auto& elem : m_test.quad_elements) {
                    bool all_active = true;
                    for (int n : elem) {
                        if (!node_active_test[n]) { all_active = false; break; }
                    }
                    if (all_active) active_quads.push_back(elem);
                }
                m_test.quad_elements = active_quads;
            }

            std::vector<int> node_map_test(num_nodes, -1);
            std::vector<Node> new_nodes_test;

            // Second pass: mark only nodes referenced by surviving elements
            std::fill(node_active_test.begin(), node_active_test.end(), false);
            for (const auto& elem : m_test.quad_elements) {
                for (int n : elem) node_active_test[n] = true;
            }
            for (const auto& elem : m_test.quad8_elements) {
                for (int n : elem) node_active_test[n] = true;
            }

            for (int i = 0; i < num_nodes; ++i) {
                if (node_active_test[i]) {
                    node_map_test[i] = static_cast<int>(new_nodes_test.size());
                    new_nodes_test.push_back(m_test.nodes[i]);
                }
            }
            m_test.nodes = new_nodes_test;
            for (auto& elem : m_test.quad_elements) {
                for (int& n : elem) n = node_map_test[n];
            }
            for (auto& elem : m_test.quad8_elements) {
                for (int& n : elem) n = node_map_test[n];
            }

            // BCs
            for (int i = 0; i < m_test.num_nodes(); ++i) {
                if (std::abs(m_test.nodes[i].x) < tol) {
                    m_test.dirichlet.push_back({i, 0, 0.0});
                }
                if (std::abs(m_test.nodes[i].y) < tol) {
                    m_test.dirichlet.push_back({i, 1, 0.0});
                }
            }

            // Load
            for (int i = 0; i < m_test.num_nodes(); ++i) {
                if (std::abs(m_test.nodes[i].x - Lx) < tol) {
                    m_test.neumann.push_back({i, 0, sigma_inf * m_test.mat.t * Ly / ny_test});
                }
            }

            // Solve
            auto result_test = fea::solve(m_test, use_cg);

            double max_stress_test = 0.0;
            for (const auto& s : result_test.stresses) {
                if (s.von_mises > max_stress_test) max_stress_test = s.von_mises;
            }

            double h = Lx / nx_test;
            max_stresses.push_back(max_stress_test);
            h_values.push_back(h);

            std::cout << "Mesh " << nx_test << "x" << ny_test
                      << ": max_stress=" << max_stress_test << " Pa, h=" << h << std::endl;
        }

        std::cout << "\nConvergence Results (Stress Concentration Factor):" << std::endl;
        for (size_t i = 0; i < meshes.size(); ++i) {
            double scf = max_stresses[i] / sigma_inf;
            std::cout << "  " << meshes[i] << "x" << meshes[i]
                      << ": SCF=" << scf << std::endl;
        }

        if (max_stresses.size() >= 2) {
            double f1 = max_stresses.back();
            double f2 = max_stresses[max_stresses.size() - 2];
            double h1 = h_values.back();
            double h2 = h_values[h_values.size() - 2];
            double r = h2 / h1;

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
