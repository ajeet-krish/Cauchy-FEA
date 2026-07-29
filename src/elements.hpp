#pragma once
#include "fea_types.hpp"
#include "sparse.hpp"
#include <cmath>
#include <array>

// ==========================================================================
// ELEMENT STIFFNESS MATRICES -- Bar + Q4 bilinear quad + T3 linear triangle
// ==========================================================================

namespace elements {

// ------------------------------------------------------------------
// Bar element: 2-node truss element (2 DOF per node)
// Local stiffness: k = EA/L * [[1, -1], [-1, 1]]
// ------------------------------------------------------------------
struct BarElement {
    // Compute bar element stiffness in local coordinates
    // Returns 4x4 global DOF indices and 4x4 stiffness matrix
    static std::array<std::array<double, 4>, 4> stiffness(
        const Node& n1, const Node& n2, double A, const Material& mat) {

        double dx = n2.x - n1.x;
        double dy = n2.y - n1.y;
        double L = std::sqrt(dx * dx + dy * dy);
        double EA_L = mat.E * A / L;

        // Direction cosines
        double c = dx / L;  // cos(theta)
        double s = dy / L;  // sin(theta)

        // Rotation matrix T: transforms local [u1, u2] to global [u1x, u1y, u2x, u2y]
        // T = [[c, 0, s, 0],    (wrong -- need proper 4x4 transform)
        //      [0, c, 0, s]]    (wrong)
        //
        // Actually: local DOFs are along the bar axis.
        // Global DOFs: [u1x, u1y, u2x, u2y]
        // Local stiffness in global coords:
        //   K = EA/L * [c^2, cs, -c^2, -cs]
        //              [cs, s^2, -cs, -s^2]
        //              [-c^2, -cs, c^2, cs]
        //              [-cs, -s^2, cs, s^2]

        std::array<std::array<double, 4>, 4> K{};
        K[0][0] =  c * c;   K[0][1] =  c * s;   K[0][2] = -c * c;   K[0][3] = -c * s;
        K[1][0] =  c * s;   K[1][1] =  s * s;   K[1][2] = -c * s;   K[1][3] = -s * s;
        K[2][0] = -c * c;   K[2][1] = -c * s;   K[2][2] =  c * c;   K[2][3] =  c * s;
        K[3][0] = -c * s;   K[3][1] = -s * s;   K[3][2] =  c * s;   K[3][3] =  s * s;

        for (auto& row : K)
            for (auto& val : row)
                val *= EA_L;

        return K;
    }

    // DOF indices for a bar element connecting nodes n1, n2
    static std::array<int, 4> dof_indices(int n1, int n2) {
        return { dof_index(n1, 0), dof_index(n1, 1),
                 dof_index(n2, 0), dof_index(n2, 1) };
    }
};

// ------------------------------------------------------------------
// Q4 element: 4-node bilinear quad (2 DOF per node, 8 DOF total)
// Shape functions: N_i(xi, eta) = 0.25 * (1 + xi_i*xi) * (1 + eta_i*eta)
// Gauss quadrature: 2x2 (full integration)
// ------------------------------------------------------------------
struct Q4Element {
    // Gauss points and weights for 2x2 integration
    static inline const double GP = 1.0 / std::sqrt(3.0);
    static inline const double GW = 1.0;

    // Shape function N_i at (xi, eta)
    // Node ordering: 0=(-1,-1), 1=(+1,-1), 2=(+1,+1), 3=(-1,+1) (CCW)
    static double shape_func(int i, double xi, double eta) {
        static const double xi_pts[4]  = { -1.0,  1.0,  1.0, -1.0 };
        static const double eta_pts[4] = { -1.0, -1.0,  1.0,  1.0 };
        return 0.25 * (1.0 + xi_pts[i] * xi) * (1.0 + eta_pts[i] * eta);
    }

