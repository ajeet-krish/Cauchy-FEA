#include "bc_panel.hpp"
#include "undo_commands.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QButtonGroup>
#include <QUndoStack>

BCPanel::BCPanel(EditorState* state, BCModel* bc_model,
                 SelectionModel* selection, QWidget* parent)
    : QWidget(parent)
    , m_state(state)
    , m_bc_model(bc_model)
    , m_selection(selection) {
    setupUI();
}

void BCPanel::setUndoStack(QUndoStack* stack) {
    m_undoStack = stack;
}

void BCPanel::setupUI() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(4);

    // ------------------------------------------------------------------
    // BC type tools group
    // ------------------------------------------------------------------
    auto* toolsGroup = new QGroupBox("Boundary Conditions", this);
    auto* toolsLayout = new QHBoxLayout(toolsGroup);
    toolsLayout->setSpacing(4);

    m_fixedBtn = new QPushButton("Fixed", this);
    m_fixedBtn->setCheckable(true);
    m_fixedBtn->setToolTip("Assign fixed support (all DOFs constrained)");
    toolsLayout->addWidget(m_fixedBtn);

    m_rollerXBtn = new QPushButton("Roller X", this);
    m_rollerXBtn->setCheckable(true);
    m_rollerXBtn->setToolTip("Assign roller (X constrained, Y free)");
    toolsLayout->addWidget(m_rollerXBtn);

    m_rollerYBtn = new QPushButton("Roller Y", this);
    m_rollerYBtn->setCheckable(true);
    m_rollerYBtn->setToolTip("Assign roller (Y constrained, X free)");
    toolsLayout->addWidget(m_rollerYBtn);

    m_forceBtn = new QPushButton("Force", this);
    m_forceBtn->setCheckable(true);
    m_forceBtn->setToolTip("Apply force at node");
    toolsLayout->addWidget(m_forceBtn);

    auto* toolGroup = new QButtonGroup(this);
    toolGroup->addButton(m_fixedBtn);
    toolGroup->addButton(m_rollerXBtn);
    toolGroup->addButton(m_rollerYBtn);
    toolGroup->addButton(m_forceBtn);
    toolGroup->setExclusive(true);

    connect(m_fixedBtn, &QPushButton::clicked, this, &BCPanel::onFixedTool);
    connect(m_rollerXBtn, &QPushButton::clicked, this, &BCPanel::onRollerXTool);
    connect(m_rollerYBtn, &QPushButton::clicked, this, &BCPanel::onRollerYTool);
    connect(m_forceBtn, &QPushButton::clicked, this, &BCPanel::onForceTool);

    mainLayout->addWidget(toolsGroup);

    // ------------------------------------------------------------------
    // Selected node info
    // ------------------------------------------------------------------
    auto* infoGroup = new QGroupBox("Selected Node", this);
    auto* infoLayout = new QFormLayout(infoGroup);

    m_nodeLabel = new QLabel("None", this);
    infoLayout->addRow("Node:", m_nodeLabel);

    m_coordLabel = new QLabel("-", this);
    infoLayout->addRow("Position:", m_coordLabel);

    mainLayout->addWidget(infoGroup);

    // ------------------------------------------------------------------
    // Force inputs
    // ------------------------------------------------------------------
    auto* forceGroup = new QGroupBox("Force Parameters", this);
    auto* forceLayout = new QFormLayout(forceGroup);

    m_forceMagSpin = new QDoubleSpinBox(this);
    m_forceMagSpin->setRange(-1e10, 1e10);
    m_forceMagSpin->setValue(-1000.0);
    m_forceMagSpin->setDecimals(1);
    m_forceMagSpin->setSingleStep(100.0);
    m_forceMagSpin->setSuffix(" N");
    forceLayout->addRow("Magnitude:", m_forceMagSpin);

    m_forceAngleSpin = new QDoubleSpinBox(this);
    m_forceAngleSpin->setRange(-360.0, 360.0);
    m_forceAngleSpin->setValue(-90.0);
    m_forceAngleSpin->setDecimals(1);
    m_forceAngleSpin->setSingleStep(15.0);
    m_forceAngleSpin->setSuffix(" deg");
    forceLayout->addRow("Angle:", m_forceAngleSpin);

    m_forceXSpin = new QDoubleSpinBox(this);
    m_forceXSpin->setRange(-1e10, 1e10);
    m_forceXSpin->setValue(0.0);
    m_forceXSpin->setDecimals(3);
    m_forceXSpin->setSingleStep(100.0);
    m_forceXSpin->setSuffix(" N");
    m_forceXSpin->setEnabled(false);
    forceLayout->addRow("Force X:", m_forceXSpin);

    m_forceYSpin = new QDoubleSpinBox(this);
    m_forceYSpin->setRange(-1e10, 1e10);
    m_forceYSpin->setValue(-1000.0);
    m_forceYSpin->setDecimals(3);
    m_forceYSpin->setSingleStep(100.0);
    m_forceYSpin->setSuffix(" N");
    m_forceYSpin->setEnabled(false);
    forceLayout->addRow("Force Y:", m_forceYSpin);

    mainLayout->addWidget(forceGroup);

    // ------------------------------------------------------------------
    // BC groups
    // ------------------------------------------------------------------
    auto* groupGroup = new QGroupBox("BC Groups", this);
    auto* groupLayout = new QVBoxLayout(groupGroup);

    auto* groupComboLayout = new QHBoxLayout();
    m_groupCombo = new QComboBox(this);
    m_groupCombo->addItem("Default");
    groupComboLayout->addWidget(m_groupCombo);

    m_addGroupBtn = new QPushButton("+", this);
    m_addGroupBtn->setMaximumWidth(30);
    m_addGroupBtn->setToolTip("Add new BC group");
    groupComboLayout->addWidget(m_addGroupBtn);

    m_removeGroupBtn = new QPushButton("-", this);
    m_removeGroupBtn->setMaximumWidth(30);
    m_removeGroupBtn->setToolTip("Remove selected group");
    groupComboLayout->addWidget(m_removeGroupBtn);

    groupLayout->addLayout(groupComboLayout);

    connect(m_groupCombo, &QComboBox::currentTextChanged,
            this, &BCPanel::onGroupChanged);

    mainLayout->addWidget(groupGroup);

    // ------------------------------------------------------------------
    // Apply / Remove buttons
    // ------------------------------------------------------------------
    auto* actionLayout = new QHBoxLayout();
    m_applyBtn = new QPushButton("Apply BC", this);
    m_removeBtn = new QPushButton("Remove BC", this);
    actionLayout->addWidget(m_applyBtn);
    actionLayout->addWidget(m_removeBtn);

    connect(m_applyBtn, &QPushButton::clicked, this, &BCPanel::onApplyBC);
    connect(m_removeBtn, &QPushButton::clicked, this, &BCPanel::onRemoveBC);

    mainLayout->addLayout(actionLayout);

    // ------------------------------------------------------------------
    // BC list
    // ------------------------------------------------------------------
    auto* listGroup = new QGroupBox("Assigned BCs", this);
    auto* listLayout = new QVBoxLayout(listGroup);

    m_bcList = new QListWidget(this);
    m_bcList->setMaximumHeight(150);
    listLayout->addWidget(m_bcList);

    mainLayout->addWidget(listGroup);
}

