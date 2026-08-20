#pragma once
#include <QString>
#include <QVariant>
#include "fea.hpp"
#include "fea_types.hpp"
#include "solver_runner.hpp"
#include "bc_model.hpp"
#include "geometry_primitive.hpp"

// Viewport display state (saved/restored with project)
struct ViewportState {
    int contourField = 0;   // 0=von_mises, 1=sigma_xx, 2=sigma_yy, ...
    int colormap = 0;       // 0=turbo, 1=viridis, 2=hot, 3=coolwarm, 4=RdBu_r
    double dispScale = 100.0;
    bool showUndeformed = true;
    bool showDeformed = true;
    bool showEdges = true;
    bool showArrows = false;
    bool showBoundary = true;
    double panX = 0.5;
    double panY = 0.5;
    double zoom = 1.0;
};

// Full application state for .cauchy project files
struct ProjectConfig {
    int version = 2;
    SolveConfig solverConfig;
    Mesh mesh;
    Material material;
    ViewportState viewport;

    // Editor-level boundary conditions (BCModel format)
    std::vector<BoundaryCondition> boundaryConditions;

    // Geometry primitives (for editor reconstruction)
    std::vector<GeometryPrimitive> geometryPrimitives;

    // Solver results
    bool hasResults = false;
    fea::SolveResult result;
};

class ProjectIO {
public:
    static bool save(const QString& filePath, const ProjectConfig& config);
    static bool load(const QString& filePath, ProjectConfig& config);
    static QString defaultProjectPath();
};
