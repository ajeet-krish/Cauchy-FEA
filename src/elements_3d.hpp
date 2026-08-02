#pragma once
#include "fea_types.hpp"
#include <array>
#include <cmath>
#include <algorithm>

// ==========================================================================
// 3D FINITE ELEMENTS -- H8 (hexahedron) and T4 (tetrahedron)
// ==========================================================================

namespace elements {

// ------------------------------------------------------------------
// H8: 8-node hexahedron (trilinear)
// ------------------------------------------------------------------
//
// Node ordering (VTK/ABAQUS convention):
//   Bottom face (zeta=-1): 0=(-1,-1,-1), 1=(+1,-1,-1), 2=(+1,+1,-1), 3=(-1,+1,-1)
//   Top face    (zeta=+1): 4=(-1,-1,+1), 5=(+1,-1,+1), 6=(+1,+1,+1), 7=(-1,+1,+1)
//
//    7-------6
//   /|      /|
//  4-------5 |    z
//  | 3-----|-2    | /
//  |/      |/     |/
//  0-------1      +--y
//                /
//               x
//
struct H8Element {
    static constexpr int NNODES = 8;
    static constexpr int NDOF = 24;  // 8 nodes * 3 DOF

    // Natural coordinates of each node
    static constexpr double XI[8]   = {-1, 1, 1,-1,-1, 1, 1,-1};
    static constexpr double ETA[8]  = {-1,-1, 1, 1,-1,-1, 1, 1};
    static constexpr double ZETA[8] = {-1,-1,-1,-1, 1, 1, 1, 1};

    // 2x2x2 Gauss quadrature points and weights
    static inline const double GP3[2] = {-1.0 / std::sqrt(3.0), 1.0 / std::sqrt(3.0)};
    static inline const double GW3[2] = {1.0, 1.0};

    // Shape function N_i at (xi, eta, zeta)
    // N_i = (1/8)(1 + xi_i*xi)(1 + eta_i*eta)(1 + zeta_i*zeta)
    static double shape_func(int i, double xi, double eta, double zeta) {
        return 0.125 * (1.0 + XI[i] * xi)
                           * (1.0 + ETA[i] * eta)
                           * (1.0 + ZETA[i] * zeta);
    }

    // Derivatives of shape function i w.r.t. natural coordinates
    // Returns {dN/dxi, dN/deta, dN/dzeta}
    static std::array<double, 3> shape_deriv(int i, double xi, double eta, double zeta) {
        return {
            0.125 * XI[i] * (1.0 + ETA[i] * eta) * (1.0 + ZETA[i] * zeta),
            0.125 * (1.0 + XI[i] * xi) * ETA[i] * (1.0 + ZETA[i] * zeta),
            0.125 * (1.0 + XI[i] * xi) * (1.0 + ETA[i] * eta) * ZETA[i]
        };
    }

    // Jacobian matrix J (3x3) at natural coordinate (xi, eta, zeta)
    // J[a][b] = sum_i (dN_i/dnatural_a * x_i_b)
    // Stored as flat array row-major: J[0..8] = {J00,J01,J02, J10,J11,J12, J20,J21,J22}
    static std::array<double, 9> jacobian(
        const std::array<Node, 8>& n,
        double xi, double eta, double zeta)
    {
        std::array<double, 9> J{};
        for (int i = 0; i < 8; ++i) {
            auto dN = shape_deriv(i, xi, eta, zeta);
            J[0] += dN[0] * n[i].x;  J[1] += dN[0] * n[i].y;  J[2] += dN[0] * n[i].z;
            J[3] += dN[1] * n[i].x;  J[4] += dN[1] * n[i].y;  J[5] += dN[1] * n[i].z;
            J[6] += dN[2] * n[i].x;  J[7] += dN[2] * n[i].y;  J[8] += dN[2] * n[i].z;
        }
        return J;
    }

    // Determinant of 3x3 matrix stored row-major
    static double det3(const std::array<double, 9>& J) {
        return J[0] * (J[4] * J[8] - J[5] * J[7])
             - J[1] * (J[3] * J[8] - J[5] * J[6])
             + J[2] * (J[3] * J[7] - J[4] * J[6]);
    }

    // Inverse of 3x3 matrix (stored row-major), returns det
    // invJ is output, stored row-major
    static double inv3(const std::array<double, 9>& J, std::array<double, 9>& invJ) {
        double det = det3(J);
        if (std::abs(det) < 1e-30) return 0.0;
        double inv_det = 1.0 / det;
        invJ[0] =  (J[4]*J[8] - J[5]*J[7]) * inv_det;
        invJ[1] = -(J[1]*J[8] - J[2]*J[7]) * inv_det;
        invJ[2] =  (J[1]*J[5] - J[2]*J[4]) * inv_det;
        invJ[3] = -(J[3]*J[8] - J[5]*J[6]) * inv_det;
        invJ[4] =  (J[0]*J[8] - J[2]*J[6]) * inv_det;
        invJ[5] = -(J[0]*J[5] - J[2]*J[3]) * inv_det;
        invJ[6] =  (J[3]*J[7] - J[4]*J[6]) * inv_det;
        invJ[7] = -(J[0]*J[7] - J[1]*J[6]) * inv_det;
        invJ[8] =  (J[0]*J[4] - J[1]*J[3]) * inv_det;
        return det;
    }

