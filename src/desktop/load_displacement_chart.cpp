#include "load_displacement_chart.hpp"
#include <QPainter>
#include <cmath>
#include <algorithm>

LoadDisplacementChart::LoadDisplacementChart(QWidget* parent)
    : QWidget(parent) {
    setMinimumSize(400, 250);
}

void LoadDisplacementChart::addPoint(double force, double max_disp) {
    m_points.push_back({force, max_disp});
    update();
}

void LoadDisplacementChart::clear() {
    m_points.clear();
    update();
}

void LoadDisplacementChart::paintEvent(QPaintEvent*) {
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

    if (m_points.empty()) {
        painter.setPen(QColor(0x8b, 0x94, 0x9e));
        painter.setFont(QFont("JetBrains Mono", 11));
        painter.drawText(rect(), Qt::AlignCenter, "No load-displacement data");
        return;
    }

    double fMin = 1e30, fMax = -1e30;
    double dMin = 1e30, dMax = -1e30;
    for (const auto& p : m_points) {
        fMin = std::min(fMin, p.force);
        fMax = std::max(fMax, p.force);
        dMin = std::min(dMin, p.max_disp);
        dMax = std::max(dMax, p.max_disp);
    }

    double fPad = (fMax - fMin) * 0.1 + 1e-10;
    double dPad = (dMax - dMin) * 0.1 + 1e-10;

    auto toX = [&](double v) { return padLeft + ((v - (fMin - fPad)) / ((fMax + fPad) - (fMin - fPad))) * plotW; };
    auto toY = [&](double v) { return padTop + plotH - ((v - (dMin - dPad)) / ((dMax + dPad) - (dMin - dPad))) * plotH; };

    // Grid
    painter.setPen(QPen(QColor(0x30, 0x36, 0x3d), 0, Qt::DotLine));
    for (int i = 0; i <= 5; ++i) {
        double y = padTop + plotH * i / 5.0;
        painter.drawLine(QPointF(padLeft, y), QPointF(padLeft + plotW, y));
    }

    // Data line (green)
    if (m_points.size() > 1) {
        painter.setPen(QPen(QColor(0x3f, 0xb9, 0x50), 2));
        QPolygonF line;
        for (const auto& p : m_points) {
            line << QPointF(toX(p.force), toY(p.max_disp));
        }
        painter.drawPolyline(line);
    }

    // Data points
    painter.setPen(QPen(QColor(0x3f, 0xb9, 0x50), 1));
    painter.setBrush(QBrush(QColor(0x3f, 0xb9, 0x50)));
    for (const auto& p : m_points) {
        painter.drawEllipse(QPointF(toX(p.force), toY(p.max_disp)), 4, 4);
    }

    // Axes
    painter.setPen(QPen(QColor(0x30, 0x36, 0x3d), 1));
    painter.drawLine(QPointF(padLeft, padTop), QPointF(padLeft, padTop + plotH));
    painter.drawLine(QPointF(padLeft, padTop + plotH), QPointF(padLeft + plotW, padTop + plotH));

    // Labels
    painter.setPen(QColor(0xc9, 0xd1, 0xd9));
    painter.setFont(QFont("JetBrains Mono", 9));
    painter.drawText(padLeft + plotW / 2 - 40, h - 8, "Force (N)");

    painter.save();
    painter.translate(12, padTop + plotH / 2);
    painter.rotate(-90);
    painter.drawText(-30, 0, "|u| max (m)");
    painter.restore();

    // Tick labels
    painter.setPen(QColor(0x8b, 0x94, 0x9e));
    painter.setFont(QFont("JetBrains Mono", 8));
    for (int i = 0; i <= 4; ++i) {
        double val = fMin + (fMax - fMin) * i / 4.0;
        painter.drawText(toX(val) - 15, padTop + plotH + 15,
                         QString::number(val, 'g', 3));
    }
    for (int i = 0; i <= 4; ++i) {
        double val = dMin + (dMax - dMin) * i / 4.0;
        painter.drawText(padLeft - 45, toY(val) + 4,
                         QString::number(val, 'g', 3));
    }

    // Title
    painter.setPen(QColor(0xff, 0xb3, 0x47));
    painter.setFont(QFont("JetBrains Mono", 10, QFont::Bold));
    painter.drawText(w / 2 - 70, 20, "Load vs Displacement");
}
