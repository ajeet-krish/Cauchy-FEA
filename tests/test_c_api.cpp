// ==========================================================================
// UNIT TESTS FOR THE CAUCHY C FFI LAYER (Phase 1)
//
// Exercises fea_generate_mesh_c() and fea_solve_c() through the same
// extern "C" boundary that the Tauri desktop app uses. Every test writes
// into its own /tmp/cauchy_test_* directory and removes it afterwards.
// ==========================================================================

#include <gtest/gtest.h>

#include "fea_solver_c_api.h"
#include "fea_types.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace fs = std::filesystem;

// ------------------------------------------------------------------
// Temporary directory helper (RAII, cleans up on destruction)
// ------------------------------------------------------------------
class TempDir {
public:
    explicit TempDir(const std::string& tag) {
        static int counter = 0;
        std::ostringstream oss;
        oss << "/tmp/cauchy_test_" << tag << "_"
            << static_cast<long>(::getpid()) << "_" << counter++;
        path_ = oss.str();
        std::error_code ec;
        fs::remove_all(path_, ec);
        fs::create_directories(path_, ec);
    }

    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path_, ec);
    }

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    const std::string& str() const { return path_; }
    const char* c_str() const { return path_.c_str(); }
    std::string file(const std::string& name) const { return path_ + "/" + name; }

private:
    std::string path_;
};

// ------------------------------------------------------------------
// Small JSON readers (only what the tests need)
// ------------------------------------------------------------------
std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f) return "";
    std::ostringstream oss;
    oss << f.rdbuf();
    return oss.str();
}

bool file_exists_nonempty(const std::string& path) {
    std::error_code ec;
    return fs::exists(path, ec) && fs::file_size(path, ec) > 0;
}

// Read a scalar number stored as "key": <number>
bool json_number(const std::string& text, const std::string& key, double& out) {
    const std::string needle = "\"" + key + "\":";
    size_t pos = text.find(needle);
    if (pos == std::string::npos) return false;
    pos += needle.size();
    return std::sscanf(text.c_str() + pos, " %lf", &out) == 1;
}

double json_number_or(const std::string& text, const std::string& key,
                      double fallback) {
    double v = fallback;
    json_number(text, key, v);
    return v;
}

// Braces and brackets balance -- a cheap structural validity check
bool json_is_balanced(const std::string& text) {
    int braces = 0;
    int brackets = 0;
    bool in_string = false;
    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (in_string) {
            if (c == '\\') { ++i; continue; }
            if (c == '"') in_string = false;
            continue;
        }
        if (c == '"') { in_string = true; continue; }
        if (c == '{') ++braces;
        if (c == '}') --braces;
        if (c == '[') ++brackets;
        if (c == ']') --brackets;
        if (braces < 0 || brackets < 0) return false;
    }
    return braces == 0 && brackets == 0 && !in_string;
}

// Parse the top-level "nodes" array of a generated mesh.json
std::vector<std::pair<double, double>> parse_nodes(const std::string& text) {
    std::vector<std::pair<double, double>> nodes;
    size_t start = text.find("\"nodes\"");
    if (start == std::string::npos) return nodes;
    size_t end = text.find("\n  ],", start);
    if (end == std::string::npos) end = text.size();
    std::string block = text.substr(start, end - start);

    size_t pos = 0;
    const std::string marker = "{\"x\":";
    while ((pos = block.find(marker, pos)) != std::string::npos) {
        double x = 0.0;
        double y = 0.0;
        if (std::sscanf(block.c_str() + pos, "{\"x\": %lf, \"y\": %lf}", &x, &y) == 2) {
            nodes.push_back({x, y});
        }
        pos += marker.size();
    }
    return nodes;
}

// Parse Q4 connectivity out of a generated mesh.json
std::vector<std::array<int, 4>> parse_q4_elements(const std::string& text) {
    std::vector<std::array<int, 4>> elems;
    size_t pos = 0;
    const std::string marker = "{\"type\": \"Q4\", \"nodes\": [";
    while ((pos = text.find(marker, pos)) != std::string::npos) {
        std::array<int, 4> e{};
        if (std::sscanf(text.c_str() + pos + marker.size(), "%d, %d, %d, %d",
                        &e[0], &e[1], &e[2], &e[3]) == 4) {
            elems.push_back(e);
        }
        pos += marker.size();
    }
    return elems;
}

int count_occurrences(const std::string& text, const std::string& needle) {
    int n = 0;
    size_t pos = 0;
    while ((pos = text.find(needle, pos)) != std::string::npos) {
        ++n;
        pos += needle.size();
    }
    return n;
}

// ------------------------------------------------------------------
// Boundary condition injection
//
// fea_generate_mesh_c() always writes empty "dirichlet"/"neumann"
// arrays. A solvable model needs both, so the tests splice them in --
// this is exactly what the desktop app has to do.
// ------------------------------------------------------------------
std::string inject_bcs(const std::string& mesh_json,
                       const std::string& dirichlet_items,
                       const std::string& neumann_items) {
    std::string out = mesh_json;
    size_t d = out.find("\"dirichlet\": []");
    if (d != std::string::npos) {
        out.replace(d, std::string("\"dirichlet\": []").size(),
                    "\"dirichlet\": [" + dirichlet_items + "]");
    }
    size_t n = out.find("\"neumann\": []");
    if (n != std::string::npos) {
        out.replace(n, std::string("\"neumann\": []").size(),
                    "\"neumann\": [" + neumann_items + "]");
    }
    return out;
}

