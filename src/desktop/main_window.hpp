#pragma once
#include <QMainWindow>
#include <QDockWidget>
#include <QProgressBar>
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

class QStackedWidget;

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

private:
    void createActions();
    void createMenuBar();
    void createToolbars();
    void createStatusBar();
    void createBottomPlots();
    void switchViewport(bool is3d);

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
