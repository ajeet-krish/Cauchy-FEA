#include "fea_solver_c_api.h"
#include "fea.hpp"
#include <string>
#include <vector>
#include <filesystem>
#include <cmath>
#include <sstream>
#include <fstream>
#include <cstring>

// ==========================================================================
// HAND-ROLLED JSON PARSER (no external dependencies)
// Follows the same pattern as lbm-2d/src/solver_c_api.cpp
// ==========================================================================

// Skip whitespace, return position after whitespace
static size_t capi_skip_ws(const std::string& s, size_t pos) {
    while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' ||
           s[pos] == '\n' || s[pos] == '\r')) {
        ++pos;
    }
    return pos;
}

// Parse a JSON number (double), handles negative, decimal, and scientific
static double capi_parse_number(const std::string& s, size_t& pos) {
    pos = capi_skip_ws(s, pos);
    size_t start = pos;
    bool neg = false;
    if (pos < s.size() && s[pos] == '-') {
        neg = true;
        ++pos;
    }
    double val = 0.0;
    while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') {
        val = val * 10.0 + (s[pos] - '0');
        ++pos;
    }
    if (pos < s.size() && s[pos] == '.') {
        ++pos;
        double frac = 0.1;
        while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') {
            val += (s[pos] - '0') * frac;
            frac *= 0.1;
            ++pos;
        }
    }
    // Handle scientific notation (e.g., 200e9, 1.5E-3)
    if (pos < s.size() && (s[pos] == 'e' || s[pos] == 'E')) {
        ++pos;
        bool exp_neg = false;
        if (pos < s.size() && (s[pos] == '+' || s[pos] == '-')) {
            exp_neg = (s[pos] == '-');
            ++pos;
        }
        int exp_val = 0;
        while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') {
            exp_val = exp_val * 10 + (s[pos] - '0');
            ++pos;
        }
        if (exp_val > 308) exp_val = 308;
        if (exp_val < -308) exp_val = -308;
        double scale = 1.0;
        for (int i = 0; i < exp_val; ++i) scale *= 10.0;
        if (exp_neg) val /= scale; else val *= scale;
    }
    (void)start;
    return neg ? -val : val;
}

// Parse a JSON integer (clamped to int range)
static int capi_parse_int(const std::string& s, size_t& pos) {
    double v = capi_parse_number(s, pos);
    if (v > 2147483647.0 || v < -2147483648.0) return 0;
    return static_cast<int>(v);
}

// Extract a quoted string value
static std::string capi_parse_string(const std::string& s, size_t& pos) {
    pos = capi_skip_ws(s, pos);
    if (pos >= s.size() || s[pos] != '"') return "";
    ++pos; // skip opening quote
    std::string result;
    while (pos < s.size() && s[pos] != '"') {
        if (s[pos] == '\\') {
            ++pos;
            if (pos < s.size()) result += s[pos];
        } else {
            result += s[pos];
        }
        ++pos;
    }
    if (pos < s.size()) ++pos; // skip closing quote
    return result;
}

// Find a key in a JSON object and return position after its value
static size_t capi_find_key(const std::string& s, const std::string& key,
                            size_t start) {
    size_t pos = start;
    // Skip opening brace if present
    pos = capi_skip_ws(s, pos);
    if (pos < s.size() && s[pos] == '{') ++pos;
    while (pos < s.size()) {
        pos = capi_skip_ws(s, pos);
        if (pos >= s.size() || s[pos] == '}') return std::string::npos;
        std::string k = capi_parse_string(s, pos);
        pos = capi_skip_ws(s, pos);
        if (pos < s.size() && s[pos] == ':') ++pos;
        if (k == key) return pos;
        // skip value
        pos = capi_skip_ws(s, pos);
        if (pos >= s.size()) break;
        if (s[pos] == '"') {
            capi_parse_string(s, pos);
        } else if (s[pos] == '[') {
            int depth = 1;
            ++pos;
            while (pos < s.size() && depth > 0) {
                if (s[pos] == '[') ++depth;
                else if (s[pos] == ']') --depth;
                ++pos;
            }
        } else if (s[pos] == '{') {
            int depth = 1;
            ++pos;
            while (pos < s.size() && depth > 0) {
                if (s[pos] == '{') ++depth;
                else if (s[pos] == '}') --depth;
                ++pos;
            }
        } else {
            while (pos < s.size() && s[pos] != ',' &&
                   s[pos] != '}' && s[pos] != ']') ++pos;
        }
        pos = capi_skip_ws(s, pos);
        if (pos < s.size() && s[pos] == ',') ++pos;
    }
    return std::string::npos;
}