    // Derivatives of shape functions w.r.t. (xi, eta)
    static std::array<double, 2> shape_deriv(int i, double xi, double eta) {
        static const double xi_pts[4]  = { -1.0,  1.0,  1.0, -1.0 };
        static const double eta_pts[4] = { -1.0, -1.0,  1.0,  1.0 };
        double dN_dxi  = 0.25 * xi_pts[i]  * (1.0 + eta_pts[i] * eta);
        double dN_deta = 0.25 * (1.0 + xi_pts[i] * xi) * eta_pts[i];
        return { dN_dxi, dN_deta };
    }

    // Compute element stiffness matrix (8x8)
    // nodes: the 4 corner nodes of the Q4 element
    // mat: material properties
    // plane: plane stress or strain
    static std::array<std::array<double, 8>, 8> stiffness(
        const std::array<Node, 4>& elem_nodes,
        const Material& mat,
        PlaneType plane) {

        auto D = mat.d_matrix(plane);
        std::array<std::array<double, 8>, 8> K{};

        // 2x2 Gauss quadrature
        const double gp[2] = { -GP, GP };
        const double gw[2] = { GW, GW };

        for (int gi = 0; gi < 2; ++gi) {
            for (int gj = 0; gj < 2; ++gj) {
                double xi  = gp[gi];
                double eta = gp[gj];

                // Compute Jacobian J = [[dx/dxi, dy/dxi], [dx/deta, dy/deta]]
                double J11 = 0.0, J12 = 0.0, J21 = 0.0, J22 = 0.0;
                for (int i = 0; i < 4; ++i) {
                    auto dN = shape_deriv(i, xi, eta);
                    J11 += dN[0] * elem_nodes[i].x;
                    J12 += dN[0] * elem_nodes[i].y;
                    J21 += dN[1] * elem_nodes[i].x;
                    J22 += dN[1] * elem_nodes[i].y;
                }

                double detJ = J11 * J22 - J12 * J21;
                if (detJ <= 0.0)
                    throw std::runtime_error("Q4: negative or zero Jacobian");

                // Inverse Jacobian
                double invJ11 =  J22 / detJ;
                double invJ12 = -J12 / detJ;
                double invJ21 = -J21 / detJ;
                double invJ22 =  J11 / detJ;

                // B matrix (3 x 8): strain-displacement
                std::array<std::array<double, 8>, 3> B{};
                for (int i = 0; i < 4; ++i) {
                    auto dN = shape_deriv(i, xi, eta);
                    // dN/dx = invJ * dN/dxi
                    double dNdx = invJ11 * dN[0] + invJ12 * dN[1];
                    double dNdy = invJ21 * dN[0] + invJ22 * dN[1];

                    int col = 2 * i;
                    B[0][col]     = dNdx;   // epsilon_xx
                    B[0][col + 1] = 0.0;
                    B[1][col]     = 0.0;
                    B[1][col + 1] = dNdy;   // epsilon_yy
                    B[2][col]     = dNdy;   // gamma_xy
                    B[2][col + 1] = dNdx;
                }

                // K += B^T * D * B * det(J) * w_i * w_j * t
                double factor = detJ * gw[gi] * gw[gj] * mat.t;

                // Compute D*B (3x8)
                std::array<std::array<double, 8>, 3> DB{};
                for (int r = 0; r < 3; ++r) {
                    for (int c = 0; c < 8; ++c) {
                        double sum = 0.0;
                        for (int k = 0; k < 3; ++k) {
                            sum += D[r][k] * B[k][c];
                        }
                        DB[r][c] = sum;
                    }
                }

                // K += B^T * (D*B) * factor
                for (int i = 0; i < 8; ++i) {
                    for (int j = 0; j < 8; ++j) {
                        double sum = 0.0;
                        for (int k = 0; k < 3; ++k) {
                            sum += B[k][i] * DB[k][j];
                        }
                        K[i][j] += sum * factor;
                    }
                }
            }
        }

        return K;
    }

    // DOF indices for a Q4 element with node indices [n0, n1, n2, n3]
    static std::array<int, 8> dof_indices(const std::array<int, 4>& nodes) {
        return { dof_index(nodes[0], 0), dof_index(nodes[0], 1),
                 dof_index(nodes[1], 0), dof_index(nodes[1], 1),
                 dof_index(nodes[2], 0), dof_index(nodes[2], 1),
                 dof_index(nodes[3], 0), dof_index(nodes[3], 1) };
    }

