#include "mesh_quality_overlay.hpp"
#include <QPainter>
#include <algorithm>

MeshQualityOverlay::MeshQualityOverlay(QWidget* parent)
    : QWidget(parent) {
    setMinimumSize(200, 120);
}

void MeshQualityOverlay::setMesh(const Mesh& mesh) {
    m_mesh = mesh;
    m_hasData = true;

    m_minJacobian = 1.0;
    m_maxAspectRatio = 1.0;
    m_invalidCount = 0;

    for (int e = 0; e < m_mesh.num_quads(); ++e) {
        const auto& elem = m_mesh.quad_elements[e];
        std::array<Node, 4> nodes;
        for (int i = 0; i < 4; ++i) {
            nodes[i] = m_mesh.nodes[elem[i]];
        }
        auto quality = mesh::compute_q4_quality(nodes);
        m_minJacobian = std::min(m_minJacobian, quality.jacobian_ratio);
        m_maxAspectRatio = std::max(m_maxAspectRatio, quality.aspect_ratio);
        if (quality.jacobian_ratio < 0.0) ++m_invalidCount;
    }

    emit qualityChanged();
    update();
}

void MeshQualityOverlay::clear() {
    m_hasData = false;
    m_minJacobian = 1.0;
    m_maxAspectRatio = 1.0;
    m_invalidCount = 0;
    update();
}

double MeshQualityOverlay::minJacobian() const { return m_minJacobian; }
double MeshQualityOverlay::maxAspectRatio() const { return m_maxAspectRatio; }
int MeshQualityOverlay::invalidElementCount() const { return m_invalidCount; }

void MeshQualityOverlay::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(0x0d, 0x11, 0x17));

    painter.setPen(QColor(0xc9, 0xd1, 0xd9));
    painter.setFont(QFont("JetBrains Mono", 10));

    if (!m_hasData) {
        painter.drawText(rect(), Qt::AlignCenter, "No mesh loaded");
        return;
    }

    int y = 20;
    painter.drawText(10, y, QString("Min Jacobian ratio: %1").arg(m_minJacobian, 0, 'f', 4));
    y += 18;
    painter.drawText(10, y, QString("Max aspect ratio: %1").arg(m_maxAspectRatio, 0, 'f', 2));
    y += 18;
    painter.drawText(10, y, QString("Invalid elements: %1").arg(m_invalidCount));
    y += 18;

    // Quality bar
    int barX = 10;
    int barY = y + 5;
    int barW = width() - 20;
    int barH = 12;

    // Jacobian ratio bar (green to red)
    painter.setPen(Qt::NoPen);
    for (int i = 0; i < barW; ++i) {
        double t = static_cast<double>(i) / barW;
        double jac = m_minJacobian + t * (1.0 - m_minJacobian);
        if (jac < 0.0) jac = 0.0;
        if (jac > 1.0) jac = 1.0;
        int r = static_cast<int>(255 * (1.0 - jac));
        int g = static_cast<int>(255 * jac);
        painter.setBrush(QColor(r, g, 0));
        painter.drawRect(barX + i, barY, 1, barH);
    }
    painter.setPen(QColor(0x30, 0x36, 0x3d));
    painter.drawRect(barX, barY, barW, barH);

    painter.setPen(QColor(0x8b, 0x94, 0x9e));
    painter.setFont(QFont("JetBrains Mono", 8));
    painter.drawText(barX, barY + barH + 12, "0 (degenerate)");
    painter.drawText(barX + barW - 40, barY + barH + 12, "1 (perfect)");
}