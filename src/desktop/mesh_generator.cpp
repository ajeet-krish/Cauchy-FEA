#include "mesh_generator.hpp"
#include "../mesh.hpp"
#include <cmath>
#include <algorithm>
#include <limits>
#include <map>
#include <set>

MeshGenerator::MeshGenerator() = default;
MeshGenerator::~MeshGenerator() = default;

std::unique_ptr<Mesh> MeshGenerator::generate(
    const GeometryModel& model,
    const Material& mat,
    int nx, int ny,
    PlaneType plane)
{
    auto bbox = model.boundingBox();
    if (!bbox.has_value()) {
        // No geometry: create a unit square as fallback
        return generateRect(0.0, 0.0, 1.0, 1.0, nx, ny, mat, plane);
    }

    // Generate mesh for the bounding box
    auto mesh = generateRect(bbox->xmin, bbox->ymin,
                             bbox->xmax - bbox->xmin,
                             bbox->ymax - bbox->ymin,
                             nx, ny, mat, plane);

    // Cut holes from the mesh (circles subtract material)
    for (const auto& prim : model.primitives()) {
        if (std::holds_alternative<CirclePrimitive>(prim)) {
            const auto& circle = std::get<CirclePrimitive>(prim);
            cutHole(*mesh, circle.cx, circle.cy, circle.radius);
        }
    }

    return mesh;
}

std::unique_ptr<Mesh> MeshGenerator::generateRect(
    double x, double y, double w, double h,
    int nx, int ny,
    const Material& mat,
    PlaneType plane)
{
    auto mesh = std::make_unique<Mesh>();

    int num_nodes_x = nx + 1;
    int num_nodes_y = ny + 1;

    // Generate node coordinates
    mesh->nodes.resize(num_nodes_x * num_nodes_y);
    for (int j = 0; j <= ny; ++j) {
        for (int i = 0; i <= nx; ++i) {
            int idx = j * num_nodes_x + i;
            mesh->nodes[idx].x = x + (static_cast<double>(i) / nx) * w;
            mesh->nodes[idx].y = y + (static_cast<double>(j) / ny) * h;
            mesh->nodes[idx].z = 0.0;
        }
    }

    // Generate Q4 elements (CCW ordering)
    mesh->quad_elements.resize(nx * ny);
    for (int j = 0; j < ny; ++j) {
        for (int i = 0; i < nx; ++i) {
            int n0 = j * num_nodes_x + i;
            int n1 = j * num_nodes_x + (i + 1);
            int n2 = (j + 1) * num_nodes_x + (i + 1);
            int n3 = (j + 1) * num_nodes_x + i;
            mesh->quad_elements[j * nx + i] = {n0, n1, n2, n3};
        }
    }

    // Set mesh properties
    mesh->mat = mat;
    mesh->plane = plane;

    return mesh;
}

std::unique_ptr<Mesh> MeshGenerator::generateRectQ8(
    double x, double y, double w, double h,
    int nx, int ny,
    const Material& mat,
    PlaneType plane)
{
    // Reuse the existing Q8 mesher
    auto q4_mesh = generateRect(x, y, w, h, nx, ny, mat, plane);

    // Build Q8 mesh by adding midside nodes
    auto mesh = std::make_unique<Mesh>();

    int num_corners = (nx + 1) * (ny + 1);
    int num_hmid = nx * (ny + 1);
    int num_vmid = (nx + 1) * ny;
    int num_total = num_corners + num_hmid + num_vmid;

    mesh->nodes.resize(num_total);

    // Copy corner nodes from Q4 mesh
    for (int i = 0; i < num_corners; ++i) {
        mesh->nodes[i] = q4_mesh->nodes[i];
    }

    // Create horizontal midside nodes (between corner pairs in x)
    for (int j = 0; j <= ny; ++j) {
        for (int i = 0; i < nx; ++i) {
            int left = j * (nx + 1) + i;
            int right = left + 1;
            int mid_idx = num_corners + j * nx + i;
            mesh->nodes[mid_idx].x = (q4_mesh->nodes[left].x + q4_mesh->nodes[right].x) / 2.0;
            mesh->nodes[mid_idx].y = (q4_mesh->nodes[left].y + q4_mesh->nodes[right].y) / 2.0;
            mesh->nodes[mid_idx].z = 0.0;
        }
    }

    // Create vertical midside nodes (between corner pairs in y)
    for (int j = 0; j < ny; ++j) {
        for (int i = 0; i <= nx; ++i) {
            int bottom = j * (nx + 1) + i;
            int top = bottom + (nx + 1);
            int mid_idx = num_corners + num_hmid + j * (nx + 1) + i;
            mesh->nodes[mid_idx].x = (q4_mesh->nodes[bottom].x + q4_mesh->nodes[top].x) / 2.0;
            mesh->nodes[mid_idx].y = (q4_mesh->nodes[bottom].y + q4_mesh->nodes[top].y) / 2.0;
            mesh->nodes[mid_idx].z = 0.0;
        }
    }

    // Create Q8 elements
    mesh->quad8_elements.resize(nx * ny);
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

            mesh->quad8_elements[j * nx + i] = {n0, n1, n2, n3, n4, n5, n6, n7};
        }
    }

    // Set mesh properties
    mesh->mat = mat;
    mesh->plane = plane;

    return mesh;
}