// Parse a nested JSON object as a substring (returns the object text)
static std::string capi_extract_object(const std::string& s, size_t pos) {
    pos = capi_skip_ws(s, pos);
    if (pos >= s.size() || s[pos] != '{') return "";
    int depth = 1;
    size_t start = pos;
    ++pos;
    while (pos < s.size() && depth > 0) {
        if (s[pos] == '{') ++depth;
        else if (s[pos] == '}') --depth;
        ++pos;
    }
    return s.substr(start, pos - start);
}

// Parse a points array: [[x1,y1],[x2,y2],...]
static std::vector<std::pair<double, double>> capi_parse_points(
    const std::string& s, size_t pos) {

    std::vector<std::pair<double, double>> result;
    pos = capi_skip_ws(s, pos);
    if (pos >= s.size() || s[pos] != '[') return result;
    ++pos; // skip outer [

    while (pos < s.size()) {
        pos = capi_skip_ws(s, pos);
        if (pos >= s.size() || s[pos] == ']') break;
        if (s[pos] == ',') { ++pos; continue; }
        if (s[pos] == '[') {
            ++pos; // inner [
            double x = capi_parse_number(s, pos);
            pos = capi_skip_ws(s, pos);
            if (pos < s.size() && s[pos] == ',') ++pos;
            double y = capi_parse_number(s, pos);
            result.push_back({x, y});
            pos = capi_skip_ws(s, pos);
            if (pos < s.size() && s[pos] == ']') ++pos;
        } else {
            ++pos;
        }
    }
    return result;
}


// ==========================================================================
// SHAPE PARSING
// ==========================================================================

struct FeaShape {
    std::string type;
    double x = 0.0;
    double y = 0.0;
    double cx = 0.0;
    double cy = 0.0;
    double radius = 0.0;
    double width = 0.0;
    double height = 0.0;
    double flange = 0.0;
    double web = 0.0;
    std::vector<std::pair<double, double>> points;
};

static std::vector<FeaShape> parse_shapes_json(const char* json) {
    std::vector<FeaShape> shapes;
    if (!json || !json[0]) return shapes;

    std::string s(json);
    size_t pos = 0;
    pos = capi_skip_ws(s, pos);
    if (pos >= s.size() || s[pos] != '[') return shapes;
    ++pos; // skip outer [

    while (pos < s.size()) {
        pos = capi_skip_ws(s, pos);
        if (pos >= s.size() || s[pos] == ']') break;
        if (s[pos] == ',') { ++pos; continue; }
        if (s[pos] != '{') { ++pos; continue; }

        // Find matching closing brace
        size_t obj_start = pos;
        int depth = 1;
        ++pos;
        while (pos < s.size() && depth > 0) {
            if (s[pos] == '{') ++depth;
            else if (s[pos] == '}') --depth;
            ++pos;
        }
        std::string obj = s.substr(obj_start, pos - obj_start);

        FeaShape shape;

        size_t kp = capi_find_key(obj, "type", 1);
        if (kp != std::string::npos)
            shape.type = capi_parse_string(obj, kp);

        kp = capi_find_key(obj, "x", 1);
        if (kp != std::string::npos)
            shape.x = capi_parse_number(obj, kp);

        kp = capi_find_key(obj, "y", 1);
        if (kp != std::string::npos)
            shape.y = capi_parse_number(obj, kp);

        kp = capi_find_key(obj, "cx", 1);
        if (kp != std::string::npos)
            shape.cx = capi_parse_number(obj, kp);

        kp = capi_find_key(obj, "cy", 1);
        if (kp != std::string::npos)
            shape.cy = capi_parse_number(obj, kp);

        kp = capi_find_key(obj, "radius", 1);
        if (kp != std::string::npos)
            shape.radius = capi_parse_number(obj, kp);

        kp = capi_find_key(obj, "width", 1);
        if (kp != std::string::npos)
            shape.width = capi_parse_number(obj, kp);

        kp = capi_find_key(obj, "height", 1);
        if (kp != std::string::npos)
            shape.height = capi_parse_number(obj, kp);

        kp = capi_find_key(obj, "flange", 1);
        if (kp != std::string::npos)
            shape.flange = capi_parse_number(obj, kp);

        kp = capi_find_key(obj, "web", 1);
        if (kp != std::string::npos)
            shape.web = capi_parse_number(obj, kp);

        kp = capi_find_key(obj, "points", 1);
        if (kp != std::string::npos)
            shape.points = capi_parse_points(obj, kp);

        if (!shape.type.empty()) {
            shapes.push_back(shape);
        }
    }
    return shapes;
}


// ==========================================================================
// GEOMETRY UTILITIES
// ==========================================================================

// Point-in-polygon test using ray casting algorithm
static bool point_in_polygon(double px, double py,
                             const std::vector<std::pair<double, double>>& poly) {
    int n = static_cast<int>(poly.size());
    if (n < 3) return false;
    bool inside = false;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        double xi = poly[i].first, yi = poly[i].second;
        double xj = poly[j].first, yj = poly[j].second;
        if (((yi > py) != (yj > py)) &&
            (px < (xj - xi) * (py - yi) / (yj - yi) + xi)) {
            inside = !inside;
        }
    }
    return inside;
}

