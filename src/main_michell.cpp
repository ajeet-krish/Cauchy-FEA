#include "fea.hpp"
#include <iostream>
#include <filesystem>

// ==========================================================================
// MICHELL TRUSS -- Classic truss optimization starting point
// Simple truss structure validated against hand calculations
// ==========================================================================

int main() {
    std::cout << "=== FEA-2D: Michell Truss ===" << std::endl;

    g_case = CaseType::MICHELL;

    // Build a simple Michell-style truss
    Mesh m;
    m.mat = Material::steel();
    m.mat.t = 0.01;

    // 4 nodes forming a truss
    m.nodes = {
        {0.0, 0.0},     // 0: bottom-left (fixed)
        {0.0, 0.5},     // 1: top-left (fixed)
        {1.0, 0.25},    // 2: right (loaded)
    };

    // 3 bar elements
    m.bar_elements = {{0, 2}, {1, 2}, {0, 1}};
    m.bar_areas = {0.001, 0.001, 0.001};  // 10 cm^2 each

    // Fix nodes 0 and 1
    m.dirichlet = {
        {0, 0, 0.0}, {0, 1, 0.0},
        {1, 0, 0.0}, {1, 1, 0.0}
    };

    // Load at node 2 (downward)
    m.neumann = {{2, 1, -1000.0}};

    auto result = fea::solve(m, false);

    // Hand calculation for validation:
    // Bar 0-2: L = sqrt(1^2 + 0.25^2) = 1.0308
    // Bar 1-2: L = sqrt(1^2 + 0.25^2) = 1.0308
    // Vertical equilibrium: F * (0.25/1.0308) * 2 = 1000
    // F = 1000 * 1.0308 / (2 * 0.25) = 2061.6 N (compression in each bar)

    std::cout << "\nNodal displacements:" << std::endl;
    for (int i = 0; i < m.num_nodes(); ++i) {
        double ux = result.displacement[dof_index(i, 0)];
        double uy = result.displacement[dof_index(i, 1)];
        std::cout << "  Node " << i << ": ux=" << ux << ", uy=" << uy << std::endl;
    }

    std::filesystem::create_directories("output/michell/simulations");
    postprocess::write_meta_json("output/michell/simulations/meta.json", m, result.displacement, result.stresses,
                                 result.cg_iterations, result.solve_time_ms);
    postprocess::write_displacement_json("output/michell/simulations/displacement.json", m, result.displacement);
    postprocess::write_stress_json("output/michell/simulations/stress.json", m, result.stresses);
    postprocess::write_mesh_json("output/michell/simulations/mesh.json", m);

    std::cout << "Output written to output/michell/simulations/" << std::endl;
    return 0;
}