    // B matrix: 6 x 24 (6 strain components x 24 DOFs)
    // Strain ordering: [eps_xx, eps_yy, eps_zz, gamma_yz, gamma_xz, gamma_xy]
    //
    // For node i (DOF at columns 3i, 3i+1, 3i+2):
    //   B[0][3i]   = dN_i/dx           (eps_xx)
    //   B[1][3i+1] = dN_i/dy           (eps_yy)
    //   B[2][3i+2] = dN_i/dz           (eps_zz)
    //   B[3][3i+1] = dN_i/dz           (gamma_yz)
    //   B[3][3i+2] = dN_i/dy
    //   B[4][3i]   = dN_i/dz           (gamma_xz)
    //   B[4][3i+2] = dN_i/dx
    //   B[5][3i]   = dN_i/dy           (gamma_xy)
    //   B[5][3i+1] = dN_i/dx
    //
    static void compute_B(
        const std::array<Node, 8>& elem_nodes,
        double xi, double eta, double zeta,
        std::array<std::array<double, 24>, 6>& B)
    {
        // Get Jacobian and its inverse
        auto J = jacobian(elem_nodes, xi, eta, zeta);
        std::array<double, 9> invJ;
        inv3(J, invJ);

        // Compute dN/dx, dN/dy, dN/dz for each node via chain rule:
        // [dN/dx, dN/dy, dN/dz]^T = J^{-1} [dN/dxi, dN/deta, dN/dzeta]^T
        for (int i = 0; i < 8; ++i) {
            auto dN_nat = shape_deriv(i, xi, eta, zeta);
            double dNdx = invJ[0]*dN_nat[0] + invJ[1]*dN_nat[1] + invJ[2]*dN_nat[2];
            double dNdy = invJ[3]*dN_nat[0] + invJ[4]*dN_nat[1] + invJ[5]*dN_nat[2];
            double dNdz = invJ[6]*dN_nat[0] + invJ[7]*dN_nat[1] + invJ[8]*dN_nat[2];

            int c = 3 * i;
            // Zero out column
            for (int r = 0; r < 6; ++r) B[r][c] = B[r][c+1] = B[r][c+2] = 0.0;
            // Fill non-zero entries
            B[0][c]   = dNdx;              // eps_xx
            B[1][c+1] = dNdy;              // eps_yy
            B[2][c+2] = dNdz;              // eps_zz
            B[3][c+1] = dNdz;  B[3][c+2] = dNdy;  // gamma_yz
            B[4][c]   = dNdz;  B[4][c+2] = dNdx;  // gamma_xz
            B[5][c]   = dNdy;  B[5][c+1] = dNdx;  // gamma_xy
        }
    }

    // Element stiffness matrix (24x24)
    static std::array<std::array<double, NDOF>, NDOF> stiffness(
        const std::array<Node, NNODES>& elem_nodes,
        const Material& mat)
    {
        auto D = mat.d_matrix_3d();
        std::array<std::array<double, NDOF>, NDOF> K{};

        // 2x2x2 Gauss quadrature
        for (int gi = 0; gi < 2; ++gi) {
            for (int gj = 0; gj < 2; ++gj) {
                for (int gk = 0; gk < 2; ++gk) {
                    double xi  = GP3[gi];
                    double eta = GP3[gj];
                    double zeta = GP3[gk];

                    auto J = jacobian(elem_nodes, xi, eta, zeta);
                    double detJ = det3(J);
                    double w = GW3[gi] * GW3[gj] * GW3[gk] * detJ;

                    // Compute B matrix
                    std::array<std::array<double, 24>, 6> B{};
                    compute_B(elem_nodes, xi, eta, zeta, B);

                    // K += B^T * D * B * w
                    // Compute D*B first (6 x 24)
                    std::array<std::array<double, 24>, 6> DB{};
                    for (int r = 0; r < 6; ++r)
                        for (int c = 0; c < 24; ++c)
                            for (int k = 0; k < 6; ++k)
                                DB[r][c] += D[r][k] * B[k][c];

                    // K += B^T * DB * w
                    for (int r = 0; r < 24; ++r)
                        for (int c = 0; c < 24; ++c)
                            for (int k = 0; k < 6; ++k)
                                K[r][c] += B[k][r] * DB[k][c] * w;
                }
            }
        }
        return K;
    }

