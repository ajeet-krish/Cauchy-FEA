#pragma once
#include <QAbstractTableModel>
#include <vector>
#include "fea.hpp"

enum class ResultTableType { DISPLACEMENT, STRESS };

class ResultModel : public QAbstractTableModel {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ResultModel)
public:
    explicit ResultModel(QObject* parent = nullptr);

    void setData(const fea::SolveResult& result, const Mesh& mesh, ResultTableType type);
    void clear();

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

private:
    ResultTableType m_type = ResultTableType::DISPLACEMENT;
    fea::SolveResult m_result;
    Mesh m_mesh;
};