// Build a clamped-left / tip-loaded-right cantilever from a rectangle mesh
std::string build_cantilever_mesh_json(const std::string& mesh_json,
                                       double x_left, double x_right,
                                       double total_load) {
    auto nodes = parse_nodes(mesh_json);
    const double tol = 1e-9;

    std::ostringstream dir;
    bool first = true;
    for (size_t i = 0; i < nodes.size(); ++i) {
        if (std::abs(nodes[i].first - x_left) > tol) continue;
        for (int dof = 0; dof < 2; ++dof) {
            if (!first) dir << ", ";
            dir << "{\"node\": " << i << ", \"dof\": " << dof
                << ", \"value\": 0.0}";
            first = false;
        }
    }

    std::vector<size_t> right_nodes;
    for (size_t i = 0; i < nodes.size(); ++i) {
        if (std::abs(nodes[i].first - x_right) < tol) right_nodes.push_back(i);
    }

    std::ostringstream neu;
    const double per_node =
        right_nodes.empty() ? 0.0 : total_load / static_cast<double>(right_nodes.size());
    for (size_t k = 0; k < right_nodes.size(); ++k) {
        if (k) neu << ", ";
        neu << "{\"node\": " << right_nodes[k] << ", \"dof\": 1, \"value\": "
            << per_node << "}";
    }

    return inject_bcs(mesh_json, dir.str(), neu.str());
}

const char* kRectangleShapes =
    "[{\"type\":\"rectangle\",\"x\":0.0,\"y\":0.0,"
    "\"width\":1.0,\"height\":0.2}]";

const char* kCantileverConfig =
    "{\"plane\":\"stress\",\"solver\":\"cholesky\","
    "\"cg_tolerance\":1e-10,\"cg_max_iterations\":10000,"
    "\"integration\":\"full\"}";

// ------------------------------------------------------------------
// Fixture: pin the global solver state to 2D before every test so the
// C API tests cannot be polluted by the 3D tests in fea_test.cpp.
// ------------------------------------------------------------------
class CApiTest : public ::testing::Test {
protected:
    void SetUp() override {
        set_dimension(2);
        g_integration = IntegrationType::FULL;
    }

    void TearDown() override {
        set_dimension(2);
        g_integration = IntegrationType::FULL;
    }
};

}  // namespace


// ==========================================================================
// 1. MESH GENERATION -- RECTANGLE
// ==========================================================================

TEST_F(CApiTest, MeshGenerationRectangle) {
    TempDir dir("rect");

    int rc = fea_generate_mesh_c(kRectangleShapes, 8, 4, 0, dir.c_str());
    ASSERT_EQ(rc, 0) << "rectangle mesh generation should succeed";

    const std::string mesh_path = dir.file("mesh.json");
    ASSERT_TRUE(file_exists_nonempty(mesh_path)) << "mesh.json must be written";

    const std::string text = read_file(mesh_path);
    EXPECT_TRUE(json_is_balanced(text)) << "mesh.json must be structurally valid JSON";

    auto nodes = parse_nodes(text);
    auto elems = parse_q4_elements(text);

    EXPECT_GT(nodes.size(), 0u) << "node count must be > 0";
    EXPECT_GT(elems.size(), 0u) << "element count must be > 0";

    // A 8x4 structured quad grid over a full rectangle keeps every element
    EXPECT_EQ(nodes.size(), 45u);   // (8+1) * (4+1)
    EXPECT_EQ(elems.size(), 32u);   // 8 * 4

    EXPECT_EQ(static_cast<int>(json_number_or(text, "num_nodes", -1)), 45);
    EXPECT_EQ(static_cast<int>(json_number_or(text, "num_elements", -1)), 32);
    EXPECT_EQ(static_cast<int>(json_number_or(text, "num_dofs", -1)), 90);

    // Geometry must land on the requested bounding box
    double xmin = 1e18, xmax = -1e18, ymin = 1e18, ymax = -1e18;
    for (const auto& n : nodes) {
        xmin = std::min(xmin, n.first);
        xmax = std::max(xmax, n.first);
        ymin = std::min(ymin, n.second);
        ymax = std::max(ymax, n.second);
    }
    EXPECT_NEAR(xmin, 0.0, 1e-6);
    EXPECT_NEAR(xmax, 1.0, 1e-6);
    EXPECT_NEAR(ymin, 0.0, 1e-6);
    EXPECT_NEAR(ymax, 0.2, 1e-6);

    // Connectivity must stay in range after renumbering
    for (const auto& e : elems) {
        for (int n : e) {
            EXPECT_GE(n, 0);
            EXPECT_LT(n, static_cast<int>(nodes.size()));
        }
    }
}

TEST_F(CApiTest, MeshGenerationRectangleOffsetOrigin) {
    TempDir dir("rect_offset");

    const char* shapes =
        "[{\"type\":\"rectangle\",\"x\":2.5,\"y\":-1.0,"
        "\"width\":1.0,\"height\":0.5}]";

    ASSERT_EQ(fea_generate_mesh_c(shapes, 4, 2, 0, dir.c_str()), 0);

    auto nodes = parse_nodes(read_file(dir.file("mesh.json")));
    ASSERT_FALSE(nodes.empty());

    double xmin = 1e18, ymin = 1e18;
    for (const auto& n : nodes) {
        xmin = std::min(xmin, n.first);
        ymin = std::min(ymin, n.second);
    }
    EXPECT_NEAR(xmin, 2.5, 1e-6) << "mesh must be shifted to the shape origin";
    EXPECT_NEAR(ymin, -1.0, 1e-6) << "negative origin must be honoured";
}


// ==========================================================================
// 2. MESH GENERATION -- CIRCLE (centroid filtering)
// ==========================================================================

