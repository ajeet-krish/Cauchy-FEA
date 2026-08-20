#include "geometry_panel.hpp"
#include "geometry_primitive.hpp"
#include "undo_commands.hpp"
#include "material_library.hpp"
#include "../fea_types.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QComboBox>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUndoStack>
#include <QSignalBlocker>

GeometryPanel::GeometryPanel(EditorState* state, GeometryModel* model,
                             BCModel* bc_model, QWidget* parent)
    : QWidget(parent)
    , m_state(state)
    , m_model(model)
    , m_bc_model(bc_model) {
    setupUI();
}

void GeometryPanel::setUndoStack(QUndoStack* stack) {
    m_undoStack = stack;
}

void GeometryPanel::setupUI() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(4);

    // ------------------------------------------------------------------
    // Material group
    // ------------------------------------------------------------------
    auto* matGroup = new QGroupBox("Material", this);
    auto* matLayout = new QFormLayout(matGroup);

    // Material preset dropdown
    m_materialCombo = new QComboBox(this);
    const auto& library = getMaterialLibrary();
    for (const auto& preset : library) {
        m_materialCombo->addItem(preset.name);
    }
    matLayout->addRow("Preset:", m_materialCombo);

    m_ESpin = new QDoubleSpinBox(this);
    m_ESpin->setRange(1e6, 1e15);
    m_ESpin->setValue(210e9);
    m_ESpin->setDecimals(3);
    m_ESpin->setSingleStep(1e9);
    m_ESpin->setSuffix(" Pa");
    matLayout->addRow("Young's Modulus (E):", m_ESpin);

    m_nuSpin = new QDoubleSpinBox(this);
    m_nuSpin->setRange(0.0, 0.499);
    m_nuSpin->setValue(0.3);
    m_nuSpin->setDecimals(3);
    m_nuSpin->setSingleStep(0.01);
    matLayout->addRow("Poisson's Ratio (v):", m_nuSpin);

    m_tSpin = new QDoubleSpinBox(this);
    m_tSpin->setRange(0.001, 10.0);
    m_tSpin->setValue(0.01);
    m_tSpin->setDecimals(4);
    m_tSpin->setSingleStep(0.001);
    m_tSpin->setSuffix(" m");
    matLayout->addRow("Thickness (t):", m_tSpin);

    m_planeCombo = new QComboBox(this);
    m_planeCombo->addItems({"Plane Stress", "Plane Strain"});
    matLayout->addRow("Plane Type:", m_planeCombo);

    connect(m_materialCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &GeometryPanel::onMaterialPresetChanged);

    mainLayout->addWidget(matGroup);

    // ------------------------------------------------------------------
    // Mesh generation group
    // ------------------------------------------------------------------
    auto* meshGroup = new QGroupBox("Mesh Generation", this);
    auto* meshLayout = new QFormLayout(meshGroup);

    m_nxSpin = new QSpinBox(this);
    m_nxSpin->setRange(2, 256);
    m_nxSpin->setValue(32);
    meshLayout->addRow("NX:", m_nxSpin);

    m_nySpin = new QSpinBox(this);
    m_nySpin->setRange(2, 256);
    m_nySpin->setValue(8);
    meshLayout->addRow("NY:", m_nySpin);

    m_generateBtn = new QPushButton("Generate Mesh", this);
    connect(m_generateBtn, &QPushButton::clicked, this, &GeometryPanel::onGenerateMesh);
    meshLayout->addRow(m_generateBtn);

    mainLayout->addWidget(meshGroup);

    // ------------------------------------------------------------------
    // CAD Worktree (Objects, Boundary Conditions, Mesh)
    // ------------------------------------------------------------------
    auto* treeGroup = new QGroupBox("Objects", this);
    auto* treeLayout = new QVBoxLayout(treeGroup);

    m_worktree = new QTreeWidget(this);
    m_worktree->setHeaderLabels({"Name", "Details"});
    m_worktree->setColumnCount(2);
    m_worktree->setHeaderHidden(false);
    m_worktree->setRootIsDecorated(true);
    m_worktree->setMinimumHeight(180);
    m_worktree->setMaximumHeight(300);
    connect(m_worktree, &QTreeWidget::itemClicked,
            this, &GeometryPanel::onWorktreeItemClicked);
    treeLayout->addWidget(m_worktree);

    // Delete button for selected primitive
    auto* btnLayout = new QHBoxLayout();
    m_deleteBtn = new QPushButton("Delete", this);
    m_deleteBtn->setToolTip("Delete selected object");
    connect(m_deleteBtn, &QPushButton::clicked, this, &GeometryPanel::onDeletePrimitive);
    btnLayout->addWidget(m_deleteBtn);
    btnLayout->addStretch();
    treeLayout->addLayout(btnLayout);

    mainLayout->addWidget(treeGroup);

    // ------------------------------------------------------------------
    // Properties panel (shows details of selected object)
    // ------------------------------------------------------------------
    auto* propGroup = new QGroupBox("Properties", this);
    auto* propLayout = new QFormLayout(propGroup);

    m_propTypeLabel = new QLabel("None", this);
    propLayout->addRow("Type:", m_propTypeLabel);

    m_propLabel = new QLabel("-", this);
    propLayout->addRow("Name:", m_propLabel);

    m_propDetails = new QLabel("-", this);
    m_propDetails->setWordWrap(true);
    propLayout->addRow("Details:", m_propDetails);

    mainLayout->addWidget(propGroup);

    // ------------------------------------------------------------------
    // Mesh info
    // ------------------------------------------------------------------
    m_meshInfoLabel = new QLabel("No mesh generated", this);
    m_meshInfoLabel->setStyleSheet("color: #8b949e; font-size: 10px;");
    mainLayout->addWidget(m_meshInfoLabel);

    mainLayout->addStretch();
}

