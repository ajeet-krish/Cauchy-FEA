#pragma once
#include "fea_types.hpp"
#include "elements.hpp"
#include <vector>
#include <cmath>
#include <algorithm>
#include <iostream>

// ==========================================================================
// CONTACT MECHANICS -- Node-to-surface frictionless contact
// ==========================================================================
//
// Master-slave formulation:
//   - Master surface: defined by a list of segments (pairs of node indices)
//   - Slave nodes: individual nodes that can contact the master surface
//   - Gap function: signed distance from slave node to nearest master segment
//   - Contact constraint: gap >= 0 (no penetration)
//
// Penalty method:
//   - If gap < 0 (penetration), apply normal force = penalty * |gap|
//   - Force direction: normal to master surface, pushing slave outward
//
// Implementation:
//   1. For each slave node, find nearest master segment
//   2. Compute gap function and contact normal
//   3. If gap < 0, compute penalty force
//   4. Assemble contact forces into global RHS

namespace contact {

// ------------------------------------------------------------------
// Contact surface segment (line segment in 2D)
// ------------------------------------------------------------------
struct ContactSegment {
    int node0;  // first node index
    int node1;  // second node index
};

// ------------------------------------------------------------------
// Contact surface: collection of segments
// ------------------------------------------------------------------
struct ContactSurface {
    std::vector<ContactSegment> segments;

    void add_segment(int n0, int n1) {
        segments.push_back({n0, n1});
    }