    // Compute Jacobian determinant at a Gauss point (for validation)
    static double jacobian_det(const std::array<Node, 4>& elem_nodes,
                               double xi, double eta) {
        double J11 = 0.0, J12 = 0.0, J21 = 0.0, J22 = 0.0;
        for (int i = 0; i < 4; ++i) {
            auto dN = shape_deriv(i, xi, eta);
            J11 += dN[0] * elem_nodes[i].x;
            J12 += dN[0] * elem_nodes[i].y;
            J21 += dN[1] * elem_nodes[i].x;
            J22 += dN[1] * elem_nodes[i].y;
        }
        return J11 * J22 - J12 * J21;
    }

    // Compute equivalent thermal load vector (8 DOFs)
    static std::array<double, 8> thermal_load(
        const std::array<Node, 4>& elem_nodes,
        const Material& mat,
        const std::array<double, 4>& temps,
        double T_ref,
        PlaneType plane) {

        auto D = mat.d_matrix(plane);
        std::array<double, 8> fe_th{};

        const double gp[2] = { -GP, GP };
        const double gw[2] = { GW, GW };

        for (int gi = 0; gi < 2; ++gi) {
            for (int gj = 0; gj < 2; ++gj) {
                double xi  = gp[gi];
                double eta = gp[gj];

                double dT = 0.0;
                for (int i = 0; i < 4; ++i) {
                    dT += shape_func(i, xi, eta) * (temps[i] - T_ref);
                }

                if (std::abs(dT) < 1e-15 || mat.alpha == 0.0) continue;

                double thermal_factor = (plane == PlaneType::STRAIN) ? (1.0 + mat.nu) : 1.0;
                double eth_scalar = thermal_factor * mat.alpha * dT;
                std::array<double, 3> e_th = { eth_scalar, eth_scalar, 0.0 };

                std::array<double, 3> s_th = {
                    D[0][0] * e_th[0] + D[0][1] * e_th[1],
                    D[1][0] * e_th[0] + D[1][1] * e_th[1],
                    D[2][0] * e_th[0] + D[2][1] * e_th[1]
                };

                double J11 = 0.0, J12 = 0.0, J21 = 0.0, J22 = 0.0;
                for (int i = 0; i < 4; ++i) {
                    auto dN = shape_deriv(i, xi, eta);
                    J11 += dN[0] * elem_nodes[i].x;
                    J12 += dN[0] * elem_nodes[i].y;
                    J21 += dN[1] * elem_nodes[i].x;
                    J22 += dN[1] * elem_nodes[i].y;
                }

                double detJ = J11 * J22 - J12 * J21;
                if (detJ <= 0.0) throw std::runtime_error("Q4: negative or zero Jacobian in thermal load");

                double invJ11 =  J22 / detJ;
                double invJ12 = -J12 / detJ;
                double invJ21 = -J21 / detJ;
                double invJ22 =  J11 / detJ;

                double factor = detJ * gw[gi] * gw[gj] * mat.t;

                for (int i = 0; i < 4; ++i) {
                    auto dN = shape_deriv(i, xi, eta);
                    double dNdx = invJ11 * dN[0] + invJ12 * dN[1];
                    double dNdy = invJ21 * dN[0] + invJ22 * dN[1];

                    int col = 2 * i;
                    fe_th[col]     += (dNdx * s_th[0] + dNdy * s_th[2]) * factor;
                    fe_th[col + 1] += (dNdy * s_th[1] + dNdx * s_th[2]) * factor;
                }
            }
        }
        return fe_th;
    }
};

// ------------------------------------------------------------------
// T3 element: 3-node linear triangle (2 DOF per node, 6 DOF total)
// Shape functions: N1 = 1 - xi - eta, N2 = xi, N3 = eta
// Gauss quadrature: 3-point rule on triangle
// ------------------------------------------------------------------
struct T3Element {
    // 3-point Gauss quadrature points and weights on reference triangle
    // Points: (1/6, 1/6), (2/3, 1/6), (1/6, 2/3)
    // Weights: 1/6 each (sum = 1/2 = area of reference triangle)
    static inline const double GP[3][2] = {
        {1.0/6.0, 1.0/6.0},
        {2.0/3.0, 1.0/6.0},
        {1.0/6.0, 2.0/3.0}
    };
    static inline const double GW[3] = {1.0/6.0, 1.0/6.0, 1.0/6.0};

