#include "tool_context.hpp"
#include "geometry_primitive.hpp"
#include "undo_commands.hpp"
#include "fea_types.hpp"
#include <QApplication>
#include <QUndoStack>
#include <cmath>

ToolContext::ToolContext(EditorState* state, GeometryModel* geometry,
                         BCModel* bc_model, SelectionModel* selection,
                         QObject* parent)
    : QObject(parent)
    , m_state(state)
    , m_geometry(geometry)
    , m_bc_model(bc_model)
    , m_selection(selection) {}

ToolContext::~ToolContext() = default;

void ToolContext::setUndoStack(QUndoStack* stack) {
    m_undoStack = stack;
}

void ToolContext::setMeshNodes(const std::vector<Node>* nodes) {
    m_mesh_nodes = nodes;
}

// ------------------------------------------------------------------
// Public mouse event dispatchers
// ------------------------------------------------------------------
void ToolContext::handleMousePress(QMouseEvent* event, const QPointF& worldPos) {
    if (event->button() != Qt::LeftButton) return;

    switch (m_state->current_mode) {
    case ToolMode::SELECT:
        handleSelectPress(worldPos);
        break;
    case ToolMode::DRAW_RECT:
        handleDrawRectPress(worldPos);
        break;
    case ToolMode::DRAW_LINE:
        handleDrawLinePress(worldPos);
        break;
    case ToolMode::DRAW_CIRCLE:
        handleDrawCirclePress(worldPos);
        break;
    case ToolMode::ASSIGN_FIXED:
    case ToolMode::ASSIGN_ROLLER_X:
    case ToolMode::ASSIGN_ROLLER_Y:
        handleBCPress(worldPos);
        break;
    case ToolMode::APPLY_FORCE:
        handleForcePress(worldPos);
        break;
    case ToolMode::PAN_ZOOM:
        // Handled by ViewportWidget directly
        break;
    }
}

void ToolContext::handleMouseMove(QMouseEvent* event, const QPointF& worldPos) {
    Q_UNUSED(event)

    switch (m_state->current_mode) {
    case ToolMode::DRAW_RECT:
        handleDrawRectMove(worldPos);
        break;
    case ToolMode::DRAW_LINE:
        handleDrawLineMove(worldPos);
        break;
    case ToolMode::DRAW_CIRCLE:
        handleDrawCircleMove(worldPos);
        break;
    case ToolMode::SELECT:
        if (m_state->is_dragging) {
            handleDragMove(worldPos);
        } else {
            handleSelectMove(worldPos);
        }
        break;
    default:
        break;
    }
}

void ToolContext::handleMouseRelease(QMouseEvent* event, const QPointF& worldPos) {
    if (event->button() != Qt::LeftButton) return;

    switch (m_state->current_mode) {
    case ToolMode::DRAW_RECT:
        handleDrawRectRelease(worldPos);
        break;
    case ToolMode::DRAW_LINE:
        handleDrawLineRelease(worldPos);
        break;
    case ToolMode::DRAW_CIRCLE:
        handleDrawCircleRelease(worldPos);
        break;
    case ToolMode::SELECT:
        if (m_state->is_dragging) {
            handleDragRelease(worldPos);
        } else {
            handleSelectRelease(worldPos);
        }
        break;
    default:
        break;
    }
}

void ToolContext::handleMouseDoubleClick(QMouseEvent* event, const QPointF& worldPos) {
    if (event->button() != Qt::LeftButton) return;
    if (m_state->current_mode != ToolMode::SELECT) return;

    // Double-click on a primitive: select it and emit for property editing
    int prim_idx = m_geometry->findNearestPrimitive(worldPos, m_state->snap_tolerance);
    if (prim_idx >= 0) {
        m_state->selected_primitive_index = prim_idx;
        m_state->selected_node_index = -1;
        m_state->selected_edge_index = -1;
        emit primitiveSelected(prim_idx);
        emit statusMessage(QString("Editing primitive %1").arg(prim_idx));
    }
}

