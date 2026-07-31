#pragma once
#include <QMainWindow>
#include <QDockWidget>
#include <QProgressBar>
#include "viewport_widget.hpp"
#include "mesh_editor.hpp"
#include "solver_panel.hpp"
#include "result_model.hpp"
#include "solver_runner.hpp"

class MainWindow : public QMainWindow {
    Q_OBJECT
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

    ViewportWidget* m_viewport = nullptr;
    MeshEditor* m_meshEditor = nullptr;
    SolverPanel* m_solverPanel = nullptr;
    SolverRunner* m_solverRunner = nullptr;
    ResultModel* m_resultModel = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_scaleLabel = nullptr;

    SolveConfig m_config;
};
