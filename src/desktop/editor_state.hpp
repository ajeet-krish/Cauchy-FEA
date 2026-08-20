#pragma once
#include <QPointF>
#include <QString>

enum class ToolMode {
    SELECT,
    DRAW_RECT,
    DRAW_LINE,
    DRAW_CIRCLE,
    ASSIGN_FIXED,
    ASSIGN_ROLLER_X,
    ASSIGN_ROLLER_Y,
    APPLY_FORCE,
    PAN_ZOOM
};

struct EditorState {
    ToolMode current_mode = ToolMode::PAN_ZOOM;
    int selected_primitive_index = -1;
    int selected_node_index = -1;
    int selected_edge_index = -1;
    bool is_drawing = false;
    QPointF draw_start;
    QPointF draw_current;
    double snap_tolerance = 0.02;

    // Selection box for drag-select
    bool is_selecting = false;
    QPointF selection_start;
    QPointF selection_end;

    // Drag-move state
    bool is_dragging = false;
    QPointF drag_start;
    QPointF drag_current;

    // Force input (cached from last apply-force interaction)
    double pending_force_magnitude = 0.0;
    double pending_force_angle_deg = 0.0;

    // Utility queries
    bool isDrawingTool() const;
    bool isBCTool() const;
    bool isForceTool() const;
    QString currentToolName() const;

    // Reset interactive state (keeps current_mode)
    void resetInteraction();
};