TEST_F(CApiTest, MeshGenerationCircle) {
    TempDir dir("circle");

    const char* shapes =
        "[{\"type\":\"circle\",\"cx\":0.5,\"cy\":0.5,\"radius\":0.5}]";

    ASSERT_EQ(fea_generate_mesh_c(shapes, 10, 10, 0, dir.c_str()), 0);

    const std::string text = read_file(dir.file("mesh.json"));
    auto nodes = parse_nodes(text);
    auto elems = parse_q4_elements(text);

    ASSERT_GT(nodes.size(), 0u);
    ASSERT_GT(elems.size(), 0u);

    // Elements outside the circle must be culled
    EXPECT_LT(elems.size(), 100u) << "elements outside the circle must be removed";
    EXPECT_EQ(elems.size(), 80u) << "exact centroid-in-circle count for a 10x10 grid";
    EXPECT_LT(nodes.size(), 121u) << "orphan nodes must be renumbered away";

    // Every surviving element centroid must sit inside the circle
    for (const auto& e : elems) {
        double cx = 0.0;
        double cy = 0.0;
        for (int n : e) {
            ASSERT_GE(n, 0);
            ASSERT_LT(n, static_cast<int>(nodes.size()));
            cx += nodes[n].first;
            cy += nodes[n].second;
        }
        cx /= 4.0;
        cy /= 4.0;
        const double r2 = (cx - 0.5) * (cx - 0.5) + (cy - 0.5) * (cy - 0.5);
        EXPECT_LE(r2, 0.25 + 1e-9)
            << "centroid (" << cx << ", " << cy << ") lies outside the circle";
    }

    // Reported counts must agree with the written arrays
    EXPECT_EQ(static_cast<int>(json_number_or(text, "num_nodes", -1)),
              static_cast<int>(nodes.size()));
    EXPECT_EQ(static_cast<int>(json_number_or(text, "num_elements", -1)),
              static_cast<int>(elems.size()));
}


// ==========================================================================
// 3. MESH GENERATION -- POLYGON (L-shape)
// ==========================================================================

TEST_F(CApiTest, MeshGenerationPolygonLShape) {
    TempDir dir("poly");

    // L-shape: unit square with the top-right quadrant removed
    const char* shapes =
        "[{\"type\":\"polygon\",\"points\":"
        "[[0.0,0.0],[1.0,0.0],[1.0,0.5],[0.5,0.5],[0.5,1.0],[0.0,1.0]]}]";

    ASSERT_EQ(fea_generate_mesh_c(shapes, 8, 8, 0, dir.c_str()), 0);

    const std::string text = read_file(dir.file("mesh.json"));
    auto nodes = parse_nodes(text);
    auto elems = parse_q4_elements(text);

    ASSERT_GT(nodes.size(), 0u);
    ASSERT_GT(elems.size(), 0u);

    // 64 grid cells, the 16 cells in [0.5,1] x [0.5,1] fall outside the L
    EXPECT_EQ(elems.size(), 48u) << "L-shape must keep 3/4 of the grid";

    // No surviving centroid may sit in the removed quadrant
    for (const auto& e : elems) {
        double cx = 0.0;
        double cy = 0.0;
        for (int n : e) {
            cx += nodes[n].first;
            cy += nodes[n].second;
        }
        cx /= 4.0;
        cy /= 4.0;
        EXPECT_FALSE(cx > 0.5 && cy > 0.5)
            << "centroid (" << cx << ", " << cy << ") is in the removed notch";
    }
}


// ==========================================================================
// 4. MESH GENERATION -- INVALID INPUT
// ==========================================================================

TEST_F(CApiTest, MeshGenerationMalformedJson) {
    TempDir dir("badjson");

    EXPECT_NE(fea_generate_mesh_c("{ this is not json", 4, 4, 0, dir.c_str()), 0)
        << "malformed JSON must be rejected";
    EXPECT_NE(fea_generate_mesh_c("[{\"type\":", 4, 4, 0, dir.c_str()), 0)
        << "truncated JSON must be rejected";
    EXPECT_NE(fea_generate_mesh_c("not json at all", 4, 4, 0, dir.c_str()), 0)
        << "plain text must be rejected";
    EXPECT_NE(fea_generate_mesh_c("[{\"foo\":1}]", 4, 4, 0, dir.c_str()), 0)
        << "shape object without a type must be rejected";
}

TEST_F(CApiTest, MeshGenerationEmptyShapes) {
    TempDir dir("empty");

    EXPECT_NE(fea_generate_mesh_c("[]", 4, 4, 0, dir.c_str()), 0)
        << "empty shape array must be rejected";
    EXPECT_NE(fea_generate_mesh_c("", 4, 4, 0, dir.c_str()), 0)
        << "empty string must be rejected";
}

TEST_F(CApiTest, MeshGenerationNullArguments) {
    TempDir dir("nullargs");

    EXPECT_NE(fea_generate_mesh_c(nullptr, 4, 4, 0, dir.c_str()), 0);
    EXPECT_NE(fea_generate_mesh_c(kRectangleShapes, 4, 4, 0, nullptr), 0);
}

TEST_F(CApiTest, MeshGenerationInvalidDensity) {
    TempDir dir("density");

    EXPECT_NE(fea_generate_mesh_c(kRectangleShapes, 0, 4, 0, dir.c_str()), 0);
    EXPECT_NE(fea_generate_mesh_c(kRectangleShapes, 4, 0, 0, dir.c_str()), 0);
    EXPECT_NE(fea_generate_mesh_c(kRectangleShapes, -1, 4, 0, dir.c_str()), 0);
    EXPECT_NE(fea_generate_mesh_c(kRectangleShapes, 4, -1, 0, dir.c_str()), 0);
}


// ==========================================================================
// 5. MESH GENERATION -- ELEMENT TYPES
// ==========================================================================

TEST_F(CApiTest, MeshGenerationQ8) {
    TempDir dir("q8");

    ASSERT_EQ(fea_generate_mesh_c(kRectangleShapes, 4, 2, 1, dir.c_str()), 0);

    const std::string text = read_file(dir.file("mesh.json"));
    EXPECT_TRUE(json_is_balanced(text));
    EXPECT_EQ(count_occurrences(text, "\"type\": \"Q8\""), 8)
        << "Q8 mesh must contain nx*ny serendipity elements";
    EXPECT_EQ(count_occurrences(text, "\"type\": \"Q4\""), 0)
        << "Q8 mesh must not emit Q4 elements";
}

