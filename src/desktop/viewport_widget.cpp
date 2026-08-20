#include "viewport_widget.hpp"
#include "editor_state.hpp"
#include "geometry_model.hpp"
#include "geometry_primitive.hpp"
#include "bc_model.hpp"
#include "selection_model.hpp"
#include "tool_context.hpp"
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QMenu>
#include <cmath>

ViewportWidget::ViewportWidget(QWidget* parent)
    : QWidget(parent) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    
    // Periodic update timer to prevent blank screen
    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, [this]() {
        update();
    });
    m_updateTimer->start(100); // Update every 100ms

    // Deformation animation timer (~60 FPS)
    m_animTimer = new QTimer(this);
    connect(m_animTimer, &QTimer::timeout, this, &ViewportWidget::onAnimationTick);
    m_animTimer->setInterval(16);
}

void ViewportWidget::setMeshAndResults(const Mesh& mesh, const fea::SolveResult& result) {
    m_mesh = mesh;
    m_result = result;
    m_hasData = true;
    m_hasMesh = true;
    m_panX = 0.5;
    m_panY = 0.5;
    m_zoom = 1.0;

    // Reset animation state when new results arrive
    m_animPlaying = false;
    m_animActive = false;
    m_animProgress = 0.0;
    m_animPausedElapsed = 0.0;
    if (m_animTimer) m_animTimer->stop();

    // Clear cached streamline paths (will recompute on next toggle)
    m_streamlineResult.paths.clear();

    update();
}

void ViewportWidget::setMesh(const Mesh& mesh) {
    m_mesh = mesh;
    m_hasMesh = true;
    update();
}

void ViewportWidget::setContourField(ContourField field) {
    m_field = field;
    update();
}

void ViewportWidget::setColormap(ColormapType cmap) {
    m_colormap = cmap;
    update();
}

void ViewportWidget::setDisplacementScale(double scale) {
    m_dispScale = scale;
    update();
}

void ViewportWidget::toggleUndeformed(bool show) {
    m_showUndeformed = show;
    update();
}

void ViewportWidget::toggleDeformed(bool show) {
    m_showDeformed = show;
    update();
}

void ViewportWidget::toggleEdges(bool show) {
    m_showEdges = show;
    update();
}

void ViewportWidget::toggleArrows(bool show) {
    m_showArrows = show;
    update();
}

void ViewportWidget::toggleBoundary(bool show) {
    m_showBoundary = show;
    update();
}

void ViewportWidget::toggleStreamlines(bool show) {
    m_showStreamlines = show;
    if (m_showStreamlines && m_hasData && m_streamlineResult.paths.empty()) {
        computeStreamlines();
    }
    update();
}

void ViewportWidget::computeStreamlines() {
    if (!m_hasData || m_result.stresses.empty()) return;
    if (m_mesh.num_quads() == 0 && m_mesh.num_tris() == 0) return;

    // Run SPR recovery for smooth nodal stresses
    auto spr = adaptivity::spr_recovery(m_mesh, m_result.stresses);

    // Trace streamlines
    m_streamlineResult = streamline::trace_all(m_mesh, spr, m_streamlineConfig);
}

void ViewportWidget::resetView() {
    m_panX = 0.5;
    m_panY = 0.5;
    m_zoom = 1.0;
    update();
}

// ------------------------------------------------------------------
// Deformation animation
// ------------------------------------------------------------------
double ViewportWidget::easeInOutCubic(double t) {
    if (t < 0.5) {
        return 4.0 * t * t * t;
    }
    return 1.0 - std::pow(-2.0 * t + 2.0, 3) / 2.0;
}

void ViewportWidget::startAnimation() {
    if (!m_hasData || m_result.displacement.empty()) return;

    if (m_animPlaying) {
        pauseAnimation();
        return;
    }

    m_animActive = true;
    m_animPlaying = true;

    // If restarting from end, reset progress
    if (m_animProgress >= 1.0) {
        m_animProgress = 0.0;
        m_animPausedElapsed = 0.0;
    }

    m_animElapsed.start();
    m_animTimer->start();
    update();
}

void ViewportWidget::pauseAnimation() {
    if (!m_animPlaying) return;

    m_animPausedElapsed += m_animElapsed.elapsed();
    m_animTimer->stop();
    m_animPlaying = false;
    update();
}

void ViewportWidget::resetAnimation() {
    m_animTimer->stop();
    m_animPlaying = false;
    m_animActive = true;
    m_animProgress = 0.0;
    m_animPausedElapsed = 0.0;
    update();
}

void ViewportWidget::onAnimationTick() {
    if (!m_animPlaying) return;

    double totalElapsed = m_animPausedElapsed + m_animElapsed.elapsed();
    double rawProgress = totalElapsed / ANIM_DURATION_MS;

    if (rawProgress >= 1.0) {
        rawProgress = 1.0;
        m_animProgress = 1.0;
        m_animPlaying = false;
        m_animTimer->stop();
    } else {
        m_animProgress = rawProgress;
    }

    update();
}

// ------------------------------------------------------------------
// Editor integration setters
// ------------------------------------------------------------------
void ViewportWidget::setEditorState(EditorState* state) {
    m_editorState = state;
}

void ViewportWidget::setGeometryModel(GeometryModel* model) {
    m_geometryModel = model;
}

void ViewportWidget::setBCModel(BCModel* model) {
    m_bcModel = model;
}

void ViewportWidget::setSelectionModel(SelectionModel* model) {
    m_selectionModel = model;
}

void ViewportWidget::setToolContext(ToolContext* context) {
    m_toolContext = context;
}

// ------------------------------------------------------------------
// Coordinate conversion
// ------------------------------------------------------------------
QPointF ViewportWidget::widgetToWorld(const QPointF& widgetPos) const {
    double halfRange = 0.6 / m_zoom;
    double aspect = static_cast<double>(width()) / static_cast<double>(height());
    double wxMin = m_panX - halfRange * aspect;
    double wxMax = m_panX + halfRange * aspect;
    double wyMin = m_panY - halfRange;
    double wyMax = m_panY + halfRange;
    double wx = wxMin + (widgetPos.x() / width()) * (wxMax - wxMin);
    double wy = wyMax - (widgetPos.y() / height()) * (wyMax - wyMin);
    return QPointF(wx, wy);
}

