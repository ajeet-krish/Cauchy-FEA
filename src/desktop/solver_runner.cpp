#include "solver_runner.hpp"
#include "mesh.hpp"
#include "adaptivity.hpp"
#include <iostream>

SolverRunner::SolverRunner(QObject* parent)
    : QThread(parent) {}

void SolverRunner::setConfig(const SolveConfig& config) {
    m_config = config;
    m_useCustomMesh = false;
}

void SolverRunner::setMesh(const Mesh& mesh, bool use_cg) {
    m_customMesh = mesh;
    m_config.use_cg = use_cg;
    m_useCustomMesh = true;
}

void SolverRunner::run() {
    try {
        emit progress(10, "Generating mesh...");

        Mesh m;
        if (m_useCustomMesh) {
            m = m_customMesh;
        } else {
            int nx = m_config.nx;
            int ny = m_config.ny;

            if (m_config.is_3d) {
                // 3D mesh generation
                set_dimension(3);
                int nz = m_config.nz;

                if (m_config.case_type == CaseType::CANTILEVER) {
                    m = mesh::generate_structured_hex(1.0, 0.25, 0.1, nx, ny, nz);
                    m.mat.E = m_config.E;
                    m.mat.nu = m_config.nu;
                    m.mat.t = 1.0;
                    for (int i = 0; i < m.num_nodes(); ++i) {
                        if (std::abs(m.nodes[i].x) < 1e-6) {
                            m.dirichlet.push_back({i, 0, 0.0});
                            m.dirichlet.push_back({i, 1, 0.0});
                            m.dirichlet.push_back({i, 2, 0.0});
                        }
                    }
                    int tip_node = 0;
                    double min_dist = 1e20;
                    for (int i = 0; i < m.num_nodes(); ++i) {
                        double dist = std::abs(m.nodes[i].x - 1.0)
                                    + std::abs(m.nodes[i].y - 0.125)
                                    + std::abs(m.nodes[i].z - 0.05);
                        if (dist < min_dist) { min_dist = dist; tip_node = i; }
                    }
                    m.neumann.push_back({tip_node, 1, -1000.0});
                } else {
                    // Default 3D: cantilever beam
                    m = mesh::generate_structured_hex(1.0, 0.25, 0.1, nx, ny, nz);
                    m.mat.E = m_config.E;
                    m.mat.nu = m_config.nu;
                    m.mat.t = 1.0;
                }
            } else {
                // 2D mesh generation
                set_dimension(2);

                switch (m_config.case_type) {
                case CaseType::CANTILEVER:
                    if (m_config.use_q8) {
                        m = mesh::generate_structured_quad8(1.0, 0.25, nx, ny);
                    } else {
                        m = mesh::generate_structured_quad(1.0, 0.25, nx, ny);
                    }
                    m.mat.E = m_config.E;
                    m.mat.nu = m_config.nu;
                    m.mat.t = m_config.t;
                    m.plane = m_config.plane_type;
                    for (int i = 0; i < m.num_nodes(); ++i) {
                        if (std::abs(m.nodes[i].x) < 1e-6) {
                            m.dirichlet.push_back({i, 0, 0.0});
                            m.dirichlet.push_back({i, 1, 0.0});
                        }
                    }
                    {
                        int tip_node = 0;
                        double min_dist = 1e20;
                        for (int i = 0; i < m.num_nodes(); ++i) {
                            double dist = std::abs(m.nodes[i].x - 1.0) + std::abs(m.nodes[i].y - 0.125);
                            if (dist < min_dist) { min_dist = dist; tip_node = i; }
                        }
                        m.neumann.push_back({tip_node, 1, -1000.0});
                    }
                    break;

                case CaseType::COOK:
                    m = mesh::generate_structured_quad(48.0, 60.0, nx, ny);
                    m.mat.E = 1.0;
                    m.mat.nu = 1.0 / 3.0;
                    m.mat.t = 1.0;
                    m.plane = m_config.plane_type;
                    break;

                case CaseType::LBRACKET:
                    m = mesh::generate_lbracket(1.0, 1.0, 0.5, 0.5, nx, ny);
                    m.mat.E = m_config.E;
                    m.mat.nu = m_config.nu;
                    m.mat.t = m_config.t;
                    m.plane = m_config.plane_type;
                    break;

                case CaseType::PATCH:
                    m = mesh::generate_structured_quad(1.0, 1.0, nx, ny);
                    m.mat.E = m_config.E;
                    m.mat.nu = m_config.nu;
                    m.mat.t = m_config.t;
                    m.plane = m_config.plane_type;
                    m.dirichlet.push_back({0, 0, 0.0});
                    m.dirichlet.push_back({0, 1, 0.0});
                    for (int i = 0; i < m.num_nodes(); ++i) {
                        if (std::abs(m.nodes[i].y) < 1e-6) m.dirichlet.push_back({i, 1, 0.0});
                    }
                    break;

                case CaseType::PLATE_HOLE:
                    m = mesh::generate_structured_quad(1.0, 0.2, nx, ny);
                    m.mat.E = m_config.E;
                    m.mat.nu = m_config.nu;
                    m.mat.t = m_config.t;
                    m.plane = m_config.plane_type;
                    break;

                case CaseType::THERMAL_CYLINDER:
                    m = mesh::generate_structured_quad(0.4, 0.4, nx, ny);
                    m.mat.E = m_config.E;
                    m.mat.nu = m_config.nu;
                    m.mat.t = m_config.t;
                    m.plane = m_config.plane_type;
                    break;

                case CaseType::MICHELL:
                    m = mesh::generate_structured_quad(1.0, 1.0, nx, ny);
                    m.mat.E = m_config.E;
                    m.mat.nu = m_config.nu;
                    m.mat.t = m_config.t;
                    m.plane = m_config.plane_type;
                    break;

                case CaseType::CANTILEVER_3D:
                case CaseType::PLATE_HOLE_3D:
                case CaseType::LAME_3D:
                    // 3D cases: handled by 3D mesh generation above
                    break;
                }
            }
        }

        emit progress(30, "Assembling stiffness matrix...");

        fea::SolveResult res;

        if (m_config.use_adaptivity && m_config.case_type == CaseType::PLATE_HOLE) {
            emit progress(50, "Running adaptive refinement loop...");
            auto adapt_history = adaptivity::adaptive_loop(m, m_config.adaptive_iters, 0.5, m_config.use_cg);
            res = fea::solve(m, m_config.use_cg);
        } else {
            emit progress(60, "Solving linear system...");
            res = fea::solve(m, m_config.use_cg);
        }

        emit progress(90, "Post-processing stresses and displacements...");

        emit progress(100, "Done.");
        emit finished(res, m);

    } catch (const std::exception& ex) {
        emit error(QString("Solver Exception: %1").arg(ex.what()));
    } catch (...) {
        emit error("Unknown solver exception occurred.");
    }
}