// ------------------------------------------------------------------
// BC type tool slots
// ------------------------------------------------------------------
void BCPanel::onFixedTool() {
    m_state->current_mode = ToolMode::ASSIGN_FIXED;
    emit toolChanged(ToolMode::ASSIGN_FIXED);
}

void BCPanel::onRollerXTool() {
    m_state->current_mode = ToolMode::ASSIGN_ROLLER_X;
    emit toolChanged(ToolMode::ASSIGN_ROLLER_X);
}

void BCPanel::onRollerYTool() {
    m_state->current_mode = ToolMode::ASSIGN_ROLLER_Y;
    emit toolChanged(ToolMode::ASSIGN_ROLLER_Y);
}

void BCPanel::onForceTool() {
    m_state->current_mode = ToolMode::APPLY_FORCE;
    m_state->pending_force_magnitude = m_forceMagSpin->value();
    m_state->pending_force_angle_deg = m_forceAngleSpin->value();
    emit toolChanged(ToolMode::APPLY_FORCE);
}

// ------------------------------------------------------------------
// Apply / Remove BC
// ------------------------------------------------------------------
void BCPanel::onApplyBC() {
    if (m_current_node < 0) return;

    // Determine BC type from current tool mode
    BCType type = BCType::FIXED;
    switch (m_state->current_mode) {
    case ToolMode::ASSIGN_FIXED:    type = BCType::FIXED; break;
    case ToolMode::ASSIGN_ROLLER_X: type = BCType::ROLLER_X; break;
    case ToolMode::ASSIGN_ROLLER_Y: type = BCType::ROLLER_Y; break;
    case ToolMode::APPLY_FORCE:     type = BCType::FORCE; break;
    default: break;
    }

    BoundaryCondition bc;
    bc.node_index = m_current_node;
    bc.type = type;
    bc.value = (type == BCType::FORCE) ? m_forceMagSpin->value() : 0.0;
    bc.group = m_groupCombo->currentText();

    if (m_undoStack) {
        m_undoStack->push(new AddBCCommand(
            m_bc_model, bc, "Apply Boundary Condition"));
    } else {
        m_bc_model->addBC(bc);
    }

    updateBCList();
    emit bcChanged();
}

