#pragma once
#include <QWidget>
#include "fea.hpp"

class QTableWidget;
class QLabel;

class ElementInspectorPanel : public QWidget {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ElementInspectorPanel)
public:
    explicit ElementInspectorPanel(QWidget* parent = nullptr);

public slots:
    void inspectElement(int elemIndex, const Mesh& mesh, const fea::SolveResult& result);
    void clear();

private:
    void addRow(const QString& label, const QString& value);
    void addSectionHeader(const QString& title);

    QTableWidget* m_table = nullptr;
    QLabel* m_titleLabel = nullptr;
};
