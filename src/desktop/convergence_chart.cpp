#include "convergence_chart.hpp"
#include <QPainter>
#include <cmath>
#include <algorithm>

ConvergenceChart::ConvergenceChart(QWidget* parent)
    : QWidget(parent) {
    setMinimumSize(400, 300);
}

void ConvergenceChart::setData(const ConvergenceData& data) {
    m_data = data;
    m_hasData = !data.samples.empty();
    update();
}

void ConvergenceChart::clear() {
    m_hasData = false;
    update();
}

void ConvergenceChart::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height();
    int padLeft = 70;
    int padRight = 20;
    int padTop = 30;
    int padBottom = 50;

    int plotW = w - padLeft - padRight;
    int plotH = h - padTop - padBottom;

    painter.fillRect(0, 0, w, h, QColor(0x0d, 0x11, 0x17));

    if (!m_hasData) {
        painter.setPen(QColor(0x8b, 0x94, 0x9e));
        painter.setFont(QFont("JetBrains Mono", 11));
        painter.drawText(rect(), Qt::AlignCenter, "No convergence data");
        return;
    }

    // Compute log ranges
    double hMin = 1e30, hMax = -1e30;
    double yMin = 1e30, yMax = -1e30;

    for (const auto& s : m_data.samples) {
        double lh = std::log10(s.h);
        double ly = std::log10(std::abs(s.value));
        hMin = std::min(hMin, lh);
        hMax = std::max(hMax, lh);
        yMin = std::min(yMin, ly);
        yMax = std::max(yMax, ly);
    }

    double hPad = (hMax - hMin) * 0.1 + 0.5;
    double yPad = (yMax - yMin) * 0.1 + 0.5;

    auto toX = [&](double lh) -> double {
        return padLeft + ((lh - (hMin - hPad)) / ((hMax + hPad) - (hMin - hPad))) * plotW;
    };
    auto toY = [&](double ly) -> double {
        return padTop + plotH - ((ly - (yMin - yPad)) / ((yMax + yPad) - (yMin - yPad))) * plotH;
    };

    // Grid lines
    painter.setPen(QPen(QColor(0x30, 0x36, 0x3d), 0, Qt::DotLine));
    for (int i = static_cast<int>(std::ceil(hMin - hPad));
         i <= static_cast<int>(std::floor(hMax + hPad)); ++i) {
        double x = toX(i);
        painter.drawLine(QPointF(x, padTop), QPointF(x, padTop + plotH));
    }
    for (int i = static_cast<int>(std::ceil(yMin - yPad));
         i <= static_cast<int>(std::floor(yMax + yPad)); ++i) {
        double y = toY(i);
        painter.drawLine(QPointF(padLeft, y), QPointF(padLeft + plotW, y));
    }

    // Analytical reference line
    if (m_data.analytical != 0.0) {
        double logA = std::log10(std::abs(m_data.analytical));
        painter.setPen(QPen(QColor(0xff, 0x47, 0x57), 1, Qt::DashLine));
        painter.drawLine(QPointF(padLeft, toY(logA)),
                         QPointF(padLeft + plotW, toY(logA)));
        painter.setPen(QColor(0xff, 0x47, 0x57));
        painter.drawText(padLeft + 4, toY(logA) - 4,
                         QString("Analytical: %1").arg(m_data.analytical, 0, 'e', 2));
    }

    // FEA data line
    painter.setPen(QPen(QColor(0x00, 0xd4, 0xff), 2));
    painter.setBrush(Qt::NoBrush);
    QPolygonF line;
    for (const auto& s : m_data.samples) {
        double lh = std::log10(s.h);
        double ly = std::log10(std::abs(s.value));
        line << QPointF(toX(lh), toY(ly));
    }
    painter.drawPolyline(line);

    // Data points
    painter.setPen(QPen(QColor(0x00, 0xd4, 0xff), 1));
    painter.setBrush(QBrush(QColor(0x00, 0xd4, 0xff)));
    for (const auto& s : m_data.samples) {
        double lh = std::log10(s.h);
        double ly = std::log10(std::abs(s.value));
        QPointF pt(toX(lh), toY(ly));
        painter.drawEllipse(pt, 4, 4);
    }

    // Axes
    painter.setPen(QPen(QColor(0x30, 0x36, 0x3d), 1));
    painter.drawLine(QPointF(padLeft, padTop),
                     QPointF(padLeft, padTop + plotH));
    painter.drawLine(QPointF(padLeft, padTop + plotH),
                     QPointF(padLeft + plotW, padTop + plotH));

    // Axis labels
    painter.setPen(QColor(0xc9, 0xd1, 0xd9));
    painter.setFont(QFont("JetBrains Mono", 9));
    painter.drawText(padLeft + plotW / 2 - 40, h - 8,
                     "Element size h (log scale)");

    painter.save();
    painter.translate(12, padTop + plotH / 2);
    painter.rotate(-90);
    painter.drawText(-30, 0, m_data.quantity + " (log scale)");
    painter.restore();

    // Tick labels
    painter.setPen(QColor(0x8b, 0x94, 0x9e));
    for (int i = static_cast<int>(std::ceil(hMin - hPad));
         i <= static_cast<int>(std::floor(hMax + hPad)); ++i) {
        painter.drawText(toX(i) - 10, padTop + plotH + 15,
                         QString("10^%1").arg(i));
    }

    // Title
    painter.setPen(QColor(0xff, 0xb3, 0x47));
    painter.setFont(QFont("JetBrains Mono", 10, QFont::Bold));
    QString title = QString("%1 Mesh Convergence").arg(m_data.caseName);
    if (m_data.observed_order != 0.0) {
        title += QString(" (p=%1)").arg(m_data.observed_order, 0, 'f', 2);
    }
    painter.drawText(w / 2 - 60, 18, title);

    // GCI info box
    if (m_data.gci_fine > 0.0) {
        int boxX = padLeft + plotW - 170;
        int boxY = padTop + 5;
        painter.setPen(QPen(QColor(0x30, 0x36, 0x3d), 1));
        painter.setBrush(QColor(0x16, 0x1b, 0x22, 200));
        painter.drawRoundedRect(boxX, boxY, 165, 50, 4, 4);

        painter.setPen(QColor(0x8b, 0x94, 0x9e));
        painter.setFont(QFont("JetBrains Mono", 8));
        painter.drawText(boxX + 5, boxY + 12,
                         QString("Order: %1").arg(m_data.observed_order, 0, 'f', 3));
        painter.drawText(boxX + 5, boxY + 26,
                         QString("GCI fine: %1").arg(m_data.gci_fine, 0, 'e', 2));
        painter.drawText(boxX + 5, boxY + 40,
                         QString("Oscillatory: %1")
                             .arg(m_data.is_oscillatory ? "Yes" : "No"));
    }
}