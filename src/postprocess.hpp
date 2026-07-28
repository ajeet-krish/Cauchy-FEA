#pragma once
#include "fea_types.hpp"
#include "sparse.hpp"
#include <vector>
#include <array>
#include <cmath>
#include <fstream>
#include <sstream>
#include <iomanip>

// ==========================================================================
// POSTPROCESSING -- Stress recovery, Von Mises, JSON output
// ==========================================================================

namespace postprocess {

// ------------------------------------------------------------------
// Element stress results for Q4
// ------------------------------------------------------------------
struct ElementStress {
    double sigma_xx = 0.0;
    double sigma_yy = 0.0;
    double sigma_xy = 0.0;
    double von_mises = 0.0;
    double sigma_1 = 0.0;   // principal stress 1
    double sigma_2 = 0.0;   // principal stress 2
};

// ------------------------------------------------------------------
// Compute element stress from Q4 element displacement
// sigma = D * B * u_element
// ------------------------------------------------------------------
inline ElementStress compute_q4_stress(
    const std::array<Node, 4>& elem_nodes,
    const std::array<double, 8>& u_elem,
    const Material& mat,
    PlaneType plane) {

    // Use center of element (xi=0, eta=0) for stress evaluation
    double xi = 0.0, eta = 0.0;

    // Compute B matrix at center
    auto D = mat.d_matrix(plane);

    // Jacobian at center
    double J11 = 0.0, J12 = 0.0, J21 = 0.0, J22 = 0.0;
    static const double xi_pts[4]  = { -1.0,  1.0,  1.0, -1.0 };
    static const double eta_pts[4] = { -1.0, -1.0,  1.0,  1.0 };

    for (int i = 0; i < 4; ++i) {
        double dN_dxi  = 0.25 * xi_pts[i]  * (1.0 + eta_pts[i] * eta);
        double dN_deta = 0.25 * (1.0 + xi_pts[i] * xi) * eta_pts[i];
        J11 += dN_dxi * elem_nodes[i].x;
        J12 += dN_dxi * elem_nodes[i].y;
        J21 += dN_deta * elem_nodes[i].x;
        J22 += dN_deta * elem_nodes[i].y;
    }

    double detJ = J11 * J22 - J12 * J21;
    double invJ11 =  J22 / detJ;
    double invJ12 = -J12 / detJ;
    double invJ21 = -J21 / detJ;
    double invJ22 =  J11 / detJ;

    // B matrix (3 x 8)
    std::array<std::array<double, 8>, 3> B{};
    for (int i = 0; i < 4; ++i) {
        double dN_dxi  = 0.25 * xi_pts[i]  * (1.0 + eta_pts[i] * eta);
        double dN_deta = 0.25 * (1.0 + xi_pts[i] * xi) * eta_pts[i];
        double dNdx = invJ11 * dN_dxi + invJ12 * dN_deta;
        double dNdy = invJ21 * dN_dxi + invJ22 * dN_deta;

        int col = 2 * i;
        B[0][col]     = dNdx;
        B[0][col + 1] = 0.0;
        B[1][col]     = 0.0;
        B[1][col + 1] = dNdy;
        B[2][col]     = dNdy;
        B[2][col + 1] = dNdx;
    }

    // Strain = B * u
    double eps_xx = 0.0, eps_yy = 0.0, gamma_xy = 0.0;
    for (int j = 0; j < 8; ++j) {
        eps_xx += B[0][j] * u_elem[j];
        eps_yy += B[1][j] * u_elem[j];
        gamma_xy += B[2][j] * u_elem[j];
    }

    // Stress = D * strain
    ElementStress s;
    s.sigma_xx = D[0][0] * eps_xx + D[0][1] * eps_yy + D[0][2] * gamma_xy;
    s.sigma_yy = D[1][0] * eps_xx + D[1][1] * eps_yy + D[1][2] * gamma_xy;
    s.sigma_xy = D[2][0] * eps_xx + D[2][1] * eps_yy + D[2][2] * gamma_xy;

    // Von Mises: sigma_vm = sqrt(s1^2 - s1*s2 + s2^2)
    // For plane stress: sigma_3 = 0
    double s1, s2;
    {
        double avg = (s.sigma_xx + s.sigma_yy) / 2.0;
        double R = std::sqrt(((s.sigma_xx - s.sigma_yy) / 2.0) * ((s.sigma_xx - s.sigma_yy) / 2.0)
                             + s.sigma_xy * s.sigma_xy);
        s1 = avg + R;
        s2 = avg - R;
    }
    s.sigma_1 = s1;
    s.sigma_2 = s2;

    if (plane == PlaneType::STRESS) {
        s.von_mises = std::sqrt(s1 * s1 - s1 * s2 + s2 * s2);
    } else {
        // Plane strain: sigma_3 = nu * (sigma_xx + sigma_yy)
        double s3 = mat.nu * (s.sigma_xx + s.sigma_yy);
        s.von_mises = std::sqrt(0.5 * ((s1 - s2) * (s1 - s2) +
                                        (s2 - s3) * (s2 - s3) +
                                        (s3 - s1) * (s3 - s1)));
    }

    return s;
}

// ------------------------------------------------------------------
// Compute all element stresses
// ------------------------------------------------------------------
inline std::vector<ElementStress> compute_all_stresses(
    const Mesh& m,
    const std::vector<double>& u) {

    std::vector<ElementStress> stresses(m.num_quads());

    #pragma omp parallel for
    for (int e = 0; e < m.num_quads(); ++e) {
        const auto& elem = m.quad_elements[e];
        std::array<Node, 4> elem_nodes;
        std::array<double, 8> u_elem;

        for (int i = 0; i < 4; ++i) {
            elem_nodes[i] = m.nodes[elem[i]];
            u_elem[2 * i]     = u[dof_index(elem[i], 0)];
            u_elem[2 * i + 1] = u[dof_index(elem[i], 1)];
        }

        stresses[e] = compute_q4_stress(elem_nodes, u_elem, m.mat, m.plane);
    }

    return stresses;
}

// ------------------------------------------------------------------
// Node-averaged stresses (smooth contours)
// ------------------------------------------------------------------
struct NodeStress {
    double sigma_xx = 0.0;
    double sigma_yy = 0.0;
    double sigma_xy = 0.0;
    double von_mises = 0.0;
    int count = 0;
};

inline std::vector<NodeStress> average_stresses_to_nodes(
    const Mesh& m,
    const std::vector<ElementStress>& elem_stresses) {

    std::vector<NodeStress> node_stress(m.num_nodes());

    for (int e = 0; e < m.num_quads(); ++e) {
        const auto& elem = m.quad_elements[e];
        for (int i = 0; i < 4; ++i) {
            int n = elem[i];
            node_stress[n].sigma_xx += elem_stresses[e].sigma_xx;
            node_stress[n].sigma_yy += elem_stresses[e].sigma_yy;
            node_stress[n].sigma_xy += elem_stresses[e].sigma_xy;
            node_stress[n].von_mises += elem_stresses[e].von_mises;
            node_stress[n].count++;
        }
    }

    for (auto& ns : node_stress) {
        if (ns.count > 0) {
            ns.sigma_xx /= ns.count;
            ns.sigma_yy /= ns.count;
            ns.sigma_xy /= ns.count;
            ns.von_mises /= ns.count;
        }
    }

    return node_stress;
}

// ------------------------------------------------------------------
// Write displacement field to JSON
// ------------------------------------------------------------------
inline void write_displacement_json(
    const std::string& filepath,
    const Mesh& m,
    const std::vector<double>& u) {

    std::ofstream f(filepath);
    f << std::fixed << std::setprecision(6);
    f << "{\n";
    f << "  \"num_nodes\": " << m.num_nodes() << ",\n";
    f << "  \"nodes\": [\n";
    for (int i = 0; i < m.num_nodes(); ++i) {
        f << "    {\"x\": " << m.nodes[i].x
          << ", \"y\": " << m.nodes[i].y
          << ", \"ux\": " << u[dof_index(i, 0)]
          << ", \"uy\": " << u[dof_index(i, 1)] << "}";
        if (i < m.num_nodes() - 1) f << ",";
        f << "\n";
    }
    f << "  ]\n";
    f << "}\n";
}

// ------------------------------------------------------------------
// Write stress field to JSON
// ------------------------------------------------------------------
inline void write_stress_json(
    const std::string& filepath,
    const Mesh& m,
    const std::vector<ElementStress>& stresses) {

    std::ofstream f(filepath);
    f << std::fixed << std::setprecision(6);
    f << "{\n";
    f << "  \"num_elements\": " << m.num_quads() << ",\n";
    f << "  \"elements\": [\n";
    for (int i = 0; i < m.num_quads(); ++i) {
        f << "    {\"sigma_xx\": " << stresses[i].sigma_xx
          << ", \"sigma_yy\": " << stresses[i].sigma_yy
          << ", \"sigma_xy\": " << stresses[i].sigma_xy
          << ", \"von_mises\": " << stresses[i].von_mises
          << ", \"sigma_1\": " << stresses[i].sigma_1
          << ", \"sigma_2\": " << stresses[i].sigma_2 << "}";
        if (i < m.num_quads() - 1) f << ",";
        f << "\n";
    }
    f << "  ]\n";
    f << "}\n";
}

// ------------------------------------------------------------------
// Write metadata JSON
// ------------------------------------------------------------------
inline void write_meta_json(
    const std::string& filepath,
    const Mesh& m,
    const std::vector<double>& u,
    const std::vector<ElementStress>& stresses,
    int cg_iterations = 0,
    double solve_time_ms = 0.0) {

    // Find max displacement
    double max_disp = 0.0;
    for (int i = 0; i < m.num_nodes(); ++i) {
        double ux = u[dof_index(i, 0)];
        double uy = u[dof_index(i, 1)];
        double d = std::sqrt(ux * ux + uy * uy);
        if (d > max_disp) max_disp = d;
    }

    // Find max stress
    double max_stress = 0.0;
    for (const auto& s : stresses) {
        if (s.von_mises > max_stress) max_stress = s.von_mises;
    }

    std::ofstream f(filepath);
    f << std::fixed << std::setprecision(6);
    f << "{\n";
    f << "  \"num_nodes\": " << m.num_nodes() << ",\n";
    f << "  \"num_elements\": " << m.num_quads() << ",\n";
    f << "  \"num_dofs\": " << m.num_dofs() << ",\n";
    f << "  \"material\": {\n";
    f << "    \"E\": " << m.mat.E << ",\n";
    f << "    \"nu\": " << m.mat.nu << ",\n";
    f << "    \"rho\": " << m.mat.rho << ",\n";
    f << "    \"t\": " << m.mat.t << "\n";
    f << "  },\n";
    f << "  \"plane\": \"" << (m.plane == PlaneType::STRESS ? "stress" : "strain") << "\",\n";
    f << "  \"max_displacement\": " << max_disp << ",\n";
    f << "  \"max_von_mises\": " << max_stress << ",\n";
    f << "  \"cg_iterations\": " << cg_iterations << ",\n";
    f << "  \"solve_time_ms\": " << solve_time_ms << "\n";
    f << "}\n";
}

}  // namespace postprocess
