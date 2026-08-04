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
// FINITE ELEMENT ANALYSIS -- Type definitions and global config
// ==========================================================================

// Mesh dimensions (runtime variables, set by entry point)
inline int g_nx = 20;
inline int g_ny = 20;

// Spatial dimension (2 or 3, set by entry point)
inline int g_dim = 2;

// Degrees of freedom per node (2D: u_x, u_y; 3D: u_x, u_y, u_z)
inline int DOF_PER_NODE = 2;

// Set dimension and DOFs atomically
inline void set_dimension(int dim) {
    g_dim = dim;
    DOF_PER_NODE = dim;
}

// Element type selection
enum class ElementType { BAR, Q4, T3 };

// Analysis type
enum class AnalysisType { STATIC, BUCKLING };

// Simulation case type (drives behavior like LBM's g_case)
enum class CaseType { CANTILEVER, MICHELL, COOK, LBRACKET, PATCH, PLATE_HOLE, THERMAL_CYLINDER, CANTILEVER_3D, PLATE_HOLE_3D, LAME_3D };

// Plane stress vs plane strain
enum class PlaneType { STRESS, STRAIN };

// Q4 integration type (for shear locking mitigation)
enum class IntegrationType { FULL, SRI, BBAR };

// Convert CaseType to string for JSON output
inline const char* case_name(CaseType c) {
    switch (c) {
        case CaseType::CANTILEVER:      return "cantilever";
        case CaseType::MICHELL:          return "michell";
        case CaseType::COOK:             return "cook";
        case CaseType::LBRACKET:         return "lbracket";
        case CaseType::PATCH:            return "patch";
        case CaseType::PLATE_HOLE:       return "plate_hole";
        case CaseType::THERMAL_CYLINDER: return "thermal_cylinder";
        case CaseType::CANTILEVER_3D:    return "cantilever_3d";
        case CaseType::PLATE_HOLE_3D:    return "plate_hole_3d";
        case CaseType::LAME_3D:          return "lame_3d";
    }
    return "unknown";
}

// Global config (inline globals like LBM-2D)
inline CaseType g_case = CaseType::CANTILEVER;
inline AnalysisType g_analysis = AnalysisType::STATIC;
inline PlaneType g_plane = PlaneType::STRESS;
inline IntegrationType g_integration = IntegrationType::FULL;

// Convert IntegrationType to string
inline const char* integration_name(IntegrationType t) {
    switch (t) {
        case IntegrationType::FULL: return "Full (2x2)";
        case IntegrationType::SRI:  return "SRI";
        case IntegrationType::BBAR: return "B-Bar";
    }
    return "Unknown";
}

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

    // Create 3D D matrix (full isotropic constitutive law, no plane assumption)
    std::array<std::array<double, 6>, 6> d_matrix_3d() const {
        double factor = E / ((1.0 + nu) * (1.0 - 2.0 * nu));
        std::array<std::array<double, 6>, 6> D{};
        D[0][0] = factor * (1.0 - nu);  D[0][1] = factor * nu;        D[0][2] = factor * nu;
        D[1][0] = factor * nu;          D[1][1] = factor * (1.0 - nu); D[1][2] = factor * nu;
        D[2][0] = factor * nu;          D[2][1] = factor * nu;          D[2][2] = factor * (1.0 - nu);
        D[3][3] = factor * (1.0 - 2.0 * nu) / 2.0;
        D[4][4] = factor * (1.0 - 2.0 * nu) / 2.0;
        D[5][5] = factor * (1.0 - 2.0 * nu) / 2.0;
        return D;
    }
};

// ------------------------------------------------------------------
// Node with coordinates
// ------------------------------------------------------------------
struct Node {
    double x, y, z = 0.0;
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
    std::vector<std::array<int, 8>> quad8_elements;   // Q8 connectivity (CCW, corners+midside)
    std::vector<std::array<int, 3>> tri_elements;     // T3 connectivity (CCW)
    std::vector<std::array<int, 2>> bar_elements;     // Bar connectivity
    std::vector<double> bar_areas;                     // Cross-sectional area per bar
    std::vector<std::array<int, 8>> hex_elements;     // H8 connectivity
    std::vector<std::array<int, 4>> tet_elements;     // T4 connectivity
    std::vector<DirichletBC> dirichlet;
    std::vector<NeumannBC> neumann;
    std::vector<double> temperature;                  // Nodal temperatures (K or C)
    double T_ref = 0.0;                                // Reference temperature
    Material mat;
    PlaneType plane = PlaneType::STRESS;

    int num_nodes() const { return static_cast<int>(nodes.size()); }
    int num_quads() const { return static_cast<int>(quad_elements.size()); }
    int num_quad8s() const { return static_cast<int>(quad8_elements.size()); }
    int num_tris() const { return static_cast<int>(tri_elements.size()); }
    int num_bars() const { return static_cast<int>(bar_elements.size()); }
    int num_hexes() const { return static_cast<int>(hex_elements.size()); }
    int num_tets() const { return static_cast<int>(tet_elements.size()); }
    int num_dofs() const { return num_nodes() * DOF_PER_NODE; }
    bool is_3d() const { return g_dim == 3; }
};

// ------------------------------------------------------------------
// Helper: DOF index for node i, component d (0=x, 1=y, 2=z)
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
