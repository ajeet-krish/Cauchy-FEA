#include "error_heatmap.hpp"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <cmath>
#include <algorithm>

ErrorHeatmap::ErrorHeatmap(QWidget* parent)
    : QWidget(parent) {
    setMinimumSize(300, 300);
}

void ErrorHeatmap::setData(const Mesh& mesh, const std::vector<double>& errorIndicators) {
    m_mesh = mesh;
    m_errors = errorIndicators;
    m_hasData = !errorIndicators.empty();

    if (m_hasData) {
        m_errMin = *std::min_element(m_errors.begin(), m_errors.end());
        m_errMax = *std::max_element(m_errors.begin(), m_errors.end());
    }
    update();
}

void ErrorHeatmap::clear() {
    m_hasData = false;
    m_errors.clear();
    update();
}

QColor ErrorHeatmap::colorForError(double val) const {
    double t = (m_errMax - m_errMin > 1e-30) ?
               (val - m_errMin) / (m_errMax - m_errMin) : 0.0;
    t = std::clamp(t, 0.0, 1.0);

    // Viridis-like: dark purple -> teal -> yellow
    if (t < 0.5) {
        double s = t * 2.0;
        return QColor(
            static_cast<int>(68 + s * (0 - 68)),
            static_cast<int>(1 + s * (135 - 1)),
            static_cast<int>(84 + s * (113 - 84)));
    } else {
        double s = (t - 0.5) * 2.0;
        return QColor(
            static_cast<int>(0 + s * 253),
            static_cast<int>(135 + s * (231 - 135)),
            static_cast<int>(113 + s * (37 - 113)));
    }
}

void ErrorHeatmap::mousePressEvent(QMouseEvent* event) {
    m_lastMousePos = event->pos();
}

void ErrorHeatmap::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton) {
        QPoint delta = event->pos() - m_lastMousePos;
        m_panX += delta.x() / static_cast<double>(width());
        m_panY -= delta.y() / static_cast<double>(height());
        m_lastMousePos = event->pos();
        update();
    }
}

void ErrorHeatmap::wheelEvent(QWheelEvent* event) {
    double factor = (event->angleDelta().y() > 0) ? 1.15 : 1.0 / 1.15;
    m_zoom = std::clamp(m_zoom * factor, 0.1, 100.0);
    update();
}

void ErrorHeatmap::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height();

    painter.fillRect(0, 0, w, h, QColor(0x0d, 0x11, 0x17));

    if (!m_hasData || m_errors.empty()) {
        painter.setPen(QColor(0x8b, 0x94, 0x9e));
        painter.setFont(QFont("JetBrains Mono", 11));
        painter.drawText(rect(), Qt::AlignCenter, "No error data");
        return;
    }

    // Compute mesh bounds
    double minX = 1e30, maxX = -1e30;
    double minY = 1e30, maxY = -1e30;
    for (const auto& n : m_mesh.nodes) {
        minX = std::min(minX, n.x);
        maxX = std::max(maxX, n.x);
        minY = std::min(minY, n.y);
        maxY = std::max(maxY, n.y);
    }

    double meshW = maxX - minX;
    double meshH = maxY - minY;
    double meshCenterX = (minX + maxX) / 2.0;
    double meshCenterY = (minY + maxY) / 2.0;

    double scale = std::min((w - 40) / meshW, (h - 60) / meshH) * m_zoom;
    auto toX = [&](double x) { return w / 2.0 + (x - meshCenterX) * scale + m_panX * w; };
    auto toY = [&](double y) { return h / 2.0 - (y - meshCenterY) * scale + m_panY * h; };

    // Draw quad elements
    int numElems = static_cast<int>(m_errors.size());
    int quadCount = m_mesh.num_quads();

    for (int e = 0; e < std::min(numElems, quadCount); ++e) {
        const auto& elem = m_mesh.quad_elements[e];
        double err = m_errors[e];
        QColor color = colorForError(err);

        QPolygonF poly;
        for (int i = 0; i < 4; ++i) {
            const auto& n = m_mesh.nodes[elem[i]];
            poly << QPointF(toX(n.x), toY(n.y));
        }

        painter.setPen(QPen(color.darker(120), 0.5));
        painter.setBrush(color);
        painter.drawPolygon(poly);
    }

    // Colorbar
    int cbX = w - 30;
    int cbY = 40;
    int cbH = h - 80;
    for (int i = 0; i < cbH; ++i) {
        double val = m_errMax - (m_errMax - m_errMin) * i / cbH;
        painter.fillRect(cbX, cbY + i, 15, 1, colorForError(val));
    }
    painter.setPen(QColor(0xc9, 0xd1, 0xd9));
    painter.setFont(QFont("JetBrains Mono", 7));
    painter.drawText(cbX - 5, cbY - 3, QString::number(m_errMax, 'e', 2));
    painter.drawText(cbX - 5, cbY + cbH + 10, QString::number(m_errMin, 'e', 2));

    // Title
    painter.setPen(QColor(0xff, 0xb3, 0x47));
    painter.setFont(QFont("JetBrains Mono", 10, QFont::Bold));
    painter.drawText(w / 2 - 60, 20, "Error Indicator Map");
}
