#include "element_inspector_panel.hpp"
#include <QTableWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QString>
#include <cmath>

ElementInspectorPanel::ElementInspectorPanel(QWidget* parent)
    : QWidget(parent) {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    m_titleLabel = new QLabel("No element selected", this);
    m_titleLabel->setStyleSheet("font-weight: bold; font-size: 13px; color: #c9d1d9; padding: 4px;");
    layout->addWidget(m_titleLabel);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(2);
    m_table->setHorizontalHeaderLabels({"Property", "Value"});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->setEditTriggers(QTableWidget::NoEditTriggers);
    m_table->setSelectionBehavior(QTableWidget::SelectRows);
    m_table->setSelectionMode(QTableWidget::SingleSelection);
    m_table->verticalHeader()->setVisible(false);
    m_table->setAlternatingRowColors(true);

    // Dark theme styling
    m_table->setStyleSheet(
        "QTableWidget { background-color: #0d1117; color: #c9d1d9; "
        "gridline-color: #21262d; border: 1px solid #30363d; } "
        "QTableWidget::item { padding: 2px 6px; } "
        "QTableWidget::item:selected { background-color: #1f6feb33; } "
        "QHeaderView::section { background-color: #161b22; color: #8b949e; "
        "border: 1px solid #30363d; padding: 4px; font-weight: bold; } "
        "QTableWidget QTableCornerButton::section { background-color: #161b22; }");

    m_table->setColumnWidth(0, 180);
    layout->addWidget(m_table);
}

void ElementInspectorPanel::addSectionHeader(const QString& title) {
    int row = m_table->rowCount();
    m_table->insertRow(row);

    QLabel* header = new QLabel(title, this);
    header->setStyleSheet("color: #58a6ff; font-weight: bold; padding: 2px;");
    m_table->setCellWidget(row, 0, header);

    QLabel* spacer = new QLabel("", this);
    m_table->setCellWidget(row, 1, spacer);

    QTableWidgetItem* item0 = new QTableWidgetItem("");
    item0->setFlags(Qt::NoItemFlags);
    m_table->setItem(row, 0, item0);

    QTableWidgetItem* item1 = new QTableWidgetItem("");
    item1->setFlags(Qt::NoItemFlags);
    m_table->setItem(row, 1, item1);
}

void ElementInspectorPanel::addRow(const QString& label, const QString& value) {
    int row = m_table->rowCount();
    m_table->insertRow(row);

    QTableWidgetItem* labelItem = new QTableWidgetItem(label);
    labelItem->setFlags(Qt::ItemIsEnabled);
    labelItem->setForeground(QColor(0x8b, 0x94, 0x9e));
    m_table->setItem(row, 0, labelItem);

    QTableWidgetItem* valueItem = new QTableWidgetItem(value);
    valueItem->setFlags(Qt::ItemIsEnabled);
    m_table->setItem(row, 1, valueItem);
}