    int num_segments() const {
        return static_cast<int>(segments.size());
    }
};

// ------------------------------------------------------------------
// Contact pair: slave node + nearest master segment info
// ------------------------------------------------------------------
struct ContactPair {
    int slave_node;
    int master_seg;     // index into master segments
    double gap;         // signed distance (< 0 means penetration)
    double normal_x;    // outward normal x-component
    double normal_y;    // outward normal y-component
    double xi;          // natural coordinate of projection on segment [-1, 1]
};

// ------------------------------------------------------------------
// Compute gap function for a slave node against a line segment
//
// Given slave node at (px, py) and master segment from (x0,y0) to (x1,y1):
//   1. Project slave onto segment line
//   2. Compute signed distance (positive = separated, negative = penetrating)
//   3. Normal points from master outward (away from interior)
//
// Returns gap (signed distance) and sets normal, xi
// ------------------------------------------------------------------
inline double compute_gap(
    double px, double py,
    double x0, double y0,
    double x1, double y1,
    double& normal_x, double& normal_y,
    double& xi) {

    double dx = x1 - x0;
    double dy = y1 - y0;
    double len = std::sqrt(dx * dx + dy * dy);

    if (len < 1e-20) {
        // Degenerate segment
        normal_x = 0.0;
        normal_y = 1.0;
        xi = 0.0;
        double ddx = px - x0;
        double ddy = py - y0;
        return std::sqrt(ddx * ddx + ddy * ddy);
    }

    // Unit tangent along segment
    double tx = dx / len;
    double ty = dy / len;

    // Project slave onto segment
    double t = ((px - x0) * tx + (py - y0) * ty) / len;
    xi = 2.0 * t - 1.0;  // map to [-1, 1]
    xi = std::max(-1.0, std::min(1.0, xi));

    // Closest point on segment
    double cp_x = x0 + t * dx;
    double cp_y = y0 + t * dy;

    // Vector from closest point to slave
    double vx = px - cp_x;
    double vy = py - cp_y;
    double gap = std::sqrt(vx * vx + vy * vy);

    // Outward normal (perpendicular to segment, pointing away from interior)
    // For a surface with CCW ordering, outward normal is to the right of tangent
    normal_x = -ty;  // rotated 90 degrees clockwise
    normal_y = tx;

    // Determine sign: gap is positive when separated, negative when penetrating
    // The vector v points from closest point on segment to slave node
    // If dot(v, normal) < 0, slave is on the "inside" (penetrating)
    double dot = vx * normal_x + vy * normal_y;
    if (dot < 0.0) {
        // Slave is penetrating: gap is negative, normal stays the same
        gap = -gap;
    }

    return gap;
}

// ------------------------------------------------------------------
// Find nearest master segment for a slave node
// Returns the index of the nearest segment and computes gap info
// ------------------------------------------------------------------
inline int find_nearest_segment(
    int slave_node,
    const std::vector<Node>& nodes,
    const ContactSurface& master,
    double& gap,
    double& normal_x, double& normal_y,
    double& xi) {

    int best_seg = -1;
    double best_gap = 1e20;

    for (int s = 0; s < master.num_segments(); ++s) {
        const auto& seg = master.segments[s];
        double nx, ny, xii;
        double g = compute_gap(
            nodes[slave_node].x, nodes[slave_node].y,
            nodes[seg.node0].x, nodes[seg.node0].y,
            nodes[seg.node1].x, nodes[seg.node1].y,
            nx, ny, xii);

        if (std::abs(g) < std::abs(best_gap)) {
            best_gap = g;
            best_seg = s;
            normal_x = nx;
            normal_y = ny;
            xi = xii;
        }
    }

    gap = best_gap;
    return best_seg;
}

// ------------------------------------------------------------------
// Contact detection: find all active contact pairs
// A pair is active if gap < 0 (penetration)
// ------------------------------------------------------------------
inline std::vector<ContactPair> detect_contact(
    const std::vector<Node>& nodes,
    const std::vector<int>& slave_nodes,
    const ContactSurface& master) {

    std::vector<ContactPair> active_pairs;

    for (int sn : slave_nodes) {
        double gap, nx, ny, xi;
        int seg = find_nearest_segment(sn, nodes, master, gap, nx, ny, xi);

        if (seg >= 0 && gap < 0.0) {
            active_pairs.push_back({sn, seg, gap, nx, ny, xi});
        }
    }

    return active_pairs;
}

// ------------------------------------------------------------------
// Compute contact force for a single active pair
// Penalty method: F_contact = penalty * |gap| * normal
// ------------------------------------------------------------------
inline std::pair<double, double> compute_contact_force(
    const ContactPair& pair,
    double penalty) {

    double magnitude = penalty * std::abs(pair.gap);
    return { magnitude * pair.normal_x, magnitude * pair.normal_y };
};

// ------------------------------------------------------------------
// Assemble contact forces into global RHS vector
// Also assembles contact contributions to global stiffness matrix
// (tangent stiffness for Newton-Raphson consistency)
//
// For penalty contact:
//   f_contact[node] += penalty * gap * normal
//   K_contact += penalty * normal^T * normal (diagonal contribution)
// ------------------------------------------------------------------
struct ContactForceResult {
    std::vector<double> f_contact;    // additional RHS contributions
    std::vector<std::pair<int, double>> K_diag_additions;  // (dof, value) pairs for K diagonal
};

inline ContactForceResult assemble_contact_forces(
    const std::vector<Node>& nodes,
    const std::vector<int>& slave_nodes,
    const ContactSurface& master,
    int num_dofs,
    double penalty,
    const std::vector<double>& u = {}) {

    ContactForceResult result;
    result.f_contact.assign(num_dofs, 0.0);

    // Detect active contacts
    auto pairs = detect_contact(nodes, slave_nodes, master);

    if (!pairs.empty()) {
        std::cout << "  Contact: " << pairs.size() << " active pairs detected" << std::endl;
    }

    for (const auto& pair : pairs) {
        auto [fx, fy] = compute_contact_force(pair, penalty);

        int dof_x = dof_index(pair.slave_node, 0);
        int dof_y = dof_index(pair.slave_node, 1);

        result.f_contact[dof_x] += fx;
        result.f_contact[dof_y] += fy;

        // Diagonal stiffness contribution for Newton-Raphson
        // K_contact = penalty * n * n^T (outer product of normal)
        result.K_diag_additions.push_back({dof_x, penalty * pair.normal_x * pair.normal_x});
        result.K_diag_additions.push_back({dof_y, penalty * pair.normal_y * pair.normal_y});
    }

    return result;
}

// ------------------------------------------------------------------
// Contact surface from mesh boundary nodes
// Utility: create contact segments from a list of boundary nodes
// (assumes nodes are ordered along the boundary)
// ------------------------------------------------------------------
inline ContactSurface create_boundary_surface(
    const std::vector<int>& boundary_nodes) {

    ContactSurface surface;
    int n = static_cast<int>(boundary_nodes.size());
    for (int i = 0; i < n; ++i) {
        int j = (i + 1) % n;
        surface.add_segment(boundary_nodes[i], boundary_nodes[j]);
    }
    return surface;
}

}  // namespace contact