// ------------------------------------------------------------------
// Worktree update
// ------------------------------------------------------------------
void GeometryPanel::updateWorktree() {
    m_worktree->clear();

    // Objects section
    auto* objectsRoot = new QTreeWidgetItem(m_worktree);
    objectsRoot->setText(0, "Objects");
    objectsRoot->setExpanded(true);

    const auto& prims = m_model->primitives();
    for (int i = 0; i < static_cast<int>(prims.size()); ++i) {
        auto* item = new QTreeWidgetItem(objectsRoot);
        QString typeName;
        QString details;
        QString label;

        std::visit([&](const auto& p) {
            using T = std::decay_t<decltype(p)>;
            label = p.label;
            if constexpr (std::is_same_v<T, RectPrimitive>) {
                typeName = "Rectangle";
                double w = std::abs(p.width);
                double h = std::abs(p.height);
                details = QString("(%1, %2) %3 x %4")
                    .arg(p.x, 0, 'f', 3).arg(p.y, 0, 'f', 3)
                    .arg(w, 0, 'f', 3).arg(h, 0, 'f', 3);
            } else if constexpr (std::is_same_v<T, LinePrimitive>) {
                typeName = "Line";
                double len = getLineLength(p);
                details = QString("(%1, %2) -> (%3, %4)  L=%5")
                    .arg(p.x1, 0, 'f', 3).arg(p.y1, 0, 'f', 3)
                    .arg(p.x2, 0, 'f', 3).arg(p.y2, 0, 'f', 3)
                    .arg(len, 0, 'f', 3);
            } else if constexpr (std::is_same_v<T, CirclePrimitive>) {
                typeName = "Circle";
                details = QString("center=(%1, %2) r=%3")
                    .arg(p.cx, 0, 'f', 3).arg(p.cy, 0, 'f', 3)
                    .arg(p.radius, 0, 'f', 3);
            }
        }, prims[i]);

        item->setText(0, QString("%1 %2").arg(typeName).arg(label));
        item->setText(1, details);
        item->setData(0, Qt::UserRole, i);
    }

    // Boundary Conditions section
    if (m_bc_model && m_bc_model->bcCount() > 0) {
        auto* bcRoot = new QTreeWidgetItem(m_worktree);
        bcRoot->setText(0, "Boundary Conditions");
        bcRoot->setExpanded(true);

        const auto& groups = m_bc_model->groups();
        for (const auto& group : groups) {
            if (group.node_indices.empty()) continue;
            auto* item = new QTreeWidgetItem(bcRoot);
            QString typeName;
            switch (group.type) {
            case BCType::FIXED:    typeName = "Fixed"; break;
            case BCType::ROLLER_X: typeName = "Roller X"; break;
            case BCType::ROLLER_Y: typeName = "Roller Y"; break;
            case BCType::FORCE:    typeName = "Force"; break;
            }
            item->setText(0, QString("%1 Group").arg(typeName));
            item->setText(1, QString("%1 nodes").arg(group.node_indices.size()));
        }

        // Ungrouped BCs
        int ungrouped = 0;
        for (const auto& bc : m_bc_model->bcs()) {
            if (bc.group.isEmpty() || bc.group == "Default") {
                ungrouped++;
            }
        }
        if (ungrouped > 0) {
            auto* item = new QTreeWidgetItem(bcRoot);
            item->setText(0, "Ungrouped");
            item->setText(1, QString("%1 nodes").arg(ungrouped));
        }
    }

    // Mesh info section
    auto* meshRoot = new QTreeWidgetItem(m_worktree);
    meshRoot->setText(0, "Mesh");
    meshRoot->setExpanded(true);
    auto* meshItem = new QTreeWidgetItem(meshRoot);
    meshItem->setText(0, "Status");
    meshItem->setText(1, "Not generated");
}