bool MeshGenerator::cutHole(Mesh& mesh, double cx, double cy, double radius) {
    // Remove Q4 elements whose centroid is inside the circle
    auto it = std::remove_if(mesh.quad_elements.begin(), mesh.quad_elements.end(),
        [&](const std::array<int, 4>& elem) {
            // Calculate element centroid from 4 corner nodes
            double centroid_x = 0.0;
            double centroid_y = 0.0;

            for (int i = 0; i < 4; ++i) {
                const auto& node = mesh.nodes[elem[i]];
                centroid_x += node.x;
                centroid_y += node.y;
            }
            centroid_x /= 4.0;
            centroid_y /= 4.0;

            // Check if centroid is inside circle
            double dx = centroid_x - cx;
            double dy = centroid_y - cy;
            return (dx * dx + dy * dy) < (radius * radius);
        });

    int removed_q4 = static_cast<int>(std::distance(it, mesh.quad_elements.end()));
    mesh.quad_elements.erase(it, mesh.quad_elements.end());

    // Remove Q8 elements whose centroid is inside the circle
    auto it8 = std::remove_if(mesh.quad8_elements.begin(), mesh.quad8_elements.end(),
        [&](const std::array<int, 8>& elem) {
            // Calculate element centroid from corner nodes (indices 0-3)
            double centroid_x = 0.0;
            double centroid_y = 0.0;

            for (int i = 0; i < 4; ++i) {
                const auto& node = mesh.nodes[elem[i]];
                centroid_x += node.x;
                centroid_y += node.y;
            }
            centroid_x /= 4.0;
            centroid_y /= 4.0;

            double dx = centroid_x - cx;
            double dy = centroid_y - cy;
            return (dx * dx + dy * dy) < (radius * radius);
        });

    int removed_q8 = static_cast<int>(std::distance(it8, mesh.quad8_elements.end()));
    mesh.quad8_elements.erase(it8, mesh.quad8_elements.end());

    return (removed_q4 + removed_q8) > 0;
}

std::vector<int> MeshGenerator::findEdgeNodes(
    const Mesh& mesh,
    double x1, double y1,
    double x2, double y2,
    double tolerance)
{
    std::vector<int> edge_nodes;

    double dx = x2 - x1;
    double dy = y2 - y1;
    double length_sq = dx * dx + dy * dy;

    if (length_sq < 1e-20) {
        // Degenerate edge: find nodes near the single point
        for (int i = 0; i < static_cast<int>(mesh.nodes.size()); ++i) {
            const auto& node = mesh.nodes[i];
            double ddx = node.x - x1;
            double ddy = node.y - y1;
            if (std::sqrt(ddx * ddx + ddy * ddy) < tolerance) {
                edge_nodes.push_back(i);
            }
        }
        return edge_nodes;
    }

    for (int i = 0; i < static_cast<int>(mesh.nodes.size()); ++i) {
        const auto& node = mesh.nodes[i];

        // Project node onto the line segment
        double t = ((node.x - x1) * dx + (node.y - y1) * dy) / length_sq;
        t = std::max(0.0, std::min(1.0, t));

        double proj_x = x1 + t * dx;
        double proj_y = y1 + t * dy;

        double dist_x = node.x - proj_x;
        double dist_y = node.y - proj_y;
        double distance = std::sqrt(dist_x * dist_x + dist_y * dist_y);

        if (distance < tolerance) {
            edge_nodes.push_back(i);
        }
    }

    // Sort by distance from (x1, y1) along the edge for consistent ordering
    std::sort(edge_nodes.begin(), edge_nodes.end(), [&](int a, int b) {
        double ta = ((mesh.nodes[a].x - x1) * dx + (mesh.nodes[a].y - y1) * dy) / length_sq;
        double tb = ((mesh.nodes[b].x - x1) * dx + (mesh.nodes[b].y - y1) * dy) / length_sq;
        return ta < tb;
    });

    return edge_nodes;
}

