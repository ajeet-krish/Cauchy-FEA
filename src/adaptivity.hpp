#pragma once
#include "fea_types.hpp"
#include "elements.hpp"
#include "postprocess.hpp"
#include "mesh.hpp"
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <iostream>
#include <map>

// ==========================================================================
// ADAPTIVITY -- ZZ error estimator (SPR) + red-green h-refinement
// ==========================================================================

namespace adaptivity {

// ------------------------------------------------------------------
// SPR (Superconvergent Patch Recovery) -- recovered nodal stress
// For each node, fit a linear polynomial through nearby Gauss-point stresses
// using weighted least squares. The recovered field is smoother and
// superconvergent at nodes.
// ------------------------------------------------------------------
struct SPRResult {
    std::vector<double> recovered_sxx;   // recovered sigma_xx at each node
    std::vector<double> recovered_syy;   // recovered sigma_yy at each node
    std::vector<double> recovered_sxy;   // recovered sigma_xy at each node
    std::vector<double> recovered_vm;    // recovered von Mises at each node
};

inline SPRResult spr_recovery(
    const Mesh& m,
    const std::vector<postprocess::ElementStress>& elem_stresses) {

    int nn = m.num_nodes();
    SPRResult result;
    result.recovered_sxx.resize(nn, 0.0);
    result.recovered_syy.resize(nn, 0.0);
    result.recovered_sxy.resize(nn, 0.0);
    result.recovered_vm.resize(nn, 0.0);

    // Accumulate weighted stress contributions per node
    std::vector<double> weight_sum(nn, 0.0);

    // For Q4 elements: each element has 4 Gauss points (2x2)
    static const double GP2 = 1.0 / std::sqrt(3.0);
    static const double xi_pts[2] = { -GP2, GP2 };
    static const double eta_pts[2] = { -GP2, GP2 };

    for (int e = 0; e < m.num_quads(); ++e) {
        const auto& elem = m.quad_elements[e];
        const auto& s = elem_stresses[e];

        for (int gi = 0; gi < 2; ++gi) {
            for (int gj = 0; gj < 2; ++gj) {
                double xi = xi_pts[gi], eta = eta_pts[gj];

                // Evaluate shape functions at this Gauss point
                static const double XI[4]  = { -1.0,  1.0,  1.0, -1.0 };
                static const double ETA[4] = { -1.0, -1.0,  1.0,  1.0 };

                for (int i = 0; i < 4; ++i) {
                    double N = 0.25 * (1.0 + XI[i] * xi) * (1.0 + ETA[i] * eta);
                    int n = elem[i];
                    result.recovered_sxx[n] += N * s.sigma_xx;
                    result.recovered_syy[n] += N * s.sigma_yy;
                    result.recovered_sxy[n] += N * s.sigma_xy;
                    weight_sum[n] += N;
                }
            }
        }
    }

    // For Q8 elements: use 2x2 Gauss points (same as Q4, but 8-node shape)
    for (int e = 0; e < m.num_quad8s(); ++e) {
        const auto& elem = m.quad8_elements[e];
        const auto& s = elem_stresses[m.num_quads() + e];

        for (int gi = 0; gi < 2; ++gi) {
            for (int gj = 0; gj < 2; ++gj) {
                double xi = xi_pts[gi], eta = eta_pts[gj];

                for (int i = 0; i < 8; ++i) {
                    double N = elements::Q8Element::shape_func(i, xi, eta);
                    int n = elem[i];
                    result.recovered_sxx[n] += N * s.sigma_xx;
                    result.recovered_syy[n] += N * s.sigma_yy;
                    result.recovered_sxy[n] += N * s.sigma_xy;
                    weight_sum[n] += N;
                }
            }
        }
    }

    // Normalize
    for (int i = 0; i < nn; ++i) {
        if (weight_sum[i] > 1e-30) {
            result.recovered_sxx[i] /= weight_sum[i];
            result.recovered_syy[i] /= weight_sum[i];
            result.recovered_sxy[i] /= weight_sum[i];
        }
        double avg = (result.recovered_sxx[i] + result.recovered_syy[i]) / 2.0;
        double R = std::sqrt(std::pow((result.recovered_sxx[i] - result.recovered_syy[i]) / 2.0, 2)
                             + result.recovered_sxy[i] * result.recovered_sxy[i]);
        double s1 = avg + R, s2 = avg - R;
        result.recovered_vm[i] = std::sqrt(s1 * s1 - s1 * s2 + s2 * s2);
    }

    return result;
}

// ------------------------------------------------------------------
// Element-wise ZZ error indicator
// eta_e^2 = integral over element of |sigma_star - sigma_h|^2
// Computed at Gauss points using recovered nodal stress interpolated
// back to Gauss points vs FE stress at those same Gauss points.
// ------------------------------------------------------------------
struct ElementError {
    double eta;          // error indicator (L2 norm of stress difference)
    double eta_squared;  // squared error indicator (for summation)
};

inline std::vector<ElementError> compute_error_indicators(
    const Mesh& m,
    const std::vector<postprocess::ElementStress>& elem_stresses,
    const SPRResult& spr) {

    int total_elements = m.num_quads() + m.num_quad8s();
    std::vector<ElementError> errors(total_elements);

    static const double GP2 = 1.0 / std::sqrt(3.0);
    static const double xi_pts[2] = { -GP2, GP2 };
    static const double eta_pts[2] = { -GP2, GP2 };

    double total_error_sq = 0.0;

    // Q4 elements
    for (int e = 0; e < m.num_quads(); ++e) {
        const auto& elem = m.quad_elements[e];
        const auto& s_fe = elem_stresses[e];
        double eta_sq = 0.0;

        for (int gi = 0; gi < 2; ++gi) {
            for (int gj = 0; gj < 2; ++gj) {
                double xi = xi_pts[gi], eta = eta_pts[gj];

                // Interpolate recovered stress to this Gauss point
                double rec_sxx = 0.0, rec_syy = 0.0, rec_sxy = 0.0;
                static const double XI[4]  = { -1.0,  1.0,  1.0, -1.0 };
                static const double ETA[4] = { -1.0, -1.0,  1.0,  1.0 };

                for (int i = 0; i < 4; ++i) {
                    double N = 0.25 * (1.0 + XI[i] * xi) * (1.0 + ETA[i] * eta);
                    rec_sxx += N * spr.recovered_sxx[elem[i]];
                    rec_syy += N * spr.recovered_syy[elem[i]];
                    rec_sxy += N * spr.recovered_sxy[elem[i]];
                }

                // Compute Jacobian for area element
                std::array<Node, 4> nodes;
                for (int i = 0; i < 4; ++i) nodes[i] = m.nodes[elem[i]];

                double J11 = 0.0, J12 = 0.0, J21 = 0.0, J22 = 0.0;
                for (int i = 0; i < 4; ++i) {
                    auto dN = elements::Q4Element::shape_deriv(i, xi, eta);
                    J11 += dN[0] * nodes[i].x;
                    J12 += dN[0] * nodes[i].y;
                    J21 += dN[1] * nodes[i].x;
                    J22 += dN[1] * nodes[i].y;
                }
                double detJ = std::abs(J11 * J22 - J12 * J21);

                // Error: |sigma_star - sigma_h|^2 * detJ * weight
                double dsxx = rec_sxx - s_fe.sigma_xx;
                double dsyy = rec_syy - s_fe.sigma_yy;
                double dsxy = rec_sxy - s_fe.sigma_xy;
                eta_sq += (dsxx * dsxx + dsyy * dsyy + 2.0 * dsxy * dsxy) * detJ;
            }
        }

        errors[e].eta_squared = eta_sq;
        errors[e].eta = std::sqrt(eta_sq);
        total_error_sq += eta_sq;
    }

    // Q8 elements
    for (int e = 0; e < m.num_quad8s(); ++e) {
        const auto& elem = m.quad8_elements[e];
        const auto& s_fe = elem_stresses[m.num_quads() + e];
        double eta_sq = 0.0;

        for (int gi = 0; gi < 2; ++gi) {
            for (int gj = 0; gj < 2; ++gj) {
                double xi = xi_pts[gi], eta = eta_pts[gj];

                double rec_sxx = 0.0, rec_syy = 0.0, rec_sxy = 0.0;
                for (int i = 0; i < 8; ++i) {
                    double N = elements::Q8Element::shape_func(i, xi, eta);
                    rec_sxx += N * spr.recovered_sxx[elem[i]];
                    rec_syy += N * spr.recovered_syy[elem[i]];
                    rec_sxy += N * spr.recovered_sxy[elem[i]];
                }

                // Compute Jacobian
                std::vector<Node> nodes(8);
                for (int i = 0; i < 8; ++i) nodes[i] = m.nodes[elem[i]];

                double J11 = 0.0, J12 = 0.0, J21 = 0.0, J22 = 0.0;
                for (int i = 0; i < 8; ++i) {
                    auto dN = elements::Q8Element::shape_deriv(i, xi, eta);
                    J11 += dN[0] * nodes[i].x;
                    J12 += dN[0] * nodes[i].y;
                    J21 += dN[1] * nodes[i].x;
                    J22 += dN[1] * nodes[i].y;
                }
                double detJ = std::abs(J11 * J22 - J12 * J21);

                double dsxx = rec_sxx - s_fe.sigma_xx;
                double dsyy = rec_syy - s_fe.sigma_yy;
                double dsxy = rec_sxy - s_fe.sigma_xy;
                eta_sq += (dsxx * dsxx + dsyy * dsyy + 2.0 * dsxy * dsxy) * detJ;
            }
        }

        errors[m.num_quads() + e].eta_squared = eta_sq;
        errors[m.num_quads() + e].eta = std::sqrt(eta_sq);
        total_error_sq += eta_sq;
    }

    return errors;
}

// ------------------------------------------------------------------
// Marking strategy: mark elements with eta_e > threshold
// Uses the "largest first" strategy: sort by error, mark until
// sum of marked errors >= theta * total_error
// ------------------------------------------------------------------
inline std::vector<bool> mark_elements(
    const std::vector<ElementError>& errors,
    double theta = 0.3) {

    int n = static_cast<int>(errors.size());
    std::vector<bool> marked(n, false);

    // Sort elements by error (descending)
    std::vector<int> sorted_idx(n);
    std::iota(sorted_idx.begin(), sorted_idx.end(), 0);
    std::sort(sorted_idx.begin(), sorted_idx.end(),
        [&errors](int a, int b) { return errors[a].eta_squared > errors[b].eta_squared; });

    double total_error_sq = 0.0;
    for (const auto& e : errors) total_error_sq += e.eta_squared;

    double marked_error_sq = 0.0;
    for (int i = 0; i < n; ++i) {
        marked[sorted_idx[i]] = true;
        marked_error_sq += errors[sorted_idx[i]].eta_squared;
        if (marked_error_sq >= theta * total_error_sq) break;
    }

    int count = 0;
    for (bool m : marked) if (m) count++;
    std::cout << "  Marked " << count << "/" << n << " elements for refinement"
              << " (theta=" << theta << ")" << std::endl;

    return marked;
}

// ------------------------------------------------------------------
// Red-green refinement for Q4 structured mesh
//
// Red refinement: split a Q4 into 4 children (1->4)
// Green refinement: split a neighbor to maintain conformity (1->2)
//
// For a structured mesh, we handle this by:
// 1. Identify all edges that need midside nodes
// 2. Add midside nodes
// 3. Split elements accordingly
// ------------------------------------------------------------------
struct RefinedMesh {
    Mesh mesh;
    int refinement_level;
};

// Compute element centroid
inline Node element_centroid(const std::array<Node, 4>& nodes) {
    return {
        (nodes[0].x + nodes[1].x + nodes[2].x + nodes[3].x) / 4.0,
        (nodes[0].y + nodes[1].y + nodes[2].y + nodes[3].y) / 4.0
    };
}

// Compute element center (midpoint of diagonal average)
inline Node element_center(const std::array<int, 4>& elem, const std::vector<Node>& nodes) {
    return {
        (nodes[elem[0]].x + nodes[elem[2]].x) / 2.0,
        (nodes[elem[0]].y + nodes[elem[2]].y) / 2.0
    };
}

// Refine a Q4 mesh: split marked elements into 4 children
// Uses a conforming refinement approach:
// 1. For each marked element, mark all 4 edges for bisection
// 2. For any element with a bisected edge but not marked, do green (1->2) refinement
// 3. Repeat until all elements conform
inline Mesh refine_mesh(
    const Mesh& m,
    const std::vector<bool>& marked) {

    int ne = m.num_quads();
    if (ne == 0) return m;

    // Step 1: Identify edges that need midside nodes
    // An edge needs a midside node if any adjacent element is marked
    // or if any neighbor's refinement forces it
    std::map<std::pair<int,int>, int> edge_to_idx;  // edge (min,max) -> unique edge id
    std::vector<std::pair<int,int>> edges;
    std::vector<std::vector<int>> elem_edges(ne);  // edges per element

    auto get_edge = [&](int n0, int n1) -> int {
        int a = std::min(n0, n1), b = std::max(n0, n1);
        auto key = std::make_pair(a, b);
        auto it = edge_to_idx.find(key);
        if (it != edge_to_idx.end()) return it->second;
        int idx = static_cast<int>(edges.size());
        edges.push_back(key);
        edge_to_idx[key] = idx;
        return idx;
    };

    for (int e = 0; e < ne; ++e) {
        const auto& elem = m.quad_elements[e];
        // 4 edges: 0-1, 1-2, 2-3, 3-0
        elem_edges[e].resize(4);
        elem_edges[e][0] = get_edge(elem[0], elem[1]);
        elem_edges[e][1] = get_edge(elem[1], elem[2]);
        elem_edges[e][2] = get_edge(elem[2], elem[3]);
        elem_edges[e][3] = get_edge(elem[3], elem[0]);
    }

    // Step 2: Mark edges that need midside nodes
    // Initially, mark edges of red-refined elements
    std::vector<bool> edge_split(edges.size(), false);
    for (int e = 0; e < ne; ++e) {
        if (marked[e]) {
            for (int ei = 0; ei < 4; ++ei) {
                edge_split[elem_edges[e][ei]] = true;
            }
        }
    }

    // Propagate: if an element has exactly 1 unsplit edge, it must be green-split
    // (bisect along the unsplit edge's opposite)
    // Repeat until stable
    bool changed = true;
    while (changed) {
        changed = false;
        for (int e = 0; e < ne; ++e) {
            if (marked[e]) continue;  // already red
            int unsplit = 0;
            for (int ei = 0; ei < 4; ++ei) {
                if (!edge_split[elem_edges[e][ei]]) unsplit++;
            }
            // Green refinement: split the element along the longest unsplit edge
            // For simplicity, if element has any split edge, split all its edges
            if (unsplit < 4) {
                for (int ei = 0; ei < 4; ++ei) {
                    if (!edge_split[elem_edges[e][ei]]) {
                        edge_split[elem_edges[e][ei]] = true;
                        changed = true;
                    }
                }
            }
        }
    }

    // Step 3: Create new nodes at edge midpoints
    Mesh new_mesh;
    new_mesh.mat = m.mat;
    new_mesh.plane = m.plane;
    new_mesh.nodes = m.nodes;

    std::vector<int> edge_mid_node(edges.size(), -1);
    for (size_t e = 0; e < edges.size(); ++e) {
        if (edge_split[e]) {
            int n0 = edges[e].first, n1 = edges[e].second;
            Node mid = {
                (m.nodes[n0].x + m.nodes[n1].x) / 2.0,
                (m.nodes[n0].y + m.nodes[n1].y) / 2.0
            };
            edge_mid_node[e] = static_cast<int>(new_mesh.nodes.size());
            new_mesh.nodes.push_back(mid);
        }
    }

    // Step 4: Create new elements
    // Each old Q4 element becomes either 4 (red) or 2 (green) or 1 (unchanged)
    auto add_child = [&](int n0, int n1, int n2, int n3) {
        // Verify CCW ordering by computing signed area
        double area = 0.5 * ((new_mesh.nodes[n1].x - new_mesh.nodes[n0].x) *
                              (new_mesh.nodes[n2].y - new_mesh.nodes[n0].y) -
                              (new_mesh.nodes[n2].x - new_mesh.nodes[n0].x) *
                              (new_mesh.nodes[n1].y - new_mesh.nodes[n0].y));
        if (area > 0) {
            new_mesh.quad_elements.push_back({n0, n1, n2, n3});
        } else {
            // Fix winding
            new_mesh.quad_elements.push_back({n0, n3, n2, n1});
        }
    };

    for (int e = 0; e < ne; ++e) {
        const auto& elem = m.quad_elements[e];
        int e01 = elem_edges[e][0];
        int e12 = elem_edges[e][1];
        int e23 = elem_edges[e][2];
        int e30 = elem_edges[e][3];

        int m01 = edge_mid_node[e01];
        int m12 = edge_mid_node[e12];
        int m23 = edge_mid_node[e23];
        int m30 = edge_mid_node[e30];

        if (marked[e]) {
            // Red refinement: split into 4 children
            // Center node
            Node center = element_centroid({
                m.nodes[elem[0]], m.nodes[elem[1]],
                m.nodes[elem[2]], m.nodes[elem[3]]
            });
            int nc = static_cast<int>(new_mesh.nodes.size());
            new_mesh.nodes.push_back(center);

            add_child(elem[0], m01, nc, m30);
            add_child(m01, elem[1], m12, nc);
            add_child(nc, m12, elem[2], m23);
            add_child(m30, nc, m23, elem[3]);
        } else if (m01 >= 0 || m12 >= 0 || m23 >= 0 || m30 >= 0) {
            // Green refinement: determine split direction
            // Find which edges are split
            if (m01 >= 0 && m23 >= 0) {
                // Split horizontally
                add_child(elem[0], m01, m23, elem[3]);
                add_child(m01, elem[1], elem[2], m23);
            } else if (m12 >= 0 && m30 >= 0) {
                // Split vertically
                add_child(elem[0], elem[1], m12, m30);
                add_child(m30, m12, elem[2], elem[3]);
            } else if (m01 >= 0) {
                // Split edge 0-1: create triangle-like split via diagonal
                add_child(elem[0], m01, elem[2], elem[3]);
                add_child(m01, elem[1], elem[2], elem[3]);
            } else if (m12 >= 0) {
                add_child(elem[0], elem[1], m12, elem[3]);
                add_child(elem[0], m12, elem[2], elem[3]);
            } else if (m23 >= 0) {
                add_child(elem[0], elem[1], m23, elem[3]);
                add_child(elem[0], elem[1], elem[2], m23);
            } else if (m30 >= 0) {
                add_child(elem[0], elem[1], elem[2], m30);
                add_child(m30, elem[1], elem[2], elem[3]);
            }
        } else {
            // Unchanged
            new_mesh.quad_elements.push_back(elem);
        }
    }

    std::cout << "  Refined mesh: " << new_mesh.num_nodes() << " nodes, "
              << new_mesh.num_quads() << " elements" << std::endl;

    return new_mesh;
}

// ------------------------------------------------------------------
// Adaptive refinement loop: solve -> estimate -> mark -> refine
// Returns the refined mesh and convergence history
// ------------------------------------------------------------------
struct AdaptSample {
    int iteration;
    int num_nodes;
    int num_elements;
    double total_error;       // L2 norm of error indicators
    double max_error;         // max element error
    double qoi;               // quantity of interest
};

inline std::vector<AdaptSample> adaptive_loop(
    Mesh m,
    int max_iterations = 5,
    double theta = 0.3,
    bool use_cg = true) {

    std::vector<AdaptSample> history;

    for (int iter = 0; iter < max_iterations; ++iter) {
        std::cout << "\n=== Adaptive Iteration " << iter << " ===" << std::endl;
        std::cout << "  Mesh: " << m.num_nodes() << " nodes, "
                  << m.num_quads() << " elements" << std::endl;

        // 1. Solve
        auto result = fea::solve(m, use_cg);

        // 2. SPR recovery
        auto spr = spr_recovery(m, result.stresses);

        // 3. Error indicators
        auto errors = compute_error_indicators(m, result.stresses, spr);

        // 4. Compute error metrics
        double total_err_sq = 0.0, max_err = 0.0;
        for (const auto& e : errors) {
            total_err_sq += e.eta_squared;
            if (e.eta > max_err) max_err = e.eta;
        }
        double total_err = std::sqrt(total_err_sq);

        // 5. Extract QoI (max displacement)
        double max_disp = 0.0;
        for (int i = 0; i < m.num_nodes(); ++i) {
            double ux = result.displacement[dof_index(i, 0)];
            double uy = result.displacement[dof_index(i, 1)];
            double d = std::sqrt(ux * ux + uy * uy);
            if (d > max_disp) max_disp = d;
        }

        history.push_back({
            iter, m.num_nodes(), m.num_quads(),
            total_err, max_err, max_disp
        });

        std::cout << "  Total error: " << std::scientific << total_err << std::endl;
        std::cout << "  Max element error: " << max_err << std::endl;
        std::cout << "  QoI (max disp): " << max_disp << std::endl;

        // 6. Check convergence: if error is small enough, stop
        if (iter > 0 && total_err < 0.01 * history[0].total_error) {
            std::cout << "  Error converged -- stopping" << std::endl;
            break;
        }

        // 7. Mark elements
        auto marked = mark_elements(errors, theta);

        // 8. Refine
        m = refine_mesh(m, marked);

        // 9. Re-apply BCs for refined mesh
        // (For the plate-hole case, we'd need to re-cut the hole)
        // For now, just clear BCs and let the caller re-apply
        m.dirichlet.clear();
        m.neumann.clear();
    }

    return history;
}

// ------------------------------------------------------------------
// Write adaptive convergence data to JSON
// ------------------------------------------------------------------
inline void write_adaptive_json(
    const std::string& filepath,
    const std::string& case_name,
    const std::vector<AdaptSample>& history) {

    std::ofstream f(filepath);
    f << "{\n";
    f << "  \"case\": \"" << case_name << "\",\n";
    f << "  \"quantity\": \"adaptive_convergence\",\n";
    f << "  \"samples\": [\n";
    for (size_t i = 0; i < history.size(); ++i) {
        const auto& s = history[i];
        f << "    {\"iteration\": " << s.iteration
          << ", \"num_nodes\": " << s.num_nodes
          << ", \"num_elements\": " << s.num_elements
          << ", \"total_error\": " << std::scientific << s.total_error
          << ", \"max_error\": " << s.max_error
          << ", \"qoi\": " << s.qoi
          << "}" << (i + 1 < history.size() ? "," : "") << "\n";
    }
    f << "  ]\n";
    f << "}\n";

    std::cout << "Adaptive data written to " << filepath << std::endl;
}

}  // namespace adaptivity