TEST_F(CApiTest, MeshGenerationT3EmitsElements) {
    TempDir dir("t3");

    ASSERT_EQ(fea_generate_mesh_c(kRectangleShapes, 4, 2, 2, dir.c_str()), 0);

    const std::string text = read_file(dir.file("mesh.json"));
    EXPECT_TRUE(json_is_balanced(text));

    // Triangulation of a 4x2 grid produces 16 T3 elements
    EXPECT_EQ(static_cast<int>(json_number_or(text, "num_elements", -1)), 16);
    EXPECT_NE(text.find("\"elements\""), std::string::npos)
        << "T3 mesh.json must contain an elements array";
    EXPECT_EQ(count_occurrences(text, "\"type\": \"T3\""), 16)
        << "T3 connectivity must be serialised";
}

TEST_F(CApiTest, MeshGenerationT3RespectsDomainCut) {
    TempDir dir("t3cut");

    const char* shapes =
        "[{\"type\":\"circle\",\"cx\":0.5,\"cy\":0.5,\"radius\":0.5}]";

    ASSERT_EQ(fea_generate_mesh_c(shapes, 10, 10, 2, dir.c_str()), 0);

    const std::string text = read_file(dir.file("mesh.json"));
    const int n_elems = static_cast<int>(json_number_or(text, "num_elements", -1));

    // 80 quads survive the centroid cut, so 160 triangles at most
    EXPECT_GT(n_elems, 0);
    EXPECT_LT(n_elems, 200) << "T3 path must also cut elements outside the circle";
}

TEST_F(CApiTest, MeshGenerationDoesNotDependOn3dGlobalState) {
    TempDir dir("dimleak");

    // Simulate a host app that previously ran a 3D solve
    set_dimension(3);
    ASSERT_EQ(fea_generate_mesh_c(kRectangleShapes, 4, 2, 0, dir.c_str()), 0);
    set_dimension(2);

    const std::string text = read_file(dir.file("mesh.json"));
    const int n_nodes = static_cast<int>(json_number_or(text, "num_nodes", -1));
    const int n_dofs = static_cast<int>(json_number_or(text, "num_dofs", -1));

    EXPECT_EQ(n_nodes, 15);
    EXPECT_EQ(n_dofs, 2 * n_nodes)
        << "2D mesh must report 2 DOFs per node regardless of prior 3D state";
}


// ==========================================================================
// 6. SOLVE -- CANTILEVER
// ==========================================================================

TEST_F(CApiTest, SolveCantilever) {
    TempDir dir("cantilever");

    ASSERT_EQ(fea_generate_mesh_c(kRectangleShapes, 16, 4, 0, dir.c_str()), 0);

    const std::string mesh_json =
        build_cantilever_mesh_json(read_file(dir.file("mesh.json")),
                                   0.0, 1.0, -1000.0);
    ASSERT_NE(mesh_json.find("\"dirichlet\": [{"), std::string::npos)
        << "test harness must have injected Dirichlet BCs";
    ASSERT_NE(mesh_json.find("\"neumann\": [{"), std::string::npos)
        << "test harness must have injected Neumann BCs";

    const int rc = fea_solve_c(mesh_json.c_str(), kCantileverConfig, dir.c_str());
    ASSERT_EQ(rc, 0) << "cantilever solve should succeed";

    const std::string meta = read_file(dir.file("meta.json"));
    ASSERT_FALSE(meta.empty());

    const double max_disp = json_number_or(meta, "max_displacement", -1.0);
    const double max_stress = json_number_or(meta, "max_stress", -1.0);

    EXPECT_GT(max_disp, 0.0) << "max_displacement must be positive";
    EXPECT_GT(max_stress, 0.0) << "max_stress must be positive";
    EXPECT_TRUE(std::isfinite(max_disp));
    EXPECT_TRUE(std::isfinite(max_stress));

    EXPECT_EQ(static_cast<int>(json_number_or(meta, "num_nodes", -1)), 85);
    EXPECT_EQ(static_cast<int>(json_number_or(meta, "num_elements", -1)), 64);
    EXPECT_EQ(static_cast<int>(json_number_or(meta, "num_dofs", -1)), 170);

    // Euler-Bernoulli reference: delta = P L^3 / (3 E I)
    // L = 1.0, h = 0.2, t = 0.01, E = 200e9  ->  I = 6.6667e-6, delta = 2.5e-4
    const double I = 0.01 * std::pow(0.2, 3) / 12.0;
    const double analytic = 1000.0 * std::pow(1.0, 3) / (3.0 * 200e9 * I);
    EXPECT_GT(max_disp, 0.3 * analytic)
        << "Q4 shear locking should not suppress more than 70 percent of the deflection"
        << " (analytic = " << analytic << ")";
    EXPECT_LT(max_disp, 2.0 * analytic)
        << "deflection must not exceed twice the analytic value"
        << " (analytic = " << analytic << ")";

    // Displacement field must be written for every node and clamped at the root
    const std::string disp = read_file(dir.file("displacement.json"));
    EXPECT_TRUE(json_is_balanced(disp));
    EXPECT_EQ(count_occurrences(disp, "{\"ux\":"), 85);
    EXPECT_EQ(static_cast<int>(json_number_or(disp, "num_nodes", -1)), 85);

    // Node 0 is a clamped corner -- displacement must be essentially zero
    double ux0 = 1.0;
    double uy0 = 1.0;
    size_t p = disp.find("{\"ux\":");
    ASSERT_NE(p, std::string::npos);
    ASSERT_EQ(std::sscanf(disp.c_str() + p, "{\"ux\": %lf, \"uy\": %lf}", &ux0, &uy0), 2);
    EXPECT_NEAR(ux0, 0.0, 1e-6);
    EXPECT_NEAR(uy0, 0.0, 1e-6);

    const std::string stress = read_file(dir.file("stress.json"));
    EXPECT_TRUE(json_is_balanced(stress));
    EXPECT_EQ(static_cast<int>(json_number_or(stress, "num_elements", -1)), 64);
    EXPECT_GT(json_number_or(stress, "max_stress", -1.0), 0.0);
}

