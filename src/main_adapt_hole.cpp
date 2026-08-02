#include "fea.hpp"
#include "adaptivity.hpp"
#include <iostream>
#include <filesystem>
#include <cmath>

// ==========================================================================
// ADAPTIVE PLATE WITH HOLE -- ZZ error estimator + red-green refinement
// Demonstrates adaptive h-refinement driving stress concentration capture
// ==========================================================================

int main(int argc, char* argv[]) {
    int nx = 16, ny = 16;
    int max_iter = 4;
    double theta = 0.3;
    bool use_q8 = false;
    if (argc > 1) nx = std::atoi(argv[1]);
    ny = nx;
    for (int i = 2; i < argc; ++i) {
        if (std::string(argv[i]) == "--q8") use_q8 = true;
        if (std::string(argv[i]) == "--theta") {
            if (i + 1 < argc) theta = std::atof(argv[++i]);
        }
        if (std::string(argv[i]) == "--iters") {
            if (i + 1 < argc) max_iter = std::atoi(argv[++i]);
        }
    }

    std::cout << "=== FEA-2D: Adaptive Plate with Hole ===" << std::endl;
    std::cout << "Initial mesh: " << nx << "x" << ny << (use_q8 ? " (Q8)" : " (Q4)") << std::endl;
    std::cout << "Refinement threshold: " << theta << std::endl;
    std::cout << "Max iterations: " << max_iter << std::endl;

    g_case = CaseType::PLATE_HOLE;

    double Lx = 2.0, Ly = 2.0;
    double cx = 0.0, cy = 0.0, R = 0.5;
    double sigma_inf = 1.0e6;
    double tol = 1e-10;

    // Lambda to create and solve a plate-hole mesh
    auto solve_plate = [&](int nx_test, int ny_test, bool q8) {
        Mesh m;
        if (q8) {
            m = mesh::generate_structured_quad8(Lx, Ly, nx_test, ny_test);
        } else {
            m = mesh::generate_structured_quad(Lx, Ly, nx_test, ny_test);
        }
        m.mat = Material::steel();
        m.mat.t = 0.01;
        m.plane = PlaneType::STRESS;

        // Remove nodes inside hole
        int num_nodes = m.num_nodes();
        std::vector<bool> node_active(num_nodes, true);
        for (int i = 0; i < num_nodes; ++i) {
            double dx = m.nodes[i].x - cx;
            double dy = m.nodes[i].y - cy;
            if (std::sqrt(dx * dx + dy * dy) < R) {
                node_active[i] = false;
            }
        }

        // Filter elements
        if (q8) {
            std::vector<std::array<int, 8>> active;
            for (const auto& elem : m.quad8_elements) {
                bool ok = true;
                for (int n : elem) { if (!node_active[n]) { ok = false; break; } }
                if (ok) active.push_back(elem);
            }
            m.quad8_elements = active;
        } else {
            std::vector<std::array<int, 4>> active;
            for (const auto& elem : m.quad_elements) {
                bool ok = true;
                for (int n : elem) { if (!node_active[n]) { ok = false; break; } }
                if (ok) active.push_back(elem);
            }
            m.quad_elements = active;
        }

        // Mark nodes referenced by surviving elements
        std::fill(node_active.begin(), node_active.end(), false);
        for (const auto& elem : m.quad_elements) { for (int n : elem) node_active[n] = true; }
        for (const auto& elem : m.quad8_elements) { for (int n : elem) node_active[n] = true; }

        // Renumber
        std::vector<int> node_map(num_nodes, -1);
        std::vector<Node> new_nodes;
        for (int i = 0; i < num_nodes; ++i) {
            if (node_active[i]) {
                node_map[i] = static_cast<int>(new_nodes.size());
                new_nodes.push_back(m.nodes[i]);
            }
        }
        m.nodes = new_nodes;
        for (auto& elem : m.quad_elements) { for (int& n : elem) n = node_map[n]; }
        for (auto& elem : m.quad8_elements) { for (int& n : elem) n = node_map[n]; }

        // BCs
        for (int i = 0; i < m.num_nodes(); ++i) {
            if (std::abs(m.nodes[i].x) < tol) m.dirichlet.push_back({i, 0, 0.0});
            if (std::abs(m.nodes[i].y) < tol) m.dirichlet.push_back({i, 1, 0.0});
        }

        // Load on right edge
        for (int i = 0; i < m.num_nodes(); ++i) {
            if (std::abs(m.nodes[i].x - Lx) < tol) {
                m.neumann.push_back({i, 0, sigma_inf * m.mat.t * Ly / ny_test});
            }
        }

        return fea::solve(m, true);
    };

    // --- Uniform refinement study (baseline) ---
    std::cout << "\n=== Uniform Refinement (Baseline) ===" << std::endl;
    std::vector<int> uniform_sizes = {8, 16, 32, 64};
    std::vector<double> uniform_stress, uniform_nodes;

    for (int sz : uniform_sizes) {
        auto result = solve_plate(sz, sz, use_q8);
        double max_stress = 0.0;
        for (const auto& s : result.stresses) {
            if (s.von_mises > max_stress) max_stress = s.von_mises;
        }
        uniform_stress.push_back(max_stress);
        uniform_nodes.push_back(static_cast<double>(sz * sz));
        std::cout << "  " << sz << "x" << sz
                  << ": nodes=" << (sz + 1) * (sz + 1)
                  << ", SCF=" << max_stress / sigma_inf << std::endl;
    }

    // --- Adaptive refinement study ---
    std::cout << "\n=== Adaptive Refinement (ZZ Error Estimator) ===" << std::endl;
    std::vector<double> adapt_stress, adapt_nodes;

    // Create initial mesh
    Mesh m;
    if (use_q8) {
        m = mesh::generate_structured_quad8(Lx, Ly, nx, ny);
    } else {
        m = mesh::generate_structured_quad(Lx, Ly, nx, ny);
    }
    m.mat = Material::steel();
    m.mat.t = 0.01;
    m.plane = PlaneType::STRESS;

    // Remove hole
    {
        int num_nodes = m.num_nodes();
        std::vector<bool> node_active(num_nodes, true);
        for (int i = 0; i < num_nodes; ++i) {
            double dx = m.nodes[i].x - cx;
            double dy = m.nodes[i].y - cy;
            if (std::sqrt(dx * dx + dy * dy) < R) node_active[i] = false;
        }

        if (use_q8) {
            std::vector<std::array<int, 8>> active;
            for (const auto& elem : m.quad8_elements) {
                bool ok = true;
                for (int n : elem) { if (!node_active[n]) { ok = false; break; } }
                if (ok) active.push_back(elem);
            }
            m.quad8_elements = active;
        } else {
            std::vector<std::array<int, 4>> active;
            for (const auto& elem : m.quad_elements) {
                bool ok = true;
                for (int n : elem) { if (!node_active[n]) { ok = false; break; } }
                if (ok) active.push_back(elem);
            }
            m.quad_elements = active;
        }

        std::fill(node_active.begin(), node_active.end(), false);
        for (const auto& elem : m.quad_elements) { for (int n : elem) node_active[n] = true; }
        for (const auto& elem : m.quad8_elements) { for (int n : elem) node_active[n] = true; }

        std::vector<int> node_map(num_nodes, -1);
        std::vector<Node> new_nodes;
        for (int i = 0; i < num_nodes; ++i) {
            if (node_active[i]) {
                node_map[i] = static_cast<int>(new_nodes.size());
                new_nodes.push_back(m.nodes[i]);
            }
        }
        m.nodes = new_nodes;
        for (auto& elem : m.quad_elements) { for (int& n : elem) n = node_map[n]; }
        for (auto& elem : m.quad8_elements) { for (int& n : elem) n = node_map[n]; }
    }

    // Apply BCs
    auto apply_bcs = [&](Mesh& mesh, int /*test_ny*/) {
        mesh.dirichlet.clear();
        mesh.neumann.clear();
        for (int i = 0; i < mesh.num_nodes(); ++i) {
            if (std::abs(mesh.nodes[i].x) < tol) mesh.dirichlet.push_back({i, 0, 0.0});
            if (std::abs(mesh.nodes[i].y) < tol) mesh.dirichlet.push_back({i, 1, 0.0});
        }
        int num_edge_nodes = 0;
        for (int i = 0; i < mesh.num_nodes(); ++i) {
            if (std::abs(mesh.nodes[i].x - Lx) < tol) num_edge_nodes++;
        }
        if (num_edge_nodes > 0) {
            for (int i = 0; i < mesh.num_nodes(); ++i) {
                if (std::abs(mesh.nodes[i].x - Lx) < tol) {
                    mesh.neumann.push_back({i, 0, sigma_inf * mesh.mat.t * Ly / num_edge_nodes});
                }
            }
        }
    };

    apply_bcs(m, ny);

    // Lambda to cut hole from mesh after refinement
    auto cut_hole = [&](Mesh& mesh) {
        int num_nodes = mesh.num_nodes();
        std::vector<bool> node_active(num_nodes, true);
        for (int i = 0; i < num_nodes; ++i) {
            double dx = mesh.nodes[i].x - cx;
            double dy = mesh.nodes[i].y - cy;
            if (std::sqrt(dx * dx + dy * dy) < R) node_active[i] = false;
        }

        // Filter Q4 elements
        std::vector<std::array<int, 4>> active_q4;
        for (const auto& elem : mesh.quad_elements) {
            bool ok = true;
            for (int n : elem) { if (!node_active[n]) { ok = false; break; } }
            if (ok) active_q4.push_back(elem);
        }
        mesh.quad_elements = active_q4;

        // Filter Q8 elements
        std::vector<std::array<int, 8>> active_q8;
        for (const auto& elem : mesh.quad8_elements) {
            bool ok = true;
            for (int n : elem) { if (!node_active[n]) { ok = false; break; } }
            if (ok) active_q8.push_back(elem);
        }
        mesh.quad8_elements = active_q8;

        // Mark only nodes referenced by surviving elements
        std::fill(node_active.begin(), node_active.end(), false);
        for (const auto& elem : mesh.quad_elements) { for (int n : elem) node_active[n] = true; }
        for (const auto& elem : mesh.quad8_elements) { for (int n : elem) node_active[n] = true; }

        // Renumber
        std::vector<int> node_map(num_nodes, -1);
        std::vector<Node> new_nodes;
        for (int i = 0; i < num_nodes; ++i) {
            if (node_active[i]) {
                node_map[i] = static_cast<int>(new_nodes.size());
                new_nodes.push_back(mesh.nodes[i]);
            }
        }
        mesh.nodes = new_nodes;
        for (auto& elem : mesh.quad_elements) { for (int& n : elem) n = node_map[n]; }
        for (auto& elem : mesh.quad8_elements) { for (int& n : elem) n = node_map[n]; }
    };

    for (int iter = 0; iter < max_iter; ++iter) {
        std::cout << "\n  --- Iteration " << iter << " ---" << std::endl;
        std::cout << "  Mesh: " << m.num_nodes() << " nodes, "
                  << m.num_quads() << " elements" << std::endl;

        // Solve
        auto result = fea::solve(m, true);

        // SPR recovery
        auto spr = adaptivity::spr_recovery(m, result.stresses);

        // Error indicators
        auto errors = adaptivity::compute_error_indicators(m, result.stresses, spr);

        // Error metrics
        double total_err = 0.0, max_err = 0.0;
        for (const auto& e : errors) {
            total_err += e.eta_squared;
            if (e.eta > max_err) max_err = e.eta;
        }
        total_err = std::sqrt(total_err);

        double max_stress = 0.0;
        for (const auto& s : result.stresses) {
            if (s.von_mises > max_stress) max_stress = s.von_mises;
        }

        std::cout << "  SCF: " << max_stress / sigma_inf
                  << ", total error: " << std::scientific << total_err
                  << ", max element error: " << max_err << std::endl;

        adapt_stress.push_back(max_stress);
        adapt_nodes.push_back(static_cast<double>(m.num_nodes()));

        // Mark elements
        auto marked = adaptivity::mark_elements(errors, theta);

        // Refine
        m = adaptivity::refine_mesh(m, marked);

        // Re-cut hole and re-apply BCs
        cut_hole(m);
        apply_bcs(m, ny);
    }

    // --- Summary ---
    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Uniform refinement:" << std::endl;
    for (size_t i = 0; i < uniform_sizes.size(); ++i) {
        std::cout << "  " << uniform_sizes[i] << "x" << uniform_sizes[i]
                  << ": SCF=" << uniform_stress[i] / sigma_inf
                  << ", nodes=" << uniform_nodes[i] << std::endl;
    }

    std::cout << "Adaptive refinement:" << std::endl;
    for (size_t i = 0; i < adapt_stress.size(); ++i) {
        std::cout << "  Iter " << i
                  << ": SCF=" << adapt_stress[i] / sigma_inf
                  << ", nodes=" << adapt_nodes[i] << std::endl;
    }

    // Write output
    std::string outdir = "output/plate_hole/simulations/adapt" + std::string(use_q8 ? "_q8" : "");
    std::filesystem::create_directories(outdir);

    // Solve final mesh for output
    auto final_result = fea::solve(m, true);
    postprocess::write_meta_json(outdir + "/meta.json", m, final_result.displacement, final_result.stresses,
                                 final_result.cg_iterations, final_result.solve_time_ms);
    postprocess::write_displacement_json(outdir + "/displacement.json", m, final_result.displacement);
    postprocess::write_stress_json(outdir + "/stress.json", m, final_result.stresses);
    postprocess::write_mesh_json(outdir + "/mesh.json", m);

    // Write adaptive convergence
    std::ofstream f(outdir + "/adaptive_convergence.json");
    f << "{\n  \"case\": \"plate_hole\",\n  \"uniform\": [\n";
    for (size_t i = 0; i < uniform_sizes.size(); ++i) {
        f << "    {\"nx\": " << uniform_sizes[i]
          << ", \"scf\": " << uniform_stress[i] / sigma_inf
          << ", \"nodes\": " << uniform_nodes[i] << "}"
          << (i + 1 < uniform_sizes.size() ? "," : "") << "\n";
    }
    f << "  ],\n  \"adaptive\": [\n";
    for (size_t i = 0; i < adapt_stress.size(); ++i) {
        f << "    {\"iteration\": " << i
          << ", \"scf\": " << adapt_stress[i] / sigma_inf
          << ", \"nodes\": " << adapt_nodes[i] << "}"
          << (i + 1 < adapt_stress.size() ? "," : "") << "\n";
    }
    f << "  ]\n}\n";

    std::cout << "\nOutput written to " << outdir << "/" << std::endl;
    return 0;
}
