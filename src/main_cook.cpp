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
    if (argc > 1) nx = std::atoi(argv[1]);
    ny = nx;

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
    // Uniform traction: q = 1/16 N/mm, total = q * h_right * t = 3.75 N
    double total_load = (1.0 / 16.0) * h_right * t;
    // Each node gets load proportional to its tributary length
    double dy = h_right / ny;  // approximate element height at right edge
    for (int j = 0; j <= ny; ++j) {
        int node = j * num_nodes_x + nx;
        double nodal_load;
        if (j == 0 || j == ny) {
            nodal_load = total_load * (dy / 2.0) / h_right;
        } else {
            nodal_load = total_load * dy / h_right;
        }
        m.neumann.push_back({node, 1, nodal_load});
    }

    // Solve with Cholesky
    auto result = fea::solve(m, false);

    // Find tip displacement (midpoint of right edge)
    int mid_node = (ny / 2) * num_nodes_x + nx;
    double tip_disp = result.displacement[dof_index(mid_node, 1)];

    std::cout << "\nTip displacement (right edge midpoint): " << tip_disp << " mm" << std::endl;
    std::cout << "Reference value: ~13.68 mm" << std::endl;
    std::cout << "Ratio to reference: " << tip_disp / 13.68 << std::endl;

    // Write output
    std::string outdir = "output/cook_" + std::to_string(nx);
    std::filesystem::create_directories(outdir);
    postprocess::write_meta_json(outdir + "/meta.json", m, result.displacement, result.stresses,
                                 result.cg_iterations, result.solve_time_ms);
    postprocess::write_displacement_json(outdir + "/displacement.json", m, result.displacement);
    postprocess::write_stress_json(outdir + "/stress.json", m, result.stresses);

    std::cout << "Output written to " << outdir << "/" << std::endl;
    return 0;
}
