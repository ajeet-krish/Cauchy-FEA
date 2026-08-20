#pragma once
#include "editor_state.hpp"
#include "geometry_model.hpp"
#include "bc_model.hpp"
#include "../fea_types.hpp"
#include <QWidget>

class QDoubleSpinBox;
class QSpinBox;
class QComboBox;
class QUndoStack;
class QTreeWidget;
class QTreeWidgetItem;
class QLabel;
class QPushButton;

class GeometryPanel : public QWidget {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(GeometryPanel)
public:
    explicit GeometryPanel(EditorState* state, GeometryModel* model,
                           BCModel* bc_model, QWidget* parent = nullptr);

    // Set the undo stack for command-based operations
    void setUndoStack(QUndoStack* stack);

    // Update the CAD worktree from all models
    void updateWorktree();

    // Update mesh info display
    void updateMeshInfo(int numNodes, int numElements, int numBoundary);

    // Material property getters
    double youngsModulus() const;
    double poissonsRatio() const;
    double thickness() const;
    PlaneType planeType() const;

    // Select an item in the worktree by primitive index
    void selectPrimitiveInTree(int index);

signals:
    void meshRequested(int nx, int ny);
    void deleteRequested();
    void primitiveSelected(int index);

private slots:
    void onDeletePrimitive();
    void onGenerateMesh();
    void onWorktreeItemClicked(QTreeWidgetItem* item, int column);
    void onMaterialPresetChanged(int index);

private:
    void setupUI();

    EditorState* m_state;
    GeometryModel* m_model;
    BCModel* m_bc_model;
    QUndoStack* m_undoStack = nullptr;

    // Material controls
    QComboBox* m_materialCombo;
    QDoubleSpinBox* m_ESpin;
    QDoubleSpinBox* m_nuSpin;
    QDoubleSpinBox* m_tSpin;
    QComboBox* m_planeCombo;

    // Mesh controls
    QSpinBox* m_nxSpin;
    QSpinBox* m_nySpin;
    QPushButton* m_generateBtn;

    // CAD worktree
    QTreeWidget* m_worktree;

    // Properties panel
    QLabel* m_propTypeLabel;
    QLabel* m_propLabel;
    QLabel* m_propDetails;
    QPushButton* m_deleteBtn;

    // Mesh info
    QLabel* m_meshInfoLabel;
};