    // DOF indices for assembly: [3*n0, 3*n0+1, 3*n0+2, 3*n1, ..., 3*n7+2]
    static std::array<int, NDOF> dof_indices(const std::array<int, NNODES>& nodes) {
        std::array<int, NDOF> idx;
        for (int i = 0; i < NNODES; ++i) {
            idx[3*i]   = dof_index(nodes[i], 0);
            idx[3*i+1] = dof_index(nodes[i], 1);
            idx[3*i+2] = dof_index(nodes[i], 2);
        }
        return idx;
    }

    // Jacobian determinant (convenience)
    static double jacobian_det(
        const std::array<Node, NNODES>& elem_nodes,
        double xi, double eta, double zeta)
    {
        return det3(jacobian(elem_nodes, xi, eta, zeta));
    }

    // Element volume (exact for trilinear hex)
    static double volume(const std::array<Node, NNODES>& elem_nodes) {
        double vol = 0.0;
        for (int gi = 0; gi < 2; ++gi)
            for (int gj = 0; gj < 2; ++gj)
                for (int gk = 0; gk < 2; ++gk) {
                    double detJ = jacobian_det(elem_nodes, GP3[gi], GP3[gj], GP3[gk]);
                    vol += GW3[gi] * GW3[gj] * GW3[gk] * detJ;
                }
        return vol;
    }

    // Stress at a Gauss point from element displacement vector
    static std::array<double, 6> stress_at_gauss_point(
        const std::array<Node, NNODES>& elem_nodes,
        const std::array<double, NDOF>& u_elem,
        const Material& mat,
        double xi, double eta, double zeta)
    {
        std::array<std::array<double, 24>, 6> B{};
        compute_B(elem_nodes, xi, eta, zeta, B);

        auto D = mat.d_matrix_3d();
        // sigma = D * B * u
        std::array<double, 6> sigma{};
        for (int r = 0; r < 6; ++r)
            for (int c = 0; c < 24; ++c)
                for (int k = 0; k < 6; ++k)
                    sigma[r] += D[r][k] * B[k][c] * u_elem[c];
        return sigma;
    }
};

// ------------------------------------------------------------------
// T4: 4-node tetrahedron (linear)
// ------------------------------------------------------------------
//
// Node ordering (volume coordinates):
//   0 = (0, 0, 0)
//   1 = (1, 0, 0)
//   2 = (0, 1, 0)
//   3 = (0, 0, 1)
//
struct T4Element {
    static constexpr int NNODES = 4;
    static constexpr int NDOF = 12;  // 4 nodes * 3 DOF

    // Shape functions: N1 = 1-xi-eta-zeta, N2 = xi, N3 = eta, N4 = zeta
    static double shape_func(int i, double xi, double eta, double zeta) {
        switch (i) {
            case 0: return 1.0 - xi - eta - zeta;
            case 1: return xi;
            case 2: return eta;
            case 3: return zeta;
        }
        return 0.0;
    }

    // Derivatives (constant for linear tet)
    static std::array<double, 3> shape_deriv(int i, double /*xi*/, double /*eta*/, double /*zeta*/) {
        switch (i) {
            case 0: return {-1.0, -1.0, -1.0};
            case 1: return { 1.0,  0.0,  0.0};
            case 2: return { 0.0,  1.0,  0.0};
            case 3: return { 0.0,  0.0,  1.0};
        }
        return {0.0, 0.0, 0.0};
    }

    // Jacobian (constant for linear tet)
    static std::array<double, 9> jacobian(const std::array<Node, 4>& n) {
        std::array<double, 9> J{};
        // Columns: x, y, z coordinates of edges from node 0
        J[0] = n[1].x - n[0].x;  J[1] = n[1].y - n[0].y;  J[2] = n[1].z - n[0].z;
        J[3] = n[2].x - n[0].x;  J[4] = n[2].y - n[0].y;  J[5] = n[2].z - n[0].z;
        J[6] = n[3].x - n[0].x;  J[7] = n[3].y - n[0].y;  J[8] = n[3].z - n[0].z;
        return J;
    }

    // Determinant of 3x3 matrix
    static double det3(const std::array<double, 9>& J) {
        return J[0] * (J[4] * J[8] - J[5] * J[7])
             - J[1] * (J[3] * J[8] - J[5] * J[6])
             + J[2] * (J[3] * J[7] - J[4] * J[6]);
    }