TEST_F(CApiTest, SolveCantileverCgMatchesCholesky) {
    TempDir dir_direct("chol");
    TempDir dir_cg("cg");

    ASSERT_EQ(fea_generate_mesh_c(kRectangleShapes, 16, 4, 0, dir_direct.c_str()), 0);
    const std::string mesh_json =
        build_cantilever_mesh_json(read_file(dir_direct.file("mesh.json")),
                                   0.0, 1.0, -1000.0);

    const char* cg_config =
        "{\"plane\":\"stress\",\"solver\":\"cg\","
        "\"cg_tolerance\":1e-12,\"cg_max_iterations\":20000}";

    ASSERT_EQ(fea_solve_c(mesh_json.c_str(), kCantileverConfig, dir_direct.c_str()), 0);
    ASSERT_EQ(fea_solve_c(mesh_json.c_str(), cg_config, dir_cg.c_str()), 0);

    const double d_chol =
        json_number_or(read_file(dir_direct.file("meta.json")), "max_displacement", -1.0);
    const double d_cg =
        json_number_or(read_file(dir_cg.file("meta.json")), "max_displacement", -2.0);

    ASSERT_GT(d_chol, 0.0);
    ASSERT_GT(d_cg, 0.0);
    // meta.json is written with 6 fixed decimals, so 1e-6 is the resolution floor
    EXPECT_NEAR(d_chol, d_cg, 1.1e-6)
        << "Cholesky and CG must agree on the maximum displacement";
}

TEST_F(CApiTest, SolvePlaneStrainDiffersFromPlaneStress) {
    TempDir dir_stress("pstress");
    TempDir dir_strain("pstrain");

    ASSERT_EQ(fea_generate_mesh_c(kRectangleShapes, 16, 4, 0, dir_stress.c_str()), 0);
    const std::string mesh_json =
        build_cantilever_mesh_json(read_file(dir_stress.file("mesh.json")),
                                   0.0, 1.0, -1000.0);

    ASSERT_EQ(fea_solve_c(mesh_json.c_str(),
                          "{\"plane\":\"stress\",\"solver\":\"cholesky\"}",
                          dir_stress.c_str()), 0);
    ASSERT_EQ(fea_solve_c(mesh_json.c_str(),
                          "{\"plane\":\"strain\",\"solver\":\"cholesky\"}",
                          dir_strain.c_str()), 0);

    const double d_stress =
        json_number_or(read_file(dir_stress.file("meta.json")), "max_displacement", -1.0);
    const double d_strain =
        json_number_or(read_file(dir_strain.file("meta.json")), "max_displacement", -1.0);

    ASSERT_GT(d_stress, 0.0);
    ASSERT_GT(d_strain, 0.0);
    // Plane strain is stiffer, so it must deflect less
    EXPECT_LT(d_strain, d_stress)
        << "plane strain must be stiffer than plane stress";
}


// ==========================================================================
// 7. SOLVE -- INVALID INPUT
// ==========================================================================

TEST_F(CApiTest, SolveMalformedMeshJson) {
    TempDir dir("solvebad");

    EXPECT_NE(fea_solve_c("{ not json", kCantileverConfig, dir.c_str()), 0)
        << "malformed mesh JSON must be rejected";
    EXPECT_NE(fea_solve_c("", kCantileverConfig, dir.c_str()), 0)
        << "empty mesh JSON must be rejected";
    EXPECT_NE(fea_solve_c("{}", kCantileverConfig, dir.c_str()), 0)
        << "mesh JSON without nodes must be rejected";
    EXPECT_NE(fea_solve_c(nullptr, kCantileverConfig, dir.c_str()), 0)
        << "null mesh JSON must be rejected";
    EXPECT_NE(fea_solve_c("{\"nodes\": []}", kCantileverConfig, nullptr), 0)
        << "null output directory must be rejected";
}

TEST_F(CApiTest, SolveMeshWithNodesButNoElements) {
    TempDir dir("noelem");

    const char* mesh =
        "{\"nodes\": [{\"x\": 0.0, \"y\": 0.0}, {\"x\": 1.0, \"y\": 0.0}],"
        " \"elements\": [], \"dirichlet\": [], \"neumann\": []}";

    EXPECT_NE(fea_solve_c(mesh, kCantileverConfig, dir.c_str()), 0)
        << "a mesh without elements is not solvable";
}

TEST_F(CApiTest, SolveWithoutDirichletBcsDoesNotProduceGarbage) {
    TempDir dir("nobc");

    ASSERT_EQ(fea_generate_mesh_c(kRectangleShapes, 4, 2, 0, dir.c_str()), 0);
    const std::string mesh_json = read_file(dir.file("mesh.json"));

    const int rc = fea_solve_c(mesh_json.c_str(), kCantileverConfig, dir.c_str());

    if (rc == 0) {
        const double d =
            json_number_or(read_file(dir.file("meta.json")), "max_displacement", -1.0);
        EXPECT_TRUE(std::isfinite(d))
            << "an unconstrained (singular) model must not report NaN or inf";
    } else {
        SUCCEED() << "unconstrained model rejected with code " << rc;
    }
}