// Point inside an I-beam cross-section
// I-beam is defined by origin (x, y) with total width and height,
// flange thickness, and web thickness
static bool point_in_ibeam(double px, double py, double ox, double oy,
                           double width, double height,
                           double flange, double web) {
    // Normalize so origin is bottom-left of bounding box
    double x0 = ox, y0 = oy;
    double x1 = ox + width, y1 = oy + height;

    // Check if point is outside bounding box
    if (px < x0 || px > x1 || py < y0 || py > y1) return false;

    // Top flange: y >= y1 - flange
    if (py >= y1 - flange) return true;
    // Bottom flange: y <= y0 + flange
    if (py <= y0 + flange) return true;

    // Web: centered in x, thickness = web
    double web_left = ox + (width - web) / 2.0;
    double web_right = web_left + web;
    if (px >= web_left && px <= web_right) return true;

    return false;
}

// Point inside an L-bracket cross-section
// L-bracket is defined by origin (x, y) with total width and height,
// flange thickness (horizontal arm), and web thickness (vertical arm)
static bool point_in_lbracket(double px, double py, double ox, double oy,
                              double width, double height,
                              double flange, double web) {
    // Check if point is outside bounding box
    if (px < ox || px > ox + width || py < oy || py > oy + height) return false;

    // Horizontal flange (bottom): height = flange
    if (py <= oy + flange) return true;
    // Vertical web (left side): width = web
    if (px <= ox + web) return true;

    return false;
}

// Compute bounding box of all shapes
static void compute_bounding_box(const std::vector<FeaShape>& shapes,
                                 double& xmin, double& ymin,
                                 double& xmax, double& ymax) {
    xmin = 1e18; ymin = 1e18;
    xmax = -1e18; ymax = -1e18;

    for (const auto& shape : shapes) {
        if (shape.type == "rectangle") {
            xmin = std::min(xmin, shape.x);
            ymin = std::min(ymin, shape.y);
            xmax = std::max(xmax, shape.x + shape.width);
            ymax = std::max(ymax, shape.y + shape.height);
        } else if (shape.type == "circle") {
            xmin = std::min(xmin, shape.cx - shape.radius);
            ymin = std::min(ymin, shape.cy - shape.radius);
            xmax = std::max(xmax, shape.cx + shape.radius);
            ymax = std::max(ymax, shape.cy + shape.radius);
        } else if (shape.type == "polygon") {
            for (const auto& pt : shape.points) {
                xmin = std::min(xmin, pt.first);
                ymin = std::min(ymin, pt.second);
                xmax = std::max(xmax, pt.first);
                ymax = std::max(ymax, pt.second);
            }
        } else if (shape.type == "ibeam" || shape.type == "lbracket") {
            xmin = std::min(xmin, shape.x);
            ymin = std::min(ymin, shape.y);
            xmax = std::max(xmax, shape.x + shape.width);
            ymax = std::max(ymax, shape.y + shape.height);
        }
    }
}

// Test if a point is inside any of the shapes (union of shapes)
static bool point_inside_domain(double px, double py,
                                const std::vector<FeaShape>& shapes) {
    for (const auto& shape : shapes) {
        if (shape.type == "rectangle") {
            if (px >= shape.x && px <= shape.x + shape.width &&
                py >= shape.y && py <= shape.y + shape.height) {
                return true;
            }
        } else if (shape.type == "circle") {
            double dx = px - shape.cx;
            double dy = py - shape.cy;
            if (dx * dx + dy * dy <= shape.radius * shape.radius) {
                return true;
            }
        } else if (shape.type == "polygon") {
            if (point_in_polygon(px, py, shape.points)) {
                return true;
            }
        } else if (shape.type == "ibeam") {
            if (point_in_ibeam(px, py, shape.x, shape.y,
                               shape.width, shape.height,
                               shape.flange, shape.web)) {
                return true;
            }
        } else if (shape.type == "lbracket") {
            if (point_in_lbracket(px, py, shape.x, shape.y,
                                  shape.width, shape.height,
                                  shape.flange, shape.web)) {
                return true;
            }
        }
    }
    return false;
}


// ==========================================================================
// MESH GENERATION
// ==========================================================================

