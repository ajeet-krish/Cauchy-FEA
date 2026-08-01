#include "displacement_line_chart.hpp"
#include <QPainter>
#include <cmath>
#include <algorithm>

DisplacementLineChart::DisplacementLineChart(QWidget* parent)
    : QWidget(parent) {
    setMinimumSize(400, 250);
}

void DisplacementLineChart::setData(const Mesh& mesh, const fea::SolveResult& result) {
    m_data = {};
    if (mesh.num_nodes() == 0 || result.displacement.empty()) return;

    // Find top-edge nodes (max y)
    double maxY = -1e30;
    for (int i = 0; i < mesh.num_nodes(); ++i) {
        maxY = std::max(maxY, mesh.nodes[i].y);
    }

    struct EdgeNode { int id; double x; double uy; };
    std::vector<EdgeNode> edgeNodes;
    for (int i = 0; i < mesh.num_nodes(); ++i) {
        if (std::abs(mesh.nodes[i].y - maxY) < 1e-6) {
            double ux = result.displacement[2 * i];
            double uy = result.displacement[2 * i + 1];
            edgeNodes.push_back({i, mesh.nodes[i].x, uy});
        }
    }

    std::sort(edgeNodes.begin(), edgeNodes.end(),
              [](const EdgeNode& a, const EdgeNode& b) { return a.x < b.x; });

    m_data.fea.reserve(edgeNodes.size());
    for (const auto& en : edgeNodes) {
        m_data.fea.push_back({en.x, en.uy});
    }

    // Analytical for cantilever: v(x) = (P*y/(6*E*I)) * (3*L*x^2 - x^3) at y=L/2
    // Simplified: just show FEA data for now
    m_data.axisLabel = "x (m)";
    m_data.valueLabel = "uy (m)";
    m_data.valid = !m_data.fea.empty();
    update();
}

void DisplacementLineChart::clear() {
    m_data = {};
    update();
}

void DisplacementLineChart::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height();
    int padLeft = 70;
    int padRight = 20;
    int padTop = 35;
    int padBottom = 40;
    int plotW = w - padLeft - padRight;
    int plotH = h - padTop - padBottom;

    painter.fillRect(0, 0, w, h, QColor(0x0d, 0x11, 0x17));

    if (!m_data.valid) {
        painter.setPen(QColor(0x8b, 0x94, 0x9e));
        painter.setFont(QFont("JetBrains Mono", 11));
        painter.drawText(rect(), Qt::AlignCenter, "No displacement data");
        return;
    }

    double xMin = 1e30, xMax = -1e30;
    double yMin = 1e30, yMax = -1e30;
    for (const auto& s : m_data.fea) {
        xMin = std::min(xMin, s.coord);
        xMax = std::max(xMax, s.coord);
        yMin = std::min(yMin, s.value);
        yMax = std::max(yMax, s.value);
    }
    for (const auto& s : m_data.analytical) {
        yMin = std::min(yMin, s.value);
        yMax = std::max(yMax, s.value);
    }

    double xPad = (xMax - xMin) * 0.05 + 1e-10;
    double yPad = (yMax - yMin) * 0.1 + 1e-10;

    auto toX = [&](double v) { return padLeft + ((v - (xMin - xPad)) / ((xMax + xPad) - (xMin - xPad))) * plotW; };
    auto toY = [&](double v) { return padTop + plotH - ((v - (yMin - yPad)) / ((yMax + yPad) - (yMin - yPad))) * plotH; };

    // Grid
    painter.setPen(QPen(QColor(0x30, 0x36, 0x3d), 0, Qt::DotLine));
    for (int i = 0; i <= 5; ++i) {
        double y = padTop + plotH * i / 5.0;
        painter.drawLine(QPointF(padLeft, y), QPointF(padLeft + plotW, y));
    }

    // Analytical line (red dashed)
    if (!m_data.analytical.empty()) {
        painter.setPen(QPen(QColor(0xff, 0x47, 0x57), 1.5, Qt::DashLine));
        QPolygonF line;
        for (const auto& s : m_data.analytical) {
            line << QPointF(toX(s.coord), toY(s.value));
        }
        painter.drawPolyline(line);
    }

    // FEA line (cyan solid)
    painter.setPen(QPen(QColor(0x00, 0xd4, 0xff), 2));
    QPolygonF feaLine;
    for (const auto& s : m_data.fea) {
        feaLine << QPointF(toX(s.coord), toY(s.value));
    }
    painter.drawPolyline(feaLine);

    // FEA data points
    painter.setBrush(QBrush(QColor(0x00, 0xd4, 0xff)));
    painter.setPen(QPen(QColor(0x00, 0xd4, 0xff), 1));
    for (const auto& s : m_data.fea) {
        painter.drawEllipse(QPointF(toX(s.coord), toY(s.value)), 3, 3);
    }

    // Axes
    painter.setPen(QPen(QColor(0x30, 0x36, 0x3d), 1));
    painter.drawLine(QPointF(padLeft, padTop), QPointF(padLeft, padTop + plotH));
    painter.drawLine(QPointF(padLeft, padTop + plotH), QPointF(padLeft + plotW, padTop + plotH));

    // Labels
    painter.setPen(QColor(0xc9, 0xd1, 0xd9));
    painter.setFont(QFont("JetBrains Mono", 9));
    painter.drawText(padLeft + plotW / 2 - 30, h - 8, m_data.axisLabel);

    painter.save();
    painter.translate(12, padTop + plotH / 2);
    painter.rotate(-90);
    painter.drawText(-20, 0, m_data.valueLabel);
    painter.restore();

    // Tick labels
    painter.setPen(QColor(0x8b, 0x94, 0x9e));
    painter.setFont(QFont("JetBrains Mono", 8));
    for (int i = 0; i <= 4; ++i) {
        double val = xMin + (xMax - xMin) * i / 4.0;
        painter.drawText(toX(val) - 15, padTop + plotH + 15,
                         QString::number(val, 'g', 3));
    }
    for (int i = 0; i <= 4; ++i) {
        double val = yMin + (yMax - yMin) * i / 4.0;
        painter.drawText(padLeft - 45, toY(val) + 4,
                         QString::number(val, 'g', 3));
    }

    // Title
    painter.setPen(QColor(0xff, 0xb3, 0x47));
    painter.setFont(QFont("JetBrains Mono", 10, QFont::Bold));
    painter.drawText(w / 2 - 80, 20, "Displacement Along Top Edge");

    // Legend
    if (!m_data.analytical.empty()) {
        int lx = padLeft + plotW - 120;
        int ly = padTop + 10;
        painter.setPen(QPen(QColor(0x00, 0xd4, 0xff), 2));
        painter.drawLine(lx, ly + 5, lx + 20, ly + 5);
        painter.setPen(QColor(0xc9, 0xd1, 0xd9));
        painter.setFont(QFont("JetBrains Mono", 8));
        painter.drawText(lx + 24, ly + 9, "FEA");

        painter.setPen(QPen(QColor(0xff, 0x47, 0x57), 1, Qt::DashLine));
        painter.drawLine(lx, ly + 20, lx + 20, ly + 20);
        painter.setPen(QColor(0xc9, 0xd1, 0xd9));
        painter.drawText(lx + 24, ly + 24, "Analytical");
    }
}
