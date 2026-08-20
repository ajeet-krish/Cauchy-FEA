#pragma once
#include <QWidget>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QPoint>
#include <QColor>
#include <QElapsedTimer>
#include <memory>
#include "fea.hpp"

enum class ContourField { VON_MISES, SIGMA_XX, SIGMA_YY, SIGMA_XY, SIGMA_1, SIGMA_2, DISPLACEMENT_MAG };
enum class ColormapType { TURBO, VIRIDIS, HOT, COOLWARM, RDBU_R };

class EditorState;
class GeometryModel;
class BCModel;
class SelectionModel;
class ToolContext;
class QTimer;

class ViewportWidget : public QWidget {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ViewportWidget)
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

    // Getters for save/load
    ContourField contourField() const { return m_field; }
    ColormapType colormap() const { return m_colormap; }
    double displacementScale() const { return m_dispScale; }
    bool showUndeformed() const { return m_showUndeformed; }
    bool showDeformed() const { return m_showDeformed; }
    bool showEdges() const { return m_showEdges; }
    bool showArrows() const { return m_showArrows; }
    bool showBoundary() const { return m_showBoundary; }
    double panX() const { return m_panX; }
    double panY() const { return m_panY; }
    double zoomLevel() const { return m_zoom; }

    // Editor integration
    void setEditorState(EditorState* state);
    void setGeometryModel(GeometryModel* model);
    void setBCModel(BCModel* model);
    void setSelectionModel(SelectionModel* model);
    void setToolContext(ToolContext* context);

    // Set mesh directly (for editor mode, before solve)
    void setMesh(const Mesh& mesh);

    // Coordinate conversion
    QPointF widgetToWorld(const QPointF& widgetPos) const;
    QPointF worldToWidget(const QPointF& worldPos) const;
    
    // Find nearest node to point (for context menu)
    int findNearestNode(const QPointF& worldPos, double tolerance = 0.05) const;

public slots:
    void startAnimation();
    void pauseAnimation();
    void resetAnimation();

signals:
    void pointProbed(int nodeId, int elemId, double x, double y, double ux, double uy, double stressVal);
    void nodeClicked(int nodeIndex, QPointF worldPos);
    void primitiveClicked(int index);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    Mesh m_mesh;
    fea::SolveResult m_result;
    bool m_hasData = false;
    bool m_hasMesh = false;

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
    bool m_isPanning = false;

    double m_fieldMin = 0.0;
    double m_fieldMax = 1.0;

    // Editor state (non-owning pointers)
    EditorState* m_editorState = nullptr;
    GeometryModel* m_geometryModel = nullptr;
    BCModel* m_bcModel = nullptr;
    SelectionModel* m_selectionModel = nullptr;
    ToolContext* m_toolContext = nullptr;

    // Geometry/BC rendering
    void drawGeometryPrimitives(QPainter& painter);
    void drawBCOverlay(QPainter& painter);
    void drawSelectionHighlight(QPainter& painter);
    void drawSnapIndicator(QPainter& painter, const QPointF& worldPos);
    void drawForceArrow(QPainter& painter, const QPointF& pos, double fx, double fy, double angle);
    void drawDragRectangle(QPainter& painter);
    void drawPendingShape(QPainter& painter);
    void drawMeshNodes(QPainter& painter);
    void drawMeshEdges(QPainter& painter);
    void drawDragMovePreview(QPainter& painter);

    QColor getColorForValue(double val) const;
    double getFieldValueForNode(int nodeIdx) const;
    double getFieldValueForElement(int elemIdx) const;
    void updateFieldRange();

    // Animation
    void onAnimationTick();
    static double easeInOutCubic(double t);

    // Periodic update timer to prevent blank screen
    QTimer* m_updateTimer = nullptr;

    // Deformation animation state
    bool m_animPlaying = false;
    bool m_animActive = false;
    double m_animProgress = 0.0;
    QTimer* m_animTimer = nullptr;
    QElapsedTimer m_animElapsed;
    double m_animPausedElapsed = 0.0;
    static constexpr double ANIM_DURATION_MS = 10000.0;
};