TEST_F(CApiTest, SolveOutOfRangeNodeIndexInBcDoesNotCrash) {
    TempDir dir("badbc");

    ASSERT_EQ(fea_generate_mesh_c(kRectangleShapes, 4, 2, 0, dir.c_str()), 0);
    const std::string mesh_json =
        inject_bcs(read_file(dir.file("mesh.json")),
                   "{\"node\": 99999, \"dof\": 0, \"value\": 0.0}",
                   "{\"node\": 99999, \"dof\": 1, \"value\": -100.0}");

    const int rc = fea_solve_c(mesh_json.c_str(), kCantileverConfig, dir.c_str());
    EXPECT_NE(rc, 0) << "boundary conditions referencing missing nodes must be rejected";
}

// DISABLED: fea::build_rhs() indexes f[dof_index(bc.node, bc.dof)] without a
// bounds check, so a Neumann BC pointing at a node just past the end of the
// mesh performs a heap-buffer-overflow WRITE instead of raising an error.
// Confirmed with AddressSanitizer:
//   heap-buffer-overflow ... in fea::build_rhs(Mesh const&) fea.hpp:190
// The test stays disabled until fea_solve_c validates node indices, because
// running it corrupts the heap of the whole test binary.
TEST_F(CApiTest, SolveOutOfRangeNeumannNodeIsRejected) {
    TempDir dir("badneumann");

    const char* mesh =
        "{\"nodes\": [{\"x\": 0.0, \"y\": 0.0}, {\"x\": 1.0, \"y\": 0.0},"
        " {\"x\": 1.0, \"y\": 1.0}, {\"x\": 0.0, \"y\": 1.0}],"
        " \"elements\": [{\"type\": \"Q4\", \"nodes\": [0, 1, 2, 3]}],"
        " \"dirichlet\": [{\"node\": 0, \"dof\": 0, \"value\": 0.0},"
        " {\"node\": 0, \"dof\": 1, \"value\": 0.0},"
        " {\"node\": 3, \"dof\": 0, \"value\": 0.0},"
        " {\"node\": 3, \"dof\": 1, \"value\": 0.0}],"
        " \"neumann\": [{\"node\": 50, \"dof\": 1, \"value\": -100.0}],"
        " \"material\": {\"E\": 200e9, \"nu\": 0.3, \"rho\": 7800.0, \"t\": 0.01}}";

    EXPECT_NE(fea_solve_c(mesh, kCantileverConfig, dir.c_str()), 0)
        << "a Neumann node index beyond the mesh must be rejected, not written past"
        << " the end of the load vector";
}

TEST_F(CApiTest, SolveAcceptsMissingConfig) {
    TempDir dir("noconfig");

    ASSERT_EQ(fea_generate_mesh_c(kRectangleShapes, 8, 2, 0, dir.c_str()), 0);
    const std::string mesh_json =
        build_cantilever_mesh_json(read_file(dir.file("mesh.json")),
                                   0.0, 1.0, -1000.0);

    // config_json is documented as optional -- null and empty must both work
    EXPECT_EQ(fea_solve_c(mesh_json.c_str(), nullptr, dir.c_str()), 0);
    EXPECT_EQ(fea_solve_c(mesh_json.c_str(), "", dir.c_str()), 0);
}


// ==========================================================================
// 8. OUTPUT FILES
// ==========================================================================

TEST_F(CApiTest, OutputFilesCreated) {
    TempDir dir("outputs");

    ASSERT_EQ(fea_generate_mesh_c(kRectangleShapes, 8, 4, 0, dir.c_str()), 0);
    EXPECT_TRUE(file_exists_nonempty(dir.file("mesh.json")));

    const std::string mesh_json =
        build_cantilever_mesh_json(read_file(dir.file("mesh.json")),
                                   0.0, 1.0, -1000.0);
    ASSERT_EQ(fea_solve_c(mesh_json.c_str(), kCantileverConfig, dir.c_str()), 0);

    const char* expected[] = {"mesh.json", "displacement.json",
                              "stress.json", "meta.json"};
    for (const char* name : expected) {
        const std::string path = dir.file(name);
        EXPECT_TRUE(file_exists_nonempty(path)) << name << " must exist and be non-empty";
        const std::string text = read_file(path);
        EXPECT_TRUE(json_is_balanced(text)) << name << " must be structurally valid JSON";
        EXPECT_EQ(text.front(), '{') << name << " must be a JSON object";
    }
}

TEST_F(CApiTest, OutputDirectoryIsCreatedIfMissing) {
    TempDir parent("mkdir");
    const std::string nested = parent.str() + "/a/b/c";

    ASSERT_FALSE(fs::exists(nested));
    ASSERT_EQ(fea_generate_mesh_c(kRectangleShapes, 4, 2, 0, nested.c_str()), 0);
    EXPECT_TRUE(fs::exists(nested)) << "nested output directory must be created";
    EXPECT_TRUE(file_exists_nonempty(nested + "/mesh.json"));
}

TEST_F(CApiTest, GeneratedMeshRoundTripsThroughSolver) {
    TempDir gen("roundtrip_gen");
    TempDir out("roundtrip_out");

    ASSERT_EQ(fea_generate_mesh_c(kRectangleShapes, 8, 4, 0, gen.c_str()), 0);
    const std::string mesh_json =
        build_cantilever_mesh_json(read_file(gen.file("mesh.json")),
                                   0.0, 1.0, -1000.0);
    ASSERT_EQ(fea_solve_c(mesh_json.c_str(), kCantileverConfig, out.c_str()), 0);

    const std::string mesh_text = read_file(gen.file("mesh.json"));
    const std::string meta = read_file(out.file("meta.json"));

    EXPECT_EQ(static_cast<int>(json_number_or(mesh_text, "num_nodes", -1)),
              static_cast<int>(json_number_or(meta, "num_nodes", -2)))
        << "node count must survive the mesh.json round trip";
    EXPECT_EQ(static_cast<int>(json_number_or(mesh_text, "num_elements", -1)),
              static_cast<int>(json_number_or(meta, "num_elements", -2)))
        << "element count must survive the mesh.json round trip";
}

