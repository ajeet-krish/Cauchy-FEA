#pragma once
#include <QMainWindow>
#include <QDockWidget>
#include <QProgressBar>
#include <QUndoStack>
#include "viewport_widget.hpp"
#include "viewport_3d.hpp"
#include "mesh_editor.hpp"
#include "solver_panel.hpp"
#include "result_model.hpp"
#include "solver_runner.hpp"
#include "convergence_chart.hpp"
#include "stress_histogram.hpp"
#include "energy_balance_chart.hpp"
#include "displacement_line_chart.hpp"
#include "load_displacement_chart.hpp"
#include "error_heatmap.hpp"
#include "editor_state.hpp"
#include "geometry_model.hpp"
#include "bc_model.hpp"
#include "selection_model.hpp"
#include "tool_context.hpp"
#include "geometry_panel.hpp"
#include "bc_panel.hpp"
#include "mesh_generator.hpp"

class QStackedWidget;
class QActionGroup;

class MainWindow : public QMainWindow {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(MainWindow)
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void onSolveFinished(const fea::SolveResult& result, const Mesh& mesh);
    void onSolverError(const QString& errorMessage);
    void onProgress(int percent, const QString& message);
    void onRunSolver();
    void onLoadCase();
    void onExportPNG();
    void onResetView();
    void onContourFieldChanged(int index);
    void onColormapChanged(int index);
    void onDisplacementScaleChanged(int value);
    void onToggleUndeformed(bool checked);
    void onToggleDeformed(bool checked);
    void onToggleEdges(bool checked);
    void onToggleArrows(bool checked);
    void onToggleBoundary(bool checked);
    void onPlayAnimation();
    void onResetAnimation();
    void onUndo();
    void onRedo();
    void onPrimitiveSelected(int index);

private:
    void createActions();
    void createMenuBar();
    void createToolbars();
    void createEditorToolbar();
    void createLeftDock();
    void createStatusBar();
    void createBottomPlots();
    void connectEditorSignals();
    void switchViewport(bool is3d);
    void updateToolbarState();

    QStackedWidget* m_viewStack = nullptr;
    ViewportWidget* m_viewport = nullptr;
    Viewport3DWidget* m_viewport3d = nullptr;
    MeshEditor* m_meshEditor = nullptr;
    SolverPanel* m_solverPanel = nullptr;
    SolverRunner* m_solverRunner = nullptr;
    ResultModel* m_resultModel = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_scaleLabel = nullptr;

    // Undo/redo stack
    QUndoStack* m_undoStack = nullptr;

    // Editor components
    EditorState* m_editorState = nullptr;
    GeometryModel* m_geometryModel = nullptr;
    BCModel* m_bcModel = nullptr;
    SelectionModel* m_selectionModel = nullptr;
    ToolContext* m_toolContext = nullptr;
    MeshGenerator* m_meshGenerator = nullptr;
    std::unique_ptr<Mesh> m_currentMesh;

    // Editor panels
    GeometryPanel* m_geometryPanel = nullptr;
    BCPanel* m_bcPanel = nullptr;

    // Editor toolbar actions
    QToolBar* m_editorToolbar = nullptr;
    QAction* m_undoAction = nullptr;
    QAction* m_redoAction = nullptr;
    QAction* m_selectAction = nullptr;
    QAction* m_drawRectAction = nullptr;
    QAction* m_drawLineAction = nullptr;
    QAction* m_drawCircleAction = nullptr;
    QAction* m_fixedAction = nullptr;
    QAction* m_rollerXAction = nullptr;
    QAction* m_rollerYAction = nullptr;
    QAction* m_forceAction = nullptr;

    // Left dock with tabs
    QDockWidget* m_leftDock = nullptr;
    QTabWidget* m_leftTabs = nullptr;

    // Plot widgets
    QTabWidget* m_plotTabs = nullptr;
    StressHistogram* m_stressHist = nullptr;
    EnergyBalanceChart* m_energyChart = nullptr;
    DisplacementLineChart* m_dispLineChart = nullptr;
    LoadDisplacementChart* m_ldChart = nullptr;
    ErrorHeatmap* m_errorHeatmap = nullptr;
    ConvergenceChart* m_convChart = nullptr;

    SolveConfig m_config;
    Mesh m_lastMesh;
    fea::SolveResult m_lastResult;
};