// Generate a structured quad mesh covering the bounding box of all shapes,
// then remove elements whose centroids fall outside the domain
static Mesh generate_domain_mesh(const std::vector<FeaShape>& shapes,
                                 int nx, int ny, int elem_type) {
    double xmin, ymin, xmax, ymax;
    compute_bounding_box(shapes, xmin, ymin, xmax, ymax);

    double Lx = xmax - xmin;
    double Ly = ymax - ymin;

    Mesh m;
    if (elem_type == 1) {
        m = mesh::generate_structured_quad8(Lx, Ly, nx, ny);
    } else {
        // Q4 for both Q4 and T3 (T3 is triangulated after culling)
        m = mesh::generate_structured_quad(Lx, Ly, nx, ny);
    }

    // Shift mesh to bounding box origin
    for (auto& node : m.nodes) {
        node.x += xmin;
        node.y += ymin;
    }

    // Remove elements outside the domain using centroid test (works for all types)
    {
        std::vector<bool> elem_active(m.quad_elements.size(), true);
        for (size_t e = 0; e < m.quad_elements.size(); ++e) {
            const auto& elem = m.quad_elements[e];
            double cx = 0.0, cy = 0.0;
            for (int i = 0; i < 4; ++i) {
                cx += m.nodes[elem[i]].x;
                cy += m.nodes[elem[i]].y;
            }
            cx /= 4.0; cy /= 4.0;
            elem_active[e] = point_inside_domain(cx, cy, shapes);
        }

        // Filter active elements
        std::vector<std::array<int, 4>> active_quads;
        for (size_t e = 0; e < m.quad_elements.size(); ++e) {
            if (elem_active[e]) {
                active_quads.push_back(m.quad_elements[e]);
            }
        }
        m.quad_elements = active_quads;
    }

    // For Q8, also filter Q8 elements
    if (elem_type == 1 && !m.quad8_elements.empty()) {
        std::vector<bool> elem_active(m.quad8_elements.size(), true);
        for (size_t e = 0; e < m.quad8_elements.size(); ++e) {
            const auto& elem = m.quad8_elements[e];
            double cx = 0.0, cy = 0.0;
            for (int i = 0; i < 4; ++i) {
                cx += m.nodes[elem[i]].x;
                cy += m.nodes[elem[i]].y;
            }
            cx /= 4.0; cy /= 4.0;
            elem_active[e] = point_inside_domain(cx, cy, shapes);
        }

        std::vector<std::array<int, 8>> active_q8s;
        for (size_t e = 0; e < m.quad8_elements.size(); ++e) {
            if (elem_active[e]) {
                active_q8s.push_back(m.quad8_elements[e]);
            }
        }
        m.quad8_elements = active_q8s;
    }

    // Renumber nodes: find which nodes are still referenced
    std::vector<bool> node_used(m.nodes.size(), false);
    for (const auto& elem : m.quad_elements) {
        for (int n : elem) node_used[n] = true;
    }
    for (const auto& elem : m.quad8_elements) {
        for (int n : elem) node_used[n] = true;
    }

    // Create mapping from old to new indices
    std::vector<int> node_map(m.nodes.size(), -1);
    std::vector<Node> new_nodes;
    for (size_t i = 0; i < m.nodes.size(); ++i) {
        if (node_used[i]) {
            node_map[i] = static_cast<int>(new_nodes.size());
            new_nodes.push_back(m.nodes[i]);
        }
    }
    m.nodes = new_nodes;

    // Update element connectivity
    for (auto& elem : m.quad_elements) {
        for (int& n : elem) n = node_map[n];
    }
    for (auto& elem : m.quad8_elements) {
        for (int& n : elem) n = node_map[n];
    }

    // For T3, split each Q4 into 2 triangles AFTER domain culling
    if (elem_type == 2) {
        for (const auto& q : m.quad_elements) {
            m.tri_elements.push_back({q[0], q[1], q[2]});
            m.tri_elements.push_back({q[0], q[2], q[3]});
        }
        m.quad_elements.clear();
    }

    return m;
}


// ==========================================================================
// MESH JSON OUTPUT
// ==========================================================================