TEST_F(CApiTest, SmallDisplacementsSurviveJsonSerialisation) {
    TempDir dir("precision");

    ASSERT_EQ(fea_generate_mesh_c(kRectangleShapes, 8, 4, 0, dir.c_str()), 0);

    // 1 N tip load -> analytic tip deflection is 2.5e-7 m
    const std::string mesh_json =
        build_cantilever_mesh_json(read_file(dir.file("mesh.json")),
                                   0.0, 1.0, -1.0);
    ASSERT_EQ(fea_solve_c(mesh_json.c_str(), kCantileverConfig, dir.c_str()), 0);

    const double d =
        json_number_or(read_file(dir.file("meta.json")), "max_displacement", -1.0);
    EXPECT_GT(d, 0.0)
        << "sub-micrometre displacements must not be truncated to zero by the"
        << " fixed 6-decimal JSON writer";
}


// ==========================================================================
// PHASE 2 -- I-BEAM AND L-BRACKET SHAPE SUPPORT
//
// The GeometryEditor frontend gained I-beam and L-bracket primitives. The C
// FFI layer must generate and solve meshes for them too. These tests pin the
// centroid-in-domain filtering and a cantilever solve for both cross-sections.
// ==========================================================================

// Independent re-implementation of the I-beam membership test (mirrors the
// spec in fea_solver_c_api.cpp) so the test validates geometry, not a copy.
static bool point_in_ibeam_test(double px, double py, double ox, double oy,
                                double width, double height,
                                double flange, double web) {
    const double x0 = ox, y0 = oy;
    const double x1 = ox + width, y1 = oy + height;
    if (px < x0 || px > x1 || py < y0 || py > y1) return false;
    if (py >= y1 - flange) return true;   // top flange
    if (py <= y0 + flange) return true;   // bottom flange
    const double web_left = ox + (width - web) / 2.0;
    const double web_right = web_left + web;
    if (px >= web_left && px <= web_right) return true;  // web
    return false;
}

// Independent re-implementation of the L-bracket membership test.
static bool point_in_lbracket_test(double px, double py, double ox, double oy,
                                   double width, double height,
                                   double flange, double web) {
    if (px < ox || px > ox + width || py < oy || py > oy + height) return false;
    if (py <= oy + flange) return true;   // horizontal flange (bottom)
    if (px <= ox + web) return true;      // vertical web (left)
    return false;
}

const char* kIBeamShapes =
    "[{\"type\":\"ibeam\",\"x\":0.0,\"y\":0.0,"
    "\"width\":1.0,\"height\":1.0,\"flange\":0.1,\"web\":0.05}]";

const char* kLBracketShapes =
    "[{\"type\":\"lbracket\",\"x\":0.0,\"y\":0.0,"
    "\"width\":1.0,\"height\":1.0,\"flange\":0.1,\"web\":0.05}]";

TEST_F(CApiTest, MeshGenerationIBeam) {
    TempDir dir("ibeam");

    ASSERT_EQ(fea_generate_mesh_c(kIBeamShapes, 20, 20, 0, dir.c_str()), 0);

    const std::string text = read_file(dir.file("mesh.json"));
    auto nodes = parse_nodes(text);
    auto elems = parse_q4_elements(text);

    ASSERT_GT(nodes.size(), 0u) << "node count must be > 0";
    ASSERT_GT(elems.size(), 0u) << "element count must be > 0";

    // A 20x20 grid has 400 cells; the I-beam keeps only the flanges + web
    EXPECT_LT(elems.size(), 400u) << "elements outside the I-beam must be culled";
    EXPECT_EQ(elems.size(), 112u)
        << "exact I-beam element count for a 20x20 grid"
        << " (2 flange rows x 20 + 16 web rows x 2)";

    // Reported counts must agree with the written arrays
    EXPECT_EQ(static_cast<int>(json_number_or(text, "num_nodes", -1)),
              static_cast<int>(nodes.size()));
    EXPECT_EQ(static_cast<int>(json_number_or(text, "num_elements", -1)),
              static_cast<int>(elems.size()));
    EXPECT_EQ(static_cast<int>(json_number_or(text, "num_dofs", -1)),
              static_cast<int>(2 * nodes.size()));

    // Every surviving element centroid must sit inside the I-beam
    for (const auto& e : elems) {
        double cx = 0.0, cy = 0.0;
        for (int n : e) {
            ASSERT_GE(n, 0);
            ASSERT_LT(n, static_cast<int>(nodes.size()));
            cx += nodes[n].first;
            cy += nodes[n].second;
        }
        cx /= 4.0;
        cy /= 4.0;
        EXPECT_TRUE(point_in_ibeam_test(cx, cy, 0.0, 0.0, 1.0, 1.0, 0.1, 0.05))
            << "centroid (" << cx << ", " << cy << ") lies outside the I-beam";
    }

    // Connectivity must stay in range after renumbering
    for (const auto& e : elems) {
        for (int n : e) {
            EXPECT_GE(n, 0);
            EXPECT_LT(n, static_cast<int>(nodes.size()));
        }
    }
}