QPointF ViewportWidget::worldToWidget(const QPointF& worldPos) const {
    double halfRange = 0.6 / m_zoom;
    double aspect = static_cast<double>(width()) / static_cast<double>(height());
    double wxMin = m_panX - halfRange * aspect;
    double wxMax = m_panX + halfRange * aspect;
    double wyMin = m_panY - halfRange;
    double wyMax = m_panY + halfRange;
    double px = (worldPos.x() - wxMin) / (wxMax - wxMin) * width();
    double py = (1.0 - (worldPos.y() - wyMin) / (wyMax - wyMin)) * height();
    return QPointF(px, py);
}

int ViewportWidget::findNearestNode(const QPointF& worldPos, double tolerance) const {
    if (m_mesh.num_nodes() == 0) return -1;
    
    int nearest = -1;
    double min_dist = tolerance * tolerance;
    
    for (int i = 0; i < m_mesh.num_nodes(); ++i) {
        const auto& node = m_mesh.nodes[i];
        double dx = worldPos.x() - node.x;
        double dy = worldPos.y() - node.y;
        double dist_sq = dx * dx + dy * dy;
        
        if (dist_sq < min_dist) {
            min_dist = dist_sq;
            nearest = i;
        }
    }
    
    return nearest;
}

QColor ViewportWidget::getColorForValue(double val) const {
    if (m_fieldMax == m_fieldMin) return QColor(128, 128, 128);

    double t = (val - m_fieldMin) / (m_fieldMax - m_fieldMin);
    t = std::max(0.0, std::min(1.0, t));

    double r = 0.0, g = 0.0, b = 0.0;

    switch (m_colormap) {
    case ColormapType::TURBO: {
        if (t < 0.25) {
            double s = t / 0.25;
            r = 0.19 * s; g = 0.33 * s; b = 0.61 + 0.19 * s;
        } else if (t < 0.5) {
            double s = (t - 0.25) / 0.25;
            r = 0.19 + 0.76 * s; g = 0.33 + 0.65 * s; b = 0.80 - 0.59 * s;
        } else if (t < 0.75) {
            double s = (t - 0.5) / 0.25;
            r = 0.95 - 0.03 * s; g = 0.98 - 0.58 * s; b = 0.21 - 0.15 * s;
        } else {
            double s = (t - 0.75) / 0.25;
            r = 0.92 + 0.08 * s; g = 0.40 - 0.37 * s; b = 0.06 - 0.06 * s;
        }
        break;
    }
    case ColormapType::VIRIDIS: {
        if (t < 0.33) {
            double s = t / 0.33;
            r = 0.27 - 0.14 * s; g = 0.00 + 0.22 * s; b = 0.38 - 0.05 * s;
        } else if (t < 0.66) {
            double s = (t - 0.33) / 0.33;
            r = 0.13 + 0.62 * s; g = 0.22 + 0.56 * s; b = 0.33 - 0.33 * s;
        } else {
            double s = (t - 0.66) / 0.34;
            r = 0.75 + 0.25 * s; g = 0.78 + 0.22 * s; b = 0.00 + 0.15 * s;
        }
        break;
    }
    case ColormapType::HOT: {
        if (t < 0.33) {
            double s = t / 0.33;
            r = 0.90 * s; g = 0.0; b = 0.0;
        } else if (t < 0.66) {
            double s = (t - 0.33) / 0.33;
            r = 0.90 + 0.10 * s; g = 0.88 * s; b = 0.0;
        } else {
            double s = (t - 0.66) / 0.34;
            r = 1.0; g = 0.88 + 0.12 * s; b = 0.0 + 1.0 * s;
        }
        break;
    }
    case ColormapType::COOLWARM: {
        if (t < 0.5) {
            double s = t / 0.5;
            r = 0.23 + 0.64 * s; g = 0.30 + 0.57 * s; b = 0.75 - 0.25 * s;
        } else {
            double s = (t - 0.5) / 0.5;
            r = 0.87 + 0.13 * s; g = 0.87 - 0.51 * s; b = 0.50 - 0.37 * s;
        }
        break;
    }
    case ColormapType::RDBU_R: {
        if (t < 0.5) {
            double s = t / 0.5;
            r = 0.05 + 0.85 * s; g = 0.24 + 0.63 * s; b = 0.56 - 0.11 * s;
        } else {
            double s = (t - 0.5) / 0.5;
            r = 0.90 - 0.28 * s; g = 0.87 - 0.64 * s; b = 0.45 + 0.13 * s;
        }
        break;
    }
    }

    return QColor(
        static_cast<int>(r * 255),
        static_cast<int>(g * 255),
        static_cast<int>(b * 255)
    );
}

double ViewportWidget::getFieldValueForNode(int nodeIdx) const {
    if (m_result.displacement.empty()) return 0.0;
    switch (m_field) {
    case ContourField::DISPLACEMENT_MAG: {
        double ux = m_result.displacement[nodeIdx * 2];
        double uy = m_result.displacement[nodeIdx * 2 + 1];
        return std::sqrt(ux * ux + uy * uy);
    }
    default:
        return 0.0;
    }
}

double ViewportWidget::getFieldValueForElement(int elemIdx) const {
    if (m_result.stresses.empty()) return 0.0;
    if (elemIdx < 0 || elemIdx >= static_cast<int>(m_result.stresses.size())) return 0.0;
    const auto& s = m_result.stresses[elemIdx];
    switch (m_field) {
    case ContourField::VON_MISES: return s.von_mises;
    case ContourField::SIGMA_XX: return s.sigma_xx;
    case ContourField::SIGMA_YY: return s.sigma_yy;
    case ContourField::SIGMA_XY: return s.sigma_xy;
    case ContourField::SIGMA_1: return s.sigma_1;
    case ContourField::SIGMA_2: return s.sigma_2;
    case ContourField::DISPLACEMENT_MAG: return 0.0;
    default: return 0.0;
    }
}