void ElementInspectorPanel::inspectElement(int elemIndex, const Mesh& mesh,
                                            const fea::SolveResult& result) {
    m_table->setRowCount(0);

    if (elemIndex < 0) {
        clear();
        return;
    }

    // Determine element type and gather node data
    bool isQuad = elemIndex < mesh.num_quads();
    bool isTri = !isQuad && elemIndex < mesh.num_quads() + mesh.num_tris();
    int localIdx = isQuad ? elemIndex : elemIndex - mesh.num_quads();

    if (!isQuad && !isTri) {
        clear();
        return;
    }

    // Element connectivity and nodes
    std::vector<int> nodeIds;
    std::vector<Node> elemNodes;

    if (isQuad) {
        const auto& elem = mesh.quad_elements[localIdx];
        for (int i = 0; i < 4; ++i) {
            nodeIds.push_back(elem[i]);
            elemNodes.push_back(mesh.nodes[elem[i]]);
        }
    } else {
        const auto& elem = mesh.tri_elements[localIdx];
        for (int i = 0; i < 3; ++i) {
            nodeIds.push_back(elem[i]);
            elemNodes.push_back(mesh.nodes[elem[i]]);
        }
    }

    // Compute element area using the shoelace formula
    double area = 0.0;
    int nNodes = static_cast<int>(elemNodes.size());
    for (int i = 0; i < nNodes; ++i) {
        int j = (i + 1) % nNodes;
        area += elemNodes[i].x * elemNodes[j].y;
        area -= elemNodes[j].x * elemNodes[i].y;
    }
    area = 0.5 * std::abs(area);

    // Compute minimum Jacobian determinant across Gauss points
    double minDetJ = 1e30;
    double maxDetJ = -1e30;
    if (isQuad) {
        static const double GP = 1.0 / std::sqrt(3.0);
        const double gp[2] = { -GP, GP };
        const double xi_pts[4] = { -1.0,  1.0,  1.0, -1.0 };
        const double eta_pts[4] = { -1.0, -1.0,  1.0,  1.0 };
        for (int gi = 0; gi < 2; ++gi) {
            for (int gj = 0; gj < 2; ++gj) {
                double xi = gp[gi], eta = gp[gj];
                double J11 = 0.0, J12 = 0.0, J21 = 0.0, J22 = 0.0;
                for (int i = 0; i < 4; ++i) {
                    double dN_dxi = 0.25 * xi_pts[i] * (1.0 + eta_pts[i] * eta);
                    double dN_deta = 0.25 * (1.0 + xi_pts[i] * xi) * eta_pts[i];
                    J11 += dN_dxi * elemNodes[i].x;
                    J12 += dN_dxi * elemNodes[i].y;
                    J21 += dN_deta * elemNodes[i].x;
                    J22 += dN_deta * elemNodes[i].y;
                }
                double detJ = J11 * J22 - J12 * J21;
                minDetJ = std::min(minDetJ, detJ);
                maxDetJ = std::max(maxDetJ, detJ);
            }
        }
    } else {
        // T3: Jacobian is constant
        double J11 = elemNodes[1].x - elemNodes[0].x;
        double J12 = elemNodes[1].y - elemNodes[0].y;
        double J21 = elemNodes[2].x - elemNodes[0].x;
        double J22 = elemNodes[2].y - elemNodes[0].y;
        double detJ = J11 * J22 - J12 * J21;
        minDetJ = detJ;
        maxDetJ = detJ;
    }

    // Compute aspect ratio (longest edge / shortest edge)
    double minEdge = 1e30, maxEdge = 0.0;
    for (int i = 0; i < nNodes; ++i) {
        int j = (i + 1) % nNodes;
        double dx = elemNodes[j].x - elemNodes[i].x;
        double dy = elemNodes[j].y - elemNodes[i].y;
        double len = std::sqrt(dx * dx + dy * dy);
        minEdge = std::min(minEdge, len);
        maxEdge = std::max(maxEdge, len);
    }
    double aspectRatio = (minEdge > 0.0) ? maxEdge / minEdge : 0.0;

    // Element stress data
    bool hasStresses = elemIndex >= 0 &&
        elemIndex < static_cast<int>(result.stresses.size());
    double sigma_xx = 0.0, sigma_yy = 0.0, sigma_xy = 0.0;
    double von_mises = 0.0, sigma_1 = 0.0, sigma_2 = 0.0;
    if (hasStresses) {
        const auto& s = result.stresses[elemIndex];
        sigma_xx = s.sigma_xx;
        sigma_yy = s.sigma_yy;
        sigma_xy = s.sigma_xy;
        von_mises = s.von_mises;
        sigma_1 = s.sigma_1;
        sigma_2 = s.sigma_2;
    }

    // Compute strain from displacement using B matrix at element center
    double eps_xx = 0.0, eps_yy = 0.0, gamma_xy = 0.0;
    bool hasDisplacement = !result.displacement.empty();
    if (hasDisplacement) {
        if (isQuad) {
            // B matrix at center (xi=0, eta=0)
            static const double xi_pts[4] = { -1.0,  1.0,  1.0, -1.0 };
            static const double eta_pts[4] = { -1.0, -1.0,  1.0,  1.0 };
            double J11 = 0.0, J12 = 0.0, J21 = 0.0, J22 = 0.0;
            for (int i = 0; i < 4; ++i) {
                double dN_dxi = 0.25 * xi_pts[i];
                double dN_deta = 0.25 * eta_pts[i];
                J11 += dN_dxi * elemNodes[i].x;
                J12 += dN_dxi * elemNodes[i].y;
                J21 += dN_deta * elemNodes[i].x;
                J22 += dN_deta * elemNodes[i].y;
            }
            double detJ = J11 * J22 - J12 * J21;
            if (std::abs(detJ) > 1e-30) {
                double invJ11 =  J22 / detJ;
                double invJ12 = -J12 / detJ;
                double invJ21 = -J21 / detJ;
                double invJ22 =  J11 / detJ;
                for (int i = 0; i < 4; ++i) {
                    double dN_dxi = 0.25 * xi_pts[i];
                    double dN_deta = 0.25 * eta_pts[i];
                    double dNdx = invJ11 * dN_dxi + invJ12 * dN_deta;
                    double dNdy = invJ21 * dN_dxi + invJ22 * dN_deta;
                    int n = nodeIds[i];
                    double ux = result.displacement[n * DOF_PER_NODE];
                    double uy = result.displacement[n * DOF_PER_NODE + 1];
                    eps_xx += dNdx * ux;
                    eps_yy += dNdy * uy;
                    gamma_xy += dNdy * ux + dNdx * uy;
                }
            }
        } else {
            // T3: constant B matrix
            double J11 = elemNodes[1].x - elemNodes[0].x;
            double J12 = elemNodes[1].y - elemNodes[0].y;
            double J21 = elemNodes[2].x - elemNodes[0].x;
            double J22 = elemNodes[2].y - elemNodes[0].y;
            double detJ = J11 * J22 - J12 * J21;
            if (std::abs(detJ) > 1e-30) {
                double invJ11 =  J22 / detJ;
                double invJ12 = -J12 / detJ;
                double invJ21 = -J21 / detJ;
                double invJ22 =  J11 / detJ;
                static const double dN_dxi[3] = { -1.0, 1.0, 0.0 };
                static const double dN_deta[3] = { -1.0, 0.0, 1.0 };
                for (int i = 0; i < 3; ++i) {
                    double dNdx = invJ11 * dN_dxi[i] + invJ12 * dN_deta[i];
                    double dNdy = invJ21 * dN_dxi[i] + invJ22 * dN_deta[i];
                    int n = nodeIds[i];
                    double ux = result.displacement[n * DOF_PER_NODE];
                    double uy = result.displacement[n * DOF_PER_NODE + 1];
                    eps_xx += dNdx * ux;
                    eps_yy += dNdy * uy;
                    gamma_xy += dNdy * ux + dNdx * uy;
                }
            }
        }
    }

    // Format node ID list
    QString nodeStr;
    for (int i = 0; i < nNodes; ++i) {
        if (i > 0) nodeStr += ", ";
        nodeStr += QString::number(nodeIds[i]);
    }

    // Populate the table
    m_titleLabel->setText(QString("Element %1").arg(elemIndex));

    // Geometry section
    addSectionHeader("Geometry");
    addRow("Element Index", QString::number(elemIndex));
    addRow("Type", isQuad ? "Q4 (Bilinear Quad)" : "T3 (Linear Triangle)");
    addRow("Node IDs", nodeStr);
    addRow("Area", QString::number(area, 'g', 6));
    addRow("Jacobian Min", QString::number(minDetJ, 'g', 6));
    addRow("Jacobian Max", QString::number(maxDetJ, 'g', 6));
    addRow("Aspect Ratio", QString::number(aspectRatio, 'g', 4));

    // Stress section
    addSectionHeader("Stress (at center)");
    addRow("Sigma XX", QString::number(sigma_xx, 'g', 6));
    addRow("Sigma YY", QString::number(sigma_yy, 'g', 6));
    addRow("Sigma XY", QString::number(sigma_xy, 'g', 6));
    addRow("Sigma 1 (Principal)", QString::number(sigma_1, 'g', 6));
    addRow("Sigma 2 (Principal)", QString::number(sigma_2, 'g', 6));
    addRow("Von Mises", QString::number(von_mises, 'g', 6));

    // Strain section
    addSectionHeader("Strain (at center)");
    addRow("Epsilon XX", QString::number(eps_xx, 'g', 6));
    addRow("Epsilon YY", QString::number(eps_yy, 'g', 6));
    addRow("Gamma XY", QString::number(gamma_xy, 'g', 6));

    // Material section
    addSectionHeader("Material");
    addRow("Young's Modulus (E)", QString::number(mesh.mat.E, 'g', 6));
    addRow("Poisson's Ratio (nu)", QString::number(mesh.mat.nu, 'g', 6));
    addRow("Thickness (t)", QString::number(mesh.mat.t, 'g', 6));

    // Nodal displacements
    addSectionHeader("Nodal Displacements");
    for (int i = 0; i < nNodes; ++i) {
        int n = nodeIds[i];
        double ux = 0.0, uy = 0.0;
        if (hasDisplacement) {
            ux = result.displacement[n * DOF_PER_NODE];
            uy = result.displacement[n * DOF_PER_NODE + 1];
        }
        addRow(QString("Node %1: ux").arg(n), QString::number(ux, 'g', 6));
        addRow(QString("Node %1: uy").arg(n), QString::number(uy, 'g', 6));
    }

    // Auto-size columns
    m_table->resizeColumnToContents(0);
}

void ElementInspectorPanel::clear() {
    m_table->setRowCount(0);
    m_titleLabel->setText("No element selected");
}