TEST_F(CApiTest, MeshGenerationLBracket) {
    TempDir dir("lbracket");

    ASSERT_EQ(fea_generate_mesh_c(kLBracketShapes, 20, 20, 0, dir.c_str()), 0);

    const std::string text = read_file(dir.file("mesh.json"));
    auto nodes = parse_nodes(text);
    auto elems = parse_q4_elements(text);

    ASSERT_GT(nodes.size(), 0u) << "node count must be > 0";
    ASSERT_GT(elems.size(), 0u) << "element count must be > 0";

    // A 20x20 grid has 400 cells; the L-bracket keeps the flange + web
    EXPECT_LT(elems.size(), 400u) << "elements outside the L-bracket must be culled";
    EXPECT_EQ(elems.size(), 58u)
        << "exact L-bracket element count for a 20x20 grid"
        << " (2 flange rows x 20 + 18 web rows x 1)";

    // Reported counts must agree with the written arrays
    EXPECT_EQ(static_cast<int>(json_number_or(text, "num_nodes", -1)),
              static_cast<int>(nodes.size()));
    EXPECT_EQ(static_cast<int>(json_number_or(text, "num_elements", -1)),
              static_cast<int>(elems.size()));
    EXPECT_EQ(static_cast<int>(json_number_or(text, "num_dofs", -1)),
              static_cast<int>(2 * nodes.size()));

    // Every surviving element centroid must sit inside the L-bracket
    for (const auto& e : elems) {
        double cx = 0.0, cy = 0.0;
        for (int n : e) {
            ASSERT_GE(n, 0);
            ASSERT_LT(n, static_cast<int>(nodes.size()));
            cx += nodes[n].first;
            cy += nodes[n].second;
        }
        cx /= 4.0;
        cy /= 4.0;
        EXPECT_TRUE(point_in_lbracket_test(cx, cy, 0.0, 0.0, 1.0, 1.0, 0.1, 0.05))
            << "centroid (" << cx << ", " << cy << ") lies outside the L-bracket";
    }

    // Connectivity must stay in range after renumbering
    for (const auto& e : elems) {
        for (int n : e) {
            EXPECT_GE(n, 0);
            EXPECT_LT(n, static_cast<int>(nodes.size()));
        }
    }
}

TEST_F(CApiTest, MeshGenerationIBeamSolve) {
    TempDir dir("ibeam_solve");

    ASSERT_EQ(fea_generate_mesh_c(kIBeamShapes, 20, 20, 0, dir.c_str()), 0);

    const std::string mesh_json =
        build_cantilever_mesh_json(read_file(dir.file("mesh.json")),
                                   0.0, 1.0, -1000.0);
    ASSERT_NE(mesh_json.find("\"dirichlet\": [{"), std::string::npos)
        << "test harness must have injected Dirichlet BCs";
    ASSERT_NE(mesh_json.find("\"neumann\": [{"), std::string::npos)
        << "test harness must have injected Neumann BCs";

    ASSERT_EQ(fea_solve_c(mesh_json.c_str(), kCantileverConfig, dir.c_str()), 0)
        << "I-beam cantilever solve should succeed";

    const std::string meta = read_file(dir.file("meta.json"));
    ASSERT_FALSE(meta.empty());

    const double max_disp = json_number_or(meta, "max_displacement", -1.0);
    const double max_stress = json_number_or(meta, "max_stress", -1.0);

    EXPECT_GT(max_disp, 0.0) << "max_displacement must be positive";
    EXPECT_GT(max_stress, 0.0) << "max_stress must be positive";
    EXPECT_TRUE(std::isfinite(max_disp));
    EXPECT_TRUE(std::isfinite(max_stress));

    // Displacement field must be written for every node
    const std::string disp = read_file(dir.file("displacement.json"));
    EXPECT_TRUE(json_is_balanced(disp));
    auto nodes = parse_nodes(read_file(dir.file("mesh.json")));
    EXPECT_EQ(count_occurrences(disp, "{\"ux\":"),
              static_cast<int>(nodes.size()))
        << "displacement.json must contain one entry per node";

    // Stress field must be written for every element
    const std::string stress = read_file(dir.file("stress.json"));
    EXPECT_TRUE(json_is_balanced(stress));
    EXPECT_GT(json_number_or(stress, "max_stress", -1.0), 0.0);
}

TEST_F(CApiTest, MeshGenerationLBracketSolve) {
    TempDir dir("lbracket_solve");

    ASSERT_EQ(fea_generate_mesh_c(kLBracketShapes, 20, 20, 0, dir.c_str()), 0);

    const std::string mesh_json =
        build_cantilever_mesh_json(read_file(dir.file("mesh.json")),
                                   0.0, 1.0, -1000.0);
    ASSERT_NE(mesh_json.find("\"dirichlet\": [{"), std::string::npos)
        << "test harness must have injected Dirichlet BCs";
    ASSERT_NE(mesh_json.find("\"neumann\": [{"), std::string::npos)
        << "test harness must have injected Neumann BCs";

    ASSERT_EQ(fea_solve_c(mesh_json.c_str(), kCantileverConfig, dir.c_str()), 0)
        << "L-bracket cantilever solve should succeed";

    const std::string meta = read_file(dir.file("meta.json"));
    ASSERT_FALSE(meta.empty());

    const double max_disp = json_number_or(meta, "max_displacement", -1.0);
    const double max_stress = json_number_or(meta, "max_stress", -1.0);

    EXPECT_GT(max_disp, 0.0) << "max_displacement must be positive";
    EXPECT_GT(max_stress, 0.0) << "max_stress must be positive";
    EXPECT_TRUE(std::isfinite(max_disp));
    EXPECT_TRUE(std::isfinite(max_stress));

    // Displacement field must be written for every node
    const std::string disp = read_file(dir.file("displacement.json"));
    EXPECT_TRUE(json_is_balanced(disp));
    auto nodes = parse_nodes(read_file(dir.file("mesh.json")));
    EXPECT_EQ(count_occurrences(disp, "{\"ux\":"),
              static_cast<int>(nodes.size()))
        << "displacement.json must contain one entry per node";

    // Stress field must be written for every element
    const std::string stress = read_file(dir.file("stress.json"));
    EXPECT_TRUE(json_is_balanced(stress));
    EXPECT_GT(json_number_or(stress, "max_stress", -1.0), 0.0);
}
