#pragma once
#include "fea_types.hpp"
#include "elements.hpp"
#include <array>
#include <cmath>

// ==========================================================================
// SHEAR LOCKING MITIGATION -- SRI and B-Bar for Q4 elements
// ==========================================================================
//
// Problem: Standard Q4 with 2x2 Gauss integration "locks" in bending
// because the shear strain cannot be represented with linear functions
// while maintaining constant volumetric strain.
//
// Solution 1: Selective Reduced Integration (SRI)
//   - Integrate volumetric terms (eps_xx + eps_yy) at full 2x2 Gauss
//   - Integrate shear term (gamma_xy) at reduced 1x1 Gauss (center only)
//   - This removes the spurious shear constraint that causes locking
//
// Solution 2: B-Bar Method (Hughes, 1980)
//   - Decompose strain: eps = eps_vol_bar + eps_dev
//   - eps_vol_bar uses 1-point integration (constant volume change)
//   - eps_dev uses full 2x2 integration
//   - Recombine: sigma = D * eps_vol_bar + D * eps_dev

namespace locking {

// ------------------------------------------------------------------
// Q4 element stiffness with Selective Reduced Integration (SRI)
//
// The shear term gamma_xy is integrated with 1 Gauss point (center),
// while the normal strains eps_xx, eps_yy use 2x2 Gauss points.
//
// This effectively relaxes the shear constraint that causes locking
// in bending-dominated problems.
// ------------------------------------------------------------------
struct Q4SRIElement {
    // Gauss points
    static inline const double GP = 1.0 / std::sqrt(3.0);
    static inline const double GW = 1.0;

    // Shape function derivative helper (same as Q4Element)
    static std::array<double, 2> shape_deriv(int i, double xi, double eta) {
        static const double xi_pts[4]  = { -1.0,  1.0,  1.0, -1.0 };
        static const double eta_pts[4] = { -1.0, -1.0,  1.0,  1.0 };
        double dN_dxi  = 0.25 * xi_pts[i]  * (1.0 + eta_pts[i] * eta);
        double dN_deta = 0.25 * (1.0 + xi_pts[i] * xi) * eta_pts[i];
        return { dN_dxi, dN_deta };
    }

    // Compute SRI element stiffness (8x8)
    // Full 2x2 for normal strains, 1x1 for shear strain
    static std::array<std::array<double, 8>, 8> stiffness(
        const std::array<Node, 4>& elem_nodes,
        const Material& mat,
        PlaneType plane) {

        auto D = mat.d_matrix(plane);
        std::array<std::array<double, 8>, 8> K{};

        // ---------------------------------------------------------------
        // Part 1: Normal strain terms (eps_xx, eps_yy) at 2x2 Gauss
        // K_volumetric += B_vol^T * D_vol * B_vol * detJ * w * t
        // where D_vol is the 2x2 submatrix for normal strains
        // ---------------------------------------------------------------
        const double gp2[2] = { -GP, GP };
        const double gw2[2] = { GW, GW };

        for (int gi = 0; gi < 2; ++gi) {
            for (int gj = 0; gj < 2; ++gj) {
                double xi  = gp2[gi];
                double eta = gp2[gj];

                // Jacobian
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
                    throw std::runtime_error("Q4-SRI: negative Jacobian");

                double invJ11 =  J22 / detJ;
                double invJ12 = -J12 / detJ;
                double invJ21 = -J21 / detJ;
                double invJ22 =  J11 / detJ;

                // Normal strain B matrix (2 x 8): [eps_xx; eps_yy]
                // Note: gamma_xy row is excluded here
                std::array<std::array<double, 8>, 2> B_vol{};
                for (int i = 0; i < 4; ++i) {
                    auto dN = shape_deriv(i, xi, eta);
                    double dNdx = invJ11 * dN[0] + invJ12 * dN[1];
                    double dNdy = invJ21 * dN[0] + invJ22 * dN[1];

                    int col = 2 * i;
                    B_vol[0][col]     = dNdx;   // eps_xx
                    B_vol[0][col + 1] = 0.0;
                    B_vol[1][col]     = 0.0;
                    B_vol[1][col + 1] = dNdy;   // eps_yy
                }

                // D_vol (2x2): normal strain submatrix of D
                // D_vol = [[D00, D01], [D10, D11]]
                double D_vol[2][2] = {
                    { D[0][0], D[0][1] },
                    { D[1][0], D[1][1] }
                };

                double factor = detJ * gw2[gi] * gw2[gj] * mat.t;

                // K += B_vol^T * D_vol * B_vol * factor
                for (int i = 0; i < 8; ++i) {
                    for (int j = 0; j < 8; ++j) {
                        double sum = 0.0;
                        for (int r = 0; r < 2; ++r) {
                            for (int c = 0; c < 2; ++c) {
                                sum += B_vol[r][i] * D_vol[r][c] * B_vol[c][j];
                            }
                        }
                        K[i][j] += sum * factor;
                    }
                }
            }
        }

        // ---------------------------------------------------------------
        // Part 2: Shear strain term (gamma_xy) at 1x1 Gauss (center only)
        // K_shear += B_shear^T * D_shear * B_shear * detJ * w * t
        // where D_shear is D[2][2] (shear modulus)
        // ---------------------------------------------------------------
        {
            double xi = 0.0, eta = 0.0;  // center point

            // Jacobian at center
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
                throw std::runtime_error("Q4-SRI: negative Jacobian at center");

            double invJ11 =  J22 / detJ;
            double invJ12 = -J12 / detJ;
            double invJ21 = -J21 / detJ;
            double invJ22 =  J11 / detJ;

            // Shear strain B row (1 x 8): [gamma_xy]
            std::array<double, 8> B_shear{};
            for (int i = 0; i < 4; ++i) {
                auto dN = shape_deriv(i, xi, eta);
                double dNdx = invJ11 * dN[0] + invJ12 * dN[1];
                double dNdy = invJ21 * dN[0] + invJ22 * dN[1];

                int col = 2 * i;
                B_shear[col]     = dNdy;   // gamma_xy = du/dy + dv/dx
                B_shear[col + 1] = dNdx;
            }

            // D_shear = D[2][2] (shear modulus)
            double D_shear = D[2][2];

            // Weight for 1-point rule on reference square: 4.0 (area of [-1,1]^2)
            double factor = detJ * 4.0 * mat.t;

            // K += B_shear^T * D_shear * B_shear * factor
            for (int i = 0; i < 8; ++i) {
                for (int j = 0; j < 8; ++j) {
                    K[i][j] += B_shear[i] * D_shear * B_shear[j] * factor;
                }
            }
        }

        return K;
    }
};

// ------------------------------------------------------------------
// Q4 element stiffness with B-Bar method (Hughes, 1980)
//
// Decompose strain into volumetric and deviatoric parts:
//   eps = (1/3) * tr(eps) * I + (eps - (1/3)*tr(eps)*I)
//        = eps_vol + eps_dev
//
// eps_vol (volumetric) is integrated with 1-point rule
// eps_dev (deviatoric) is integrated with 2x2 rule
// D is applied to each part separately:
//   sigma = D * eps_vol + D * eps_dev
// ------------------------------------------------------------------
struct Q4BBarElement {
    static inline const double GP = 1.0 / std::sqrt(3.0);
    static inline const double GW = 1.0;

