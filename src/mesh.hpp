#pragma once
#include "fea_types.hpp"
#include <vector>
#include <array>
#include <cmath>
#include <fstream>
#include <stdexcept>

// ==========================================================================
// MESH GENERATION -- Structured quad mesher + JSON input
// ==========================================================================

namespace mesh {

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

}  // namespace mesh
