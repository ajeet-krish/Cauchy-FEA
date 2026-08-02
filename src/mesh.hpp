#pragma once
#include "fea_types.hpp"
#include <vector>
#include <array>
#include <cmath>
#include <fstream>
#include <stdexcept>

// ==========================================================================
// MESH GENERATION -- Structured quad mesher + JSON input + boundary snapping
// ==========================================================================

namespace mesh {

// ------------------------------------------------------------------
// Curve definitions for boundary snapping
// ------------------------------------------------------------------
struct CurveCircle {
    double cx, cy, radius;
};

struct CurveEllipse {
    double cx, cy, a, b;  // a = semi-major in x, b = semi-minor in y
};

struct CurveLine {
    double x1, y1, x2, y2;
};

struct CurvePolygon {
    std::vector<std::pair<double, double>> vertices;  // CCW ordered
};

// Find nearest point on a line segment (x1,y1)-(x2,y2) to point (px,py)
inline std::pair<double, double> nearest_point_on_segment(
    double px, double py,
    double x1, double y1,
    double x2, double y2) {

    double dx = x2 - x1;
    double dy = y2 - y1;
    double len_sq = dx * dx + dy * dy;

    if (len_sq < 1e-20) return {x1, y1};

    double t = ((px - x1) * dx + (py - y1) * dy) / len_sq;
    t = std::max(0.0, std::min(1.0, t));

    return {x1 + t * dx, y1 + t * dy};
}

// Find nearest point on a circle to point (px,py)
inline std::pair<double, double> nearest_point_on_circle(
    double px, double py,
    double cx, double cy, double radius) {

    double dx = px - cx;
    double dy = py - cy;
    double dist = std::sqrt(dx * dx + dy * dy);

    if (dist < 1e-20) return {cx + radius, cy};

    return {cx + radius * dx / dist, cy + radius * dy / dist};
}

// Find nearest point on an ellipse to point (px,py) (iterative)
inline std::pair<double, double> nearest_point_on_ellipse(
    double px, double py,
    double cx, double cy, double a, double b) {

    // Simple iterative approach (Newton's method)
    double x = px - cx;
    double y = py - cy;

    // Initial guess: project onto ellipse
    double dist = std::sqrt(x * x + y * y);
    if (dist < 1e-20) return {cx + a, cy};

    for (int iter = 0; iter < 10; ++iter) {
        double t = std::atan2(a * y, b * x);
        double ex = a * std::cos(t);
        double ey = b * std::sin(t);
        double dtx = -a * std::sin(t);
        double dty = b * std::cos(t);
        double f = (ex - x) * dtx + (ey - y) * dty;
        double df = dtx * dtx + (ex - x) * (-a * std::cos(t)) +
                    dty * dty + (ey - y) * (-b * std::sin(t));
        if (std::abs(df) > 1e-20) t -= f / df;
        x = a * std::cos(t);
        y = b * std::sin(t);
    }

    return {cx + x, cy + y};
}

// ------------------------------------------------------------------
// Snap boundary nodes to a circle
// m: mesh to modify
// cx, cy, radius: circle definition
// snap_distance: maximum distance to snap (default: element size)
// ------------------------------------------------------------------
inline void snap_to_circle(Mesh& m, double cx, double cy, double radius,
                           double snap_distance = -1.0) {

    // Auto-compute snap distance if not provided
    if (snap_distance < 0.0 && m.num_quads() > 0) {
        // Use average element size
        double avg_size = 0.0;
        for (const auto& elem : m.quad_elements) {
            double dx = m.nodes[elem[1]].x - m.nodes[elem[0]].x;
            double dy = m.nodes[elem[1]].y - m.nodes[elem[0]].y;
            avg_size += std::sqrt(dx * dx + dy * dy);
        }
        snap_distance = avg_size / m.num_quads() * 0.5;
    }

    for (auto& node : m.nodes) {
        double dx = node.x - cx;
        double dy = node.y - cy;
        double dist = std::sqrt(dx * dx + dy * dy);

        // Check if node is close to the circle boundary
        if (std::abs(dist - radius) < snap_distance) {
            auto [sx, sy] = nearest_point_on_circle(node.x, node.y, cx, cy, radius);
            node.x = sx;
            node.y = sy;
        }
    }
}

// ------------------------------------------------------------------
// Snap boundary nodes to an ellipse
// ------------------------------------------------------------------
inline void snap_to_ellipse(Mesh& m, double cx, double cy, double a, double b,
                            double snap_distance = -1.0) {

    if (snap_distance < 0.0 && m.num_quads() > 0) {
        double avg_size = 0.0;
        for (const auto& elem : m.quad_elements) {
            double dx = m.nodes[elem[1]].x - m.nodes[elem[0]].x;
            double dy = m.nodes[elem[1]].y - m.nodes[elem[0]].y;
            avg_size += std::sqrt(dx * dx + dy * dy);
        }
        snap_distance = avg_size / m.num_quads() * 0.5;
    }

    for (auto& node : m.nodes) {
        // Check if node is close to the ellipse
        double dx = (node.x - cx) / a;
        double dy = (node.y - cy) / b;
        double dist_ellipse = std::sqrt(dx * dx + dy * dy);

        if (std::abs(dist_ellipse - 1.0) < snap_distance / std::min(a, b)) {
            auto [sx, sy] = nearest_point_on_ellipse(node.x, node.y, cx, cy, a, b);
            node.x = sx;
            node.y = sy;
        }
    }
}

// ------------------------------------------------------------------
// Snap boundary nodes to a polygon (series of line segments)
// ------------------------------------------------------------------
inline void snap_to_polygon(Mesh& m,
                            const std::vector<std::pair<double, double>>& vertices,
                            double snap_distance = -1.0) {

    if (snap_distance < 0.0 && m.num_quads() > 0) {
        double avg_size = 0.0;
        for (const auto& elem : m.quad_elements) {
            double dx = m.nodes[elem[1]].x - m.nodes[elem[0]].x;
            double dy = m.nodes[elem[1]].y - m.nodes[elem[0]].y;
            avg_size += std::sqrt(dx * dx + dy * dy);
        }
        snap_distance = avg_size / m.num_quads() * 0.5;
    }

    int n_vertices = static_cast<int>(vertices.size());
    for (auto& node : m.nodes) {
        double min_dist = 1e20;
        double best_x = node.x, best_y = node.y;

        for (int i = 0; i < n_vertices; ++i) {
            int j = (i + 1) % n_vertices;
            auto [sx, sy] = nearest_point_on_segment(
                node.x, node.y,
                vertices[i].first, vertices[i].second,
                vertices[j].first, vertices[j].second);

            double dx = node.x - sx;
            double dy = node.y - sy;
            double dist = std::sqrt(dx * dx + dy * dy);

            if (dist < min_dist) {
                min_dist = dist;
                best_x = sx;
                best_y = sy;
            }
        }

        if (min_dist < snap_distance) {
            node.x = best_x;
            node.y = best_y;
        }
    }
}

// ------------------------------------------------------------------
// Element quality metrics
// ------------------------------------------------------------------
struct ElementQuality {
    double jacobian_ratio = 1.0;   // min/max Jacobian ratio (1.0 = perfect)
    double aspect_ratio = 1.0;     // max/min edge length ratio (1.0 = perfect)
    double skewness = 0.0;         // deviation from ideal (0.0 = perfect)
    double area = 0.0;             // element area
};

// Compute edge length between two nodes
inline double edge_length(const Node& a, const Node& b) {
    double dx = b.x - a.x;
    double dy = b.y - a.y;
    return std::sqrt(dx * dx + dy * dy);
}

// Compute Jacobian determinant at a point for Q4
inline double q4_jacobian_det(const std::array<Node, 4>& nodes,
                               double xi, double eta) {
    static const double xi_pts[4]  = { -1.0,  1.0,  1.0, -1.0 };
    static const double eta_pts[4] = { -1.0, -1.0,  1.0,  1.0 };

    double J11 = 0.0, J12 = 0.0, J21 = 0.0, J22 = 0.0;
    for (int i = 0; i < 4; ++i) {
        double dN_dxi  = 0.25 * xi_pts[i]  * (1.0 + eta_pts[i] * eta);
        double dN_deta = 0.25 * (1.0 + xi_pts[i] * xi) * eta_pts[i];
        J11 += dN_dxi * nodes[i].x;
        J12 += dN_dxi * nodes[i].y;
        J21 += dN_deta * nodes[i].x;
        J22 += dN_deta * nodes[i].y;
    }
    return J11 * J22 - J12 * J21;
}

// Compute quality metrics for a Q4 element
inline ElementQuality compute_q4_quality(const std::array<Node, 4>& nodes) {
    ElementQuality q;

    // Compute Jacobian at 4 corner points and center
    double j_min = 1e20, j_max = -1e20;
    double xi_pts[5] = {-1.0, 1.0, 1.0, -1.0, 0.0};
    double eta_pts[5] = {-1.0, -1.0, 1.0, 1.0, 0.0};

    for (int i = 0; i < 5; ++i) {
        double detJ = q4_jacobian_det(nodes, xi_pts[i], eta_pts[i]);
        j_min = std::min(j_min, detJ);
        j_max = std::max(j_max, detJ);
    }

    q.jacobian_ratio = (j_max > 1e-20) ? j_min / j_max : 0.0;

    // Compute edge lengths
    double e0 = edge_length(nodes[0], nodes[1]);  // bottom
    double e1 = edge_length(nodes[1], nodes[2]);  // right
    double e2 = edge_length(nodes[2], nodes[3]);  // top
    double e3 = edge_length(nodes[3], nodes[0]);  // left

    double e_min = std::min({e0, e1, e2, e3});
    double e_max = std::max({e0, e1, e2, e3});

    q.aspect_ratio = (e_min > 1e-20) ? e_max / e_min : 1e20;

    // Compute area (shoelace formula)
    q.area = 0.5 * std::abs(
        (nodes[1].x - nodes[0].x) * (nodes[2].y - nodes[0].y) -
        (nodes[2].x - nodes[0].x) * (nodes[1].y - nodes[0].y)) +
        0.5 * std::abs(
        (nodes[2].x - nodes[0].x) * (nodes[3].y - nodes[0].y) -
        (nodes[3].x - nodes[0].x) * (nodes[2].y - nodes[0].y));

    // Skewness: deviation from ideal square (0 = perfect, 1 = degenerate)
    // Based on angle deviation from 90 degrees
    double max_angle = 0.0, min_angle = 180.0;
    for (int i = 0; i < 4; ++i) {
        int prev = (i + 3) % 4;
        int next = (i + 1) % 4;
        double v1x = nodes[prev].x - nodes[i].x;
        double v1y = nodes[prev].y - nodes[i].y;
        double v2x = nodes[next].x - nodes[i].x;
        double v2y = nodes[next].y - nodes[i].y;
        double dot = v1x * v2x + v1y * v2y;
        double len1 = std::sqrt(v1x * v1x + v1y * v1y);
        double len2 = std::sqrt(v2x * v2x + v2y * v2y);
        double cos_angle = (len1 * len2 > 1e-20) ? dot / (len1 * len2) : 0.0;
        cos_angle = std::max(-1.0, std::min(1.0, cos_angle));
        double angle = std::acos(cos_angle) * 180.0 / M_PI;
        max_angle = std::max(max_angle, angle);
        min_angle = std::min(min_angle, angle);
    }
    // Skewness based on angle deviation from 90 degrees
    q.skewness = std::max(std::abs(max_angle - 90.0), std::abs(90.0 - min_angle)) / 90.0;

    return q;
}

// Compute quality metrics for a T3 element
inline ElementQuality compute_t3_quality(const std::array<Node, 3>& nodes) {
    ElementQuality q;

    // Compute area
    q.area = 0.5 * std::abs(
        (nodes[1].x - nodes[0].x) * (nodes[2].y - nodes[0].y) -
        (nodes[2].x - nodes[0].x) * (nodes[1].y - nodes[0].y));

    // Jacobian ratio for T3 is always 1.0 (constant Jacobian)
    q.jacobian_ratio = 1.0;

    // Compute edge lengths
    double e0 = edge_length(nodes[0], nodes[1]);
    double e1 = edge_length(nodes[1], nodes[2]);
    double e2 = edge_length(nodes[2], nodes[0]);

    double e_min = std::min({e0, e1, e2});
    double e_max = std::max({e0, e1, e2});

    q.aspect_ratio = (e_min > 1e-20) ? e_max / e_min : 1e20;

    // Skewness: deviation from equilateral triangle (0 = perfect)
    double max_angle = 0.0, min_angle = 180.0;
    for (int i = 0; i < 3; ++i) {
        int prev = (i + 2) % 3;
        int next = (i + 1) % 3;
        double v1x = nodes[prev].x - nodes[i].x;
        double v1y = nodes[prev].y - nodes[i].y;
        double v2x = nodes[next].x - nodes[i].x;
        double v2y = nodes[next].y - nodes[i].y;
        double dot = v1x * v2x + v1y * v2y;
        double len1 = std::sqrt(v1x * v1x + v1y * v1y);
        double len2 = std::sqrt(v2x * v2x + v2y * v2y);
        double cos_angle = (len1 * len2 > 1e-20) ? dot / (len1 * len2) : 0.0;
        cos_angle = std::max(-1.0, std::min(1.0, cos_angle));
        double angle = std::acos(cos_angle) * 180.0 / M_PI;
        max_angle = std::max(max_angle, angle);
        min_angle = std::min(min_angle, angle);
    }
    // Skewness based on angle deviation from 60 degrees (equilateral)
    q.skewness = std::max(std::abs(max_angle - 60.0), std::abs(60.0 - min_angle)) / 60.0;

    return q;
}

// Compute quality metrics for all elements in the mesh
struct MeshQuality {
    std::vector<ElementQuality> quad_quality;
    std::vector<ElementQuality> tri_quality;
    std::vector<ElementQuality> hex_quality;
    std::vector<ElementQuality> tet_quality;
    double min_jacobian_ratio = 1.0;
    double max_aspect_ratio = 1.0;
    double max_skewness = 0.0;
};

// ------------------------------------------------------------------
// Compute quality metrics for an H8 element
// ------------------------------------------------------------------
inline ElementQuality compute_h8_quality(const std::array<Node, 8>& nodes) {
    ElementQuality q;

    static const double GP = 1.0 / std::sqrt(3.0);
    static const double GP3[2] = {-GP, GP};
    static const double XI[8]   = {-1, 1, 1,-1,-1, 1, 1,-1};
    static const double ETA[8]  = {-1,-1, 1, 1,-1,-1, 1, 1};
    static const double ZETA[8] = {-1,-1,-1,-1, 1, 1, 1, 1};

    double vol = 0.0;
    double j_min = 1e20, j_max = -1e20;

    for (int gi = 0; gi < 2; ++gi) {
        for (int gj = 0; gj < 2; ++gj) {
            for (int gk = 0; gk < 2; ++gk) {
                double xi = GP3[gi], eta = GP3[gj], zeta = GP3[gk];
                double J[3][3] = {};
                for (int n = 0; n < 8; ++n) {
                    double dN_dxi   = 0.125 * XI[n] * (1.0 + ETA[n] * eta) * (1.0 + ZETA[n] * zeta);
                    double dN_deta  = 0.125 * (1.0 + XI[n] * xi) * ETA[n] * (1.0 + ZETA[n] * zeta);
                    double dN_dzeta = 0.125 * (1.0 + XI[n] * xi) * (1.0 + ETA[n] * eta) * ZETA[n];
                    J[0][0] += dN_dxi * nodes[n].x;   J[0][1] += dN_dxi * nodes[n].y;   J[0][2] += dN_dxi * nodes[n].z;
                    J[1][0] += dN_deta * nodes[n].x;  J[1][1] += dN_deta * nodes[n].y;  J[1][2] += dN_deta * nodes[n].z;
                    J[2][0] += dN_dzeta * nodes[n].x; J[2][1] += dN_dzeta * nodes[n].y; J[2][2] += dN_dzeta * nodes[n].z;
                }
                double detJ = J[0][0]*(J[1][1]*J[2][2] - J[1][2]*J[2][1])
                            - J[0][1]*(J[1][0]*J[2][2] - J[1][2]*J[2][0])
                            + J[0][2]*(J[1][0]*J[2][1] - J[1][1]*J[2][0]);
                vol += detJ;
                j_min = std::min(j_min, detJ);
                j_max = std::max(j_max, detJ);
            }
        }
    }
    q.area = std::abs(vol);
    q.jacobian_ratio = (j_max > 1e-20) ? j_min / j_max : 0.0;

    static const int edges_h8[12][2] = {
        {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}
    };
    double e_min = 1e20, e_max = -1e20;
    for (const auto& e : edges_h8) {
        double dx = nodes[e[1]].x - nodes[e[0]].x;
        double dy = nodes[e[1]].y - nodes[e[0]].y;
        double dz = nodes[e[1]].z - nodes[e[0]].z;
        double len = std::sqrt(dx*dx + dy*dy + dz*dz);
        e_min = std::min(e_min, len);
        e_max = std::max(e_max, len);
    }
    q.aspect_ratio = (e_min > 1e-20) ? e_max / e_min : 1e20;
    q.skewness = 0.0;
    return q;
}

// ------------------------------------------------------------------
// Compute quality metrics for a T4 element
// ------------------------------------------------------------------
inline ElementQuality compute_t4_quality(const std::array<Node, 4>& nodes) {
    ElementQuality q;

    double vol = std::abs(
        (nodes[1].x - nodes[0].x) * ((nodes[2].y - nodes[0].y) * (nodes[3].z - nodes[0].z) -
                                       (nodes[3].y - nodes[0].y) * (nodes[2].z - nodes[0].z)) -
        (nodes[1].y - nodes[0].y) * ((nodes[2].x - nodes[0].x) * (nodes[3].z - nodes[0].z) -
                                       (nodes[3].x - nodes[0].x) * (nodes[2].z - nodes[0].z)) +
        (nodes[1].z - nodes[0].z) * ((nodes[2].x - nodes[0].x) * (nodes[3].y - nodes[0].y) -
                                       (nodes[3].x - nodes[0].x) * (nodes[2].y - nodes[0].y))
    ) / 6.0;
    q.area = vol;
    q.jacobian_ratio = 1.0;

    static const int edges_t4[6][2] = {{0,1},{0,2},{0,3},{1,2},{1,3},{2,3}};
    double e_min = 1e20, e_max = -1e20;
    for (const auto& e : edges_t4) {
        double dx = nodes[e[1]].x - nodes[e[0]].x;
        double dy = nodes[e[1]].y - nodes[e[0]].y;
        double dz = nodes[e[1]].z - nodes[e[0]].z;
        double len = std::sqrt(dx*dx + dy*dy + dz*dz);
        e_min = std::min(e_min, len);
        e_max = std::max(e_max, len);
    }
    q.aspect_ratio = (e_min > 1e-20) ? e_max / e_min : 1e20;
    q.skewness = 0.0;
    return q;
}

inline MeshQuality compute_mesh_quality(const Mesh& m) {
    MeshQuality mq;

    mq.quad_quality.resize(m.num_quads());
    for (int e = 0; e < m.num_quads(); ++e) {
        const auto& elem = m.quad_elements[e];
        std::array<Node, 4> nodes;
        for (int i = 0; i < 4; ++i) nodes[i] = m.nodes[elem[i]];
        mq.quad_quality[e] = compute_q4_quality(nodes);

        mq.min_jacobian_ratio = std::min(mq.min_jacobian_ratio, mq.quad_quality[e].jacobian_ratio);
        mq.max_aspect_ratio = std::max(mq.max_aspect_ratio, mq.quad_quality[e].aspect_ratio);
        mq.max_skewness = std::max(mq.max_skewness, mq.quad_quality[e].skewness);
    }

    mq.tri_quality.resize(m.num_tris());
    for (int e = 0; e < m.num_tris(); ++e) {
        const auto& elem = m.tri_elements[e];
        std::array<Node, 3> nodes;
        for (int i = 0; i < 3; ++i) nodes[i] = m.nodes[elem[i]];
        mq.tri_quality[e] = compute_t3_quality(nodes);

        mq.min_jacobian_ratio = std::min(mq.min_jacobian_ratio, mq.tri_quality[e].jacobian_ratio);
        mq.max_aspect_ratio = std::max(mq.max_aspect_ratio, mq.tri_quality[e].aspect_ratio);
        mq.max_skewness = std::max(mq.max_skewness, mq.tri_quality[e].skewness);
    }

    mq.hex_quality.resize(m.num_hexes());
    for (int e = 0; e < m.num_hexes(); ++e) {
        const auto& elem = m.hex_elements[e];
        std::array<Node, 8> nodes;
        for (int i = 0; i < 8; ++i) nodes[i] = m.nodes[elem[i]];
        mq.hex_quality[e] = compute_h8_quality(nodes);
        mq.min_jacobian_ratio = std::min(mq.min_jacobian_ratio, mq.hex_quality[e].jacobian_ratio);
        mq.max_aspect_ratio = std::max(mq.max_aspect_ratio, mq.hex_quality[e].aspect_ratio);
        mq.max_skewness = std::max(mq.max_skewness, mq.hex_quality[e].skewness);
    }

    mq.tet_quality.resize(m.num_tets());
    for (int e = 0; e < m.num_tets(); ++e) {
        const auto& elem = m.tet_elements[e];
        std::array<Node, 4> nodes;
        for (int i = 0; i < 4; ++i) nodes[i] = m.nodes[elem[i]];
        mq.tet_quality[e] = compute_t4_quality(nodes);
        mq.min_jacobian_ratio = std::min(mq.min_jacobian_ratio, mq.tet_quality[e].jacobian_ratio);
        mq.max_aspect_ratio = std::max(mq.max_aspect_ratio, mq.tet_quality[e].aspect_ratio);
        mq.max_skewness = std::max(mq.max_skewness, mq.tet_quality[e].skewness);
    }

    return mq;
}

// ------------------------------------------------------------------
// Generate a structured quad mesh on [0, Lx] x [0, Ly]
// nx, ny: number of elements in each direction
// grading_x, grading_y: optional grading ratios (1.0 = uniform)
//   grading > 1.0 concentrates elements toward the right/top
//   grading < 1.0 concentrates elements toward the left/bottom
// ------------------------------------------------------------------
inline Mesh generate_structured_quad(
    double Lx, double Ly, int nx, int ny,
    double grading_x = 1.0, double grading_y = 1.0) {

    Mesh m;
    int num_nodes_x = nx + 1;
    int num_nodes_y = ny + 1;

    // Generate node coordinates with grading
    std::vector<double> x_coords(num_nodes_x);
    std::vector<double> y_coords(num_nodes_y);

    // X coordinates
    if (std::abs(grading_x - 1.0) < 1e-12) {
        for (int i = 0; i <= nx; ++i)
            x_coords[i] = Lx * i / nx;
    } else {
        for (int i = 0; i <= nx; ++i) {
            double t = static_cast<double>(i) / nx;
            x_coords[i] = Lx * (std::pow(grading_x, t) - 1.0) / (grading_x - 1.0);
        }
    }

    // Y coordinates
    if (std::abs(grading_y - 1.0) < 1e-12) {
        for (int j = 0; j <= ny; ++j)
            y_coords[j] = Ly * j / ny;
    } else {
        for (int j = 0; j <= ny; ++j) {
            double t = static_cast<double>(j) / ny;
            y_coords[j] = Ly * (std::pow(grading_y, t) - 1.0) / (grading_y - 1.0);
        }
    }

    // Create nodes (row-major: node_index = j * (nx+1) + i)
    m.nodes.resize(num_nodes_x * num_nodes_y);
    for (int j = 0; j <= ny; ++j) {
        for (int i = 0; i <= nx; ++i) {
            int idx = j * num_nodes_x + i;
            m.nodes[idx] = { x_coords[i], y_coords[j] };
        }
    }

    // Create Q4 elements (CCW ordering)
    m.quad_elements.resize(nx * ny);
    for (int j = 0; j < ny; ++j) {
        for (int i = 0; i < nx; ++i) {
            int n0 = j * num_nodes_x + i;         // bottom-left
            int n1 = j * num_nodes_x + (i + 1);   // bottom-right
            int n2 = (j + 1) * num_nodes_x + (i + 1); // top-right
            int n3 = (j + 1) * num_nodes_x + i;   // top-left
            m.quad_elements[j * nx + i] = { n0, n1, n2, n3 };
        }
    }

    return m;
}

// ------------------------------------------------------------------
// Generate a structured Q8 (8-node serendipity) mesh
// Same node layout as Q4 but adds midside nodes at edge midpoints
// ------------------------------------------------------------------
inline Mesh generate_structured_quad8(
    double Lx, double Ly, int nx, int ny,
    double grading_x = 1.0, double grading_y = 1.0) {

    Mesh m;

    // Step 1: Generate Q4 mesh first (for corner nodes)
    auto q4_mesh = generate_structured_quad(Lx, Ly, nx, ny, grading_x, grading_y);

    // Q8 node numbering: corners (0-3) same as Q4, then midside (4-7)
    // Node layout: (nx+1)*(ny+1) corners + nx*(ny+1) horiz mid + (nx+1)*ny vert mid + nx*ny center
    int num_corners = (nx + 1) * (ny + 1);
    int num_hmid = nx * (ny + 1);           // horizontal midside nodes
    int num_vmid = (nx + 1) * ny;           // vertical midside nodes
    int num_total = num_corners + num_hmid + num_vmid;

    m.nodes.resize(num_total);

    // Copy corner nodes
    for (int i = 0; i < num_corners; ++i) {
        m.nodes[i] = q4_mesh.nodes[i];
    }

    // Create midside nodes on horizontal edges (between columns)
    // Node index: corner + j*nx + i
    for (int j = 0; j <= ny; ++j) {
        for (int i = 0; i < nx; ++i) {
            int left = j * (nx + 1) + i;
            int right = left + 1;
            int mid_idx = num_corners + j * nx + i;
            m.nodes[mid_idx] = {
                (q4_mesh.nodes[left].x + q4_mesh.nodes[right].x) / 2.0,
                (q4_mesh.nodes[left].y + q4_mesh.nodes[right].y) / 2.0
            };
        }
    }

    // Create midside nodes on vertical edges (between rows)
    for (int j = 0; j < ny; ++j) {
        for (int i = 0; i <= nx; ++i) {
            int bottom = j * (nx + 1) + i;
            int top = bottom + (nx + 1);
            int mid_idx = num_corners + num_hmid + j * (nx + 1) + i;
            m.nodes[mid_idx] = {
                (q4_mesh.nodes[bottom].x + q4_mesh.nodes[top].x) / 2.0,
                (q4_mesh.nodes[bottom].y + q4_mesh.nodes[top].y) / 2.0
            };
        }
    }

    // Create Q8 elements
    m.quad8_elements.resize(nx * ny);
    for (int j = 0; j < ny; ++j) {
        for (int i = 0; i < nx; ++i) {
            int n0 = j * (nx + 1) + i;                    // bottom-left
            int n1 = j * (nx + 1) + (i + 1);              // bottom-right
            int n2 = (j + 1) * (nx + 1) + (i + 1);       // top-right
            int n3 = (j + 1) * (nx + 1) + i;              // top-left

            // Midside nodes
            int n4 = num_corners + j * nx + i;             // bottom midside
            int n5 = num_corners + num_hmid + j * (nx + 1) + (i + 1); // right midside
            int n6 = num_corners + (j + 1) * nx + i;      // top midside
            int n7 = num_corners + num_hmid + j * (nx + 1) + i;       // left midside

            m.quad8_elements[j * nx + i] = { n0, n1, n2, n3, n4, n5, n6, n7 };
        }
    }

    return m;
}

// ------------------------------------------------------------------
// Generate a structured quad mesh for an L-bracket
// The L-shape is defined by cutting out a rectangle from a larger one
// Full domain: [0, Lx] x [0, Ly]
// Cutout: [Lx-cx, Lx] x [0, cy]  (bottom-right corner removed)
// ------------------------------------------------------------------
inline Mesh generate_lbracket(
    double Lx, double Ly, double cx, double cy,
    int nx, int ny) {

    // Start with full rectangular mesh
    auto m = generate_structured_quad(Lx, Ly, nx, ny);

    // Identify nodes inside the cutout region
    // Cutout: x >= (Lx - cx) AND y <= cy
    std::vector<bool> node_active(m.nodes.size(), true);
    for (int i = 0; i < static_cast<int>(m.nodes.size()); ++i) {
        if (m.nodes[i].x >= (Lx - cx) - 1e-12 && m.nodes[i].y <= cy + 1e-12) {
            node_active[i] = false;
        }
    }

    // Filter elements: keep only elements where all 4 nodes are active
    std::vector<std::array<int, 4>> active_quads;
    for (const auto& elem : m.quad_elements) {
        if (node_active[elem[0]] && node_active[elem[1]] &&
            node_active[elem[2]] && node_active[elem[3]]) {
            active_quads.push_back(elem);
        }
    }
    m.quad_elements = active_quads;

    // Renumber nodes: create mapping from old to new indices
    std::vector<int> node_map(m.nodes.size(), -1);
    std::vector<Node> new_nodes;
    for (int i = 0; i < static_cast<int>(m.nodes.size()); ++i) {
        if (node_active[i]) {
            node_map[i] = static_cast<int>(new_nodes.size());
            new_nodes.push_back(m.nodes[i]);
        }
    }
    m.nodes = new_nodes;

    // Update element connectivity
    for (auto& elem : m.quad_elements) {
        for (int& n : elem) {
            n = node_map[n];
        }
    }

    return m;
}

// ------------------------------------------------------------------
// Load mesh from JSON file
// Format: { "nodes": [[x,y], ...], "elements": [[n0,n1,n2,n3], ...],
//           "dirichlet": [[node, dof, value], ...],
//           "neumann": [[node, dof, value], ...],
//           "material": { "E": ..., "nu": ..., "rho": ..., "t": ... },
//           "plane": "stress" | "strain" }
// ------------------------------------------------------------------
inline Mesh load_json(const std::string& filepath) {
    std::string content = json::read_file(filepath);
    Mesh m;
    size_t i = 0;

    // Skip to "nodes" key
    while (i < content.size()) {
        json::skip_ws(content, i);
        if (i < content.size() && content[i] == '"') {
            size_t key_start = ++i;
            while (i < content.size() && content[i] != '"') ++i;
            std::string key = content.substr(key_start, i - key_start);
            ++i;  // skip closing quote
            json::skip_ws(content, i);
            if (i < content.size() && content[i] == ':') ++i;

            if (key == "nodes") {
                // Parse array of [x, y] pairs
                json::skip_ws(content, i);
                if (i < content.size() && content[i] == '[') ++i;
                while (i < content.size() && content[i] != ']') {
                    json::skip_ws(content, i);
                    if (content[i] == ',') { ++i; continue; }
                    if (content[i] == '[') {
                        ++i;
                        double x = json::parse_number(content, i);
                        json::skip_ws(content, i);
                        if (content[i] == ',') ++i;
                        double y = json::parse_number(content, i);
                        json::skip_ws(content, i);
                        if (content[i] == ']') ++i;
                        m.nodes.push_back({x, y});
                    }
                }
                if (i < content.size() && content[i] == ']') ++i;
            } else if (key == "elements") {
                // Parse array of [n0, n1, n2, n3] arrays
                json::skip_ws(content, i);
                if (i < content.size() && content[i] == '[') ++i;
                while (i < content.size() && content[i] != ']') {
                    json::skip_ws(content, i);
                    if (content[i] == ',') { ++i; continue; }
                    if (content[i] == '[') {
                        ++i;
                        std::array<int, 4> elem{};
                        for (int k = 0; k < 4; ++k) {
                            json::skip_ws(content, i);
                            elem[k] = json::parse_int(content, i);
                            json::skip_ws(content, i);
                            if (k < 3 && content[i] == ',') ++i;
                        }
                        if (content[i] == ']') ++i;
                        m.quad_elements.push_back(elem);
                    }
                }
                if (i < content.size() && content[i] == ']') ++i;
            } else if (key == "dirichlet") {
                json::skip_ws(content, i);
                if (i < content.size() && content[i] == '[') ++i;
                while (i < content.size() && content[i] != ']') {
                    json::skip_ws(content, i);
                    if (content[i] == ',') { ++i; continue; }
                    if (content[i] == '[') {
                        ++i;
                        DirichletBC bc;
                        bc.node = json::parse_int(content, i);
                        json::skip_ws(content, i); if (content[i] == ',') ++i;
                        bc.dof = json::parse_int(content, i);
                        json::skip_ws(content, i); if (content[i] == ',') ++i;
                        bc.value = json::parse_number(content, i);
                        json::skip_ws(content, i);
                        if (content[i] == ']') ++i;
                        m.dirichlet.push_back(bc);
                    }
                }
                if (i < content.size() && content[i] == ']') ++i;
            } else if (key == "neumann") {
                json::skip_ws(content, i);
                if (i < content.size() && content[i] == '[') ++i;
                while (i < content.size() && content[i] != ']') {
                    json::skip_ws(content, i);
                    if (content[i] == ',') { ++i; continue; }
                    if (content[i] == '[') {
                        ++i;
                        NeumannBC bc;
                        bc.node = json::parse_int(content, i);
                        json::skip_ws(content, i); if (content[i] == ',') ++i;
                        bc.dof = json::parse_int(content, i);
                        json::skip_ws(content, i); if (content[i] == ',') ++i;
                        bc.value = json::parse_number(content, i);
                        json::skip_ws(content, i);
                        if (content[i] == ']') ++i;
                        m.neumann.push_back(bc);
                    }
                }
                if (i < content.size() && content[i] == ']') ++i;
            } else if (key == "material") {
                json::skip_ws(content, i);
                if (i < content.size() && content[i] == '{') ++i;
                while (i < content.size() && content[i] != '}') {
                    json::skip_ws(content, i);
                    if (content[i] == ',') { ++i; continue; }
                    // Parse "key": value
                    json::skip_ws(content, i);
                    size_t mk_start = ++i;
                    while (i < content.size() && content[i] != '"') ++i;
                    std::string mk = content.substr(mk_start, i - mk_start);
                    ++i;
                    json::skip_ws(content, i); if (content[i] == ':') ++i;
                    double mv = json::parse_number(content, i);
                    if (mk == "E") m.mat.E = mv;
                    else if (mk == "nu") m.mat.nu = mv;
                    else if (mk == "rho") m.mat.rho = mv;
                    else if (mk == "t") m.mat.t = mv;
                }
                if (i < content.size() && content[i] == '}') ++i;
            } else {
                json::skip_value(content, i);
            }
        } else {
            ++i;
        }
    }

    return m;
}

// ------------------------------------------------------------------
// Generate a Cook's membrane Q8 mesh (trapezoidal panel)
// Standard geometry: L=48, h_left=44, h_right=60
// ------------------------------------------------------------------
inline Mesh generate_cook_quad8(double L, double h_left, double h_right,
    int nx, int ny) {

    Mesh m;
    int num_corners = (nx + 1) * (ny + 1);
    int num_hmid = nx * (ny + 1);
    int num_vmid = (nx + 1) * ny;
    m.nodes.resize(num_corners + num_hmid + num_vmid);

    // Corner nodes: row-major
    for (int j = 0; j <= ny; ++j) {
        double eta = static_cast<double>(j) / ny;
        for (int i = 0; i <= nx; ++i) {
            double xi = static_cast<double>(i) / nx;
            double x = L * xi;
            double h = h_left + (h_right - h_left) * xi;
            double y = h * (eta - 0.5);
            m.nodes[j * (nx + 1) + i] = {x, y};
        }
    }

    // Horizontal midside nodes: between corner pairs in x
    for (int j = 0; j <= ny; ++j) {
        double eta = static_cast<double>(j) / ny;
        for (int i = 0; i < nx; ++i) {
            double xi = (i + 0.5) / nx;
            double x = L * xi;
            double h = h_left + (h_right - h_left) * xi;
            double y = h * (eta - 0.5);
            int mid_idx = num_corners + j * nx + i;
            m.nodes[mid_idx] = {x, y};
        }
    }

    // Vertical midside nodes: between corner pairs in y
    for (int j = 0; j < ny; ++j) {
        double eta = (j + 0.5) / ny;
        for (int i = 0; i <= nx; ++i) {
            double xi = static_cast<double>(i) / nx;
            double x = L * xi;
            double h = h_left + (h_right - h_left) * xi;
            double y = h * (eta - 0.5);
            int mid_idx = num_corners + num_hmid + j * (nx + 1) + i;
            m.nodes[mid_idx] = {x, y};
        }
    }

    // Q8 elements: CCW corner ordering
    m.quad8_elements.resize(nx * ny);
    for (int j = 0; j < ny; ++j) {
        for (int i = 0; i < nx; ++i) {
            int n0 = j * (nx + 1) + i;
            int n1 = j * (nx + 1) + (i + 1);
            int n2 = (j + 1) * (nx + 1) + (i + 1);
            int n3 = (j + 1) * (nx + 1) + i;

            int n4 = num_corners + j * nx + i;
            int n5 = num_corners + num_hmid + j * (nx + 1) + (i + 1);
            int n6 = num_corners + (j + 1) * nx + i;
            int n7 = num_corners + num_hmid + j * (nx + 1) + i;

            m.quad8_elements[j * nx + i] = {n0, n1, n2, n3, n4, n5, n6, n7};
        }
    }

    return m;
}

// ------------------------------------------------------------------
// Generate a structured hex (H8) mesh on [0,Lx] x [0,Ly] x [0,Lz]
// ------------------------------------------------------------------
inline Mesh generate_structured_hex(
    double Lx, double Ly, double Lz,
    int nx, int ny, int nz,
    double grading_x = 1.0, double grading_y = 1.0, double grading_z = 1.0)
{
    Mesh m;
    set_dimension(3);
    int num_nodes_x = nx + 1;
    int num_nodes_y = ny + 1;
    int num_nodes_z = nz + 1;

    std::vector<double> xc(num_nodes_x), yc(num_nodes_y), zc(num_nodes_z);

    if (std::abs(grading_x - 1.0) < 1e-12) {
        for (int i = 0; i <= nx; ++i) xc[i] = Lx * i / nx;
    } else {
        for (int i = 0; i <= nx; ++i) {
            double t = static_cast<double>(i) / nx;
            xc[i] = Lx * (std::pow(grading_x, t) - 1.0) / (grading_x - 1.0);
        }
    }
    if (std::abs(grading_y - 1.0) < 1e-12) {
        for (int j = 0; j <= ny; ++j) yc[j] = Ly * j / ny;
    } else {
        for (int j = 0; j <= ny; ++j) {
            double t = static_cast<double>(j) / ny;
            yc[j] = Ly * (std::pow(grading_y, t) - 1.0) / (grading_y - 1.0);
        }
    }
    if (std::abs(grading_z - 1.0) < 1e-12) {
        for (int k = 0; k <= nz; ++k) zc[k] = Lz * k / nz;
    } else {
        for (int k = 0; k <= nz; ++k) {
            double t = static_cast<double>(k) / nz;
            zc[k] = Lz * (std::pow(grading_z, t) - 1.0) / (grading_z - 1.0);
        }
    }

    m.nodes.resize(num_nodes_x * num_nodes_y * num_nodes_z);
    for (int k = 0; k <= nz; ++k)
        for (int j = 0; j <= ny; ++j)
            for (int i = 0; i <= nx; ++i) {
                int idx = k * num_nodes_y * num_nodes_x + j * num_nodes_x + i;
                m.nodes[idx] = { xc[i], yc[j], zc[k] };
            }

    m.hex_elements.resize(nx * ny * nz);
    for (int k = 0; k < nz; ++k)
        for (int j = 0; j < ny; ++j)
            for (int i = 0; i < nx; ++i) {
                int n0 = k * num_nodes_y * num_nodes_x + j * num_nodes_x + i;
                int n1 = n0 + 1;
                int n2 = n0 + 1 + num_nodes_x;
                int n3 = n0 + num_nodes_x;
                int n4 = n0 + num_nodes_y * num_nodes_x;
                int n5 = n4 + 1;
                int n6 = n4 + 1 + num_nodes_x;
                int n7 = n4 + num_nodes_x;
                m.hex_elements[k * ny * nx + j * nx + i] = { n0, n1, n2, n3, n4, n5, n6, n7 };
            }

    return m;
}

// ------------------------------------------------------------------
// Convert hex mesh to tet mesh (6 tets per hex, Freudenthal subdivision)
// ------------------------------------------------------------------
inline Mesh hex_to_tet(const Mesh& hex_mesh) {
    Mesh m = hex_mesh;
    m.tet_elements.clear();
    m.tet_elements.reserve(m.hex_elements.size() * 6);

    for (const auto& h : m.hex_elements) {
        m.tet_elements.push_back({h[0], h[1], h[2], h[6]});
        m.tet_elements.push_back({h[0], h[2], h[3], h[6]});
        m.tet_elements.push_back({h[0], h[1], h[5], h[6]});
        m.tet_elements.push_back({h[0], h[4], h[5], h[6]});
        m.tet_elements.push_back({h[0], h[3], h[4], h[6]});
        m.tet_elements.push_back({h[0], h[4], h[7], h[6]});
    }
    m.hex_elements.clear();
    return m;
}

// ------------------------------------------------------------------
// Generate a structured tet mesh by refining a hex mesh
// ------------------------------------------------------------------
inline Mesh generate_structured_tet(
    double Lx, double Ly, double Lz,
    int nx, int ny, int nz,
    double grading_x = 1.0, double grading_y = 1.0, double grading_z = 1.0)
{
    auto hex_mesh = generate_structured_hex(Lx, Ly, Lz, nx, ny, nz,
                                            grading_x, grading_y, grading_z);
    return hex_to_tet(hex_mesh);
}

}  // namespace mesh
