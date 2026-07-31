#include "result_model.hpp"
#include <QString>

ResultModel::ResultModel(QObject* parent)
    : QAbstractTableModel(parent) {}

void ResultModel::setData(const fea::SolveResult& result, const Mesh& mesh, ResultTableType type) {
    beginResetModel();
    m_result = result;
    m_mesh = mesh;
    m_type = type;
    endResetModel();
}

void ResultModel::clear() {
    beginResetModel();
    m_result = fea::SolveResult();
    m_mesh = Mesh();
    endResetModel();
}

int ResultModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    if (m_type == ResultTableType::DISPLACEMENT) {
        return m_mesh.num_nodes();
    } else {
        return m_result.stresses.size();
    }
}

int ResultModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    if (m_type == ResultTableType::DISPLACEMENT) {
        return 6; // Node ID, X, Y, Ux, Uy, |u|
    } else {
        return 7; // Elem ID, sig_xx, sig_yy, sig_xy, Von Mises, sig_1, sig_2
    }
}

QVariant ResultModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) return QVariant();

    int row = index.row();
    int col = index.column();

    if (role == Qt::TextAlignmentRole) {
        return int(Qt::AlignRight | Qt::AlignVCenter);
    }

    if (role != Qt::DisplayRole) return QVariant();

    if (m_type == ResultTableType::DISPLACEMENT) {
        if (row < 0 || row >= m_mesh.num_nodes()) return QVariant();
        const auto& node = m_mesh.nodes[row];
        double ux = (row * 2 < (int)m_result.displacement.size()) ? m_result.displacement[row * 2] : 0.0;
        double uy = (row * 2 + 1 < (int)m_result.displacement.size()) ? m_result.displacement[row * 2 + 1] : 0.0;
        double mag = std::sqrt(ux * ux + uy * uy);

        switch (col) {
        case 0: return row;
        case 1: return QString::number(node.x, 'f', 4);
        case 2: return QString::number(node.y, 'f', 4);
        case 3: return QString::number(ux, 'e', 4);
        case 4: return QString::number(uy, 'e', 4);
        case 5: return QString::number(mag, 'e', 4);
        }
    } else {
        if (row < 0 || row >= (int)m_result.stresses.size()) return QVariant();
        const auto& s = m_result.stresses[row];

        switch (col) {
        case 0: return row;
        case 1: return QString::number(s.sigma_xx, 'e', 3);
        case 2: return QString::number(s.sigma_yy, 'e', 3);
        case 3: return QString::number(s.sigma_xy, 'e', 3);
        case 4: return QString::number(s.von_mises, 'e', 3);
        case 5: return QString::number(s.sigma_1, 'e', 3);
        case 6: return QString::number(s.sigma_2, 'e', 3);
        }
    }

    return QVariant();
}

QVariant ResultModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole) return QVariant();

    if (orientation == Qt::Horizontal) {
        if (m_type == ResultTableType::DISPLACEMENT) {
            static const QString headers[] = {"Node", "X (m)", "Y (m)", "Ux (m)", "Uy (m)", "|u| (m)"};
            if (section >= 0 && section < 6) return headers[section];
        } else {
            static const QString headers[] = {"Elem", "sig_xx (Pa)", "sig_yy (Pa)", "sig_xy (Pa)", "Von Mises (Pa)", "sig_1 (Pa)", "sig_2 (Pa)"};
            if (section >= 0 && section < 7) return headers[section];
        }
    }

    return QVariant();
}
