#include "analytical_panel.hpp"
#include <QTableWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QFont>

AnalyticalPanel::AnalyticalPanel(QWidget* parent)
    : QWidget(parent) {

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels({
        "Quantity", "Analytical", "FEA", "Error %", "Status", "Formula"
    });
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QTableWidget::NoEditTriggers);
    m_table->setSelectionBehavior(QTableWidget::SelectRows);
    m_table->setSelectionMode(QTableWidget::SingleSelection);
    m_table->setAlternatingRowColors(true);

    // Dark theme stylesheet
    m_table->setStyleSheet(
        "QTableWidget {"
        "  background-color: #0d1117;"
        "  alternate-background-color: #161b22;"
        "  color: #c9d1d9;"
        "  gridline-color: #30363d;"
        "  border: 1px solid #30363d;"
        "  font-family: 'JetBrains Mono', monospace;"
        "  font-size: 10px;"
        "}"
        "QTableWidget::item {"
        "  padding: 4px 6px;"
        "}"
        "QHeaderView::section {"
        "  background-color: #161b22;"
        "  color: #c9d1d9;"
        "  border: 1px solid #30363d;"
        "  padding: 4px 6px;"
        "  font-weight: bold;"
        "}"
    );

    m_summaryLabel = new QLabel("No analytical comparison data", this);
    m_summaryLabel->setStyleSheet(
        "QLabel {"
        "  color: #8b949e;"
        "  font-family: 'JetBrains Mono', monospace;"
        "  font-size: 10px;"
        "  padding: 4px;"
        "}"
    );

    layout->addWidget(m_table);
    layout->addWidget(m_summaryLabel);
}

void AnalyticalPanel::formatScientific(QTableWidgetItem* item, double value) {
    if (std::abs(value) >= 1.0e6 || (std::abs(value) < 1.0e-3 && value != 0.0)) {
        item->setText(QString::number(value, 'g', 4));
    } else {
        item->setText(QString::number(value, 'g', 6));
    }
}

void AnalyticalPanel::setData(const std::vector<AnalyticalRow>& rows) {
    m_table->setRowCount(static_cast<int>(rows.size()));

    int passed = 0;
    int total = static_cast<int>(rows.size());

    for (int i = 0; i < total; ++i) {
        const auto& row = rows[i];

        if (row.passed) ++passed;

        // Quantity
        auto* qtyItem = new QTableWidgetItem(QString::fromStdString(row.quantity));
        qtyItem->setForeground(QColor(0xc9, 0xd1, 0xd9));
        m_table->setItem(i, 0, qtyItem);

        // Analytical value
        auto* anaItem = new QTableWidgetItem();
        formatScientific(anaItem, row.analytical_value);
        anaItem->setForeground(QColor(0x00, 0xd4, 0xff));
        m_table->setItem(i, 1, anaItem);

        // FEA value
        auto* feaItem = new QTableWidgetItem();
        formatScientific(feaItem, row.fea_value);
        feaItem->setForeground(QColor(0xff, 0xb3, 0x47));
        m_table->setItem(i, 2, feaItem);

        // Error %
        auto* errItem = new QTableWidgetItem();
        errItem->setText(QString::number(row.error_pct, 'f', 2) + "%");
        if (row.error_pct < 5.0) {
            errItem->setForeground(QColor(0x3f, 0xb9, 0x50));
        } else if (row.error_pct < 10.0) {
            errItem->setForeground(QColor(0xff, 0xb3, 0x47));
        } else {
            errItem->setForeground(QColor(0xff, 0x47, 0x57));
        }
        m_table->setItem(i, 3, errItem);

        // Status
        auto* statusItem = new QTableWidgetItem();
        if (row.passed) {
            statusItem->setText("PASS");
            statusItem->setForeground(QColor(0x3f, 0xb9, 0x50));
        } else {
            statusItem->setText("FAIL");
            statusItem->setForeground(QColor(0xff, 0x47, 0x57));
        }
        QFont boldFont;
        boldFont.setBold(true);
        statusItem->setFont(boldFont);
        m_table->setItem(i, 4, statusItem);

        // Formula
        auto* formulaItem = new QTableWidgetItem(QString::fromStdString(row.formula));
        formulaItem->setForeground(QColor(0x8b, 0x94, 0x9e));
        m_table->setItem(i, 5, formulaItem);
    }

    // Summary
    double passRate = (total > 0) ? (static_cast<double>(passed) / total * 100.0) : 0.0;
    QColor summaryColor = (passed == total) ? QColor(0x3f, 0xb9, 0x50) : QColor(0xff, 0xb3, 0x47);
    m_summaryLabel->setText(QString("%1/%2 checks passed (%3%)")
        .arg(passed).arg(total).arg(passRate, 0, 'f', 0));
    m_summaryLabel->setStyleSheet(
        QString("QLabel {"
                "  color: %1;"
                "  font-family: 'JetBrains Mono', monospace;"
                "  font-size: 11px;"
                "  font-weight: bold;"
                "  padding: 4px;"
                "}"
        ).arg(summaryColor.name()));
}

void AnalyticalPanel::clear() {
    m_table->setRowCount(0);
    m_summaryLabel->setText("No analytical comparison data");
    m_summaryLabel->setStyleSheet(
        "QLabel {"
        "  color: #8b949e;"
        "  font-family: 'JetBrains Mono', monospace;"
        "  font-size: 10px;"
        "  padding: 4px;"
        "}"
    );
}
