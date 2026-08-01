#pragma once
#include <QWidget>
#include <vector>
#include "fea.hpp"

class ErrorHeatmap : public QWidget {
    Q_OBJECT
public:
    explicit ErrorHeatmap(QWidget* parent = nullptr);

    void setData(const Mesh& mesh, const std::vector<double>& errorIndicators);
    void clear();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

signals:
    void elementHovered(int elemId, double error);

private:
    Mesh m_mesh;
    std::vector<double> m_errors;
    bool m_hasData = false;

    double m_panX = 0.5;
    double m_panY = 0.5;
    double m_zoom = 1.0;
    QPoint m_lastMousePos;

    double m_errMin = 0.0;
    double m_errMax = 1.0;

    QColor colorForError(double val) const;
};