void ViewportWidget::updateFieldRange() {
    m_fieldMin = 1e30;
    m_fieldMax = -1e30;

    if (m_field == ContourField::DISPLACEMENT_MAG) {
        // Displacement is a nodal field
        for (int i = 0; i < m_mesh.num_nodes(); ++i) {
            double val = getFieldValueForNode(i);
            m_fieldMin = std::min(m_fieldMin, val);
            m_fieldMax = std::max(m_fieldMax, val);
        }
    } else if (!m_result.stresses.empty()) {
        // Stress is an element field
        for (size_t i = 0; i < m_result.stresses.size(); ++i) {
            double val = getFieldValueForElement(static_cast<int>(i));
            m_fieldMin = std::min(m_fieldMin, val);
            m_fieldMax = std::max(m_fieldMax, val);
        }
    }

    if (m_fieldMin >= m_fieldMax) {
        m_fieldMin = 0.0;
        m_fieldMax = 1.0;
    }
}

// ------------------------------------------------------------------
// Geometry primitive rendering
// ------------------------------------------------------------------
void ViewportWidget::drawGeometryPrimitives(QPainter& painter) {
    if (!m_geometryModel) return;

    const auto& prims = m_geometryModel->primitives();
    for (int i = 0; i < static_cast<int>(prims.size()); ++i) {
        bool selected = (m_editorState && m_editorState->selected_primitive_index == i);

        std::visit([&](const auto& p) {
            using T = std::decay_t<decltype(p)>;

            if constexpr (std::is_same_v<T, RectPrimitive>) {
                double left   = p.x;
                double right  = p.x + p.width;
                double bottom = p.y;
                double top    = p.y + p.height;
                if (left > right) std::swap(left, right);
                if (bottom > top) std::swap(bottom, top);

                QPointF tl = worldToWidget(QPointF(left, top));
                QPointF br = worldToWidget(QPointF(right, bottom));
                QRectF rect(tl, br);

                QPen pen(selected ? QColor(0x00, 0xff, 0xff) : QColor(0x58, 0xa6, 0xff),
                         selected ? 2.0 : 1.0);
                painter.setPen(pen);
                painter.setBrush(QBrush(QColor(0x58, 0xa6, 0xff, 30)));
                painter.drawRect(rect);

            } else if constexpr (std::is_same_v<T, LinePrimitive>) {
                QPointF p1 = worldToWidget(QPointF(p.x1, p.y1));
                QPointF p2 = worldToWidget(QPointF(p.x2, p.y2));

                QPen pen(selected ? QColor(0x00, 0xff, 0xff) : QColor(0x3f, 0xb9, 0x50),
                         selected ? 2.0 : 1.5);
                painter.setPen(pen);
                painter.drawLine(p1, p2);

            } else if constexpr (std::is_same_v<T, CirclePrimitive>) {
                QPointF center = worldToWidget(QPointF(p.cx, p.cy));
                // Approximate radius in screen pixels
                QPointF edge = worldToWidget(QPointF(p.cx + p.radius, p.cy));
                double radius_px = std::abs(edge.x() - center.x());

                QPen pen(selected ? QColor(0x00, 0xff, 0xff) : QColor(0xf0, 0x88, 0x3e),
                         selected ? 2.0 : 1.0);
                painter.setPen(pen);
                painter.setBrush(QBrush(QColor(0xf0, 0x88, 0x3e, 30)));
                painter.drawEllipse(center, radius_px, radius_px);
            }
        }, prims[i]);
    }
}

// ------------------------------------------------------------------
// BC overlay rendering
// ------------------------------------------------------------------
void ViewportWidget::drawBCOverlay(QPainter& painter) {
    if (!m_bcModel) return;

    // Get mesh nodes for coordinate lookup
    const auto& bcs = m_bcModel->bcs();
    for (const auto& bc : bcs) {
        if (bc.node_index < 0 || bc.node_index >= m_mesh.num_nodes()) continue;

        const auto& node = m_mesh.nodes[bc.node_index];
        QPointF pos = worldToWidget(QPointF(node.x, node.y));

        switch (bc.type) {
        case BCType::FIXED: {
            // Yellow triangle pointing down (fixed support)
            double s = 0.015;
            painter.setPen(Qt::NoPen);
            painter.setBrush(QBrush(QColor(0xff, 0xff, 0x00, 230)));
            QPolygonF tri;
            tri << pos
                << worldToWidget(QPointF(node.x - s, node.y - s * 1.5))
                << worldToWidget(QPointF(node.x + s, node.y - s * 1.5));
            painter.drawPolygon(tri);
            break;
        }
        case BCType::ROLLER_X: {
            // Circle (roller in X, fixed in Y)
            double s = 0.012;
            painter.setPen(QPen(QColor(0x00, 0xaa, 0xff, 230), 1.5));
            painter.setBrush(Qt::NoBrush);
            QPointF center = pos;
            double radius = std::abs(worldToWidget(QPointF(s, 0)).x() - worldToWidget(QPointF(0, 0)).x());
            painter.drawEllipse(center, radius, radius);
            break;
        }
        case BCType::ROLLER_Y: {
            // Circle (roller in Y, fixed in X)
            double s = 0.012;
            painter.setPen(QPen(QColor(0x00, 0xaa, 0xff, 230), 1.5));
            painter.setBrush(Qt::NoBrush);
            QPointF center = pos;
            double radius = std::abs(worldToWidget(QPointF(s, 0)).x() - worldToWidget(QPointF(0, 0)).x());
            painter.drawEllipse(center, radius, radius);
            break;
        }
        case BCType::FORCE: {
            // Green/red arrow for force - use angle from BC
            double angle_rad = bc.angle * M_PI / 180.0;
            double len = 0.04;
            QPointF tip = worldToWidget(QPointF(
                node.x + len * std::cos(angle_rad),
                node.y + len * std::sin(angle_rad)));

            painter.setPen(QPen(QColor(0x00, 0xff, 0x00), 2));
            painter.drawLine(pos, tip);

            // Arrowhead
            double head_len = 0.01;
            double head_angle = 0.4;
            QPointF h1 = worldToWidget(QPointF(
                node.x + (len - head_len) * std::cos(angle_rad - head_angle),
                node.y + (len - head_len) * std::sin(angle_rad - head_angle)));
            QPointF h2 = worldToWidget(QPointF(
                node.x + (len - head_len) * std::cos(angle_rad + head_angle),
                node.y + (len - head_len) * std::sin(angle_rad + head_angle)));

            QPolygonF arrow;
            arrow << tip << h1 << h2;
            painter.setBrush(QBrush(QColor(0x00, 0xff, 0x00)));
            painter.setPen(Qt::NoPen);
            painter.drawPolygon(arrow);
            break;
        }
        }
    }
}