    // Shape function N_i at (xi, eta) in area coordinates
    // N1 = 1 - xi - eta, N2 = xi, N3 = eta
    static double shape_func(int i, double xi, double eta) {
        if (i == 0) return 1.0 - xi - eta;
        if (i == 1) return xi;
        return eta;  // i == 2
    }

    // Derivatives of shape functions w.r.t. (xi, eta)
    // dN1/dxi = -1, dN1/deta = -1
    // dN2/dxi =  1, dN2/deta =  0
    // dN3/dxi =  0, dN3/deta =  1
    static std::array<double, 2> shape_deriv(int i, double /*xi*/, double /*eta*/) {
        if (i == 0) return {-1.0, -1.0};
        if (i == 1) return { 1.0,  0.0};
        return {0.0, 1.0};  // i == 2
    }

    // Compute element stiffness matrix (6x6)
    // nodes: the 3 corner nodes of the T3 element
    // mat: material properties
    // plane: plane stress or strain
    static std::array<std::array<double, 6>, 6> stiffness(
        const std::array<Node, 3>& elem_nodes,
        const Material& mat,
        PlaneType plane) {

        auto D = mat.d_matrix(plane);
        std::array<std::array<double, 6>, 6> K{};

        // Compute Jacobian (constant for linear triangle)
        // J = [[dx/dxi, dy/dxi], [dx/deta, dy/deta]]
        // For T3: dx/dxi = x2-x1, dy/dxi = y2-y1
        //         dx/deta = x3-x1, dy/deta = y3-y1
        double J11 = elem_nodes[1].x - elem_nodes[0].x;
        double J12 = elem_nodes[1].y - elem_nodes[0].y;
        double J21 = elem_nodes[2].x - elem_nodes[0].x;
        double J22 = elem_nodes[2].y - elem_nodes[0].y;

        double detJ = J11 * J22 - J12 * J21;
        if (detJ <= 0.0)
            throw std::runtime_error("T3: negative or zero Jacobian (element may be inverted)");

        // Inverse Jacobian (constant for linear triangle)
        double invJ11 =  J22 / detJ;
        double invJ12 = -J12 / detJ;
        double invJ21 = -J21 / detJ;
        double invJ22 =  J11 / detJ;

        // B matrix (3 x 6): strain-displacement (constant for linear triangle)
        std::array<std::array<double, 6>, 3> B{};
        for (int i = 0; i < 3; ++i) {
            auto dN = shape_deriv(i, 0.0, 0.0);  // derivatives are constant
            double dNdx = invJ11 * dN[0] + invJ12 * dN[1];
            double dNdy = invJ21 * dN[0] + invJ22 * dN[1];

            int col = 2 * i;
            B[0][col]     = dNdx;   // epsilon_xx
            B[0][col + 1] = 0.0;
            B[1][col]     = 0.0;
            B[1][col + 1] = dNdy;   // epsilon_yy
            B[2][col]     = dNdy;   // gamma_xy
            B[2][col + 1] = dNdx;
        }

        // Compute D*B (3x6) -- constant for linear triangle
        std::array<std::array<double, 6>, 3> DB{};
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 6; ++c) {
                double sum = 0.0;
                for (int k = 0; k < 3; ++k) {
                    sum += D[r][k] * B[k][c];
                }
                DB[r][c] = sum;
            }
        }

        // K = B^T * D * B * det(J) * t * (sum of weights)
        // For 3-point rule: sum of weights = 3 * (1/6) = 1/2 = area of ref triangle
        // For linear triangle with constant B, this is exact
        double total_weight = 0.0;
        for (int g = 0; g < 3; ++g) {
            total_weight += GW[g];
        }

