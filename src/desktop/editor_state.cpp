#include "editor_state.hpp"

bool EditorState::isDrawingTool() const {
    return current_mode == ToolMode::DRAW_RECT ||
           current_mode == ToolMode::DRAW_LINE ||
           current_mode == ToolMode::DRAW_CIRCLE;
}

bool EditorState::isBCTool() const {
    return current_mode == ToolMode::ASSIGN_FIXED ||
           current_mode == ToolMode::ASSIGN_ROLLER_X ||
           current_mode == ToolMode::ASSIGN_ROLLER_Y;
}

bool EditorState::isForceTool() const {
    return current_mode == ToolMode::APPLY_FORCE;
}

QString EditorState::currentToolName() const {
    switch (current_mode) {
    case ToolMode::SELECT:          return QStringLiteral("Select");
    case ToolMode::DRAW_RECT:       return QStringLiteral("Draw Rectangle");
    case ToolMode::DRAW_LINE:       return QStringLiteral("Draw Line");
    case ToolMode::DRAW_CIRCLE:     return QStringLiteral("Draw Circle");
    case ToolMode::ASSIGN_FIXED:    return QStringLiteral("Assign Fixed BC");
    case ToolMode::ASSIGN_ROLLER_X: return QStringLiteral("Assign Roller (X-fixed)");
    case ToolMode::ASSIGN_ROLLER_Y: return QStringLiteral("Assign Roller (Y-fixed)");
    case ToolMode::APPLY_FORCE:     return QStringLiteral("Apply Force");
    case ToolMode::PAN_ZOOM:        return QStringLiteral("Pan / Zoom");
    }
    return QStringLiteral("Unknown");
}

void EditorState::resetInteraction() {
    is_drawing = false;
    is_selecting = false;
    is_dragging = false;
    selected_primitive_index = -1;
    selected_node_index = -1;
    selected_edge_index = -1;
}