std::vector<int> MeshGenerator::findBoundaryNodes(const Mesh& mesh) {
    // Count how many elements each edge belongs to
    // An edge is shared by at most 2 elements in a conforming mesh
    // Boundary edges appear in exactly 1 element
    std::map<std::pair<int, int>, int> edge_count;

    auto count_edges = [&](int n1, int n2) {
        if (n1 > n2) std::swap(n1, n2);
        edge_count[{n1, n2}]++;
    };

    // Count edges from Q4 elements
    for (const auto& elem : mesh.quad_elements) {
        for (int i = 0; i < 4; ++i) {
            int n1 = elem[i];
            int n2 = elem[(i + 1) % 4];
            count_edges(n1, n2);
        }
    }

    // Count edges from Q8 elements (use corner nodes 0-3)
    for (const auto& elem : mesh.quad8_elements) {
        for (int i = 0; i < 4; ++i) {
            int n1 = elem[i];
            int n2 = elem[(i + 1) % 4];
            count_edges(n1, n2);
        }
    }

    // Collect nodes on boundary edges (edges appearing exactly once)
    std::set<int> boundary_set;
    for (const auto& [edge, count] : edge_count) {
        if (count == 1) {
            boundary_set.insert(edge.first);
            boundary_set.insert(edge.second);
        }
    }

    return std::vector<int>(boundary_set.begin(), boundary_set.end());
}

MeshGenerator::MeshStats MeshGenerator::getStats(const Mesh& mesh) const {
    MeshStats stats;
    stats.node_count = mesh.num_nodes();
    stats.element_count = mesh.num_quads() + mesh.num_quad8s();

    // Compute boundary node count (non-const version)
    MeshGenerator gen;
    stats.boundary_nodes = static_cast<int>(gen.findBoundaryNodes(mesh).size());

    if (mesh.nodes.empty()) {
        stats.xmin = stats.ymin = stats.xmax = stats.ymax = 0.0;
    } else {
        stats.xmin = mesh.nodes[0].x;
        stats.xmax = mesh.nodes[0].x;
        stats.ymin = mesh.nodes[0].y;
        stats.ymax = mesh.nodes[0].y;

        for (const auto& node : mesh.nodes) {
            stats.xmin = std::min(stats.xmin, node.x);
            stats.xmax = std::max(stats.xmax, node.x);
            stats.ymin = std::min(stats.ymin, node.y);
            stats.ymax = std::max(stats.ymax, node.y);
        }
    }

    return stats;
}

void MeshGenerator::createStructuredQuadMesh(
    Mesh& mesh,
    double x, double y, double w, double h,
    int nx, int ny,
    const Material& mat,
    PlaneType plane)
{
    int num_nodes_x = nx + 1;

    // Generate node coordinates
    mesh.nodes.resize(num_nodes_x * (ny + 1));
    for (int j = 0; j <= ny; ++j) {
        for (int i = 0; i <= nx; ++i) {
            int idx = j * num_nodes_x + i;
            mesh.nodes[idx].x = x + (static_cast<double>(i) / nx) * w;
            mesh.nodes[idx].y = y + (static_cast<double>(j) / ny) * h;
            mesh.nodes[idx].z = 0.0;
        }
    }

    // Generate Q4 elements (CCW ordering)
    mesh.quad_elements.resize(nx * ny);
    for (int j = 0; j < ny; ++j) {
        for (int i = 0; i < nx; ++i) {
            int n0 = j * num_nodes_x + i;
            int n1 = j * num_nodes_x + (i + 1);
            int n2 = (j + 1) * num_nodes_x + (i + 1);
            int n3 = (j + 1) * num_nodes_x + i;
            mesh.quad_elements[j * nx + i] = {n0, n1, n2, n3};
        }
    }

    // Set mesh properties
    mesh.mat = mat;
    mesh.plane = plane;
}