// ------------------------------------------------------------------
// Selection highlight
// ------------------------------------------------------------------
void ViewportWidget::drawSelectionHighlight(QPainter& painter) {
    if (!m_selectionModel || !m_mesh.num_nodes()) return;

    const auto& selected = m_selectionModel->selectedNodes();
    if (selected.empty()) return;

    painter.setPen(Qt::NoPen);
    painter.setBrush(QBrush(QColor(0x00, 0xff, 0xff, 180)));

    for (int node_idx : selected) {
        if (node_idx < 0 || node_idx >= m_mesh.num_nodes()) continue;
        const auto& node = m_mesh.nodes[node_idx];
        QPointF pos = worldToWidget(QPointF(node.x, node.y));

        double radius = 4.0;
        painter.drawEllipse(pos, radius, radius);
    }
}

// ------------------------------------------------------------------
// Snap indicator
// ------------------------------------------------------------------
void ViewportWidget::drawSnapIndicator(QPainter& painter, const QPointF& worldPos) {
    QPointF pos = worldToWidget(worldPos);
    painter.setPen(QPen(QColor(0x00, 0xff, 0xff, 120), 1, Qt::DashLine));
    painter.setBrush(Qt::NoBrush);

    double snap_size = 8.0;
    painter.drawEllipse(pos, snap_size, snap_size);
}

// ------------------------------------------------------------------
// Force arrow with label
// ------------------------------------------------------------------
void ViewportWidget::drawForceArrow(QPainter& painter, const QPointF& pos,
                                     double fx, double fy, double angle) {
    Q_UNUSED(fx)
    Q_UNUSED(fy)
    Q_UNUSED(angle)

    QPointF widgetPos = worldToWidget(pos);
    painter.setPen(QPen(QColor(0x00, 0xff, 0x00), 2));
    painter.setBrush(QBrush(QColor(0x00, 0xff, 0x00, 180)));

    // Draw a simple arrow marker
    double arrow_size = 6.0;
    QPolygonF arrow;
    arrow << QPointF(widgetPos.x(), widgetPos.y() - arrow_size)
          << QPointF(widgetPos.x() - arrow_size * 0.5, widgetPos.y())
          << QPointF(widgetPos.x() + arrow_size * 0.5, widgetPos.y());
    painter.drawPolygon(arrow);
}

// ------------------------------------------------------------------
// Drag selection rectangle
// ------------------------------------------------------------------
void ViewportWidget::drawDragRectangle(QPainter& painter) {
    if (!m_editorState || !m_editorState->is_selecting) return;

    QPointF tl = worldToWidget(m_editorState->selection_start);
    QPointF br = worldToWidget(m_editorState->selection_end);

    QRectF rect(tl, br);
    painter.setPen(QPen(QColor(0x00, 0xff, 0xff, 150), 1, Qt::DashLine));
    painter.setBrush(QBrush(QColor(0x00, 0xff, 0xff, 20)));
    painter.drawRect(rect);
}

// ------------------------------------------------------------------
// Pending shape preview (while drawing)
// ------------------------------------------------------------------
void ViewportWidget::drawPendingShape(QPainter& painter) {
    if (!m_editorState || !m_editorState->is_drawing) return;

    QPointF start = worldToWidget(m_editorState->draw_start);
    QPointF current = worldToWidget(m_editorState->draw_current);

    painter.setPen(QPen(QColor(0x00, 0xff, 0xff, 180), 1.5, Qt::DashLine));
    painter.setBrush(Qt::NoBrush);

    switch (m_editorState->current_mode) {
    case ToolMode::DRAW_RECT: {
        QRectF rect(start, current);
        painter.drawRect(rect);
        break;
    }
    case ToolMode::DRAW_LINE: {
        painter.drawLine(start, current);
        break;
    }
    case ToolMode::DRAW_CIRCLE: {
        double dx = current.x() - start.x();
        double dy = current.y() - start.y();
        double radius = std::sqrt(dx * dx + dy * dy);
        painter.drawEllipse(start, radius, radius);
        break;
    }
    default:
        break;
    }
}

// ------------------------------------------------------------------
// Mesh node visualization (editor mode, before solve)
// ------------------------------------------------------------------
void ViewportWidget::drawMeshNodes(QPainter& painter) {
    if (!m_hasMesh || m_hasData) return;
    if (m_mesh.num_nodes() == 0) return;

    // Determine which nodes are boundary (have BCs)
    bool hasBCs = m_bcModel && m_bcModel->bcCount() > 0;

    for (int i = 0; i < m_mesh.num_nodes(); ++i) {
        const auto& node = m_mesh.nodes[i];
        QPointF pos = worldToWidget(QPointF(node.x, node.y));

        // Check if this node has a BC
        bool isBoundary = hasBCs && m_bcModel->hasBC(i);

        // Check if selected
        bool isSelected = m_selectionModel && m_selectionModel->isNodeSelected(i);

        if (isSelected) {
            painter.setPen(QPen(QColor(0x00, 0xff, 0xff), 1.5));
            painter.setBrush(QBrush(QColor(0x00, 0xff, 0xff, 200)));
            painter.drawEllipse(pos, 4.0, 4.0);
        } else if (isBoundary) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QBrush(QColor(0xff, 0x44, 0x44, 200)));
            painter.drawEllipse(pos, 2.5, 2.5);
        } else {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QBrush(QColor(0x55, 0x88, 0xff, 160)));
            painter.drawEllipse(pos, 2.0, 2.0);
        }
    }
}

