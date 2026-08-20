#include "selection_model.hpp"
#include "fea_types.hpp"
#include <algorithm>
#include <cmath>

SelectionModel::SelectionModel() = default;
SelectionModel::~SelectionModel() = default;

// ------------------------------------------------------------------
// Single selection
// ------------------------------------------------------------------
void SelectionModel::selectNode(int node_index) {
    m_selected_nodes.clear();
    m_selected_edges.clear();
    m_selected_nodes.push_back(node_index);
}

void SelectionModel::selectEdge(int edge_index) {
    m_selected_nodes.clear();
    m_selected_edges.clear();
    m_selected_edges.push_back(edge_index);
}

void SelectionModel::clearSelection() {
    m_selected_nodes.clear();
    m_selected_edges.clear();
}

// ------------------------------------------------------------------
// Multi-selection (drag box)
// ------------------------------------------------------------------
void SelectionModel::startSelection(const QPointF& start) {
    m_is_selecting = true;
    m_selection_start = start;
    m_selection_end = start;
}

void SelectionModel::updateSelection(const QPointF& end) {
    m_selection_end = end;
}

void SelectionModel::finishSelection() {
    m_is_selecting = false;
}

// ------------------------------------------------------------------
// Query
// ------------------------------------------------------------------
const std::vector<int>& SelectionModel::selectedNodes() const {
    return m_selected_nodes;
}

const std::vector<int>& SelectionModel::selectedEdges() const {
    return m_selected_edges;
}

bool SelectionModel::isSelecting() const {
    return m_is_selecting;
}

QPointF SelectionModel::selectionStart() const {
    return m_selection_start;
}

QPointF SelectionModel::selectionEnd() const {
    return m_selection_end;
}

QRectF SelectionModel::selectionRect() const {
    double x1 = std::min(m_selection_start.x(), m_selection_end.x());
    double y1 = std::min(m_selection_start.y(), m_selection_end.y());
    double x2 = std::max(m_selection_start.x(), m_selection_end.x());
    double y2 = std::max(m_selection_start.y(), m_selection_end.y());
    return QRectF(QPointF(x1, y1), QPointF(x2, y2));
}

// ------------------------------------------------------------------
// Membership checks
// ------------------------------------------------------------------
bool SelectionModel::isNodeSelected(int node_index) const {
    return std::find(m_selected_nodes.begin(), m_selected_nodes.end(), node_index)
           != m_selected_nodes.end();
}

bool SelectionModel::isEdgeSelected(int edge_index) const {
    return std::find(m_selected_edges.begin(), m_selected_edges.end(), edge_index)
           != m_selected_edges.end();
}

int SelectionModel::selectedNodeCount() const {
    return static_cast<int>(m_selected_nodes.size());
}

int SelectionModel::selectedEdgeCount() const {
    return static_cast<int>(m_selected_edges.size());
}

// ------------------------------------------------------------------
// Toggle selection
// ------------------------------------------------------------------
void SelectionModel::toggleNodeSelection(int node_index) {
    auto it = std::find(m_selected_nodes.begin(), m_selected_nodes.end(), node_index);
    if (it != m_selected_nodes.end()) {
        m_selected_nodes.erase(it);
    } else {
        m_selected_nodes.push_back(node_index);
    }
}

void SelectionModel::selectNodesInRect(const QRectF& rect, const std::vector<Node>& nodes) {
    for (int i = 0; i < static_cast<int>(nodes.size()); ++i) {
        QPointF node_pos(nodes[i].x, nodes[i].y);
        if (rect.contains(node_pos)) {
            if (!isNodeSelected(i)) {
                m_selected_nodes.push_back(i);
            }
        }
    }
}
