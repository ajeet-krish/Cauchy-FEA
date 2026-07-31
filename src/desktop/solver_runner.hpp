#pragma once
#include <QThread>
#include <QString>
#include "fea.hpp"

struct SolveConfig {
    CaseType case_type = CaseType::CANTILEVER;
    ElementType element_type = ElementType::Q4;
    PlaneType plane_type = PlaneType::STRESS;
    int nx = 32;
    int ny = 8;
    bool use_q8 = false;
    bool use_cg = false;
    double E = 210e9;       // Pa (Steel)
    double nu = 0.3;
    double t = 0.01;        // m
    bool use_adaptivity = false;
    int adaptive_iters = 3;
};

class SolverRunner : public QThread {
    Q_OBJECT
public:
    explicit SolverRunner(QObject* parent = nullptr);
    ~SolverRunner() override = default;

    void setConfig(const SolveConfig& config);
    void setMesh(const Mesh& mesh, bool use_cg = false);

signals:
    void progress(int percent, const QString& message);
    void finished(const fea::SolveResult& result, const Mesh& mesh);
    void error(const QString& errorMessage);

protected:
    void run() override;

private:
    SolveConfig m_config;
    Mesh m_customMesh;
    bool m_useCustomMesh = false;
};