static void capi_write_mesh_json(const std::string& filepath, const Mesh& m) {
    std::ofstream f(filepath);
    f << std::fixed << std::setprecision(6);
    f << "{\n";

    // Nodes
    f << "  \"nodes\": [\n";
    for (int i = 0; i < m.num_nodes(); ++i) {
        f << "    {\"x\": " << m.nodes[i].x
          << ", \"y\": " << m.nodes[i].y << "}";
        if (i < m.num_nodes() - 1) f << ",";
        f << "\n";
    }
    f << "  ],\n";

    // Elements (Q4)
    if (m.num_quads() > 0) {
        f << "  \"elements\": [\n";
        for (int i = 0; i < m.num_quads(); ++i) {
            f << "    {\"type\": \"Q4\", \"nodes\": ["
              << m.quad_elements[i][0] << ", "
              << m.quad_elements[i][1] << ", "
              << m.quad_elements[i][2] << ", "
              << m.quad_elements[i][3] << "]}";
            if (i < m.num_quads() - 1) f << ",";
            f << "\n";
        }
        f << "  ],\n";
    }

    // Elements (Q8)
    if (m.num_quad8s() > 0) {
        f << "  \"elements\": [\n";
        for (int i = 0; i < m.num_quad8s(); ++i) {
            f << "    {\"type\": \"Q8\", \"nodes\": [";
            for (int j = 0; j < 8; ++j) {
                f << m.quad8_elements[i][j];
                if (j < 7) f << ", ";
            }
            f << "]}";
            if (i < m.num_quad8s() - 1) f << ",";
            f << "\n";
        }
        f << "  ],\n";
    }

    // Elements (T3) -- CAPI-3 fix
    if (m.num_tris() > 0) {
        f << "  \"elements\": [\n";
        for (int i = 0; i < m.num_tris(); ++i) {
            f << "    {\"type\": \"T3\", \"nodes\": ["
              << m.tri_elements[i][0] << ", "
              << m.tri_elements[i][1] << ", "
              << m.tri_elements[i][2] << "]}";
            if (i < m.num_tris() - 1) f << ",";
            f << "\n";
        }
        f << "  ],\n";
    }

    // Dirichlet BCs (empty for generated meshes)
    f << "  \"dirichlet\": [],\n";
    f << "  \"neumann\": [],\n";

    // Material (default steel)
    f << "  \"material\": {\n";
    f << "    \"E\": 200000000000.0,\n";
    f << "    \"nu\": 0.3,\n";
    f << "    \"rho\": 7800.0,\n";
    f << "    \"t\": 0.01\n";
    f << "  },\n";

    f << "  \"plane\": \"stress\",\n";
    f << "  \"num_nodes\": " << m.num_nodes() << ",\n";
    f << "  \"num_elements\": "
      << (m.num_quads() + m.num_quad8s() + m.num_tris()) << ",\n";
    f << "  \"num_dofs\": " << m.num_dofs() << "\n";
    f << "}\n";
}


// ==========================================================================
// MESH JSON PARSER (for fea_solve_c)
// ==========================================================================