    static std::array<double, 2> shape_deriv(int i, double xi, double eta) {
        static const double xi_pts[4]  = { -1.0,  1.0,  1.0, -1.0 };
        static const double eta_pts[4] = { -1.0, -1.0,  1.0,  1.0 };
        double dN_dxi  = 0.25 * xi_pts[i]  * (1.0 + eta_pts[i] * eta);
        double dN_deta = 0.25 * (1.0 + xi_pts[i] * xi) * eta_pts[i];
        return { dN_dxi, dN_deta };
    }

    // Compute BBar element stiffness (8x8)
    static std::array<std::array<double, 8>, 8> stiffness(
        const std::array<Node, 4>& elem_nodes,
        const Material& mat,
        PlaneType plane) {

        auto D = mat.d_matrix(plane);
        std::array<std::array<double, 8>, 8> K{};

        // ---------------------------------------------------------------
        // Step 1: Compute average B-bar for volumetric strain
        // B_bar = (1/Area) * integral of B_vol over element
        // B_vol = [dN1/dx + dN1/dy, dN2/dx + dN2/dy, ...] (1 x 8)
        // This is the "mean" volumetric strain operator
        // ---------------------------------------------------------------
        std::array<double, 8> B_bar_mean{};

        // Integrate B_vol over element with 2x2 Gauss
        double total_area = 0.0;
        const double gp2[2] = { -GP, GP };
        const double gw2[2] = { GW, GW };

        for (int gi = 0; gi < 2; ++gi) {
            for (int gj = 0; gj < 2; ++gj) {
                double xi  = gp2[gi];
                double eta = gp2[gj];

                double J11 = 0.0, J12 = 0.0, J21 = 0.0, J22 = 0.0;
                for (int i = 0; i < 4; ++i) {
                    auto dN = shape_deriv(i, xi, eta);
                    J11 += dN[0] * elem_nodes[i].x;
                    J12 += dN[0] * elem_nodes[i].y;
                    J21 += dN[1] * elem_nodes[i].x;
                    J22 += dN[1] * elem_nodes[i].y;
                }

                double detJ = J11 * J22 - J12 * J21;
                double factor = detJ * gw2[gi] * gw2[gj];
                total_area += factor;

                double invJ11 =  J22 / detJ;
                double invJ12 = -J12 / detJ;
                double invJ21 = -J21 / detJ;
                double invJ22 =  J11 / detJ;

                for (int i = 0; i < 4; ++i) {
                    auto dN = shape_deriv(i, xi, eta);
                    double dNdx = invJ11 * dN[0] + invJ12 * dN[1];
                    double dNdy = invJ21 * dN[0] + invJ22 * dN[1];

                    int col = 2 * i;
                    B_bar_mean[col]     += dNdx * factor;
                    B_bar_mean[col + 1] += dNdy * factor;
                }
            }
        }

        // Normalize to get mean volumetric strain operator
        if (total_area > 1e-30) {
            for (int i = 0; i < 8; ++i) B_bar_mean[i] /= total_area;
        }

        // ---------------------------------------------------------------
        // Step 2: Assemble stiffness using B-bar for volumetric part
        // K = integral of (B^T * D * B) dA using 2x2 Gauss
        //   but replace the volumetric part with B_bar_mean
        //
        // Split: D * eps = D * (eps_vol * I_vol + eps_dev)
        // where eps_vol = B_bar_mean * u (scalar)
        //       eps_dev = (B - B_vol_mean) * u (2x1 deviatoric part)
        //
        // K = K_dev + K_vol where:
        //   K_vol = B_bar_mean^T * D_vol * B_bar_mean * Area
        //   K_dev = integral B_dev^T * D * B_dev dA
        // ---------------------------------------------------------------

        // Volumetric stiffness: K_vol = B_bar^T * D_vol * B_bar * Area
        // D_vol for plane stress: [[lambda+2mu, lambda], [lambda, lambda+2mu]]
        // Simplified: use hydrostatic part of D
        double lambda = (plane == PlaneType::STRESS) ?
            mat.E * mat.nu / (1.0 - mat.nu * mat.nu) :
            mat.E * mat.nu / ((1.0 + mat.nu) * (1.0 - 2.0 * mat.nu));
        double mu = mat.E / (2.0 * (1.0 + mat.nu));

        // Volumetric contribution: (lambda + mu) for plane stress
        // For 2D: volumetric stiffness factor
        double vol_factor = lambda + mu;  // bulk-like term

        // K_vol[i][j] = vol_factor * B_bar[i] * B_bar[j] * total_area * mat.t
        for (int i = 0; i < 8; ++i) {
            for (int j = 0; j < 8; ++j) {
                K[i][j] += vol_factor * B_bar_mean[i] * B_bar_mean[j] * total_area * mat.t;
            }
        }

        // Deviatoric + remaining volumetric: full 2x2 integration
        for (int gi = 0; gi < 2; ++gi) {
            for (int gj = 0; gj < 2; ++gj) {
                double xi  = gp2[gi];
                double eta = gp2[gj];

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
                    throw std::runtime_error("Q4-BBar: negative Jacobian");

                double invJ11 =  J22 / detJ;
                double invJ12 = -J12 / detJ;
                double invJ21 = -J21 / detJ;
                double invJ22 =  J11 / detJ;

                // Full B matrix (3 x 8)
                std::array<std::array<double, 8>, 3> B{};
                for (int i = 0; i < 4; ++i) {
                    auto dN = shape_deriv(i, xi, eta);
                    double dNdx = invJ11 * dN[0] + invJ12 * dN[1];
                    double dNdy = invJ21 * dN[0] + invJ22 * dN[1];

                    int col = 2 * i;
                    B[0][col]     = dNdx;
                    B[0][col + 1] = 0.0;
                    B[1][col]     = 0.0;
                    B[1][col + 1] = dNdy;
                    B[2][col]     = dNdy;
                    B[2][col + 1] = dNdx;
                }

                // Compute B_dev: deviatoric part of B
                // eps_vol = trace(eps) = B[0] + B[1] (for 2D)
                // B_vol_mean_contrib = B_bar_mean (already computed)
                // B_dev = B - (1/2) * trace(B) * I_vol
                // For 2D plane: trace(B) = B[0][j] + B[1][j] for each DOF j
                std::array<std::array<double, 8>, 3> B_dev{};
                for (int j = 0; j < 8; ++j) {
                    double trace_B = B[0][j] + B[1][j];
                    B_dev[0][j] = B[0][j] - 0.5 * trace_B - 0.5 * B_bar_mean[j];
                    B_dev[1][j] = B[1][j] - 0.5 * trace_B - 0.5 * B_bar_mean[j];
                    B_dev[2][j] = B[2][j];  // shear unchanged
                }

                double factor = detJ * gw2[gi] * gw2[gj] * mat.t;

                // K += B_dev^T * D * B_dev * factor
                // Compute D*B_dev first (3x8)
                std::array<std::array<double, 8>, 3> DB_dev{};
                for (int r = 0; r < 3; ++r) {
                    for (int c = 0; c < 8; ++c) {
                        double sum = 0.0;
                        for (int k = 0; k < 3; ++k) {
                            sum += D[r][k] * B_dev[k][c];
                        }
                        DB_dev[r][c] = sum;
                    }
                }

                for (int i = 0; i < 8; ++i) {
                    for (int j = 0; j < 8; ++j) {
                        double sum = 0.0;
                        for (int k = 0; k < 3; ++k) {
                            sum += B_dev[k][i] * DB_dev[k][j];
                        }
                        K[i][j] += sum * factor;
                    }
                }
            }
        }

        return K;
    }
};

}  // namespace locking
