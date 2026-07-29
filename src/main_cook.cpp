#include "fea.hpp"
#include <iostream>
#include <filesystem>

// ==========================================================================
// COOK'S MEMBRANE -- Trapezoidal panel under distributed shear load
// Benchmark: tip displacement ~13.68 mm (linear elastic, plane stress)
// Reference: Cook, Malkus, Plesha "Concepts and Applications of FEA"
//
// Standard geometry:
//   Length (x) = 48 mm
//   Left height (fixed) = 44 mm
//   Right height (loaded) = 60 mm
//   Thickness = 1 mm
//   E = 1.0 MPa, nu = 1/3
//   Total shear load on right edge = 1.0 N
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

    std::cout << "=== FEA-2D: Cook's Membrane ===" << std::endl;
    std::cout << "Mesh: " << nx << "x" << ny << std::endl;

    g_nx = nx;
    g_ny = ny;
    g_case = CaseType::COOK;

    // Standard Cook's membrane dimensions
    double L = 48.0;       // mm (length in x-direction)
    double h_left = 44.0;  // mm (height at left, fixed end)
    double h_right = 60.0; // mm (height at right, loaded end)
    double t = 1.0;        // mm (thickness)

    Material mat;
    mat.E = 1.0;       // MPa (normalized)
    mat.nu = 1.0 / 3.0;
    mat.t = t;

    // Generate trapezoidal mesh
    int num_nodes_x = nx + 1;
    int num_nodes_y = ny + 1;

    Mesh m;
    m.mat = mat;
    m.plane = PlaneType::STRESS;

    m.nodes.resize(num_nodes_x * num_nodes_y);
    for (int j = 0; j <= ny; ++j) {
        double eta = static_cast<double>(j) / ny;
        for (int i = 0; i <= nx; ++i) {
            double xi = static_cast<double>(i) / nx;
            double x = L * xi;
            double h = h_left + (h_right - h_left) * xi;
            double y = h * (eta - 0.5);
            int idx = j * num_nodes_x + i;
            m.nodes[idx] = {x, y};
        }
    }

    // Create Q4 elements (CCW ordering)
    m.quad_elements.resize(nx * ny);
    for (int j = 0; j < ny; ++j) {
        for (int i = 0; i < nx; ++i) {
            int n0 = j * num_nodes_x + i;
            int n1 = j * num_nodes_x + (i + 1);
            int n2 = (j + 1) * num_nodes_x + (i + 1);
            int n3 = (j + 1) * num_nodes_x + i;
            m.quad_elements[j * nx + i] = {n0, n1, n2, n3};
        }
    }

    // Fix left edge (x=0): ux=0, uy=0
    for (int j = 0; j <= ny; ++j) {
        int node = j * num_nodes_x;
        m.dirichlet.push_back({node, 0, 0.0});
        m.dirichlet.push_back({node, 1, 0.0});
    }

    // Apply distributed shear load on right edge
    // Standard Cook's membrane: total shear load = 1.0 N
    // For a uniform traction, distribute total load equally to all edge nodes
    double total_load = 1.0;
    int num_edge_nodes = ny + 1;
    for (int j = 0; j <= ny; ++j) {
        int node = j * num_nodes_x + nx;
        m.neumann.push_back({node, 1, total_load / num_edge_nodes});
    }

    // Solve
    auto result = fea::solve(m, use_cg);

    // Find tip displacement (midpoint of right edge)
    int mid_node = (ny / 2) * num_nodes_x + nx;
    double tip_disp = result.displacement[dof_index(mid_node, 1)];

    std::cout << "\nTip displacement (right edge midpoint): " << tip_disp << " mm" << std::endl;
    std::cout << "Reference value: ~13.68 mm (for T=1/16 N/mm per unit area)" << std::endl;
    std::cout << "Ratio to reference: " << tip_disp / 13.68 << std::endl;

    // Energy balance
    double U = fea::compute_strain_energy(result.K_csr, result.displacement);
    double W = fea::compute_work_done(result.f, result.displacement);
    std::cout << "Energy balance: U=" << U << ", W=" << W
              << ", error=" << std::abs(U - W) / (std::abs(W) + 1e-30) * 100.0 << "%" << std::endl;

    // Write output
    std::string outdir = "output/cook_" + std::to_string(nx);
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
        std::cout << "Reference: tip displacement ~13.68 mm" << std::endl;
        
        std::vector<int> meshes = {4, 8, 16, 32, 64};
        std::vector<double> displacements;
        std::vector<double> h_values;
        
        for (int mesh_size : meshes) {
            int nx_test = mesh_size;
            int ny_test = mesh_size;
            
            // Create mesh
            Mesh m_test;
            m_test.mat = mat;
            m_test.plane = PlaneType::STRESS;
            
            int num_nodes_x_test = nx_test + 1;
            int num_nodes_y_test = ny_test + 1;
            m_test.nodes.resize(num_nodes_x_test * num_nodes_y_test);
            
            for (int j = 0; j <= ny_test; ++j) {
                double eta = static_cast<double>(j) / ny_test;
                for (int i = 0; i <= nx_test; ++i) {
                    double xi = static_cast<double>(i) / nx_test;
                    double x = L * xi;
                    double h = h_left + (h_right - h_left) * xi;
                    double y = h * (eta - 0.5);
                    int idx = j * num_nodes_x_test + i;
                    m_test.nodes[idx] = {x, y};
                }
            }
            
            // Create elements
            m_test.quad_elements.resize(nx_test * ny_test);
            for (int j = 0; j < ny_test; ++j) {
                for (int i = 0; i < nx_test; ++i) {
                    int n0 = j * num_nodes_x_test + i;
                    int n1 = j * num_nodes_x_test + (i + 1);
                    int n2 = (j + 1) * num_nodes_x_test + (i + 1);
                    int n3 = (j + 1) * num_nodes_x_test + i;
                    m_test.quad_elements[j * nx_test + i] = {n0, n1, n2, n3};
                }
            }
            
            // Apply BCs
            for (int j = 0; j <= ny_test; ++j) {
                int node = j * num_nodes_x_test;
                m_test.dirichlet.push_back({node, 0, 0.0});
                m_test.dirichlet.push_back({node, 1, 0.0});
            }
            
            // Apply load
            double total_load_test = 1.0;
            int num_edge_nodes_test = ny_test + 1;
            for (int j = 0; j <= ny_test; ++j) {
                int node = j * num_nodes_x_test + nx_test;
                m_test.neumann.push_back({node, 1, total_load_test / num_edge_nodes_test});
            }
            
            // Solve
            auto result_test = fea::solve(m_test, use_cg);
            
            // Get tip displacement
            int mid_node_test = (ny_test / 2) * num_nodes_x_test + nx_test;
            double tip_disp_test = result_test.displacement[dof_index(mid_node_test, 1)];
            
            // Compute h (element size)
            double h = L / nx_test;
            
            displacements.push_back(tip_disp_test);
            h_values.push_back(h);
            
            std::cout << "Mesh " << nx_test << "x" << ny_test 
                      << ": tip_disp=" << tip_disp_test << " mm, h=" << h << std::endl;
        }
        
        // Compute errors
        double reference = 13.68;
        std::cout << "\nConvergence Results:" << std::endl;
        for (size_t i = 0; i < meshes.size(); ++i) {
            double error = std::abs(displacements[i] - reference) / reference * 100.0;
            std::cout << "  " << meshes[i] << "x" << meshes[i] 
                      << ": error=" << error << "%" << std::endl;
        }
        
        // Compute GCI (simplified)
        if (displacements.size() >= 2) {
            double f1 = displacements.back();
            double f2 = displacements[displacements.size() - 2];
            double h1 = h_values.back();
            double h2 = h_values[h_values.size() - 2];
            double r = h2 / h1;
            
            // Assume second-order convergence
            double p = 2.0;
            double gci = std::abs(f1 - f2) / (std::pow(r, p) - 1.0);
            double extrapolated = f1 + gci;
            
            std::cout << "\nRichardson Extrapolation:" << std::endl;
            std::cout << "  Extrapolated value: " << extrapolated << " mm" << std::endl;
            std::cout << "  Error vs reference: " << std::abs(extrapolated - reference) / reference * 100.0 << "%" << std::endl;
        }
    }

    return 0;
}
