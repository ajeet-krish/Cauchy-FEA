#pragma once
#include <QWidget>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPoint>
#include <QColor>
#include "fea.hpp"

enum class ContourField { VON_MISES, SIGMA_XX, SIGMA_YY, SIGMA_XY, SIGMA_1, SIGMA_2, DISPLACEMENT_MAG };
enum class ColormapType { TURBO, VIRIDIS, HOT, COOLWARM, RDBU_R };

class ViewportWidget : public QWidget {
    Q_OBJECT
public:
    explicit ViewportWidget(QWidget* parent = nullptr);
    ~ViewportWidget() override = default;

    void setMeshAndResults(const Mesh& mesh, const fea::SolveResult& result);
    void setContourField(ContourField field);
    void setColormap(ColormapType colormap);
    void setDisplacementScale(double scale);

    void toggleUndeformed(bool show);
    void toggleDeformed(bool show);
    void toggleEdges(bool show);
    void toggleArrows(bool show);
    void toggleBoundary(bool show);

    void resetView();

signals:
    void pointProbed(int nodeId, int elemId, double x, double y, double ux, double uy, double stressVal);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    Mesh m_mesh;
    fea::SolveResult m_result;
    bool m_hasData = false;

    ContourField m_field = ContourField::VON_MISES;
    ColormapType m_colormap = ColormapType::TURBO;
    double m_dispScale = 100.0;

    bool m_showUndeformed = true;
    bool m_showDeformed = true;
    bool m_showEdges = true;
    bool m_showArrows = false;
    bool m_showBoundary = true;

    double m_panX = 0.5;
    double m_panY = 0.5;
    double m_zoom = 1.0;
    QPoint m_lastMousePos;

    double m_fieldMin = 0.0;
    double m_fieldMax = 1.0;

    QColor getColorForValue(double val) const;
    double getFieldValueForNode(int nodeIdx) const;
    double getFieldValueForElement(int elemIdx) const;
    void updateFieldRange();
};