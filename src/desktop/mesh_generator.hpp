#pragma once
#include "geometry_model.hpp"
#include "../fea_types.hpp"
#include <memory>

// ------------------------------------------------------------------
// MeshGenerator: converts geometry primitives to a finite element mesh
// Supports structured quad meshing with hole cutting and boundary queries
// ------------------------------------------------------------------
class MeshGenerator {
public:
    MeshGenerator();
    ~MeshGenerator();

    // Generate mesh from geometry model (bounding box of all primitives)
    std::unique_ptr<Mesh> generate(const GeometryModel& model,
                                   const Material& mat,
                                   int nx, int ny,
                                   PlaneType plane = PlaneType::STRESS);

    // Generate mesh for a single rectangle
    std::unique_ptr<Mesh> generateRect(double x, double y, double w, double h,
                                       int nx, int ny,
                                       const Material& mat,
                                       PlaneType plane = PlaneType::STRESS);

    // Generate Q8 mesh for a single rectangle
    std::unique_ptr<Mesh> generateRectQ8(double x, double y, double w, double h,
                                         int nx, int ny,
                                         const Material& mat,
                                         PlaneType plane = PlaneType::STRESS);

    // Cut circular hole from existing mesh (removes elements with centroid inside circle)
    bool cutHole(Mesh& mesh, double cx, double cy, double radius);

    // Find boundary nodes along a line segment
    std::vector<int> findEdgeNodes(const Mesh& mesh,
                                   double x1, double y1,
                                   double x2, double y2,
                                   double tolerance = 0.01);

    // Find all boundary nodes of the mesh (edges shared by only one element)
    std::vector<int> findBoundaryNodes(const Mesh& mesh);

    // Mesh statistics
    struct MeshStats {
        int node_count;
        int element_count;
        int boundary_nodes;
        double xmin, ymin, xmax, ymax;
    };
    MeshStats getStats(const Mesh& mesh) const;

private:
    // Helper: create structured quad mesh for a rectangle
    void createStructuredQuadMesh(Mesh& mesh,
                                  double x, double y, double w, double h,
                                  int nx, int ny,
                                  const Material& mat,
                                  PlaneType plane);
};