// ------------------------------------------------------------------
// Node/edge geometry queries
// ------------------------------------------------------------------
int ToolContext::findNearestNode(const QPointF& worldPos, double tolerance) const {
    if (!m_mesh_nodes || m_mesh_nodes->empty()) return -1;

    int best_index = -1;
    double best_dist = tolerance;

    for (int i = 0; i < static_cast<int>(m_mesh_nodes->size()); ++i) {
        double dx = worldPos.x() - (*m_mesh_nodes)[i].x;
        double dy = worldPos.y() - (*m_mesh_nodes)[i].y;
        double dist = std::sqrt(dx * dx + dy * dy);
        if (dist < best_dist) {
            best_index = i;
            best_dist = dist;
        }
    }

    return best_index;
}

int ToolContext::findNearestEdge(const QPointF& worldPos, double tolerance) const {
    if (!m_mesh_nodes || m_mesh_nodes->empty()) return -1;

    // Edge index is encoded as quad_elem_index * 4 + local_edge (0..3)
    // We do not have the mesh here, so return -1 for now.
    // Edge selection is handled by the viewport which has mesh access.
    return -1;
}

std::vector<int> ToolContext::findNodesInRect(const QRectF& rect) const {
    std::vector<int> result;
    if (!m_mesh_nodes || m_mesh_nodes->empty()) return result;

    for (int i = 0; i < static_cast<int>(m_mesh_nodes->size()); ++i) {
        QPointF node_pos((*m_mesh_nodes)[i].x, (*m_mesh_nodes)[i].y);
        if (rect.contains(node_pos)) {
            result.push_back(i);
        }
    }

    return result;
}

// ------------------------------------------------------------------
// SELECT tool
// ------------------------------------------------------------------
void ToolContext::handleSelectPress(const QPointF& worldPos) {
    // Priority 1: Check if clicking on a node (when mesh exists)
    int node_idx = findNearestNode(worldPos, m_state->snap_tolerance);
    if (node_idx >= 0) {
        if (QApplication::keyboardModifiers() & Qt::ControlModifier) {
            m_selection->toggleNodeSelection(node_idx);
        } else {
            m_selection->selectNode(node_idx);
        }
        m_state->selected_node_index = node_idx;
        m_state->selected_primitive_index = -1;
        m_state->selected_edge_index = -1;
        emit nodeSelected(node_idx);
        emit statusMessage(QString("Selected node %1").arg(node_idx));
        return;
    }

    // Priority 2: Check if clicking on a primitive
    int prim_idx = m_geometry->findNearestPrimitive(worldPos, m_state->snap_tolerance);
    if (prim_idx >= 0) {
        m_state->selected_primitive_index = prim_idx;
        m_state->selected_node_index = -1;
        m_state->selected_edge_index = -1;
        m_selection->clearSelection();
        emit primitiveSelected(prim_idx);
        emit statusMessage(QString("Selected primitive %1").arg(prim_idx));

        // Start drag-move on next mouse move (drag_start is set here)
        m_state->is_dragging = false;
        m_state->drag_start = worldPos;
        m_state->drag_current = worldPos;
        return;
    }

    // Click on empty space: start drag selection or clear
    if (QApplication::keyboardModifiers() & Qt::ControlModifier) {
        // Start additive drag selection
        m_state->is_selecting = true;
        m_state->selection_start = worldPos;
        m_state->selection_end = worldPos;
        m_selection->startSelection(worldPos);
    } else {
        // Clear selection
        m_selection->clearSelection();
        m_state->selected_node_index = -1;
        m_state->selected_primitive_index = -1;
        m_state->selected_edge_index = -1;
        emit statusMessage("Selection cleared");
    }
}

void ToolContext::handleSelectMove(const QPointF& worldPos) {
    if (m_state->is_selecting) {
        m_state->selection_end = worldPos;
        m_selection->updateSelection(worldPos);
        return;
    }

    // If a primitive was pressed and mouse moved beyond a threshold, start drag-move
    if (m_state->selected_primitive_index >= 0) {
        double dx = worldPos.x() - m_state->drag_start.x();
        double dy = worldPos.y() - m_state->drag_start.y();
        double dist = std::sqrt(dx * dx + dy * dy);
        if (dist > 0.005) {
            m_state->is_dragging = true;
            m_state->drag_current = worldPos;
        }
    }
}