// ------------------------------------------------------------------
// Mesh edge visualization (editor mode, before solve)
// ------------------------------------------------------------------
void ViewportWidget::drawMeshEdges(QPainter& painter) {
    if (!m_hasMesh || m_hasData) return;
    if (m_mesh.num_quads() == 0 && m_mesh.num_tris() == 0) return;

    painter.setPen(QPen(QColor(0x44, 0x55, 0x66, 120), 0.5));
    painter.setBrush(Qt::NoBrush);

    for (int e = 0; e < m_mesh.num_quads(); ++e) {
        const auto& elem = m_mesh.quad_elements[e];
        for (int i = 0; i < 4; ++i) {
            int n0 = elem[i];
            int n1 = elem[(i + 1) % 4];
            painter.drawLine(
                worldToWidget(QPointF(m_mesh.nodes[n0].x, m_mesh.nodes[n0].y)),
                worldToWidget(QPointF(m_mesh.nodes[n1].x, m_mesh.nodes[n1].y)));
        }
    }

    for (int e = 0; e < m_mesh.num_tris(); ++e) {
        const auto& elem = m_mesh.tri_elements[e];
        for (int i = 0; i < 3; ++i) {
            int n0 = elem[i];
            int n1 = elem[(i + 1) % 3];
            painter.drawLine(
                worldToWidget(QPointF(m_mesh.nodes[n0].x, m_mesh.nodes[n0].y)),
                worldToWidget(QPointF(m_mesh.nodes[n1].x, m_mesh.nodes[n1].y)));
        }
    }
}

// ------------------------------------------------------------------
// Drag-move preview (shows where the primitive will land)
// ------------------------------------------------------------------
void ViewportWidget::drawDragMovePreview(QPainter& painter) {
    if (!m_editorState || !m_editorState->is_dragging) return;
    if (m_editorState->selected_primitive_index < 0) return;
    if (!m_geometryModel) return;

    int idx = m_editorState->selected_primitive_index;
    if (idx >= m_geometryModel->primitiveCount()) return;

    double dx = m_editorState->drag_current.x() - m_editorState->drag_start.x();
    double dy = m_editorState->drag_current.y() - m_editorState->drag_start.y();

    const auto& prims = m_geometryModel->primitives();
    const auto& prim = prims[idx];

    painter.setPen(QPen(QColor(0x00, 0xff, 0xff, 100), 1.0, Qt::DashDotLine));
    painter.setBrush(QBrush(QColor(0x00, 0xff, 0xff, 15)));

    std::visit([&](const auto& p) {
        using T = std::decay_t<decltype(p)>;
        if constexpr (std::is_same_v<T, RectPrimitive>) {
            double left   = p.x + dx;
            double right  = p.x + p.width + dx;
            double bottom = p.y + dy;
            double top    = p.y + p.height + dy;
            if (left > right) std::swap(left, right);
            if (bottom > top) std::swap(bottom, top);
            QPointF tl = worldToWidget(QPointF(left, top));
            QPointF br = worldToWidget(QPointF(right, bottom));
            painter.drawRect(QRectF(tl, br));
        } else if constexpr (std::is_same_v<T, LinePrimitive>) {
            QPointF p1 = worldToWidget(QPointF(p.x1 + dx, p.y1 + dy));
            QPointF p2 = worldToWidget(QPointF(p.x2 + dx, p.y2 + dy));
            painter.drawLine(p1, p2);
        } else if constexpr (std::is_same_v<T, CirclePrimitive>) {
            QPointF center = worldToWidget(QPointF(p.cx + dx, p.cy + dy));
            QPointF edge = worldToWidget(QPointF(p.cx + p.radius + dx, p.cy + dy));
            double radius_px = std::abs(edge.x() - center.x());
            painter.drawEllipse(center, radius_px, radius_px);
        }
    }, prim);
}

// ------------------------------------------------------------------
// Stress streamline visualization
// ------------------------------------------------------------------
void ViewportWidget::drawStreamlines(QPainter& painter) {
    if (!m_showStreamlines || m_streamlineResult.paths.empty()) return;

    auto toWidget = [&](double wx, double wy) -> QPointF {
        double halfRange = 0.6 / m_zoom;
        double aspect = static_cast<double>(width()) / static_cast<double>(height());
        double wxMin = m_panX - halfRange * aspect;
        double wxMax = m_panX + halfRange * aspect;
        double wyMin = m_panY - halfRange;
        double wyMax = m_panY + halfRange;
        double px = (wx - wxMin) / (wxMax - wxMin) * width();
        double py = (1.0 - (wy - wyMin) / (wyMax - wyMin)) * height();
        return QPointF(px, py);
    };

    double max_abs = std::max(std::abs(m_streamlineResult.max_sigma_1),
                              std::abs(m_streamlineResult.min_sigma_1));
    if (max_abs < 1.0e-10) max_abs = 1.0;

    for (const auto& path : m_streamlineResult.paths) {
        if (path.x.size() < 2) continue;

        // Draw path as a polyline colored by sigma_1
        for (size_t i = 0; i + 1 < path.x.size(); ++i) {
            double s = path.sigma_1[i];

            // Map sigma_1 to color: red for tension, blue for compression
            int r, g, b;
            if (s >= 0.0) {
                // Tension: white to red
                double t = std::min(s / max_abs, 1.0);
                r = 255;
                g = static_cast<int>(255 * (1.0 - t));
                b = static_cast<int>(255 * (1.0 - t));
            } else {
                // Compression: white to blue
                double t = std::min(-s / max_abs, 1.0);
                r = static_cast<int>(255 * (1.0 - t));
                g = static_cast<int>(255 * (1.0 - t));
                b = 255;
            }

            QPen pen(QColor(r, g, b, 200), 1.5);
            painter.setPen(pen);
            painter.drawLine(toWidget(path.x[i], path.y[i]),
                             toWidget(path.x[i + 1], path.y[i + 1]));
        }
    }
}

