#pragma once
#include <QString>
#include "fea.hpp"
#include "fea_types.hpp"
#include "solver_runner.hpp"

struct ProjectConfig {
    SolveConfig solverConfig;
    Mesh mesh;
    fea::SolveResult result;
};

class ProjectIO {
public:
    static bool save(const QString& filePath, const ProjectConfig& config);
    static bool load(const QString& filePath, ProjectConfig& config);
    static QString defaultProjectPath();
};