void ToolContext::handleSelectRelease(const QPointF& worldPos) {
    if (m_state->is_selecting) {
        m_state->selection_end = worldPos;
        m_selection->updateSelection(worldPos);

        // Select all nodes in the drag rectangle
        QRectF rect = m_selection->selectionRect();
        std::vector<int> nodes = findNodesInRect(rect);
        for (int n : nodes) {
            m_selection->toggleNodeSelection(n);
        }

        m_selection->finishSelection();
        m_state->is_selecting = false;

        emit statusMessage(QString("Selected %1 nodes in box").arg(
            static_cast<int>(nodes.size())));
    }
}

// ------------------------------------------------------------------
// Drag-move for selected primitives
// ------------------------------------------------------------------
void ToolContext::handleDragMove(const QPointF& worldPos) {
    m_state->drag_current = worldPos;
}

void ToolContext::handleDragRelease(const QPointF& worldPos) {
    if (!m_state->is_dragging) return;

    m_state->drag_current = worldPos;
    m_state->is_dragging = false;

    int idx = m_state->selected_primitive_index;
    if (idx < 0 || idx >= m_geometry->primitiveCount()) return;

    double dx = worldPos.x() - m_state->drag_start.x();
    double dy = worldPos.y() - m_state->drag_start.y();

    if (std::abs(dx) < 1e-8 && std::abs(dy) < 1e-8) return;

    // Move the primitive by updating its coordinates
    const auto& prims = m_geometry->primitives();
    GeometryPrimitive prim = prims[idx];

    std::visit([&](auto& p) {
        using T = std::decay_t<decltype(p)>;
        if constexpr (std::is_same_v<T, RectPrimitive>) {
            p.x += dx;
            p.y += dy;
        } else if constexpr (std::is_same_v<T, LinePrimitive>) {
            p.x1 += dx;
            p.y1 += dy;
            p.x2 += dx;
            p.y2 += dy;
        } else if constexpr (std::is_same_v<T, CirclePrimitive>) {
            p.cx += dx;
            p.cy += dy;
        }
    }, prim);

    // Remove old, add new (simplified; a proper undo would store both)
    if (m_undoStack) {
        m_undoStack->push(new RemovePrimitiveCommand(m_geometry, idx, "Move Primitive"));
        m_undoStack->push(new AddPrimitiveCommand(m_geometry, std::move(prim), "Move Primitive"));
    } else {
        m_geometry->removePrimitive(idx);
        m_geometry->addPrimitive(std::move(prim));
    }

    emit geometryChanged();
    emit statusMessage(QString("Moved primitive by (%1, %2)").arg(dx, 0, 'f', 4).arg(dy, 0, 'f', 4));
}

// ------------------------------------------------------------------
// DRAW_RECT tool
// ------------------------------------------------------------------
void ToolContext::handleDrawRectPress(const QPointF& worldPos) {
    m_state->is_drawing = true;
    m_state->draw_start = worldPos;
    m_state->draw_current = worldPos;
    emit statusMessage("Drawing rectangle (drag to size)");
}

void ToolContext::handleDrawRectMove(const QPointF& worldPos) {
    if (m_state->is_drawing) {
        m_state->draw_current = worldPos;
    }
}

void ToolContext::handleDrawRectRelease(const QPointF& worldPos) {
    if (!m_state->is_drawing) return;

    m_state->draw_current = worldPos;
    m_state->is_drawing = false;

    double dx = m_state->draw_current.x() - m_state->draw_start.x();
    double dy = m_state->draw_current.y() - m_state->draw_start.y();

    // Require minimum size
    if (std::abs(dx) < 0.005 || std::abs(dy) < 0.005) {
        emit statusMessage("Rectangle too small, cancelled");
        return;
    }

    RectPrimitive rect;
    rect.x = m_state->draw_start.x();
    rect.y = m_state->draw_start.y();
    rect.width = dx;
    rect.height = dy;
    rect.label = QString("Rect%1").arg(m_geometry->primitiveCount());

    if (m_undoStack) {
        m_undoStack->push(new AddPrimitiveCommand(
            m_geometry, std::move(rect), "Draw Rectangle"));
    } else {
        m_geometry->addPrimitive(std::move(rect));
    }

    emit geometryChanged();
    emit statusMessage(QString("Added rectangle (%1 elements)").arg(
        m_geometry->primitiveCount()));
}

