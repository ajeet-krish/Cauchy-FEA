#pragma once
#include "editor_state.hpp"
#include "bc_model.hpp"
#include "selection_model.hpp"
#include <QWidget>

class QPushButton;
class QListWidget;
class QDoubleSpinBox;
class QComboBox;
class QLabel;
class QUndoStack;

class BCPanel : public QWidget {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(BCPanel)
public:
    explicit BCPanel(EditorState* state, BCModel* bc_model,
                     SelectionModel* selection, QWidget* parent = nullptr);

    // Set the undo stack for command-based operations
    void setUndoStack(QUndoStack* stack);

    // Update BC list from model
    void updateBCList();

    // Update selected node info
    void updateSelectedNode(int nodeIndex);

public slots:
    void onNodeSelected(int nodeIndex);

signals:
    void bcChanged();
    void toolChanged(ToolMode mode);

private slots:
    void onFixedTool();
    void onRollerXTool();
    void onRollerYTool();
    void onForceTool();
    void onApplyBC();
    void onRemoveBC();
    void onGroupChanged(const QString& group);

private:
    void setupUI();

    EditorState* m_state;
    BCModel* m_bc_model;
    SelectionModel* m_selection;
    QUndoStack* m_undoStack = nullptr;

    // BC type buttons
    QPushButton* m_fixedBtn;
    QPushButton* m_rollerXBtn;
    QPushButton* m_rollerYBtn;
    QPushButton* m_forceBtn;

    // Selected node info
    QLabel* m_nodeLabel;
    QLabel* m_coordLabel;

    // Force inputs
    QDoubleSpinBox* m_forceXSpin;
    QDoubleSpinBox* m_forceYSpin;
    QDoubleSpinBox* m_forceAngleSpin;
    QDoubleSpinBox* m_forceMagSpin;

    // Group controls
    QComboBox* m_groupCombo;
    QPushButton* m_addGroupBtn;
    QPushButton* m_removeGroupBtn;

    // Apply/Remove buttons
    QPushButton* m_applyBtn;
    QPushButton* m_removeBtn;

    // BC list
    QListWidget* m_bcList;

    int m_current_node = -1;
};