        double factor = detJ * total_weight * mat.t;

        // K += B^T * (D*B) * factor
        for (int i = 0; i < 6; ++i) {
            for (int j = 0; j < 6; ++j) {
                double sum = 0.0;
                for (int k = 0; k < 3; ++k) {
                    sum += B[k][i] * DB[k][j];
                }
                K[i][j] = sum * factor;
            }
        }

        return K;
    }

    // DOF indices for a T3 element with node indices [n0, n1, n2]
    static std::array<int, 6> dof_indices(const std::array<int, 3>& nodes) {
        return { dof_index(nodes[0], 0), dof_index(nodes[0], 1),
                 dof_index(nodes[1], 0), dof_index(nodes[1], 1),
                 dof_index(nodes[2], 0), dof_index(nodes[2], 1) };
    }

    // Compute element area (for quality metrics)
    static double area(const std::array<Node, 3>& elem_nodes) {
        double J11 = elem_nodes[1].x - elem_nodes[0].x;
        double J12 = elem_nodes[1].y - elem_nodes[0].y;
        double J21 = elem_nodes[2].x - elem_nodes[0].x;
        double J22 = elem_nodes[2].y - elem_nodes[0].y;
        return 0.5 * std::abs(J11 * J22 - J12 * J21);
    }

    // Compute equivalent thermal load vector (6 DOFs)
    static std::array<double, 6> thermal_load(
        const std::array<Node, 3>& elem_nodes,
        const Material& mat,
        const std::array<double, 3>& temps,
        double T_ref,
        PlaneType plane) {

        auto D = mat.d_matrix(plane);
        std::array<double, 6> fe_th{};

        for (int g = 0; g < 3; ++g) {
            double xi = GP[g][0];
            double eta = GP[g][1];

            double dT = 0.0;
            for (int i = 0; i < 3; ++i) {
                dT += shape_func(i, xi, eta) * (temps[i] - T_ref);
            }

            if (std::abs(dT) < 1e-15 || mat.alpha == 0.0) continue;

            double thermal_factor = (plane == PlaneType::STRAIN) ? (1.0 + mat.nu) : 1.0;
            double eth_scalar = thermal_factor * mat.alpha * dT;
            std::array<double, 3> e_th = { eth_scalar, eth_scalar, 0.0 };

            std::array<double, 3> s_th = {
                D[0][0] * e_th[0] + D[0][1] * e_th[1],
                D[1][0] * e_th[0] + D[1][1] * e_th[1],
                D[2][0] * e_th[0] + D[2][1] * e_th[1]
            };

            double J11 = elem_nodes[1].x - elem_nodes[0].x;
            double J12 = elem_nodes[1].y - elem_nodes[0].y;
            double J21 = elem_nodes[2].x - elem_nodes[0].x;
            double J22 = elem_nodes[2].y - elem_nodes[0].y;

            double detJ = J11 * J22 - J12 * J21;
            if (detJ <= 0.0) throw std::runtime_error("T3: negative or zero Jacobian in thermal load");

            double invJ11 =  J22 / detJ;
            double invJ12 = -J12 / detJ;
            double invJ21 = -J21 / detJ;
            double invJ22 =  J11 / detJ;

            double factor = detJ * GW[g] * mat.t;

            static const double dN_dxi[3]  = {-1.0, 1.0, 0.0};
            static const double dN_deta[3] = {-1.0, 0.0, 1.0};

            for (int i = 0; i < 3; ++i) {
                double dNdx = invJ11 * dN_dxi[i] + invJ12 * dN_deta[i];
                double dNdy = invJ21 * dN_dxi[i] + invJ22 * dN_deta[i];

                int col = 2 * i;
                fe_th[col]     += (dNdx * s_th[0] + dNdy * s_th[2]) * factor;
                fe_th[col + 1] += (dNdy * s_th[1] + dNdx * s_th[2]) * factor;
            }
        }
        return fe_th;
    }
};

}  // namespace elements