// ------------------------------------------------------------------
// DRAW_LINE tool
// ------------------------------------------------------------------
void ToolContext::handleDrawLinePress(const QPointF& worldPos) {
    m_state->is_drawing = true;
    m_state->draw_start = worldPos;
    m_state->draw_current = worldPos;
    emit statusMessage("Drawing line (click end point)");
}

void ToolContext::handleDrawLineMove(const QPointF& worldPos) {
    if (m_state->is_drawing) {
        m_state->draw_current = worldPos;
    }
}

void ToolContext::handleDrawLineRelease(const QPointF& worldPos) {
    if (!m_state->is_drawing) return;

    m_state->draw_current = worldPos;
    m_state->is_drawing = false;

    double dx = m_state->draw_current.x() - m_state->draw_start.x();
    double dy = m_state->draw_current.y() - m_state->draw_start.y();
    double len = std::sqrt(dx * dx + dy * dy);

    if (len < 0.005) {
        emit statusMessage("Line too short, cancelled");
        return;
    }

    LinePrimitive line;
    line.x1 = m_state->draw_start.x();
    line.y1 = m_state->draw_start.y();
    line.x2 = m_state->draw_current.x();
    line.y2 = m_state->draw_current.y();
    line.label = QString("Line%1").arg(m_geometry->primitiveCount());

    if (m_undoStack) {
        m_undoStack->push(new AddPrimitiveCommand(
            m_geometry, std::move(line), "Draw Line"));
    } else {
        m_geometry->addPrimitive(std::move(line));
    }

    emit geometryChanged();
    emit statusMessage(QString("Added line (length=%.4f)").arg(len));
}

// ------------------------------------------------------------------
// DRAW_CIRCLE tool
// ------------------------------------------------------------------
void ToolContext::handleDrawCirclePress(const QPointF& worldPos) {
    m_state->is_drawing = true;
    m_state->draw_start = worldPos;
    m_state->draw_current = worldPos;
    emit statusMessage("Drawing circle (drag for radius)");
}

void ToolContext::handleDrawCircleMove(const QPointF& worldPos) {
    if (m_state->is_drawing) {
        m_state->draw_current = worldPos;
    }
}

void ToolContext::handleDrawCircleRelease(const QPointF& worldPos) {
    if (!m_state->is_drawing) return;

    m_state->draw_current = worldPos;
    m_state->is_drawing = false;

    double dx = m_state->draw_current.x() - m_state->draw_start.x();
    double dy = m_state->draw_current.y() - m_state->draw_start.y();
    double radius = std::sqrt(dx * dx + dy * dy);

    if (radius < 0.005) {
        emit statusMessage("Circle too small, cancelled");
        return;
    }

    CirclePrimitive circle;
    circle.cx = m_state->draw_start.x();
    circle.cy = m_state->draw_start.y();
    circle.radius = radius;
    circle.label = QString("Circle%1").arg(m_geometry->primitiveCount());

    if (m_undoStack) {
        m_undoStack->push(new AddPrimitiveCommand(
            m_geometry, std::move(circle), "Draw Circle"));
    } else {
        m_geometry->addPrimitive(std::move(circle));
    }

    emit geometryChanged();
    emit statusMessage(QString("Added circle (r=%.4f)").arg(radius));
}

