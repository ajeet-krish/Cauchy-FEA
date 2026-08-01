#include "stress_histogram.hpp"
#include <QPainter>
#include <cmath>
#include <algorithm>

StressHistogram::StressHistogram(QWidget* parent)
    : QWidget(parent) {
    setMinimumSize(400, 250);
}

void StressHistogram::setData(const std::vector<postprocess::ElementStress>& stresses) {
    m_sigma_xx.clear();
    m_sigma_yy.clear();
    m_von_mises.clear();
    m_sigma_xx.reserve(stresses.size());
    m_sigma_yy.reserve(stresses.size());
    m_von_mises.reserve(stresses.size());

    for (const auto& s : stresses) {
        m_sigma_xx.push_back(s.sigma_xx);
        m_sigma_yy.push_back(s.sigma_yy);
        m_von_mises.push_back(s.von_mises);
    }
    m_hasData = !stresses.empty();
    update();
}

void StressHistogram::clear() {
    m_hasData = false;
    m_sigma_xx.clear();
    m_sigma_yy.clear();
    m_von_mises.clear();
    update();
}

void StressHistogram::paintEvent(QPaintEvent*) {
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

    if (!m_hasData) {
        painter.setPen(QColor(0x8b, 0x94, 0x9e));
        painter.setFont(QFont("JetBrains Mono", 11));
        painter.drawText(rect(), Qt::AlignCenter, "No stress data");
        return;
    }

    // Compute global range across all three series
    double globalMin = 1e30, globalMax = -1e30;
    for (double v : m_sigma_xx) { globalMin = std::min(globalMin, v); globalMax = std::max(globalMax, v); }
    for (double v : m_sigma_yy) { globalMin = std::min(globalMin, v); globalMax = std::max(globalMax, v); }
    for (double v : m_von_mises) { globalMax = std::max(globalMax, v); }

    double range = globalMax - globalMin;
    if (range < 1e-20) range = 1.0;
    double binWidth = range / NUM_BINS;

    auto buildHistogram = [&](const std::vector<double>& data) {
        std::vector<int> counts(NUM_BINS, 0);
        for (double v : data) {
            int bin = static_cast<int>((v - globalMin) / binWidth);
            bin = std::clamp(bin, 0, NUM_BINS - 1);
            counts[bin]++;
        }
        return counts;
    };

    auto hxx = buildHistogram(m_sigma_xx);
    auto hyy = buildHistogram(m_sigma_yy);
    auto hvm = buildHistogram(m_von_mises);

    int maxCount = 0;
    for (int i = 0; i < NUM_BINS; ++i) {
        maxCount = std::max(maxCount, std::max({hxx[i], hyy[i], hvm[i]}));
    }
    if (maxCount == 0) maxCount = 1;

    double barW = plotW / static_cast<double>(NUM_BINS);
    double subW = barW / 3.0;

    // Draw bars (sigma_xx = cyan, sigma_yy = orange, von_mises = magenta)
    QColor colors[3] = {
        QColor(0x00, 0xd4, 0xff),
        QColor(0xff, 0xb3, 0x47),
        QColor(0xff, 0x00, 0x66)
    };
    int* counts[3] = { hxx.data(), hyy.data(), hvm.data() };

    for (int b = 0; b < NUM_BINS; ++b) {
        for (int s = 0; s < 3; ++s) {
            double barH = (static_cast<double>(counts[s][b]) / maxCount) * plotH;
            double x = padLeft + b * barW + s * subW;
            double y = padTop + plotH - barH;
            painter.fillRect(QRectF(x, y, subW - 1, barH), colors[s]);
        }
    }

    // Axes
    painter.setPen(QPen(QColor(0x30, 0x36, 0x3d), 1));
    painter.drawLine(QPointF(padLeft, padTop), QPointF(padLeft, padTop + plotH));
    painter.drawLine(QPointF(padLeft, padTop + plotH), QPointF(padLeft + plotW, padTop + plotH));

    // Axis labels
    painter.setPen(QColor(0xc9, 0xd1, 0xd9));
    painter.setFont(QFont("JetBrains Mono", 9));
    painter.drawText(padLeft + plotW / 2 - 30, h - 8, "Stress (Pa)");

    painter.save();
    painter.translate(12, padTop + plotH / 2);
    painter.rotate(-90);
    painter.drawText(-20, 0, "Count");
    painter.restore();

    // Tick labels
    painter.setPen(QColor(0x8b, 0x94, 0x9e));
    painter.setFont(QFont("JetBrains Mono", 8));
    for (int i = 0; i <= 4; ++i) {
        double val = globalMin + range * i / 4.0;
        double x = padLeft + plotW * i / 4.0;
        painter.drawText(x - 15, padTop + plotH + 15, QString::number(val, 'g', 3));
    }

    // Title
    painter.setPen(QColor(0xff, 0xb3, 0x47));
    painter.setFont(QFont("JetBrains Mono", 10, QFont::Bold));
    painter.drawText(w / 2 - 60, 20, "Stress Distribution");

    // Legend
    int lx = padLeft + 5;
    int ly = padTop + 5;
    painter.setFont(QFont("JetBrains Mono", 8));
    for (int s = 0; s < 3; ++s) {
        painter.fillRect(lx, ly + s * 14, 10, 10, colors[s]);
        painter.setPen(QColor(0xc9, 0xd1, 0xd9));
        QString labels[] = { "Sigma XX", "Sigma YY", "Von Mises" };
        painter.drawText(lx + 14, ly + s * 14 + 9, labels[s]);
    }
}
