#pragma once
#include <QObject>
#include "fea.hpp"

struct ProbeResult {
    int nodeId = -1;
    int elemId = -1;
    double x = 0.0;
    double y = 0.0;
    double ux = 0.0;
    double uy = 0.0;
    double vonMises = 0.0;
    double sigma1 = 0.0;
    double sigma2 = 0.0;
    bool valid = false;
};

class ProbeTool : public QObject {
    Q_OBJECT
public:
    explicit ProbeTool(QObject* parent = nullptr);

    ProbeResult probe(double wx, double wy, const Mesh& mesh,
                      fea::SolveResult& result);

signals:
    void probed(const ProbeResult& result);
};