void GeometryPanel::updateMeshInfo(int numNodes, int numElements, int numBoundary) {
    m_meshInfoLabel->setText(QString("Mesh: %1 nodes, %2 elements, %3 boundary")
        .arg(numNodes).arg(numElements).arg(numBoundary));

    // Update mesh section in worktree
    auto* meshRoot = m_worktree->topLevelItem(2);  // Third top-level item is "Mesh"
    if (meshRoot) {
        meshRoot->takeChildren();  // Clear old items

        auto* nodeItem = new QTreeWidgetItem(meshRoot);
        nodeItem->setText(0, "Nodes");
        nodeItem->setText(1, QString::number(numNodes));

        auto* elemItem = new QTreeWidgetItem(meshRoot);
        elemItem->setText(0, "Elements");
        elemItem->setText(1, QString::number(numElements));

        auto* bndItem = new QTreeWidgetItem(meshRoot);
        bndItem->setText(0, "Boundary");
        bndItem->setText(1, QString::number(numBoundary));
    }
}

void GeometryPanel::selectPrimitiveInTree(int index) {
    if (index < 0) return;

    // Find the item under Objects root
    auto* objectsRoot = m_worktree->topLevelItem(0);
    if (!objectsRoot) return;

    if (index < objectsRoot->childCount()) {
        auto* item = objectsRoot->child(index);
        m_worktree->setCurrentItem(item);

        // Update properties panel
        const auto& prims = m_model->primitives();
        if (index < static_cast<int>(prims.size())) {
            std::visit([&](const auto& p) {
                using T = std::decay_t<decltype(p)>;
                if constexpr (std::is_same_v<T, RectPrimitive>) {
                    m_propTypeLabel->setText("Rectangle");
                    m_propLabel->setText(p.label);
                    m_propDetails->setText(QString("x=%1, y=%2, w=%3, h=%4")
                        .arg(p.x, 0, 'f', 4).arg(p.y, 0, 'f', 4)
                        .arg(p.width, 0, 'f', 4).arg(p.height, 0, 'f', 4));
                } else if constexpr (std::is_same_v<T, LinePrimitive>) {
                    m_propTypeLabel->setText("Line");
                    m_propLabel->setText(p.label);
                    m_propDetails->setText(QString("(%1, %2) -> (%3, %4)")
                        .arg(p.x1, 0, 'f', 4).arg(p.y1, 0, 'f', 4)
                        .arg(p.x2, 0, 'f', 4).arg(p.y2, 0, 'f', 4));
                } else if constexpr (std::is_same_v<T, CirclePrimitive>) {
                    m_propTypeLabel->setText("Circle");
                    m_propLabel->setText(p.label);
                    m_propDetails->setText(QString("center=(%1, %2), r=%3")
                        .arg(p.cx, 0, 'f', 4).arg(p.cy, 0, 'f', 4)
                        .arg(p.radius, 0, 'f', 4));
                }
            }, prims[index]);
        }
    }
}