static Mesh parse_mesh_json(const std::string& s) {
    Mesh m;
    size_t pos = 0;

    // Helper: find the top-level value for a key
    auto find_top_key = [&](const std::string& key) -> size_t {
        return capi_find_key(s, key, 0);
    };

    // Parse nodes
    size_t kp = find_top_key("nodes");
    if (kp != std::string::npos) {
        pos = capi_skip_ws(s, kp);
        if (pos < s.size() && s[pos] == '[') {
            ++pos; // skip [
            while (pos < s.size()) {
                pos = capi_skip_ws(s, pos);
                if (pos >= s.size() || s[pos] == ']') break;
                if (s[pos] == ',') { ++pos; continue; }
                if (s[pos] == '{') {
                    Node node;
                    // Find "x" key
                    size_t xk = capi_find_key(s, "x", pos);
                    if (xk != std::string::npos)
                        node.x = capi_parse_number(s, xk);
                    // Find "y" key
                    size_t yk = capi_find_key(s, "y", pos);
                    if (yk != std::string::npos)
                        node.y = capi_parse_number(s, yk);
                    m.nodes.push_back(node);
                    // Skip past this object
                    int depth = 1;
                    ++pos;
                    while (pos < s.size() && depth > 0) {
                        if (s[pos] == '{') ++depth;
                        else if (s[pos] == '}') --depth;
                        ++pos;
                    }
                } else {
                    ++pos;
                }
            }
        }
    }

    // Parse elements
    kp = find_top_key("elements");
    if (kp != std::string::npos) {
        pos = capi_skip_ws(s, kp);
        if (pos < s.size() && s[pos] == '[') {
            ++pos; // skip [
            while (pos < s.size()) {
                pos = capi_skip_ws(s, pos);
                if (pos >= s.size() || s[pos] == ']') break;
                if (s[pos] == ',') { ++pos; continue; }
                if (s[pos] == '{') {
                    // Extract element object
                    size_t obj_start = pos;
                    int depth = 1;
                    ++pos;
                    while (pos < s.size() && depth > 0) {
                        if (s[pos] == '{') ++depth;
                        else if (s[pos] == '}') --depth;
                        ++pos;
                    }
                    std::string obj = s.substr(obj_start, pos - obj_start);

                    // Parse type
                    std::string elem_type_str;
                    size_t tk = capi_find_key(obj, "type", 1);
                    if (tk != std::string::npos)
                        elem_type_str = capi_parse_string(obj, tk);

                    // Parse nodes array
                    std::vector<int> nodes;
                    size_t nk = capi_find_key(obj, "nodes", 1);
                    if (nk != std::string::npos) {
                        size_t npos = capi_skip_ws(obj, nk);
                        if (npos < obj.size() && obj[npos] == '[') {
                            ++npos;
                            while (npos < obj.size()) {
                                npos = capi_skip_ws(obj, npos);
                                if (npos >= obj.size() || obj[npos] == ']') break;
                                if (obj[npos] == ',') { ++npos; continue; }
                                nodes.push_back(capi_parse_int(obj, npos));
                            }
                        }
                    }

                    if (elem_type_str == "Q4" && nodes.size() >= 4) {
                        m.quad_elements.push_back(
                            {nodes[0], nodes[1], nodes[2], nodes[3]});
                    } else if (elem_type_str == "Q8" && nodes.size() >= 8) {
                        m.quad8_elements.push_back(
                            {nodes[0], nodes[1], nodes[2], nodes[3],
                             nodes[4], nodes[5], nodes[6], nodes[7]});
                    } else if (elem_type_str == "T3" && nodes.size() >= 3) {
                        m.tri_elements.push_back(
                            {nodes[0], nodes[1], nodes[2]});
                    }
                } else {
                    ++pos;
                }
            }
        }
    }

    // Parse dirichlet BCs
    kp = find_top_key("dirichlet");
    if (kp != std::string::npos) {
        pos = capi_skip_ws(s, kp);
        if (pos < s.size() && s[pos] == '[') {
            ++pos;
            while (pos < s.size()) {
                pos = capi_skip_ws(s, pos);
                if (pos >= s.size() || s[pos] == ']') break;
                if (s[pos] == ',') { ++pos; continue; }
                if (s[pos] == '{') {
                    DirichletBC bc;
                    size_t nk = capi_find_key(s, "node", pos);
                    if (nk != std::string::npos)
                        bc.node = capi_parse_int(s, nk);
                    size_t dk = capi_find_key(s, "dof", pos);
                    if (dk != std::string::npos)
                        bc.dof = capi_parse_int(s, dk);
                    size_t vk = capi_find_key(s, "value", pos);
                    if (vk != std::string::npos)
                        bc.value = capi_parse_number(s, vk);
                    m.dirichlet.push_back(bc);
                    // Skip past object
                    int depth = 1;
                    ++pos;
                    while (pos < s.size() && depth > 0) {
                        if (s[pos] == '{') ++depth;
                        else if (s[pos] == '}') --depth;
                        ++pos;
                    }
                } else {
                    ++pos;
                }
            }
        }
    }

    // Parse neumann BCs
    kp = find_top_key("neumann");
    if (kp != std::string::npos) {
        pos = capi_skip_ws(s, kp);
        if (pos < s.size() && s[pos] == '[') {
            ++pos;
            while (pos < s.size()) {
                pos = capi_skip_ws(s, pos);
                if (pos >= s.size() || s[pos] == ']') break;
                if (s[pos] == ',') { ++pos; continue; }
                if (s[pos] == '{') {
                    NeumannBC bc;
                    size_t nk = capi_find_key(s, "node", pos);
                    if (nk != std::string::npos)
                        bc.node = capi_parse_int(s, nk);
                    size_t dk = capi_find_key(s, "dof", pos);
                    if (dk != std::string::npos)
                        bc.dof = capi_parse_int(s, dk);
                    size_t vk = capi_find_key(s, "value", pos);
                    if (vk != std::string::npos)
                        bc.value = capi_parse_number(s, vk);
                    m.neumann.push_back(bc);
                    int depth = 1;
                    ++pos;
                    while (pos < s.size() && depth > 0) {
                        if (s[pos] == '{') ++depth;
                        else if (s[pos] == '}') --depth;
                        ++pos;
                    }
                } else {
                    ++pos;
                }
            }
        }
    }

    // Parse material
    kp = find_top_key("material");
    if (kp != std::string::npos) {
        pos = capi_skip_ws(s, kp);
        if (pos < s.size() && s[pos] == '{') {
            size_t obj_start = pos;
            int depth = 1;
            ++pos;
            while (pos < s.size() && depth > 0) {
                if (s[pos] == '{') ++depth;
                else if (s[pos] == '}') --depth;
                ++pos;
            }
            std::string mat_obj = s.substr(obj_start, pos - obj_start);
            size_t mk;

            mk = capi_find_key(mat_obj, "E", 1);
            if (mk != std::string::npos)
                m.mat.E = capi_parse_number(mat_obj, mk);

            mk = capi_find_key(mat_obj, "nu", 1);
            if (mk != std::string::npos)
                m.mat.nu = capi_parse_number(mat_obj, mk);

            mk = capi_find_key(mat_obj, "rho", 1);
            if (mk != std::string::npos)
                m.mat.rho = capi_parse_number(mat_obj, mk);

            mk = capi_find_key(mat_obj, "t", 1);
            if (mk != std::string::npos)
                m.mat.t = capi_parse_number(mat_obj, mk);

            mk = capi_find_key(mat_obj, "alpha", 1);
            if (mk != std::string::npos)
                m.mat.alpha = capi_parse_number(mat_obj, mk);
        }
    }

    return m;
}