// ------------------------------------------------------------------
// Mouse events
// ------------------------------------------------------------------
void ViewportWidget::mousePressEvent(QMouseEvent* event) {
    m_lastMousePos = event->pos();

    // Middle mouse button always pans (regardless of tool mode)
    if (event->button() == Qt::MiddleButton) {
        m_isPanning = true;
        return;
    }

    // Right mouse button: show context menu or pan
    if (event->button() == Qt::RightButton) {
        // Check if right-clicked on an object
        QPointF worldPos = widgetToWorld(event->pos());
        
        // Check if right-clicked on a primitive
        if (m_geometryModel && m_editorState) {
            int prim_idx = m_geometryModel->findNearestPrimitive(worldPos, 0.05);
            if (prim_idx >= 0) {
                // Select the primitive
                m_editorState->selected_primitive_index = prim_idx;
                m_editorState->selected_node_index = -1;
                update();
                
                // Show context menu
                QMenu contextMenu(this);
                contextMenu.addAction("Edit Properties", this, [this, prim_idx]() {
                    emit primitiveClicked(prim_idx);
                });
                contextMenu.addAction("Delete", this, [this]() {
                    m_toolContext->deleteSelected();
                    update();
                });
                contextMenu.exec(event->globalPosition().toPoint());
                return;
            }
        }
        
        // Check if right-clicked on a BC node
        if (m_bcModel && m_mesh.num_nodes() > 0) {
            int node_idx = findNearestNode(worldPos, 0.05);
            if (node_idx >= 0 && m_bcModel->hasBC(node_idx)) {
                // Select the BC node
                m_editorState->selected_node_index = node_idx;
                m_editorState->selected_primitive_index = -1;
                update();
                
                // Show context menu
                QMenu contextMenu(this);
                contextMenu.addAction("Edit BC", this, [this, node_idx]() {
                    // TODO: Show BC edit dialog
                    // For now, just select the node
                    if (m_toolContext) {
                        m_toolContext->nodeSelected(node_idx);
                    }
                });
                contextMenu.addAction("Delete BC", this, [this, node_idx]() {
                    m_toolContext->deleteSelected();
                    update();
                });
                contextMenu.exec(event->globalPosition().toPoint());
                return;
            }
        }
        
        // Default: pan
        m_isPanning = true;
        return;
    }

    // Left mouse button: route to ToolContext if available
    if (event->button() == Qt::LeftButton && m_toolContext && m_editorState) {
        QPointF worldPos = widgetToWorld(event->pos());
        m_toolContext->handleMousePress(event, worldPos);
        update();
        return;
    }

    // Default pan behavior (left button when no tool context)
    if (event->button() == Qt::LeftButton) {
        m_lastMousePos = event->pos();
    }
}

void ViewportWidget::mouseMoveEvent(QMouseEvent* event) {
    // Handle panning with middle or right mouse button
    if (m_isPanning) {
        QPoint delta = event->pos() - m_lastMousePos;
        double halfRange = 0.6 / m_zoom;
        double aspect = static_cast<double>(width()) / static_cast<double>(height());
        m_panX -= static_cast<double>(delta.x()) * halfRange * aspect / width();
        m_panY += static_cast<double>(delta.y()) * halfRange / height();
        m_lastMousePos = event->pos();
        update();
        return;
    }

    // Route to ToolContext if available (for left button only)
    if (m_toolContext && m_editorState && (event->buttons() & Qt::LeftButton)) {
        QPointF worldPos = widgetToWorld(event->pos());
        m_toolContext->handleMouseMove(event, worldPos);
        update();
        return;
    }

    // Default pan behavior (left button when no tool context)
    if (event->buttons() & Qt::LeftButton) {
        QPoint delta = event->pos() - m_lastMousePos;
        double halfRange = 0.6 / m_zoom;
        double aspect = static_cast<double>(width()) / static_cast<double>(height());
        m_panX -= static_cast<double>(delta.x()) * halfRange * aspect / width();
        m_panY += static_cast<double>(delta.y()) * halfRange / height();
    }
    m_lastMousePos = event->pos();
    update();
}

void ViewportWidget::mouseReleaseEvent(QMouseEvent* event) {
    // Stop panning on middle or right button release
    if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton) {
        m_isPanning = false;
        return;
    }

    // Route left button release to ToolContext if available
    if (event->button() == Qt::LeftButton && m_toolContext && m_editorState) {
        QPointF worldPos = widgetToWorld(event->pos());
        m_toolContext->handleMouseRelease(event, worldPos);
        update();
        return;
    }
}

void ViewportWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    // Route to ToolContext if available
    if (m_toolContext && m_editorState) {
        QPointF worldPos = widgetToWorld(event->pos());
        m_toolContext->handleMouseDoubleClick(event, worldPos);
        update();
    }

    // Find element under cursor for drill-down inspector
    if (m_hasData) {
        QPointF worldPos = widgetToWorld(event->pos());

        // Point-in-polygon test for quad elements
        for (int e = 0; e < m_mesh.num_quads(); ++e) {
            const auto& elem = m_mesh.quad_elements[e];
            double px = worldPos.x();
            double py = worldPos.y();

            // Winding number algorithm for convex quad
            bool inside = true;
            for (int i = 0; i < 4; ++i) {
                int j = (i + 1) % 4;
                double ex0 = m_mesh.nodes[elem[i]].x;
                double ey0 = m_mesh.nodes[elem[i]].y;
                double ex1 = m_mesh.nodes[elem[j]].x;
                double ey1 = m_mesh.nodes[elem[j]].y;

                // Cross product of edge and point-to-vertex
                double cross = (ex1 - ex0) * (py - ey0) - (ey1 - ey0) * (px - ex0);
                if (cross < 0.0) {
                    inside = false;
                    break;
                }
            }
            if (inside) {
                emit elementDoubleClicked(e);
                update();
                return;
            }
        }

        // Point-in-polygon test for tri elements (cross-product winding)
        for (int e = 0; e < m_mesh.num_tris(); ++e) {
            const auto& elem = m_mesh.tri_elements[e];
            double px = worldPos.x();
            double py = worldPos.y();

            bool inside = true;
            for (int i = 0; i < 3; ++i) {
                int j = (i + 1) % 3;
                double ex0 = m_mesh.nodes[elem[i]].x;
                double ey0 = m_mesh.nodes[elem[i]].y;
                double ex1 = m_mesh.nodes[elem[j]].x;
                double ey1 = m_mesh.nodes[elem[j]].y;

                double cross = (ex1 - ex0) * (py - ey0) - (ey1 - ey0) * (px - ex0);
                if (cross < 0.0) {
                    inside = false;
                    break;
                }
            }
            if (inside) {
                emit elementDoubleClicked(m_mesh.num_quads() + e);
                update();
                return;
            }
        }
    }
}