// ------------------------------------------------------------------
// Worktree item click handler
// ------------------------------------------------------------------
void GeometryPanel::onWorktreeItemClicked(QTreeWidgetItem* item, int column) {
    Q_UNUSED(column)
    if (!item) return;

    // Check if this is a primitive item (has UserRole data)
    QVariant data = item->data(0, Qt::UserRole);
    if (data.isValid()) {
        int primIdx = data.toInt();
        m_state->selected_primitive_index = primIdx;
        m_state->selected_node_index = -1;
        m_state->selected_edge_index = -1;
        emit primitiveSelected(primIdx);

        // Update properties panel
        selectPrimitiveInTree(primIdx);
    }
}

// ------------------------------------------------------------------
// Delete primitive
// ------------------------------------------------------------------
void GeometryPanel::onDeletePrimitive() {
    int idx = m_state->selected_primitive_index;
    if (idx < 0 || idx >= m_model->primitiveCount()) return;

    if (m_undoStack) {
        m_undoStack->push(new RemovePrimitiveCommand(
            m_model, idx, "Delete Primitive"));
    } else {
        m_model->removePrimitive(idx);
    }

    m_state->selected_primitive_index = -1;
    m_propTypeLabel->setText("None");
    m_propLabel->setText("-");
    m_propDetails->setText("-");
    updateWorktree();
    emit deleteRequested();
}

// ------------------------------------------------------------------
// Generate mesh
// ------------------------------------------------------------------
void GeometryPanel::onGenerateMesh() {
    emit meshRequested(m_nxSpin->value(), m_nySpin->value());
}

// ------------------------------------------------------------------
// Material property getters
// ------------------------------------------------------------------
double GeometryPanel::youngsModulus() const {
    return m_ESpin->value();
}

double GeometryPanel::poissonsRatio() const {
    return m_nuSpin->value();
}

double GeometryPanel::thickness() const {
    return m_tSpin->value();
}

PlaneType GeometryPanel::planeType() const {
    return (m_planeCombo->currentIndex() == 0) ? PlaneType::STRESS : PlaneType::STRAIN;
}

void GeometryPanel::onMaterialPresetChanged(int index) {
    const auto& library = getMaterialLibrary();
    if (index < 0 || index >= static_cast<int>(library.size())) return;

    const auto& preset = library[index];
    bool isCustom = (preset.name == "Custom");

    // Temporarily block signals to avoid recursive emission
    QSignalBlocker blockE(m_ESpin);
    QSignalBlocker blockNu(m_nuSpin);
    QSignalBlocker blockT(m_tSpin);

    if (!isCustom) {
        m_ESpin->setValue(preset.E);
        m_nuSpin->setValue(preset.nu);
        m_tSpin->setValue(preset.t);
    }

    // Enable/disable fields: preset locks them, custom allows editing
    m_ESpin->setEnabled(isCustom);
    m_nuSpin->setEnabled(isCustom);
    m_tSpin->setEnabled(isCustom);
}