// ==========================================================================
// CONFIG JSON PARSER
// ==========================================================================

struct SolverConfig {
    PlaneType plane = PlaneType::STRESS;
    bool use_cg = false;
    double cg_tolerance = 1e-10;
    int cg_max_iterations = 10000;
    IntegrationType integration = IntegrationType::FULL;
};

static SolverConfig parse_config_json(const std::string& s) {
    SolverConfig cfg;
    size_t pos = 0;

    // Parse "plane"
    size_t kp = capi_find_key(s, "plane", 0);
    if (kp != std::string::npos) {
        std::string plane_str = capi_parse_string(s, kp);
        if (plane_str == "strain") {
            cfg.plane = PlaneType::STRAIN;
        } else {
            cfg.plane = PlaneType::STRESS;
        }
    }

    // Parse "solver"
    kp = capi_find_key(s, "solver", 0);
    if (kp != std::string::npos) {
        std::string solver_str = capi_parse_string(s, kp);
        if (solver_str == "cg") {
            cfg.use_cg = true;
        }
    }

    // Parse "cg_tolerance"
    kp = capi_find_key(s, "cg_tolerance", 0);
    if (kp != std::string::npos) {
        cfg.cg_tolerance = capi_parse_number(s, kp);
    }

    // Parse "cg_max_iterations"
    kp = capi_find_key(s, "cg_max_iterations", 0);
    if (kp != std::string::npos) {
        cfg.cg_max_iterations = capi_parse_int(s, kp);
    }

    // Parse "integration"
    kp = capi_find_key(s, "integration", 0);
    if (kp != std::string::npos) {
        std::string int_str = capi_parse_string(s, kp);
        if (int_str == "sri") {
            cfg.integration = IntegrationType::SRI;
        } else if (int_str == "bbar") {
            cfg.integration = IntegrationType::BBAR;
        } else {
            cfg.integration = IntegrationType::FULL;
        }
    }

    // Parse "material" (override defaults if provided)
    // Material is parsed separately in parse_mesh_json;
    // config_json material overrides mesh_json material
    (void)pos;
    return cfg;
}


// ==========================================================================
// RESULTS JSON OUTPUT
// ==========================================================================

static void capi_write_displacement_json(const std::string& filepath,
                                    const Mesh& m,
                                    const std::vector<double>& u) {
    std::ofstream f(filepath);
    f << std::scientific << std::setprecision(9);
    f << "{\n";
    f << "  \"displacements\": [\n";
    for (int i = 0; i < m.num_nodes(); ++i) {
        f << "    {\"ux\": " << u[dof_index(i, 0)]
          << ", \"uy\": " << u[dof_index(i, 1)] << "}";
        if (i < m.num_nodes() - 1) f << ",";
        f << "\n";
    }
    f << "  ],\n";

    double max_disp = 0.0;
    for (int i = 0; i < m.num_nodes(); ++i) {
        double ux = u[dof_index(i, 0)];
        double uy = u[dof_index(i, 1)];
        double d = std::sqrt(ux * ux + uy * uy);
        if (d > max_disp) max_disp = d;
    }

    f << "  \"max_displacement\": " << max_disp << ",\n";
    f << "  \"num_nodes\": " << m.num_nodes() << "\n";
    f << "}\n";
}

static void capi_write_stress_json(const std::string& filepath,
                               const Mesh& m,
                               const std::vector<postprocess::ElementStress>& stresses) {
    std::ofstream f(filepath);
    f << std::scientific << std::setprecision(9);
    f << "{\n";
    f << "  \"stresses\": [\n";
    for (size_t i = 0; i < stresses.size(); ++i) {
        f << "    {\"sigma_xx\": " << stresses[i].sigma_xx
          << ", \"sigma_yy\": " << stresses[i].sigma_yy
          << ", \"sigma_xy\": " << stresses[i].sigma_xy
          << ", \"von_mises\": " << stresses[i].von_mises
          << ", \"sigma_1\": " << stresses[i].sigma_1
          << ", \"sigma_2\": " << stresses[i].sigma_2 << "}";
        if (i < stresses.size() - 1) f << ",";
        f << "\n";
    }
    f << "  ],\n";

    double max_stress = 0.0;
    for (const auto& s : stresses) {
        if (s.von_mises > max_stress) max_stress = s.von_mises;
    }

    f << "  \"max_stress\": " << max_stress << ",\n";
    f << "  \"num_elements\": " << stresses.size() << "\n";
    f << "}\n";
}

