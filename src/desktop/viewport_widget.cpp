#include "viewport_widget.hpp"
#include <QPainter>
#include <cmath>

ViewportWidget::ViewportWidget(QWidget* parent)
    : QWidget(parent) {}

void ViewportWidget::setMeshAndResults(const Mesh& mesh, const fea::SolveResult& result) {
    m_mesh = mesh;
    m_result = result;
    m_hasData = true;
    m_panX = 0.5;
    m_panY = 0.5;
    m_zoom = 1.0;
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

void ViewportWidget::resetView() {
    m_panX = 0.5;
    m_panY = 0.5;
    m_zoom = 1.0;
    update();
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
        for (int i = 0; i < m_mesh.num_nodes(); ++i) {
            double val = getFieldValueForNode(i);
            m_fieldMin = std::min(m_fieldMin, val);
            m_fieldMax = std::max(m_fieldMax, val);
        }
    } else {
        for (size_t i = 0; i < m_result.stresses.size(); ++i) {
            double val = getFieldValueForElement(static_cast<int>(i));
            m_fieldMin = std::min(m_fieldMin, val);
            m_fieldMax = std::max(m_fieldMax, val);
        }
    }

    if (m_fieldMin == m_fieldMax) {
        m_fieldMin -= 0.5;
        m_fieldMax += 0.5;
    }
}

void ViewportWidget::mousePressEvent(QMouseEvent* event) {
    m_lastMousePos = event->pos();
}

void ViewportWidget::mouseMoveEvent(QMouseEvent* event) {
    QPoint delta = event->pos() - m_lastMousePos;
    if (event->buttons() & Qt::LeftButton) {
        double halfRange = 0.6 / m_zoom;
        double aspect = static_cast<double>(width()) / static_cast<double>(height());
        m_panX -= static_cast<double>(delta.x()) * halfRange * aspect / width();
        m_panY += static_cast<double>(delta.y()) * halfRange / height();
    }
    m_lastMousePos = event->pos();
    update();
}

void ViewportWidget::wheelEvent(QWheelEvent* event) {
    double factor = (event->angleDelta().y() > 0) ? 1.15 : 1.0 / 1.15;
    m_zoom *= factor;
    m_zoom = std::max(0.1, std::min(100.0, m_zoom));
    update();
}

void ViewportWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(0x0d, 0x11, 0x17));

    if (!m_hasData) {
        painter.setPen(QColor(0x8b, 0x94, 0x9e));
        painter.setFont(QFont("JetBrains Mono", 12));
        painter.drawText(rect(), Qt::AlignCenter, "Load a case or mesh to begin");
        return;
    }

    if (!m_result.displacement.empty()) {
        updateFieldRange();
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
            double val = getFieldValueForElement(e);
            QColor c = getColorForValue(val);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QBrush(c));
            QPolygonF poly;
            for (int i = 0; i < 4; ++i) {
                int n = elem[i];
                double ux = m_result.displacement[n * 2];
                double uy = m_result.displacement[n * 2 + 1];
                poly << worldToWidget(m_mesh.nodes[n].x + ux * m_dispScale,
                                       m_mesh.nodes[n].y + uy * m_dispScale);
            }
            painter.drawPolygon(poly);
        }

        for (int e = 0; e < m_mesh.num_tris(); ++e) {
            const auto& elem = m_mesh.tri_elements[e];
            double val = getFieldValueForElement(e);
            QColor c = getColorForValue(val);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QBrush(c));
            QPolygonF poly;
            for (int i = 0; i < 3; ++i) {
                int n = elem[i];
                double ux = m_result.displacement[n * 2];
                double uy = m_result.displacement[n * 2 + 1];
                poly << worldToWidget(m_mesh.nodes[n].x + ux * m_dispScale,
                                       m_mesh.nodes[n].y + uy * m_dispScale);
            }
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