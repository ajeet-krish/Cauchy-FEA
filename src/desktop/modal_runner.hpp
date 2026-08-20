#pragma once
#include <QThread>
#include <QString>
#include "fea.hpp"
#include "dynamics.hpp"

class ModalRunner : public QThread {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ModalRunner)
public:
    explicit ModalRunner(QObject* parent = nullptr);
    ~ModalRunner() override = default;

    void setMesh(const Mesh& mesh);
    void setK(const CSRMatrix& K);
    void setNumModes(int numModes);

signals:
    void modalFinished(const dynamics::ModalResult& result);
    void modalError(const QString& errorMessage);
    void progress(int percent, const QString& message);

protected:
    void run() override;

private:
    Mesh m_mesh;
    CSRMatrix m_K;
    bool m_hasK = false;
    int m_numModes = 10;
};
