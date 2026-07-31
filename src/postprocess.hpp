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
// sigma = D * (B * u - epsilon_th)
// ------------------------------------------------------------------
inline ElementStress compute_q4_stress(
    const std::array<Node, 4>& elem_nodes,
    const std::array<double, 8>& u_elem,
    const Material& mat,
    PlaneType plane,
    const std::array<double, 4>& temps = {{0.0, 0.0, 0.0, 0.0}},
    double T_ref = 0.0) {

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

    // Subtract thermal strain if temperature data is provided
    if (mat.alpha != 0.0) {
        double dT = 0.0;
        for (int i = 0; i < 4; ++i) {
            double N_i = 0.25 * (1.0 + xi_pts[i] * xi) * (1.0 + eta_pts[i] * eta);
            dT += N_i * (temps[i] - T_ref);
        }
        double thermal_factor = (plane == PlaneType::STRAIN) ? (1.0 + mat.nu) : 1.0;
        double eth_scalar = thermal_factor * mat.alpha * dT;
        eps_xx -= eth_scalar;
        eps_yy -= eth_scalar;
    }

    // Stress = D * (strain - epsilon_th)
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
// Compute element stress from T3 element displacement
// sigma = D * (B * u - epsilon_th) (constant stress for linear triangle)
// ------------------------------------------------------------------
inline ElementStress compute_t3_stress(
    const std::array<Node, 3>& elem_nodes,
    const std::array<double, 6>& u_elem,
    const Material& mat,
    PlaneType plane,
    const std::array<double, 3>& temps = {{0.0, 0.0, 0.0}},
    double T_ref = 0.0) {

    auto D = mat.d_matrix(plane);

    // Compute Jacobian (constant for linear triangle)
    double J11 = elem_nodes[1].x - elem_nodes[0].x;
    double J12 = elem_nodes[1].y - elem_nodes[0].y;
    double J21 = elem_nodes[2].x - elem_nodes[0].x;
    double J22 = elem_nodes[2].y - elem_nodes[0].y;

    double detJ = J11 * J22 - J12 * J21;
    double invJ11 =  J22 / detJ;
    double invJ12 = -J12 / detJ;
    double invJ21 = -J21 / detJ;
    double invJ22 =  J11 / detJ;

    // B matrix (3 x 6) -- constant for linear triangle
    std::array<std::array<double, 6>, 3> B{};
    // Shape function derivatives: dN1/dxi=-1, dN1/deta=-1, dN2/dxi=1, dN3/deta=1
    static const double dN_dxi[3]  = {-1.0, 1.0, 0.0};
    static const double dN_deta[3] = {-1.0, 0.0, 1.0};

    for (int i = 0; i < 3; ++i) {
        double dNdx = invJ11 * dN_dxi[i] + invJ12 * dN_deta[i];
        double dNdy = invJ21 * dN_dxi[i] + invJ22 * dN_deta[i];

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
    for (int j = 0; j < 6; ++j) {
        eps_xx += B[0][j] * u_elem[j];
        eps_yy += B[1][j] * u_elem[j];
        gamma_xy += B[2][j] * u_elem[j];
    }

    // Subtract thermal strain if temperature data is provided
    if (mat.alpha != 0.0) {
        double dT = 0.0;
        for (int i = 0; i < 3; ++i) {
            dT += temps[i] - T_ref;  // T3 shape functions at centroid sum to 1
        }
        dT /= 3.0;  // Average temperature at element level
        double thermal_factor = (plane == PlaneType::STRAIN) ? (1.0 + mat.nu) : 1.0;
        double eth_scalar = thermal_factor * mat.alpha * dT;
        eps_xx -= eth_scalar;
        eps_yy -= eth_scalar;
    }

    // Stress = D * (strain - epsilon_th)
    ElementStress s;
    s.sigma_xx = D[0][0] * eps_xx + D[0][1] * eps_yy + D[0][2] * gamma_xy;
    s.sigma_yy = D[1][0] * eps_xx + D[1][1] * eps_yy + D[1][2] * gamma_xy;
    s.sigma_xy = D[2][0] * eps_xx + D[2][1] * eps_yy + D[2][2] * gamma_xy;

    // Principal stresses and Von Mises
    double avg = (s.sigma_xx + s.sigma_yy) / 2.0;
    double R = std::sqrt(((s.sigma_xx - s.sigma_yy) / 2.0) * ((s.sigma_xx - s.sigma_yy) / 2.0)
                         + s.sigma_xy * s.sigma_xy);
    double s1 = avg + R;
    double s2 = avg - R;
    s.sigma_1 = s1;
    s.sigma_2 = s2;

    if (plane == PlaneType::STRESS) {
        s.von_mises = std::sqrt(s1 * s1 - s1 * s2 + s2 * s2);
    } else {
        double s3 = mat.nu * (s.sigma_xx + s.sigma_yy);
        s.von_mises = std::sqrt(0.5 * ((s1 - s2) * (s1 - s2) +
                                        (s2 - s3) * (s2 - s3) +
                                        (s3 - s1) * (s3 - s1)));
    }

    return s;
}

// ------------------------------------------------------------------
// Compute element stress from Q8 element displacement
// Uses 2x2 Gauss point averaging (superconvergent points for Q8)
// Center point evaluation is wrong for Q8: corner node shape
// function derivatives are zero at the center.
// ------------------------------------------------------------------
inline ElementStress compute_q8_stress(
    const std::vector<Node>& elem_nodes,
    const std::array<double, 16>& u_elem,
    const Material& mat,
    PlaneType plane) {

    auto D = mat.d_matrix(plane);
    static const double SQRT3_INV = 0.5773502691896257; // 1/sqrt(3)
    static const double GP2[2] = { -SQRT3_INV, SQRT3_INV };

    // Average stress at 2x2 Gauss points
    double avg_sigma_xx = 0.0, avg_sigma_yy = 0.0, avg_sigma_xy = 0.0;

    for (int gi = 0; gi < 2; ++gi) {
        for (int gj = 0; gj < 2; ++gj) {
            double xi = GP2[gi], eta = GP2[gj];
            double J11 = 0.0, J12 = 0.0, J21 = 0.0, J22 = 0.0;

            // Reuse elements.hpp shape_deriv for these Gauss points
            // But we need it inline here since elements.hpp is separate
            for (int i = 0; i < 8; ++i) {
                double dN_dxi = 0.0, dN_deta = 0.0;
                if (i < 4) {
                    static const double XI[4]  = { -1.0,  1.0,  1.0, -1.0 };
                    static const double ETA[4] = { -1.0, -1.0,  1.0,  1.0 };
                    double xi_i = XI[i], eta_i = ETA[i];
                    dN_dxi  = 0.25 * xi_i * (1.0 + eta_i * eta) * (2.0 * xi_i * xi + eta_i * eta);
                    dN_deta = 0.25 * eta_i * (1.0 + xi_i * xi) * (xi_i * xi + 2.0 * eta_i * eta);
                } else {
                    double xi2 = xi * xi, eta2 = eta * eta;
                    switch (i) {
                        case 4: dN_dxi = -xi * (1.0 - eta); dN_deta = -0.5 * (1.0 - xi2); break;
                        case 5: dN_dxi =  0.5 * (1.0 - eta2); dN_deta = -eta * (1.0 + xi); break;
                        case 6: dN_dxi = -xi * (1.0 + eta); dN_deta =  0.5 * (1.0 - xi2); break;
                        case 7: dN_dxi = -0.5 * (1.0 - eta2); dN_deta = -eta * (1.0 - xi); break;
                    }
                }
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

            // B matrix at this Gauss point
            double eps_xx = 0.0, eps_yy = 0.0, gamma_xy = 0.0;
            for (int i = 0; i < 8; ++i) {
                double dN_dxi = 0.0, dN_deta = 0.0;
                if (i < 4) {
                    static const double XI[4]  = { -1.0,  1.0,  1.0, -1.0 };
                    static const double ETA[4] = { -1.0, -1.0,  1.0,  1.0 };
                    double xi_i = XI[i], eta_i = ETA[i];
                    dN_dxi  = 0.25 * xi_i * (1.0 + eta_i * eta) * (2.0 * xi_i * xi + eta_i * eta);
                    dN_deta = 0.25 * eta_i * (1.0 + xi_i * xi) * (xi_i * xi + 2.0 * eta_i * eta);
                } else {
                    double xi2 = xi * xi, eta2 = eta * eta;
                    switch (i) {
                        case 4: dN_dxi = -xi * (1.0 - eta); dN_deta = -0.5 * (1.0 - xi2); break;
                        case 5: dN_dxi =  0.5 * (1.0 - eta2); dN_deta = -eta * (1.0 + xi); break;
                        case 6: dN_dxi = -xi * (1.0 + eta); dN_deta =  0.5 * (1.0 - xi2); break;
                        case 7: dN_dxi = -0.5 * (1.0 - eta2); dN_deta = -eta * (1.0 - xi); break;
                    }
                }
                double dNdx = invJ11 * dN_dxi + invJ12 * dN_deta;
                double dNdy = invJ21 * dN_dxi + invJ22 * dN_deta;

                int col = 2 * i;
                eps_xx    += dNdx * u_elem[col];
                eps_xx    += 0.0 * u_elem[col + 1];
                eps_yy    += 0.0 * u_elem[col];
                eps_yy    += dNdy * u_elem[col + 1];
                gamma_xy  += dNdy * u_elem[col];
                gamma_xy  += dNdx * u_elem[col + 1];
            }

            avg_sigma_xx += D[0][0] * eps_xx + D[0][1] * eps_yy + D[0][2] * gamma_xy;
            avg_sigma_yy += D[1][0] * eps_xx + D[1][1] * eps_yy + D[1][2] * gamma_xy;
            avg_sigma_xy += D[2][0] * eps_xx + D[2][1] * eps_yy + D[2][2] * gamma_xy;
        }
    }

    double npoints = 4.0;
    ElementStress s;
    s.sigma_xx = avg_sigma_xx / npoints;
    s.sigma_yy = avg_sigma_yy / npoints;
    s.sigma_xy = avg_sigma_xy / npoints;

    // Principal stresses and Von Mises
    double avg = (s.sigma_xx + s.sigma_yy) / 2.0;
    double R = std::sqrt(((s.sigma_xx - s.sigma_yy) / 2.0) * ((s.sigma_xx - s.sigma_yy) / 2.0)
                         + s.sigma_xy * s.sigma_xy);
    double s1 = avg + R;
    double s2 = avg - R;
    s.sigma_1 = s1;
    s.sigma_2 = s2;

    if (plane == PlaneType::STRESS) {
        s.von_mises = std::sqrt(s1 * s1 - s1 * s2 + s2 * s2);
    } else {
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

    int total_elements = m.num_quads() + m.num_quad8s() + m.num_tris();
    std::vector<ElementStress> stresses(total_elements);

    // Build temperature arrays if available
    bool has_temps = m.temperature.size() == static_cast<size_t>(m.num_nodes()) && m.mat.alpha != 0.0;

    // Compute Q4 stresses
    #pragma omp parallel for
    for (int e = 0; e < m.num_quads(); ++e) {
        const auto& elem = m.quad_elements[e];
        std::array<Node, 4> elem_nodes;
        std::array<double, 8> u_elem;
        std::array<double, 4> temps{};

        for (int i = 0; i < 4; ++i) {
            elem_nodes[i] = m.nodes[elem[i]];
            u_elem[2 * i]     = u[dof_index(elem[i], 0)];
            u_elem[2 * i + 1] = u[dof_index(elem[i], 1)];
            if (has_temps) temps[i] = m.temperature[elem[i]];
        }

        stresses[e] = compute_q4_stress(elem_nodes, u_elem, m.mat, m.plane, temps, m.T_ref);
    }

    // Compute Q8 stresses
    #pragma omp parallel for
    for (int e = 0; e < m.num_quad8s(); ++e) {
        const auto& elem = m.quad8_elements[e];
        std::vector<Node> elem_nodes(8);
        std::array<double, 16> u_elem;

        for (int i = 0; i < 8; ++i) {
            elem_nodes[i] = m.nodes[elem[i]];
            u_elem[2 * i]     = u[dof_index(elem[i], 0)];
            u_elem[2 * i + 1] = u[dof_index(elem[i], 1)];
        }

        stresses[m.num_quads() + e] = compute_q8_stress(elem_nodes, u_elem, m.mat, m.plane);
    }

    // Compute T3 stresses
    #pragma omp parallel for
    for (int e = 0; e < m.num_tris(); ++e) {
        const auto& elem = m.tri_elements[e];
        std::array<Node, 3> elem_nodes;
        std::array<double, 6> u_elem;
        std::array<double, 3> temps{};

        for (int i = 0; i < 3; ++i) {
            elem_nodes[i] = m.nodes[elem[i]];
            u_elem[2 * i]     = u[dof_index(elem[i], 0)];
            u_elem[2 * i + 1] = u[dof_index(elem[i], 1)];
            if (has_temps) temps[i] = m.temperature[elem[i]];
        }

        stresses[m.num_quads() + m.num_quad8s() + e] = compute_t3_stress(elem_nodes, u_elem, m.mat, m.plane, temps, m.T_ref);
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

    // Average Q4 element stresses to nodes
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

    // Average Q8 element stresses to nodes
    for (int e = 0; e < m.num_quad8s(); ++e) {
        const auto& elem = m.quad8_elements[e];
        int idx = m.num_quads() + e;
        for (int i = 0; i < 8; ++i) {
            int n = elem[i];
            node_stress[n].sigma_xx += elem_stresses[idx].sigma_xx;
            node_stress[n].sigma_yy += elem_stresses[idx].sigma_yy;
            node_stress[n].sigma_xy += elem_stresses[idx].sigma_xy;
            node_stress[n].von_mises += elem_stresses[idx].von_mises;
            node_stress[n].count++;
        }
    }

    // Average T3 element stresses to nodes
    for (int e = 0; e < m.num_tris(); ++e) {
        const auto& elem = m.tri_elements[e];
        int idx = m.num_quads() + m.num_quad8s() + e;
        for (int i = 0; i < 3; ++i) {
            int n = elem[i];
            node_stress[n].sigma_xx += elem_stresses[idx].sigma_xx;
            node_stress[n].sigma_yy += elem_stresses[idx].sigma_yy;
            node_stress[n].sigma_xy += elem_stresses[idx].sigma_xy;
            node_stress[n].von_mises += elem_stresses[idx].von_mises;
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

    int total_elements = m.num_quads() + m.num_quad8s() + m.num_tris();
    std::ofstream f(filepath);
    f << std::fixed << std::setprecision(6);
    f << "{\n";
    f << "  \"num_elements\": " << total_elements << ",\n";
    f << "  \"elements\": [\n";
    for (int i = 0; i < total_elements; ++i) {
        f << "    {\"sigma_xx\": " << stresses[i].sigma_xx
          << ", \"sigma_yy\": " << stresses[i].sigma_yy
          << ", \"sigma_xy\": " << stresses[i].sigma_xy
          << ", \"von_mises\": " << stresses[i].von_mises
          << ", \"sigma_1\": " << stresses[i].sigma_1
          << ", \"sigma_2\": " << stresses[i].sigma_2 << "}";
        if (i < total_elements - 1) f << ",";
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
    f << "  \"case\": \"" << case_name(g_case) << "\",\n";
    f << "  \"num_nodes\": " << m.num_nodes() << ",\n";
    f << "  \"num_elements\": " << (m.num_quads() + m.num_tris()) << ",\n";
    f << "  \"num_quads\": " << m.num_quads() << ",\n";
    f << "  \"num_tris\": " << m.num_tris() << ",\n";
    f << "  \"num_dofs\": " << m.num_dofs() << ",\n";
    f << "  \"material\": {\n";
    f << "    \"E\": " << m.mat.E << ",\n";
    f << "    \"nu\": " << m.mat.nu << ",\n";
    f << "    \"rho\": " << m.mat.rho << ",\n";
    f << "    \"t\": " << m.mat.t << ",\n";
    f << "    \"alpha\": " << m.mat.alpha << "\n";
    f << "  },\n";
    f << "  \"plane\": \"" << (m.plane == PlaneType::STRESS ? "stress" : "strain") << "\",\n";
    f << "  \"max_displacement\": " << max_disp << ",\n";
    f << "  \"max_von_mises\": " << max_stress << ",\n";
    f << "  \"cg_iterations\": " << cg_iterations << ",\n";
    f << "  \"solve_time_ms\": " << solve_time_ms << "\n";
    if (m.temperature.size() == static_cast<size_t>(m.num_nodes())) {
        double t_min = *std::min_element(m.temperature.begin(), m.temperature.end());
        double t_max = *std::max_element(m.temperature.begin(), m.temperature.end());
        f << "  ,\"has_thermal\": true,\n";
        f << "  \"T_min\": " << t_min << ",\n";
        f << "  \"T_max\": " << t_max << ",\n";
        f << "  \"T_ref\": " << m.T_ref << "\n";
    }
    f << "}\n";
}

// ------------------------------------------------------------------
// Write mesh connectivity to JSON (for browser viewer)
// ------------------------------------------------------------------
inline void write_mesh_json(
    const std::string& filepath,
    const Mesh& m) {

    // Compute quality metrics
    auto mq = mesh::compute_mesh_quality(m);

    std::ofstream f(filepath);
    f << std::fixed << std::setprecision(6);
    f << "{\n";
    f << "  \"num_nodes\": " << m.num_nodes() << ",\n";
    f << "  \"num_elements\": " << (m.num_quads() + m.num_quad8s() + m.num_tris()) << ",\n";
    f << "  \"num_quads\": " << m.num_quads() << ",\n";
    f << "  \"num_quad8s\": " << m.num_quad8s() << ",\n";
    f << "  \"num_tris\": " << m.num_tris() << ",\n";
    f << "  \"nodes\": [\n";
    for (int i = 0; i < m.num_nodes(); ++i) {
        f << "    {\"x\": " << m.nodes[i].x
          << ", \"y\": " << m.nodes[i].y << "}";
        if (i < m.num_nodes() - 1) f << ",";
        f << "\n";
    }
    f << "  ],\n";
    // Output Q4 elements with quality
    f << "  \"quad_elements\": [\n";
    for (int i = 0; i < m.num_quads(); ++i) {
        f << "    {\"n0\": " << m.quad_elements[i][0]
          << ", \"n1\": " << m.quad_elements[i][1]
          << ", \"n2\": " << m.quad_elements[i][2]
          << ", \"n3\": " << m.quad_elements[i][3]
          << ", \"jacobian_ratio\": " << mq.quad_quality[i].jacobian_ratio
          << ", \"aspect_ratio\": " << mq.quad_quality[i].aspect_ratio
          << ", \"skewness\": " << mq.quad_quality[i].skewness
          << ", \"area\": " << mq.quad_quality[i].area << "}";
        if (i < m.num_quads() - 1) f << ",";
        f << "\n";
    }
    f << "  ],\n";
    // Output Q8 elements
    f << "  \"quad8_elements\": [\n";
    for (int i = 0; i < m.num_quad8s(); ++i) {
        f << "    {\"n0\": " << m.quad8_elements[i][0]
          << ", \"n1\": " << m.quad8_elements[i][1]
          << ", \"n2\": " << m.quad8_elements[i][2]
          << ", \"n3\": " << m.quad8_elements[i][3]
          << ", \"n4\": " << m.quad8_elements[i][4]
          << ", \"n5\": " << m.quad8_elements[i][5]
          << ", \"n6\": " << m.quad8_elements[i][6]
          << ", \"n7\": " << m.quad8_elements[i][7] << "}";
        if (i < m.num_quad8s() - 1) f << ",";
        f << "\n";
    }
    f << "  ],\n";
    // Output T3 elements with quality
    f << "  \"tri_elements\": [\n";
    for (int i = 0; i < m.num_tris(); ++i) {
        f << "    {\"n0\": " << m.tri_elements[i][0]
          << ", \"n1\": " << m.tri_elements[i][1]
          << ", \"n2\": " << m.tri_elements[i][2]
          << ", \"jacobian_ratio\": " << mq.tri_quality[i].jacobian_ratio
          << ", \"aspect_ratio\": " << mq.tri_quality[i].aspect_ratio
          << ", \"skewness\": " << mq.tri_quality[i].skewness
          << ", \"area\": " << mq.tri_quality[i].area << "}";
        if (i < m.num_tris() - 1) f << ",";
        f << "\n";
    }
    f << "  ],\n";
    // Output quality summary
    f << "  \"quality_summary\": {\n";
    f << "    \"min_jacobian_ratio\": " << mq.min_jacobian_ratio << ",\n";
    f << "    \"max_aspect_ratio\": " << mq.max_aspect_ratio << ",\n";
    f << "    \"max_skewness\": " << mq.max_skewness << "\n";
    f << "  },\n";
    // Output boundary conditions
    f << "  \"dirichlet\": [\n";
    for (int i = 0; i < static_cast<int>(m.dirichlet.size()); ++i) {
        const auto& bc = m.dirichlet[i];
        f << "    {\"node\": " << bc.node
          << ", \"dof\": " << bc.dof
          << ", \"value\": " << bc.value << "}";
        if (i < static_cast<int>(m.dirichlet.size()) - 1) f << ",";
        f << "\n";
    }
    f << "  ],\n";
    f << "  \"neumann\": [\n";
    for (int i = 0; i < static_cast<int>(m.neumann.size()); ++i) {
        const auto& bc = m.neumann[i];
        f << "    {\"node\": " << bc.node
          << ", \"dof\": " << bc.dof
          << ", \"value\": " << bc.value << "}";
        if (i < static_cast<int>(m.neumann.size()) - 1) f << ",";
        f << "\n";
    }
    f << "  ]\n";
    f << "}\n";
}

}  // namespace postprocess