void ViewportWidget::keyPressEvent(QKeyEvent* event) {
    // Delete key removes selected object
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        if (m_toolContext && m_editorState) {
            m_toolContext->deleteSelected();
            update();
            return;
        }
    }
    
    // Escape key clears selection and returns to pan mode
    if (event->key() == Qt::Key_Escape) {
        if (m_editorState) {
            m_editorState->current_mode = ToolMode::PAN_ZOOM;
            m_editorState->selected_primitive_index = -1;
            m_editorState->selected_node_index = -1;
            m_editorState->selected_edge_index = -1;
            if (m_selectionModel) {
                m_selectionModel->clearSelection();
            }
            update();
            return;
        }
    }
    
    QWidget::keyPressEvent(event);
}

void ViewportWidget::wheelEvent(QWheelEvent* event) {
    double factor = (event->angleDelta().y() > 0) ? 1.15 : 1.0 / 1.15;
    m_zoom *= factor;
    m_zoom = std::max(0.1, std::min(100.0, m_zoom));
    update();
}

// ------------------------------------------------------------------
// Paint event
// ------------------------------------------------------------------
void ViewportWidget::paintEvent(QPaintEvent*) {
    // Guard against invalid widget size
    if (width() <= 0 || height() <= 0) return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(0x0d, 0x11, 0x17));

    // Draw geometry primitives if in editor mode
    if (m_geometryModel && m_editorState) {
        drawGeometryPrimitives(painter);
        drawPendingShape(painter);
        drawDragRectangle(painter);
        drawDragMovePreview(painter);
        drawSelectionHighlight(painter);

        if (m_bcModel) {
            drawBCOverlay(painter);
        }

        // Draw mesh nodes/edges when mesh exists (editor mode, before solve)
        if (m_hasMesh && !m_hasData) {
            drawMeshEdges(painter);
            drawMeshNodes(painter);
        }

        if (!m_hasData && !m_hasMesh) {
            painter.setPen(QColor(0x8b, 0x94, 0x9e));
            painter.setFont(QFont("JetBrains Mono", 12));
            painter.drawText(rect(), Qt::AlignCenter, "Draw geometry or generate a mesh to begin");
            return;
        }
    }

    if (!m_hasData && !m_hasMesh) {
        painter.setPen(QColor(0x8b, 0x94, 0x9e));
        painter.setFont(QFont("JetBrains Mono", 12));
        painter.drawText(rect(), Qt::AlignCenter, "Load a case or mesh to begin");
        return;
    }

    if (!m_hasData) {
        return;
    }

    if (!m_result.displacement.empty()) {
        updateFieldRange();
    }

    // Compute effective displacement scale (animation or static)
    double effectiveScale = m_dispScale;
    if (m_animActive) {
        effectiveScale = easeInOutCubic(m_animProgress) * m_dispScale;
    }

    auto worldToWidget = [&](double wx, double wy) -> QPointF {
        double halfRange = 0.6 / m_zoom;
        double aspect = static_cast<double>(width()) / static_cast<double>(height());
        double wxMin = m_panX - halfRange * aspect;
        double wxMax = m_panX + halfRange * aspect;
        double wyMin = m_panY - halfRange;
        double wyMax = m_panY + halfRange;
        double px = (wx - wxMin) / (wxMax - wxMin) * width();
        double py = (1.0 - (wy - wyMin) / (wyMax - wyMin)) * height();
        return QPointF(px, py);
    };

    if (m_showUndeformed) {
        painter.setPen(QPen(QColor(0x25, 0x35, 0x45), 0, Qt::SolidLine));
        painter.setBrush(Qt::NoBrush);
        for (int e = 0; e < m_mesh.num_quads(); ++e) {
            const auto& elem = m_mesh.quad_elements[e];
            QPolygonF poly;
            for (int i = 0; i < 4; ++i) {
                poly << worldToWidget(m_mesh.nodes[elem[i]].x, m_mesh.nodes[elem[i]].y);
            }
            painter.drawPolygon(poly);
        }
        for (int e = 0; e < m_mesh.num_tris(); ++e) {
            const auto& elem = m_mesh.tri_elements[e];
            QPolygonF poly;
            for (int i = 0; i < 3; ++i) {
                poly << worldToWidget(m_mesh.nodes[elem[i]].x, m_mesh.nodes[elem[i]].y);
            }
            painter.drawPolygon(poly);
        }
    }

    if (m_showDeformed && !m_result.displacement.empty()) {
        for (int e = 0; e < m_mesh.num_quads(); ++e) {
            const auto& elem = m_mesh.quad_elements[e];
            painter.setPen(Qt::NoPen);
            QPolygonF poly;
            for (int i = 0; i < 4; ++i) {
                int n = elem[i];
                double ux = m_result.displacement[n * 2];
                double uy = m_result.displacement[n * 2 + 1];
                poly << worldToWidget(m_mesh.nodes[n].x + ux * effectiveScale,
                                       m_mesh.nodes[n].y + uy * effectiveScale);
            }
            // Use average nodal value for element color
            double val = 0.0;
            for (int i = 0; i < 4; ++i) {
                val += getFieldValueForNode(elem[i]);
            }
            val /= 4.0;
            QColor c = getColorForValue(val);
            painter.setBrush(QBrush(c));
            painter.drawPolygon(poly);
        }

        for (int e = 0; e < m_mesh.num_tris(); ++e) {
            const auto& elem = m_mesh.tri_elements[e];
            painter.setPen(Qt::NoPen);
            QPolygonF poly;
            for (int i = 0; i < 3; ++i) {
                int n = elem[i];
                double ux = m_result.displacement[n * 2];
                double uy = m_result.displacement[n * 2 + 1];
                poly << worldToWidget(m_mesh.nodes[n].x + ux * effectiveScale,
                                       m_mesh.nodes[n].y + uy * effectiveScale);
            }
            // Use average nodal value for element color
            double val = 0.0;
            for (int i = 0; i < 3; ++i) {
                val += getFieldValueForNode(elem[i]);
            }
            val /= 3.0;
            QColor c = getColorForValue(val);
            painter.setBrush(QBrush(c));
            painter.drawPolygon(poly);
        }
    }

    if (m_showEdges) {
        painter.setPen(QPen(QColor(0xff, 0xff, 0xff, 80), 0, Qt::SolidLine));
        for (int e = 0; e < m_mesh.num_quads(); ++e) {
            const auto& elem = m_mesh.quad_elements[e];
            for (int i = 0; i < 4; ++i) {
                int n0 = elem[i];
                int n1 = elem[(i + 1) % 4];
                painter.drawLine(worldToWidget(m_mesh.nodes[n0].x, m_mesh.nodes[n0].y),
                                 worldToWidget(m_mesh.nodes[n1].x, m_mesh.nodes[n1].y));
            }
        }
        for (int e = 0; e < m_mesh.num_tris(); ++e) {
            const auto& elem = m_mesh.tri_elements[e];
            for (int i = 0; i < 3; ++i) {
                int n0 = elem[i];
                int n1 = elem[(i + 1) % 3];
                painter.drawLine(worldToWidget(m_mesh.nodes[n0].x, m_mesh.nodes[n0].y),
                                 worldToWidget(m_mesh.nodes[n1].x, m_mesh.nodes[n1].y));
            }
        }
    }

    if (m_showBoundary) {
        painter.setBrush(QBrush(QColor(0xff, 0xff, 0x00, 230)));
        painter.setPen(Qt::NoPen);
        for (const auto& bc : m_mesh.dirichlet) {
            if (bc.dof == 0) {
                const auto& n = m_mesh.nodes[bc.node];
                double s = 0.015;
                QPointF p1 = worldToWidget(n.x, n.y);
                QPointF p2 = worldToWidget(n.x - s, n.y - s * 1.5);
                QPointF p3 = worldToWidget(n.x + s, n.y - s * 1.5);
                QPolygonF tri;
                tri << p1 << p2 << p3;
                painter.drawPolygon(tri);
            }
        }

        painter.setPen(QPen(QColor(0x00, 0xff, 0x00, 230), 1));
        for (const auto& bc : m_mesh.neumann) {
            const auto& n = m_mesh.nodes[bc.node];
            double len = 0.03;
            QPointF p1 = worldToWidget(n.x, n.y);
            QPointF p2;
            if (bc.dof == 0) {
                p2 = worldToWidget(n.x + len, n.y);
            } else {
                p2 = worldToWidget(n.x, n.y + len);
            }
            painter.drawLine(p1, p2);
        }
    }

    if (m_showArrows && !m_result.stresses.empty()) {
        for (int e = 0; e < static_cast<int>(m_result.stresses.size()); ++e) {
            const auto& s = m_result.stresses[e];
            int n0 = m_mesh.quad_elements[e][0];
            int n1 = m_mesh.quad_elements[e][1];
            int n2 = m_mesh.quad_elements[e][2];
            int n3 = m_mesh.quad_elements[e][3];
            double cx = (m_mesh.nodes[n0].x + m_mesh.nodes[n1].x +
                         m_mesh.nodes[n2].x + m_mesh.nodes[n3].x) / 4.0;
            double cy = (m_mesh.nodes[n0].y + m_mesh.nodes[n1].y +
                         m_mesh.nodes[n2].y + m_mesh.nodes[n3].y) / 4.0;

            double scale = 0.005;
            if (std::abs(s.sigma_1) > 1e-6) {
                double len = std::abs(s.sigma_1) * scale;
                painter.setPen(QPen(QColor(0xff, 0x00, 0x00), 1));
                painter.drawLine(worldToWidget(cx - len, cy),
                                 worldToWidget(cx + len, cy));
            }
            if (std::abs(s.sigma_2) > 1e-6) {
                double len = std::abs(s.sigma_2) * scale;
                painter.setPen(QPen(QColor(0x99, 0x33, 0xff), 1));
                painter.drawLine(worldToWidget(cx, cy - len),
                                 worldToWidget(cx, cy + len));
            }
        }
    }

    // Draw stress streamlines (principal stress direction tracing)
    drawStreamlines(painter);

    int barWidth = 20;
    int barHeight = height() - 40;
    int barX = width() - barWidth - 15;
    int barY = 20;

    for (int y = 0; y < barHeight; ++y) {
        double t = 1.0 - static_cast<double>(y) / barHeight;
        double val = m_fieldMin + t * (m_fieldMax - m_fieldMin);
        QColor c = getColorForValue(val);
        painter.fillRect(barX, barY + y, barWidth, 1, c);
    }

    painter.setPen(QColor(0x80, 0x80, 0x80));
    painter.drawRect(barX, barY, barWidth, barHeight);

    painter.setPen(QColor(0xc9, 0xd1, 0xd9));
    painter.setFont(QFont("JetBrains Mono", 8));
    painter.drawText(barX + barWidth + 3, barY + 8,
                     QString::number(m_fieldMax, 'g', 3));
    painter.drawText(barX + barWidth + 3, barY + barHeight,
                     QString::number(m_fieldMin, 'g', 3));
}
