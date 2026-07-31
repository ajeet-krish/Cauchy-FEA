#include "probe_tool.hpp"
#include <cmath>

ProbeTool::ProbeTool(QObject* parent)
    : QObject(parent) {}

ProbeResult ProbeTool::probe(double wx, double wy,
                              const Mesh& mesh,
                              fea::SolveResult& result) {
    ProbeResult r;
    r.valid = false;

    if (mesh.num_nodes() == 0 || result.displacement.empty()) {
        return r;
    }

    // Find nearest node
    double minDist = 1e20;
    int nearestNode = 0;
    for (int i = 0; i < mesh.num_nodes(); ++i) {
        double dx = mesh.nodes[i].x - wx;
        double dy = mesh.nodes[i].y - wy;
        double dist = dx * dx + dy * dy;
        if (dist < minDist) {
            minDist = dist;
            nearestNode = i;
        }
    }

    r.nodeId = nearestNode;
    r.x = mesh.nodes[nearestNode].x;
    r.y = mesh.nodes[nearestNode].y;
    r.ux = result.displacement[nearestNode * 2];
    r.uy = result.displacement[nearestNode * 2 + 1];

    // Find nearest element
    double minElemDist = 1e20;
    int nearestElem = 0;
    for (int e = 0; e < mesh.num_quads(); ++e) {
        const auto& elem = mesh.quad_elements[e];
        double cx = 0.0, cy = 0.0;
        for (int i = 0; i < 4; ++i) {
            cx += mesh.nodes[elem[i]].x;
            cy += mesh.nodes[elem[i]].y;
        }
        cx /= 4.0;
        cy /= 4.0;
        double dx = cx - wx;
        double dy = cy - wy;
        double dist = dx * dx + dy * dy;
        if (dist < minElemDist) {
            minElemDist = dist;
            nearestElem = e;
        }
    }

    r.elemId = nearestElem;
    if (nearestElem < static_cast<int>(result.stresses.size())) {
        const auto& s = result.stresses[nearestElem];
        r.vonMises = s.von_mises;
        r.sigma1 = s.sigma_1;
        r.sigma2 = s.sigma_2;
    }

    r.valid = true;
    emit probed(r);
    return r;
}