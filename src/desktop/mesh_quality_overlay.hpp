#pragma once
#include <QWidget>
#include "fea.hpp"

class MeshQualityOverlay : public QWidget {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(MeshQualityOverlay)
public:
    explicit MeshQualityOverlay(QWidget* parent = nullptr);

    void setMesh(const Mesh& mesh);
    void clear();

    double minJacobian() const;
    double maxAspectRatio() const;
    int invalidElementCount() const;

signals:
    void qualityChanged();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    Mesh m_mesh;
    bool m_hasData = false;
    double m_minJacobian = 1.0;
    double m_maxAspectRatio = 1.0;
    int m_invalidCount = 0;
};