#pragma once
#include <vector>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <stdexcept>
#include <fstream>
#include <sstream>

// ==========================================================================
// 2D FINITE ELEMENT ANALYSIS -- Type definitions and global config
// ==========================================================================

// Mesh dimensions (runtime variables, set by entry point)
inline int g_nx = 20;
inline int g_ny = 20;

// Degrees of freedom per node (2D: u_x, u_y)
constexpr int DOF_PER_NODE = 2;

// Element type selection
enum class ElementType { BAR, Q4, T3 };

// Analysis type
enum class AnalysisType { STATIC, BUCKLING };

// Simulation case type (drives behavior like LBM's g_case)
enum class CaseType { CANTILEVER, MICHELL, COOK, LBRACKET, PATCH, PLATE_HOLE, THERMAL_CYLINDER };

// Plane stress vs plane strain
enum class PlaneType { STRESS, STRAIN };

// Global config (inline globals like LBM-2D)
inline CaseType g_case = CaseType::CANTILEVER;
inline AnalysisType g_analysis = AnalysisType::STATIC;
inline PlaneType g_plane = PlaneType::STRESS;

// ------------------------------------------------------------------
// Material properties
// ------------------------------------------------------------------
struct Material {
    double E;       // Young's modulus (Pa)
    double nu;      // Poisson's ratio
    double rho;     // Density (kg/m3)
    double t;       // Thickness (m) for 2D
    double alpha = 0.0; // Thermal expansion coefficient (1/K)

    // Default: structural steel
    static Material steel() {
        return { 200.0e9, 0.3, 7800.0, 0.01, 12.0e-6 };
    }

    // Aluminum
    static Material aluminum() {
        return { 70.0e9, 0.33, 2700.0, 0.01, 23.0e-6 };
    }

    // Create D matrix (constitutive law) for plane stress or strain
    std::array<std::array<double, 3>, 3> d_matrix(PlaneType plane) const {
        std::array<std::array<double, 3>, 3> D{};
        double factor;
        if (plane == PlaneType::STRESS) {
            factor = E / (1.0 - nu * nu);
            D[0][0] = factor;
            D[0][1] = factor * nu;
            D[0][2] = 0.0;
            D[1][0] = factor * nu;
            D[1][1] = factor;
            D[1][2] = 0.0;
            D[2][0] = 0.0;
            D[2][1] = 0.0;
            D[2][2] = factor * (1.0 - nu) / 2.0;
        } else {
            factor = E / ((1.0 + nu) * (1.0 - 2.0 * nu));
            D[0][0] = factor * (1.0 - nu);
            D[0][1] = factor * nu;
            D[0][2] = 0.0;
            D[1][0] = factor * nu;
            D[1][1] = factor * (1.0 - nu);
            D[1][2] = 0.0;
            D[2][0] = 0.0;
            D[2][1] = 0.0;
            D[2][2] = factor * (1.0 - 2.0 * nu) / 2.0;
        }
        return D;
    }
};

// ------------------------------------------------------------------
// Node with coordinates
// ------------------------------------------------------------------
struct Node {
    double x, y;
};

// ------------------------------------------------------------------
// Boundary conditions
// ------------------------------------------------------------------
struct DirichletBC {
    int node;       // node index
    int dof;        // 0 = x, 1 = y
    double value;   // prescribed displacement (usually 0.0)
};

struct NeumannBC {
    int node;       // node index
    int dof;        // 0 = x, 1 = y
    double value;   // applied force (N)
};

// ------------------------------------------------------------------
// Mesh structure (holds all data for the finite element model)
// ------------------------------------------------------------------
struct Mesh {
    std::vector<Node> nodes;
    std::vector<std::array<int, 4>> quad_elements;    // Q4 connectivity (CCW)
    std::vector<std::array<int, 3>> tri_elements;     // T3 connectivity (CCW)
    std::vector<std::array<int, 2>> bar_elements;     // Bar connectivity
    std::vector<double> bar_areas;                     // Cross-sectional area per bar
    std::vector<DirichletBC> dirichlet;
    std::vector<NeumannBC> neumann;
    std::vector<double> temperature;                  // Nodal temperatures (K or C)
    double T_ref = 0.0;                                // Reference temperature
    Material mat;
    PlaneType plane = PlaneType::STRESS;

    int num_nodes() const { return static_cast<int>(nodes.size()); }
    int num_quads() const { return static_cast<int>(quad_elements.size()); }
    int num_tris() const { return static_cast<int>(tri_elements.size()); }
    int num_bars() const { return static_cast<int>(bar_elements.size()); }
    int num_dofs() const { return num_nodes() * DOF_PER_NODE; }
};

// ------------------------------------------------------------------
// Helper: DOF index for node i, component d (0=x, 1=y)
// ------------------------------------------------------------------
inline int dof_index(int node, int dof) {
    return node * DOF_PER_NODE + dof;
}

// ------------------------------------------------------------------
// JSON parsing helpers (minimal, no external library)
// ------------------------------------------------------------------
namespace json {

// Skip whitespace in JSON string
inline void skip_ws(const std::string& s, size_t& i) {
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r'))
        ++i;
}

// Parse a JSON number (double)
inline double parse_number(const std::string& s, size_t& i) {
    skip_ws(s, i);
    size_t start = i;
    if (i < s.size() && s[i] == '-') ++i;
    while (i < s.size() && s[i] >= '0' && s[i] <= '9') ++i;
    if (i < s.size() && s[i] == '.') {
        ++i;
        while (i < s.size() && s[i] >= '0' && s[i] <= '9') ++i;
    }
    if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
        ++i;
        if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
        while (i < s.size() && s[i] >= '0' && s[i] <= '9') ++i;
    }
    return std::stod(s.substr(start, i - start));
}

// Parse a JSON integer
inline int parse_int(const std::string& s, size_t& i) {
    return static_cast<int>(parse_number(s, i));
}

// Skip a JSON value (number, string, array, object)
inline void skip_value(const std::string& s, size_t& i) {
    skip_ws(s, i);
    if (i >= s.size()) return;
    if (s[i] == '"') {
        ++i;
        while (i < s.size() && s[i] != '"') {
            if (s[i] == '\\') ++i;
            ++i;
        }
        if (i < s.size()) ++i;
    } else if (s[i] == '[') {
        ++i;
        while (i < s.size() && s[i] != ']') {
            if (s[i] == ',') { ++i; continue; }
            skip_value(s, i);
        }
        if (i < s.size()) ++i;
    } else if (s[i] == '{') {
        ++i;
        while (i < s.size() && s[i] != '}') {
            if (s[i] == ',') { ++i; continue; }
            skip_ws(s, i);
            skip_value(s, i);  // key
            skip_ws(s, i);
            if (i < s.size() && s[i] == ':') ++i;
            skip_value(s, i);  // value
        }
        if (i < s.size()) ++i;
    } else {
        // number or true/false/null
        while (i < s.size() && s[i] != ',' && s[i] != '}' && s[i] != ']' &&
               s[i] != ' ' && s[i] != '\t' && s[i] != '\n' && s[i] != '\r')
            ++i;
    }
}

// Read entire file to string
inline std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) throw std::runtime_error("Cannot open file: " + path);
    return std::string((std::istreambuf_iterator<char>(f)),
                       std::istreambuf_iterator<char>());
}

}  // namespace json