// ------------------------------------------------------------------
// BC assignment tools (FIXED, ROLLER_X, ROLLER_Y)
// ------------------------------------------------------------------
void ToolContext::handleBCPress(const QPointF& worldPos) {
    int node_idx = findNearestNode(worldPos, m_state->snap_tolerance);
    if (node_idx < 0) {
        emit statusMessage("No node nearby to assign BC");
        return;
    }

    BCType type;
    QString type_name;
    switch (m_state->current_mode) {
    case ToolMode::ASSIGN_FIXED:
        type = BCType::FIXED;
        type_name = "Fixed";
        break;
    case ToolMode::ASSIGN_ROLLER_X:
        type = BCType::ROLLER_X;
        type_name = "Roller (X-fixed)";
        break;
    case ToolMode::ASSIGN_ROLLER_Y:
        type = BCType::ROLLER_Y;
        type_name = "Roller (Y-fixed)";
        break;
    default:
        return;
    }

    BoundaryCondition bc;
    bc.node_index = node_idx;
    bc.type = type;
    bc.value = 0.0;
    bc.group = type_name;

    if (m_undoStack) {
        m_undoStack->push(new AddBCCommand(
            m_bc_model, bc, QString("Assign %1 BC").arg(type_name)));
    } else {
        m_bc_model->addBC(bc);
    }

    emit nodeSelected(node_idx);
    emit statusMessage(QString("%1 BC assigned to node %2").arg(type_name).arg(node_idx));
}

// ------------------------------------------------------------------
// APPLY_FORCE tool
// ------------------------------------------------------------------
void ToolContext::handleForcePress(const QPointF& worldPos) {
    int node_idx = findNearestNode(worldPos, m_state->snap_tolerance);
    if (node_idx < 0) {
        emit statusMessage("No node nearby to apply force");
        return;
    }

    // Convert angle and magnitude to force components
    double angle_rad = m_state->pending_force_angle_deg * M_PI / 180.0;
    double fx = m_state->pending_force_magnitude * std::cos(angle_rad);
    double fy = m_state->pending_force_magnitude * std::sin(angle_rad);

    BoundaryCondition bc;
    bc.node_index = node_idx;
    bc.type = BCType::FORCE;
    bc.value = m_state->pending_force_magnitude;
    bc.angle = m_state->pending_force_angle_deg;
    bc.group = "Force";

    if (m_undoStack) {
        m_undoStack->push(new AddBCCommand(
            m_bc_model, bc, "Apply Force"));
    } else {
        m_bc_model->addBC(bc);
    }

    emit nodeSelected(node_idx);
    emit statusMessage(QString("Force %.1f N at %.1f deg applied to node %2")
        .arg(m_state->pending_force_magnitude)
        .arg(m_state->pending_force_angle_deg)
        .arg(node_idx));
}

// ------------------------------------------------------------------
// Delete selected object
// ------------------------------------------------------------------
void ToolContext::deleteSelected() {
    // Delete selected primitive
    if (m_state->selected_primitive_index >= 0) {
        int idx = m_state->selected_primitive_index;
        if (idx < m_geometry->primitiveCount()) {
            if (m_undoStack) {
                m_undoStack->push(new RemovePrimitiveCommand(m_geometry, idx, "Delete Primitive"));
            } else {
                m_geometry->removePrimitive(idx);
            }
            m_state->selected_primitive_index = -1;
            emit geometryChanged();
            emit statusMessage("Primitive deleted");
        }
    }
    // Delete selected BC (by node index)
    else if (m_state->selected_node_index >= 0) {
        int node_idx = m_state->selected_node_index;
        if (m_bc_model->hasBC(node_idx)) {
            if (m_undoStack) {
                m_undoStack->push(new RemoveBCCommand(m_bc_model, node_idx, "Delete BC"));
            } else {
                m_bc_model->removeBC(node_idx);
            }
            m_state->selected_node_index = -1;
            emit geometryChanged();
            emit statusMessage(QString("BC deleted from node %1").arg(node_idx));
        }
    }
    // Delete all selected nodes (from drag selection)
    else if (m_selection && !m_selection->selectedNodes().empty()) {
        const auto& selected = m_selection->selectedNodes();
        int count = 0;
        for (int node_idx : selected) {
            if (m_bc_model->hasBC(node_idx)) {
                if (m_undoStack) {
                    m_undoStack->push(new RemoveBCCommand(m_bc_model, node_idx, "Delete BC"));
                } else {
                    m_bc_model->removeBC(node_idx);
                }
                count++;
            }
        }
        m_selection->clearSelection();
        emit geometryChanged();
        emit statusMessage(QString("Deleted %1 BCs").arg(count));
    }
}
