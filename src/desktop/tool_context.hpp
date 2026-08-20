#pragma once
#include "editor_state.hpp"
#include "geometry_model.hpp"
#include "bc_model.hpp"
#include "selection_model.hpp"
#include <QObject>
#include <QMouseEvent>

class QUndoStack;

class ToolContext : public QObject {
    Q_OBJECT
public:
    explicit ToolContext(EditorState* state, GeometryModel* geometry,
                        BCModel* bc_model, SelectionModel* selection,
                        QObject* parent = nullptr);
    ~ToolContext();

    // Set the undo stack for command-based operations
    void setUndoStack(QUndoStack* stack);

    // Set mesh nodes for node-based operations (call after mesh generation)
    void setMeshNodes(const std::vector<struct Node>* nodes);

    // Mouse event handlers
    void handleMousePress(QMouseEvent* event, const QPointF& worldPos);
    void handleMouseMove(QMouseEvent* event, const QPointF& worldPos);
    void handleMouseRelease(QMouseEvent* event, const QPointF& worldPos);
    void handleMouseDoubleClick(QMouseEvent* event, const QPointF& worldPos);

    // Find nearest node to point (for BC assignment)
    int findNearestNode(const QPointF& worldPos, double tolerance = 0.02) const;

    // Find nearest edge to point
    int findNearestEdge(const QPointF& worldPos, double tolerance = 0.02) const;

    // Find nodes in selection rectangle
    std::vector<int> findNodesInRect(const QRectF& rect) const;
    
    // Delete selected object
    void deleteSelected();

signals:
    void geometryChanged();
    void nodeSelected(int nodeIndex);
    void primitiveSelected(int index);
    void statusMessage(const QString& msg);
    void modeChanged(ToolMode mode);
    void probeRequested(double wx, double wy);

private:
    EditorState* m_state;
    GeometryModel* m_geometry;
    BCModel* m_bc_model;
    SelectionModel* m_selection;
    QUndoStack* m_undoStack = nullptr;

    // Current mesh nodes for node-based operations
    const std::vector<struct Node>* m_mesh_nodes = nullptr;

    // Tool-specific handlers
    void handleSelectPress(const QPointF& worldPos);
    void handleDrawRectPress(const QPointF& worldPos);
    void handleDrawRectMove(const QPointF& worldPos);
    void handleDrawRectRelease(const QPointF& worldPos);
    void handleDrawLinePress(const QPointF& worldPos);
    void handleDrawLineMove(const QPointF& worldPos);
    void handleDrawLineRelease(const QPointF& worldPos);
    void handleDrawCirclePress(const QPointF& worldPos);
    void handleDrawCircleMove(const QPointF& worldPos);
    void handleDrawCircleRelease(const QPointF& worldPos);
    void handleBCPress(const QPointF& worldPos);
    void handleForcePress(const QPointF& worldPos);

    // Drag selection handlers
    void handleSelectMove(const QPointF& worldPos);
    void handleSelectRelease(const QPointF& worldPos);

    // Drag-move handlers for selected primitives
    void handleDragMove(const QPointF& worldPos);
    void handleDragRelease(const QPointF& worldPos);
};