    // Inverse of 3x3 matrix, returns det
    static double inv3(const std::array<double, 9>& J, std::array<double, 9>& invJ) {
        double det = det3(J);
        if (std::abs(det) < 1e-30) return 0.0;
        double inv_det = 1.0 / det;
        invJ[0] =  (J[4]*J[8] - J[5]*J[7]) * inv_det;
        invJ[1] = -(J[1]*J[8] - J[2]*J[7]) * inv_det;
        invJ[2] =  (J[1]*J[5] - J[2]*J[4]) * inv_det;
        invJ[3] = -(J[3]*J[8] - J[5]*J[6]) * inv_det;
        invJ[4] =  (J[0]*J[8] - J[2]*J[6]) * inv_det;
        invJ[5] = -(J[0]*J[5] - J[2]*J[3]) * inv_det;
        invJ[6] =  (J[3]*J[7] - J[4]*J[6]) * inv_det;
        invJ[7] = -(J[0]*J[7] - J[1]*J[6]) * inv_det;
        invJ[8] =  (J[0]*J[4] - J[1]*J[3]) * inv_det;
        return det;
    }

    // B matrix: 6 x 12 (constant for linear tet)
    static void compute_B(
        const std::array<Node, 4>& elem_nodes,
        std::array<std::array<double, 12>, 6>& B)
    {
        auto J = jacobian(elem_nodes);
        std::array<double, 9> invJ;
        inv3(J, invJ);

        for (int i = 0; i < 4; ++i) {
            auto dN_nat = shape_deriv(i, 0, 0, 0);
            double dNdx = invJ[0]*dN_nat[0] + invJ[3]*dN_nat[1] + invJ[6]*dN_nat[2];
            double dNdy = invJ[1]*dN_nat[0] + invJ[4]*dN_nat[1] + invJ[7]*dN_nat[2];
            double dNdz = invJ[2]*dN_nat[0] + invJ[5]*dN_nat[1] + invJ[8]*dN_nat[2];

            int c = 3 * i;
            for (int r = 0; r < 6; ++r) B[r][c] = B[r][c+1] = B[r][c+2] = 0.0;
            B[0][c]   = dNdx;
            B[1][c+1] = dNdy;
            B[2][c+2] = dNdz;
            B[3][c+1] = dNdz;  B[3][c+2] = dNdy;
            B[4][c]   = dNdz;  B[4][c+2] = dNdx;
            B[5][c]   = dNdy;  B[5][c+1] = dNdx;
        }
    }

    // Element stiffness matrix (12x12)
    static std::array<std::array<double, NDOF>, NDOF> stiffness(
        const std::array<Node, NNODES>& elem_nodes,
        const Material& mat)
    {
        auto D = mat.d_matrix_3d();
        std::array<std::array<double, NDOF>, NDOF> K{};

        // B is constant, compute once
        std::array<std::array<double, 12>, 6> B{};
        compute_B(elem_nodes, B);

        double detJ = det3(jacobian(elem_nodes));
        double vol_factor = std::abs(detJ) / 6.0;  // volume of reference tet * weight

        // K = B^T * D * B * volume
        // Compute D*B first (6 x 12)
        std::array<std::array<double, 12>, 6> DB{};
        for (int r = 0; r < 6; ++r)
            for (int c = 0; c < 12; ++c)
                for (int k = 0; k < 6; ++k)
                    DB[r][c] += D[r][k] * B[k][c];

        // K = B^T * DB * vol_factor
        for (int r = 0; r < 12; ++r)
            for (int c = 0; c < 12; ++c)
                for (int k = 0; k < 6; ++k)
                    K[r][c] += B[k][r] * DB[k][c] * vol_factor;

        return K;
    }

    // DOF indices for assembly
    static std::array<int, NDOF> dof_indices(const std::array<int, NNODES>& nodes) {
        std::array<int, NDOF> idx;
        for (int i = 0; i < NNODES; ++i) {
            idx[3*i]   = dof_index(nodes[i], 0);
            idx[3*i+1] = dof_index(nodes[i], 1);
            idx[3*i+2] = dof_index(nodes[i], 2);
        }
        return idx;
    }

    // Element volume
    static double volume(const std::array<Node, NNODES>& elem_nodes) {
        return std::abs(det3(jacobian(elem_nodes))) / 6.0;
    }

    // Jacobian determinant
    static double jacobian_det(const std::array<Node, NNODES>& elem_nodes) {
        return det3(jacobian(elem_nodes));
    }

    // Stress at a point (constant throughout element for linear tet)
    static std::array<double, 6> stress(
        const std::array<Node, NNODES>& elem_nodes,
        const std::array<double, NDOF>& u_elem,
        const Material& mat)
    {
        std::array<std::array<double, 12>, 6> B{};
        compute_B(elem_nodes, B);

        auto D = mat.d_matrix_3d();
        std::array<double, 6> sigma{};
        for (int r = 0; r < 6; ++r)
            for (int c = 0; c < 12; ++c)
                for (int k = 0; k < 6; ++k)
                    sigma[r] += D[r][k] * B[k][c] * u_elem[c];
        return sigma;
    }
};

}  // namespace elements