void BCPanel::onRemoveBC() {
    int row = m_bcList->currentRow();
    if (row < 0 || row >= m_bc_model->bcCount()) return;

    const auto& bcs = m_bc_model->bcs();
    if (m_undoStack) {
        m_undoStack->push(new RemoveBCCommand(
            m_bc_model, bcs[row].node_index, "Remove Boundary Condition"));
    } else {
        m_bc_model->removeBC(bcs[row].node_index);
    }

    updateBCList();
    emit bcChanged();
}

void BCPanel::onGroupChanged(const QString& group) {
    Q_UNUSED(group)
}

// ------------------------------------------------------------------
// Update BC list from model
// ------------------------------------------------------------------
void BCPanel::updateBCList() {
    m_bcList->clear();
    const auto& bcs = m_bc_model->bcs();
    for (const auto& bc : bcs) {
        QString type_str;
        switch (bc.type) {
        case BCType::FIXED:    type_str = "Fixed"; break;
        case BCType::ROLLER_X: type_str = "Roller X"; break;
        case BCType::ROLLER_Y: type_str = "Roller Y"; break;
        case BCType::FORCE:    type_str = "Force"; break;
        }

        QString item_text = QString("Node %1: %2 [%3]")
            .arg(bc.node_index)
            .arg(type_str)
            .arg(bc.group);

        if (bc.type == BCType::FORCE) {
            item_text += QString(" (%1 N)").arg(bc.value, 0, 'f', 1);
        }

        m_bcList->addItem(item_text);
    }
}

// ------------------------------------------------------------------
// Update selected node info
// ------------------------------------------------------------------
void BCPanel::updateSelectedNode(int nodeIndex) {
    m_current_node = nodeIndex;
    if (nodeIndex >= 0) {
        m_nodeLabel->setText(QString::number(nodeIndex));
        m_coordLabel->setText(QString("(%1, %2)")
            .arg(nodeIndex)
            .arg("-"));
    } else {
        m_nodeLabel->setText("None");
        m_coordLabel->setText("-");
    }
}

void BCPanel::onNodeSelected(int nodeIndex) {
    updateSelectedNode(nodeIndex);
}
