#pragma once
#include <QPointF>
#include <QRectF>
#include <vector>

class SelectionModel {
public:
    SelectionModel();
    ~SelectionModel();

    // Single selection
    void selectNode(int node_index);
    void selectEdge(int edge_index);
    void clearSelection();

    // Multi-selection (drag box)
    void startSelection(const QPointF& start);
    void updateSelection(const QPointF& end);
    void finishSelection();

    // Query
    const std::vector<int>& selectedNodes() const;
    const std::vector<int>& selectedEdges() const;
    bool isSelecting() const;
    QPointF selectionStart() const;
    QPointF selectionEnd() const;
    QRectF selectionRect() const;

    // Check if node/edge is selected
    bool isNodeSelected(int node_index) const;
    bool isEdgeSelected(int edge_index) const;

    // Selection counts
    int selectedNodeCount() const;
    int selectedEdgeCount() const;

    // Toggle node in selection (for multi-select with Ctrl)
    void toggleNodeSelection(int node_index);

    // Select all nodes within a rectangle
    void selectNodesInRect(const QRectF& rect, const std::vector<struct Node>& nodes);

private:
    std::vector<int> m_selected_nodes;
    std::vector<int> m_selected_edges;
    bool m_is_selecting = false;
    QPointF m_selection_start;
    QPointF m_selection_end;
};
