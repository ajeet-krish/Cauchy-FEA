#include "modal_runner.hpp"

ModalRunner::ModalRunner(QObject* parent)
    : QThread(parent) {}

void ModalRunner::setMesh(const Mesh& mesh) {
    m_mesh = mesh;
}

void ModalRunner::setK(const CSRMatrix& K) {
    m_K = K;
    m_hasK = true;
}

void ModalRunner::setNumModes(int numModes) {
    m_numModes = std::max(1, numModes);
}

void ModalRunner::run() {
    try {
        if (!m_hasK) {
            emit modalError("No stiffness matrix available. Run a static solve first.");
            return;
        }

        if (m_mesh.num_nodes() == 0) {
            emit modalError("No mesh available. Generate a mesh first.");
            return;
        }

        emit progress(10, "Assembling mass matrix...");
        emit progress(30, "Running subspace iteration...");

        auto result = dynamics::modal_analysis(m_mesh, m_K, m_numModes);

        emit progress(90, "Computing mode shapes...");
        emit progress(100, "Modal analysis complete.");
        emit modalFinished(result);

    } catch (const std::exception& ex) {
        emit modalError(QString("Modal analysis exception: %1").arg(ex.what()));
    } catch (...) {
        emit modalError("Unknown modal analysis exception occurred.");
    }
}