static void capi_write_meta_json(const std::string& filepath,
                             const Mesh& m,
                             const std::vector<double>& u,
                             const std::vector<postprocess::ElementStress>& stresses,
                             double solve_time_ms,
                             int cg_iterations,
                             bool cg_converged) {
    double max_disp = 0.0;
    for (int i = 0; i < m.num_nodes(); ++i) {
        double ux = u[dof_index(i, 0)];
        double uy = u[dof_index(i, 1)];
        double d = std::sqrt(ux * ux + uy * uy);
        if (d > max_disp) max_disp = d;
    }

    double max_stress = 0.0;
    for (const auto& s : stresses) {
        if (s.von_mises > max_stress) max_stress = s.von_mises;
    }

    std::ofstream f(filepath);
    f << std::scientific << std::setprecision(9);
    f << "{\n";
    f << "  \"case\": \"custom\",\n";
    f << "  \"num_nodes\": " << m.num_nodes() << ",\n";
    f << "  \"num_elements\": "
      << (m.num_quads() + m.num_quad8s() + m.num_tris()) << ",\n";
    f << "  \"num_dofs\": " << m.num_dofs() << ",\n";
    f << "  \"max_displacement\": " << max_disp << ",\n";
    f << "  \"max_stress\": " << max_stress << ",\n";
    f << "  \"solve_time_ms\": " << solve_time_ms << ",\n";
    f << "  \"cg_iterations\": " << cg_iterations << ",\n";
    f << "  \"cg_converged\": " << (cg_converged ? "true" : "false") << "\n";
    f << "}\n";
}


// ==========================================================================
// C API IMPLEMENTATIONS
// ==========================================================================

extern "C" {

int fea_generate_mesh_c(
    const char* shapes_json,
    int nx, int ny,
    int elem_type,
    const char* output_dir
) {
    try {
        if (!shapes_json || nx <= 0 || ny <= 0 || !output_dir) {
            return 1;  // invalid arguments
        }
        if (nx > 1000 || ny > 1000) {
            return 1;  // mesh density too large
        }

        // Pin 2D dimension (CAPI-6 fix)
        set_dimension(2);
        DOF_PER_NODE = 2;

        // Parse shape primitives from JSON
        auto shapes = parse_shapes_json(shapes_json);
        if (shapes.empty()) {
            return 2;  // no shapes parsed
        }

        // Create output directory
        std::string out_dir(output_dir);
        std::filesystem::create_directories(out_dir);

        // Generate mesh covering bounding box, then cut to domain
        auto m = generate_domain_mesh(shapes, nx, ny, elem_type);

        // Write mesh.json
        capi_write_mesh_json(out_dir + "/mesh.json", m);

        return 0;
    } catch (...) {
        return -1;  // unexpected error
    }
}

int fea_solve_c(
    const char* mesh_json,
    const char* config_json,
    const char* output_dir
) {
    try {
        if (!mesh_json || !output_dir) {
            return 1;  // invalid arguments
        }

        // Set 2D defaults
        set_dimension(2);
        g_nx = 0;
        g_ny = 0;
        g_case = CaseType::CANTILEVER;  // placeholder for custom

        // Parse mesh JSON
        std::string mesh_str(mesh_json);
        auto m = parse_mesh_json(mesh_str);

        if (m.num_nodes() == 0) {
            return 2;  // no nodes parsed
        }
        if (m.num_quads() == 0 && m.num_quad8s() == 0 && m.num_tris() == 0) {
            return 3;  // no elements parsed
        }

        // Validate boundary condition node indices (CAPI-1/2 fix)
        for (const auto& bc : m.dirichlet) {
            if (bc.node < 0 || bc.node >= m.num_nodes() || bc.dof < 0 || bc.dof >= DOF_PER_NODE)
                return -1;
        }
        for (const auto& bc : m.neumann) {
            if (bc.node < 0 || bc.node >= m.num_nodes() || bc.dof < 0 || bc.dof >= DOF_PER_NODE)
                return -1;
        }

        // Parse solver config (if provided)
        SolverConfig cfg;
        if (config_json && config_json[0]) {
            std::string config_str(config_json);
            cfg = parse_config_json(config_str);

            // Override integration type (applied globally for the solve)
            g_integration = cfg.integration;
        }

        // Apply plane type
        m.plane = cfg.plane;

        // Create output directory
        std::string out_dir(output_dir);
        std::filesystem::create_directories(out_dir);

        // Run solve
        auto result = fea::solve(m, cfg.use_cg);

        // Write results
        capi_write_displacement_json(out_dir + "/displacement.json",
                                m, result.displacement);
        capi_write_stress_json(out_dir + "/stress.json",
                          m, result.stresses);
        capi_write_meta_json(out_dir + "/meta.json",
                        m, result.displacement, result.stresses,
                        result.solve_time_ms,
                        result.cg_iterations,
                        result.cg_converged);

        return 0;
    } catch (...) {
        return -1;  // unexpected error
    }
}

}  // extern "C"
