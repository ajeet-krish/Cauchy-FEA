#pragma once
#include "../fea_types.hpp"
#include "../elements.hpp"
#include "../postprocess.hpp"
#include "../adaptivity.hpp"
#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <limits>

// ==========================================================================
// STRESS STREAMLINE TRACER -- Principal stress direction integration
// ==========================================================================

namespace streamline {

// ------------------------------------------------------------------
// Configuration
// ------------------------------------------------------------------
struct StreamlineConfig {
    int num_seeds = 20;          // seeds per axis (total = num_seeds^2)
    double step_size = 0.005;    // RK4 step in world coords
    int max_steps = 500;         // max integration steps per direction
    double min_stress = 1.0e3;   // skip seeds where |sigma_1| < min_stress
};

// ------------------------------------------------------------------
// A single streamline path
// ------------------------------------------------------------------
struct StreamlinePath {
    std::vector<double> x;       // x coords along path
    std::vector<double> y;       // y coords along path
    std::vector<double> sigma_1; // principal stress along path
};

// ------------------------------------------------------------------
// Result of tracing all streamlines
// ------------------------------------------------------------------
struct StreamlineResult {
    std::vector<StreamlinePath> paths;
    double max_sigma_1 = 0.0;
    double min_sigma_1 = 0.0;
};

// ------------------------------------------------------------------
// Q4 natural coordinate constants
// ------------------------------------------------------------------
static const double Q4_XI[4]  = { -1.0,  1.0,  1.0, -1.0 };
static const double Q4_ETA[4] = { -1.0, -1.0,  1.0,  1.0 };

// ------------------------------------------------------------------
// Q4 shape function N_i at (xi, eta)
// ------------------------------------------------------------------
inline double q4_shape(int i, double xi, double eta) {
    return 0.25 * (1.0 + Q4_XI[i] * xi) * (1.0 + Q4_ETA[i] * eta);
}

// ------------------------------------------------------------------
// Q4 shape function derivatives [dN/dxi, dN/deta]
// ------------------------------------------------------------------
inline std::array<double, 2> q4_shape_deriv(int i, double xi, double eta) {
    double dN_dxi  = 0.25 * Q4_XI[i]  * (1.0 + Q4_ETA[i] * eta);
    double dN_deta = 0.25 * (1.0 + Q4_XI[i] * xi) * Q4_ETA[i];
    return { dN_dxi, dN_deta };
}

// ------------------------------------------------------------------
// Compute Jacobian at (xi, eta) for a Q4 element
// Returns [J11, J12, J21, J22]
// ------------------------------------------------------------------
inline std::array<double, 4> q4_jacobian(
    const std::array<Node, 4>& nodes, double xi, double eta) {

    double J11 = 0.0, J12 = 0.0, J21 = 0.0, J22 = 0.0;
    for (int i = 0; i < 4; ++i) {
        auto dN = q4_shape_deriv(i, xi, eta);
        J11 += dN[0] * nodes[i].x;
        J12 += dN[0] * nodes[i].y;
        J21 += dN[1] * nodes[i].x;
        J22 += dN[1] * nodes[i].y;
    }
    return { J11, J12, J21, J22 };
}

// ------------------------------------------------------------------
// Forward mapping: (xi, eta) -> (x, y)
// ------------------------------------------------------------------
inline std::array<double, 2> q4_forward(
    const std::array<Node, 4>& nodes, double xi, double eta) {

    double x = 0.0, y = 0.0;
    for (int i = 0; i < 4; ++i) {
        double N = q4_shape(i, xi, eta);
        x += N * nodes[i].x;
        y += N * nodes[i].y;
    }
    return { x, y };
}

// ------------------------------------------------------------------
// Newton-Raphson inverse mapping: (x, y) -> (xi, eta)
// ------------------------------------------------------------------
inline std::array<double, 2> physical_to_natural(
    const std::array<Node, 4>& nodes,
    double px, double py,
    double xi_init = 0.0, double eta_init = 0.0) {

    double xi = xi_init, eta = eta_init;

    for (int iter = 0; iter < 20; ++iter) {
        auto xy = q4_forward(nodes, xi, eta);
        double dx = xy[0] - px;
        double dy = xy[1] - py;

        if (dx * dx + dy * dy < 1e-24) break;

        auto J = q4_jacobian(nodes, xi, eta);
        double detJ = J[0] * J[3] - J[1] * J[2];
        if (std::abs(detJ) < 1e-30) break;

        double invJ11 =  J[3] / detJ;
        double invJ12 = -J[1] / detJ;
        double invJ21 = -J[2] / detJ;
        double invJ22 =  J[0] / detJ;

        double dxi  = invJ11 * dx + invJ12 * dy;
        double deta = invJ21 * dx + invJ22 * dy;

        xi  -= dxi;
        eta -= deta;
    }

    return { xi, eta };
}

// ------------------------------------------------------------------
// Find the Q4 element containing point (px, py)
// Returns element index or -1 if not found
// ------------------------------------------------------------------
inline int find_containing_element(
    const Mesh& m, double px, double py) {

    for (int e = 0; e < m.num_quads(); ++e) {
        const auto& elem = m.quad_elements[e];
        std::array<Node, 4> nodes;
        for (int i = 0; i < 4; ++i) nodes[i] = m.nodes[elem[i]];

        // Check bounding box first (fast reject)
        double xmin = std::min({nodes[0].x, nodes[1].x, nodes[2].x, nodes[3].x});
        double xmax = std::max({nodes[0].x, nodes[1].x, nodes[2].x, nodes[3].x});
        double ymin = std::min({nodes[0].y, nodes[1].y, nodes[2].y, nodes[3].y});
        double ymax = std::max({nodes[0].y, nodes[1].y, nodes[2].y, nodes[3].y});

        if (px < xmin || px > xmax || py < ymin || py > ymax) continue;

        // Use inverse mapping: if natural coords are inside [-1,1]^2, point is inside
        auto nat = physical_to_natural(nodes, px, py);
        if (nat[0] >= -1.001 && nat[0] <= 1.001 &&
            nat[1] >= -1.001 && nat[1] <= 1.001) {
            return e;
        }
    }

    return -1;
}

// ------------------------------------------------------------------
// Find containing T3 element (simple signed area test)
// ------------------------------------------------------------------
inline int find_containing_tri(
    const Mesh& m, double px, double py) {

    for (int e = 0; e < m.num_tris(); ++e) {
        const auto& elem = m.tri_elements[e];
        const auto& n0 = m.nodes[elem[0]];
        const auto& n1 = m.nodes[elem[1]];
        const auto& n2 = m.nodes[elem[2]];

        // Signed area method
        double denom = (n1.y - n2.y) * (n0.x - n2.x) + (n2.x - n1.x) * (n0.y - n2.y);
        if (std::abs(denom) < 1e-30) continue;

        double a = ((n1.y - n2.y) * (px - n2.x) + (n2.x - n1.x) * (py - n2.y)) / denom;
        double b = ((n2.y - n0.y) * (px - n2.x) + (n0.x - n2.x) * (py - n2.y)) / denom;
        double c = 1.0 - a - b;

        if (a >= -1e-6 && b >= -1e-6 && c >= -1e-6) {
            return e;
        }
    }

    return -1;
}

// ------------------------------------------------------------------
// Interpolate SPR nodal stress at a point using Q4 shape functions
// ------------------------------------------------------------------
struct StressAtPoint {
    double sxx;
    double syy;
    double sxy;
    double sigma_1;
};

inline StressAtPoint interpolate_stress_at_point(
    const Mesh& m,
    const adaptivity::SPRResult& spr,
    int elem_idx,
    double px, double py) {

    const auto& elem = m.quad_elements[elem_idx];
    std::array<Node, 4> nodes;
    for (int i = 0; i < 4; ++i) nodes[i] = m.nodes[elem[i]];

    auto nat = physical_to_natural(nodes, px, py);
    double xi = nat[0], eta = nat[1];

    // Clamp to reference element bounds
    xi  = std::max(-1.0, std::min(1.0, xi));
    eta = std::max(-1.0, std::min(1.0, eta));

    double sxx = 0.0, syy = 0.0, sxy = 0.0;
    for (int i = 0; i < 4; ++i) {
        double N = q4_shape(i, xi, eta);
        int n = elem[i];
        sxx += N * spr.recovered_sxx[n];
        syy += N * spr.recovered_syy[n];
        sxy += N * spr.recovered_sxy[n];
    }

    // Principal stress from Mohr's circle
    double avg = (sxx + syy) / 2.0;
    double R = std::sqrt(((sxx - syy) / 2.0) * ((sxx - syy) / 2.0)
                         + sxy * sxy);
    double sigma_1 = avg + R;

    return { sxx, syy, sxy, sigma_1 };
}

// ------------------------------------------------------------------
// Interpolate SPR stress at a point in a T3 element
// ------------------------------------------------------------------
inline StressAtPoint interpolate_stress_tri(
    const Mesh& m,
    const adaptivity::SPRResult& spr,
    int elem_idx,
    double px, double py) {

    const auto& elem = m.tri_elements[elem_idx];
    const auto& n0 = m.nodes[elem[0]];
    const auto& n1 = m.nodes[elem[1]];
    const auto& n2 = m.nodes[elem[2]];

    double denom = (n1.y - n2.y) * (n0.x - n2.x) + (n2.x - n1.x) * (n0.y - n2.y);
    if (std::abs(denom) < 1e-30) return { 0.0, 0.0, 0.0, 0.0 };

    double L0 = ((n1.y - n2.y) * (px - n2.x) + (n2.x - n1.x) * (py - n2.y)) / denom;
    double L1 = ((n2.y - n0.y) * (px - n2.x) + (n0.x - n2.x) * (py - n2.y)) / denom;
    double L2 = 1.0 - L0 - L1;

    double sxx = L0 * spr.recovered_sxx[elem[0]]
               + L1 * spr.recovered_sxx[elem[1]]
               + L2 * spr.recovered_sxx[elem[2]];
    double syy = L0 * spr.recovered_syy[elem[0]]
               + L1 * spr.recovered_syy[elem[1]]
               + L2 * spr.recovered_syy[elem[2]];
    double sxy = L0 * spr.recovered_sxy[elem[0]]
               + L1 * spr.recovered_sxy[elem[1]]
               + L2 * spr.recovered_sxy[elem[2]];

    double avg = (sxx + syy) / 2.0;
    double R = std::sqrt(((sxx - syy) / 2.0) * ((sxx - syy) / 2.0)
                         + sxy * sxy);
    double sigma_1 = avg + R;

    return { sxx, syy, sxy, sigma_1 };
}

// ------------------------------------------------------------------
// Unified stress interpolation at a point (tries Q4 then T3)
// ------------------------------------------------------------------
inline StressAtPoint interpolate_stress(
    const Mesh& m,
    const adaptivity::SPRResult& spr,
    double px, double py) {

    int q4_idx = find_containing_element(m, px, py);
    if (q4_idx >= 0) {
        return interpolate_stress_at_point(m, spr, q4_idx, px, py);
    }

    int t3_idx = find_containing_tri(m, px, py);
    if (t3_idx >= 0) {
        return interpolate_stress_tri(m, spr, t3_idx, px, py);
    }

    return { 0.0, 0.0, 0.0, 0.0 };
}

// ------------------------------------------------------------------
// Principal stress direction angle at a point
// Returns the angle of sigma_1 from the x-axis (radians)
// ------------------------------------------------------------------
inline double principal_stress_direction(double sxx, double syy, double sxy) {
    return 0.5 * std::atan2(2.0 * sxy, sxx - syy);
}

// ------------------------------------------------------------------
// Principal stress magnitude (sigma_1) at a point
// ------------------------------------------------------------------
inline double principal_stress_magnitude(double sxx, double syy, double sxy) {
    double avg = (sxx + syy) / 2.0;
    double R = std::sqrt(((sxx - syy) / 2.0) * ((sxx - syy) / 2.0)
                         + sxy * sxy);
    return avg + R;
}

// ------------------------------------------------------------------
// Check if a point is inside the mesh bounding box
// ------------------------------------------------------------------
inline bool in_bounds(const Mesh& m, double x, double y,
                      double margin = 0.0) {
    double xmin = 1e30, xmax = -1e30;
    double ymin = 1e30, ymax = -1e30;
    for (const auto& n : m.nodes) {
        xmin = std::min(xmin, n.x);
        xmax = std::max(xmax, n.x);
        ymin = std::min(ymin, n.y);
        ymax = std::max(ymax, n.y);
    }
    return x >= xmin - margin && x <= xmax + margin &&
           y >= ymin - margin && y <= ymax + margin;
}

// ------------------------------------------------------------------
// Get mesh bounding box
// ------------------------------------------------------------------
inline void mesh_bbox(const Mesh& m,
                      double& xmin, double& xmax,
                      double& ymin, double& ymax) {
    xmin = 1e30; xmax = -1e30;
    ymin = 1e30; ymax = -1e30;
    for (const auto& n : m.nodes) {
        xmin = std::min(xmin, n.x);
        xmax = std::max(xmax, n.x);
        ymin = std::min(ymin, n.y);
        ymax = std::max(ymax, n.y);
    }
}

// ------------------------------------------------------------------
// Trace a single streamline from a seed point using RK4
// direction: +1 for forward, -1 for backward
// ------------------------------------------------------------------
inline StreamlinePath trace_one_direction(
    const Mesh& m,
    const adaptivity::SPRResult& spr,
    double seed_x, double seed_y,
    int direction,
    const StreamlineConfig& cfg) {

    StreamlinePath path;
    path.x.push_back(seed_x);
    path.y.push_back(seed_y);

    // Get initial stress at seed
    auto init_stress = interpolate_stress(m, spr, seed_x, seed_y);
    path.sigma_1.push_back(init_stress.sigma_1);

    double cur_x = seed_x;
    double cur_y = seed_y;

    for (int step = 0; step < cfg.max_steps; ++step) {
        // RK4 integration along principal stress direction
        // k1
        auto s1 = interpolate_stress(m, spr, cur_x, cur_y);
        double theta1 = principal_stress_direction(s1.sxx, s1.syy, s1.sxy);
        double k1x = direction * std::cos(theta1);
        double k1y = direction * std::sin(theta1);

        // k2
        double x2 = cur_x + 0.5 * cfg.step_size * k1x;
        double y2 = cur_y + 0.5 * cfg.step_size * k1y;
        if (!in_bounds(m, x2, y2, 0.01)) break;
        auto s2 = interpolate_stress(m, spr, x2, y2);
        double theta2 = principal_stress_direction(s2.sxx, s2.syy, s2.sxy);
        double k2x = direction * std::cos(theta2);
        double k2y = direction * std::sin(theta2);

        // k3
        double x3 = cur_x + 0.5 * cfg.step_size * k2x;
        double y3 = cur_y + 0.5 * cfg.step_size * k2y;
        if (!in_bounds(m, x3, y3, 0.01)) break;
        auto s3 = interpolate_stress(m, spr, x3, y3);
        double theta3 = principal_stress_direction(s3.sxx, s3.syy, s3.sxy);
        double k3x = direction * std::cos(theta3);
        double k3y = direction * std::sin(theta3);

        // k4
        double x4 = cur_x + cfg.step_size * k3x;
        double y4 = cur_y + cfg.step_size * k3y;
        if (!in_bounds(m, x4, y4, 0.01)) break;
        auto s4 = interpolate_stress(m, spr, x4, y4);
        double theta4 = principal_stress_direction(s4.sxx, s4.syy, s4.sxy);
        double k4x = direction * std::cos(theta4);
        double k4y = direction * std::sin(theta4);

        // Update
        cur_x += cfg.step_size * (k1x + 2.0 * k2x + 2.0 * k3x + k4x) / 6.0;
        cur_y += cfg.step_size * (k1y + 2.0 * k2y + 2.0 * k3y + k4y) / 6.0;

        // Check bounds
        if (!in_bounds(m, cur_x, cur_y, 0.001)) break;

        // Check if we left the mesh (no containing element)
        if (find_containing_element(m, cur_x, cur_y) < 0 &&
            find_containing_tri(m, cur_x, cur_y) < 0) {
            break;
        }

        // Record
        auto s_cur = interpolate_stress(m, spr, cur_x, cur_y);
        path.x.push_back(cur_x);
        path.y.push_back(cur_y);
        path.sigma_1.push_back(s_cur.sigma_1);
    }

    return path;
}

// ------------------------------------------------------------------
// Trace a streamline from a seed point (forward + backward)
// ------------------------------------------------------------------
inline StreamlinePath trace_streamline(
    const Mesh& m,
    const adaptivity::SPRResult& spr,
    double seed_x, double seed_y,
    const StreamlineConfig& cfg) {

    // Trace backward (reverse direction)
    auto backward = trace_one_direction(m, spr, seed_x, seed_y, -1, cfg);

    // Trace forward
    auto forward = trace_one_direction(m, spr, seed_x, seed_y, +1, cfg);

    // Combine: backward (reversed) + seed + forward
    StreamlinePath combined;
    // Add backward points in reverse order (excluding seed duplicate)
    for (int i = static_cast<int>(backward.x.size()) - 1; i >= 1; --i) {
        combined.x.push_back(backward.x[i]);
        combined.y.push_back(backward.y[i]);
        combined.sigma_1.push_back(backward.sigma_1[i]);
    }

    // Add seed point (from backward[0] or forward[0], they are the same)
    combined.x.push_back(seed_x);
    combined.y.push_back(seed_y);
    auto seed_stress = interpolate_stress(m, spr, seed_x, seed_y);
    combined.sigma_1.push_back(seed_stress.sigma_1);

    // Add forward points (excluding seed duplicate at index 0)
    for (size_t i = 1; i < forward.x.size(); ++i) {
        combined.x.push_back(forward.x[i]);
        combined.y.push_back(forward.y[i]);
        combined.sigma_1.push_back(forward.sigma_1[i]);
    }

    return combined;
}

// ------------------------------------------------------------------
// Generate seed points on a uniform grid across the mesh bounding box
// ------------------------------------------------------------------
inline std::vector<std::array<double, 2>> generate_seeds(
    const Mesh& m, const StreamlineConfig& cfg) {

    std::vector<std::array<double, 2>> seeds;

    double xmin, xmax, ymin, ymax;
    mesh_bbox(m, xmin, xmax, ymin, ymax);

    double dx = (xmax - xmin) / (cfg.num_seeds + 1);
    double dy = (ymax - ymin) / (cfg.num_seeds + 1);

    for (int i = 1; i <= cfg.num_seeds; ++i) {
        for (int j = 1; j <= cfg.num_seeds; ++j) {
            double sx = xmin + i * dx;
            double sy = ymin + j * dy;
            seeds.push_back({ sx, sy });
        }
    }

    return seeds;
}

// ------------------------------------------------------------------
// Trace streamlines from all seed points
// ------------------------------------------------------------------
inline StreamlineResult trace_all(
    const Mesh& m,
    const adaptivity::SPRResult& spr,
    const StreamlineConfig& cfg) {

    StreamlineResult result;

    // Skip if no Q4 elements (streamlines need quad mesh)
    if (m.num_quads() == 0 && m.num_tris() == 0) return result;

    auto seeds = generate_seeds(m, cfg);

    for (const auto& seed : seeds) {
        // Check stress magnitude at seed
        auto seed_stress = interpolate_stress(m, spr, seed[0], seed[1]);
        if (std::abs(seed_stress.sigma_1) < cfg.min_stress) continue;

        // Check that seed is inside an element
        if (find_containing_element(m, seed[0], seed[1]) < 0 &&
            find_containing_tri(m, seed[0], seed[1]) < 0) {
            continue;
        }

        auto path = trace_streamline(m, spr, seed[0], seed[1], cfg);

        // Only keep paths with at least 3 points
        if (path.x.size() >= 3) {
            result.paths.push_back(std::move(path));
        }
    }

    // Compute global min/max sigma_1
    result.max_sigma_1 = -1e30;
    result.min_sigma_1 = 1e30;
    for (const auto& p : result.paths) {
        for (double s : p.sigma_1) {
            result.max_sigma_1 = std::max(result.max_sigma_1, s);
            result.min_sigma_1 = std::min(result.min_sigma_1, s);
        }
    }

    // Handle empty or degenerate case
    if (result.paths.empty()) {
        result.max_sigma_1 = 0.0;
        result.min_sigma_1 = 0.0;
    }

    std::cout << "Streamline tracer: " << result.paths.size()
              << " paths, sigma_1 range: ["
              << result.min_sigma_1 << ", " << result.max_sigma_1 << "]"
              << std::endl;

    return result;
}

}  // namespace streamline
