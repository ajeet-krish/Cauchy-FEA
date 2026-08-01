#include "energy_balance_chart.hpp"
#include <QPainter>
#include <cmath>
#include <algorithm>

EnergyBalanceChart::EnergyBalanceChart(QWidget* parent)
    : QWidget(parent) {
    setMinimumSize(400, 200);
}

void EnergyBalanceChart::setData(const EnergyBalanceData& data) {
    m_data = data;
    update();
}

void EnergyBalanceChart::clear() {
    m_data = EnergyBalanceData();
    update();
}

void EnergyBalanceChart::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height();

    painter.fillRect(0, 0, w, h, QColor(0x0d, 0x11, 0x17));

    if (!m_data.valid) {
        painter.setPen(QColor(0x8b, 0x94, 0x9e));
        painter.setFont(QFont("JetBrains Mono", 11));
        painter.drawText(rect(), Qt::AlignCenter, "No energy data");
        return;
    }

    int padLeft = 80;
    int padRight = 30;
    int padTop = 40;
    int padBottom = 50;
    int plotW = w - padLeft - padRight;
    int plotH = h - padTop - padBottom;

    double maxVal = std::max(std::abs(m_data.strain_energy), std::abs(m_data.work_done));
    if (maxVal < 1e-30) maxVal = 1.0;

    double barH = plotH / 2.5;
    double gap = barH * 0.4;

    // Strain energy bar (cyan)
    double seW = (std::abs(m_data.strain_energy) / maxVal) * plotW;
    double seY = padTop + (plotH - 2 * barH - gap) / 2;
    painter.setBrush(QColor(0x00, 0xd4, 0xff));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(padLeft, seY, seW, barH, 4, 4);

    // Work done bar (magenta)
    double wdW = (std::abs(m_data.work_done) / maxVal) * plotW;
    double wdY = seY + barH + gap;
    painter.setBrush(QColor(0xff, 0x00, 0x66));
    painter.drawRoundedRect(padLeft, wdY, wdW, barH, 4, 4);

    // Labels
    painter.setPen(QColor(0xc9, 0xd1, 0xd9));
    painter.setFont(QFont("JetBrains Mono", 9));
    painter.drawText(QRectF(0, seY, padLeft - 10, barH),
                     Qt::AlignRight | Qt::AlignVCenter, "Strain E");
    painter.drawText(QRectF(0, wdY, padLeft - 10, barH),
                     Qt::AlignRight | Qt::AlignVCenter, "Work");

    // Values on bars
    painter.setFont(QFont("JetBrains Mono", 9));
    painter.setPen(QColor(0x0d, 0x11, 0x17));
    if (seW > 80)
        painter.drawText(QRectF(padLeft + 5, seY, seW - 10, barH),
                         Qt::AlignVCenter,
                         QString::number(m_data.strain_energy, 'e', 3));
    if (wdW > 80)
        painter.drawText(QRectF(padLeft + 5, wdY, wdW - 10, barH),
                         Qt::AlignVCenter,
                         QString::number(m_data.work_done, 'e', 3));

    // Error percentage
    double error = 0.0;
    if (m_data.work_done != 0.0) {
        error = std::abs(m_data.strain_energy - m_data.work_done) / std::abs(m_data.work_done) * 100.0;
    }

    painter.setPen(QColor(0x00, 0xd4, 0xff));
    painter.setFont(QFont("JetBrains Mono", 10, QFont::Bold));
    painter.drawText(w / 2 - 80, 22, "Energy Balance");

    painter.setPen(error < 1.0 ? QColor(0x3f, 0xb9, 0x50) : QColor(0xff, 0x47, 0x57));
    painter.setFont(QFont("JetBrains Mono", 9));
    painter.drawText(w / 2 - 50, h - 12,
                     QString("Error: %1%").arg(error, 0, 'f', 4));
}
