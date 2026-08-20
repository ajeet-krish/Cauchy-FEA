#pragma once
#include <QWidget>
#include <vector>
#include "analytical.hpp"

class QTableWidget;
class QTableWidgetItem;
class QLabel;

class AnalyticalPanel : public QWidget {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(AnalyticalPanel)
public:
    explicit AnalyticalPanel(QWidget* parent = nullptr);

    void setData(const std::vector<AnalyticalRow>& rows);
    void clear();

private:
    void formatScientific(QTableWidgetItem* item, double value);

    QTableWidget* m_table = nullptr;
    QLabel* m_summaryLabel = nullptr;
};
