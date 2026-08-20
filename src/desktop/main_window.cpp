#include "main_window.hpp"
#include "analytical.hpp"
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QToolBar>
#include <QStatusBar>
#include <QLabel>
#include <QProgressBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QTabWidget>
#include <QTableView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QTextEdit>
#include <QSplitter>
#include <QStackedWidget>
#include <QApplication>
#include <QClipboard>
#include <QImageWriter>
#include <QShortcut>
#include "project_io.hpp"
#include "parametric_panel.hpp"
#include "parametric_engine.hpp"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle("Crucible-FEA -- 2D/3D Finite Element Structural Solver");
    resize(1400, 900);

    // Undo/redo stack (created first so all editors can reference it)
    m_undoStack = new QUndoStack(this);

    // Editor components (created first so viewport can reference them)
    m_editorState = new EditorState();
    m_geometryModel = new GeometryModel();
    m_bcModel = new BCModel();
    m_selectionModel = new SelectionModel();
    m_toolContext = new ToolContext(m_editorState, m_geometryModel,
                                    m_bcModel, m_selectionModel, this);
    m_toolContext->setUndoStack(m_undoStack);
    m_meshGenerator = new MeshGenerator();

    // Probe tool
    m_probeTool = new ProbeTool(this);

    // Stacked widget for 2D/3D viewport switching
    m_viewStack = new QStackedWidget(this);
    m_viewport = new ViewportWidget(this);
    m_viewport3d = new Viewport3DWidget(this);
    m_viewStack->addWidget(m_viewport);    // index 0 = 2D
    m_viewStack->addWidget(m_viewport3d);  // index 1 = 3D
    m_viewStack->setCurrentIndex(0);
    setCentralWidget(m_viewStack);

    // Wire viewport to editor components
    m_viewport->setEditorState(m_editorState);
    m_viewport->setGeometryModel(m_geometryModel);
    m_viewport->setBCModel(m_bcModel);
    m_viewport->setSelectionModel(m_selectionModel);
    m_viewport->setToolContext(m_toolContext);

    // Editor panels
    m_geometryPanel = new GeometryPanel(m_editorState, m_geometryModel, m_bcModel, this);
    m_geometryPanel->setUndoStack(m_undoStack);
    m_bcPanel = new BCPanel(m_editorState, m_bcModel, m_selectionModel, this);
    m_bcPanel->setUndoStack(m_undoStack);

    // Solver runner
    m_solverRunner = new SolverRunner(this);
    connect(m_solverRunner, &SolverRunner::finished,
            this, &MainWindow::onSolveFinished);
    connect(m_solverRunner, &SolverRunner::error,
            this, &MainWindow::onSolverError);
    connect(m_solverRunner, &SolverRunner::progress,
            this, &MainWindow::onProgress);

    // Modal analysis runner
    m_modalRunner = new ModalRunner(this);
    connect(m_modalRunner, &ModalRunner::modalFinished,
            this, &MainWindow::onModalFinished);
    connect(m_modalRunner, &ModalRunner::modalError,
            this, &MainWindow::onModalError);

    // Result model
    m_resultModel = new ResultModel(this);

    // Create dock widgets
    m_meshEditor = new MeshEditor(this);
    m_solverPanel = new SolverPanel(this);

    // Left dock with editor tabs (replaces the old mesh dock)
    createLeftDock();

    QDockWidget* solverDock = new QDockWidget("Solver & Results", this);
    solverDock->setWidget(m_solverPanel);
    solverDock->setAllowedAreas(Qt::RightDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, solverDock);

    // Element inspector panel (right dock, below solver)
    m_inspectorPanel = new ElementInspectorPanel(this);
    QDockWidget* inspectorDock = new QDockWidget("Element Inspector", this);
    inspectorDock->setWidget(m_inspectorPanel);
    inspectorDock->setAllowedAreas(Qt::RightDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, inspectorDock);
    tabifyDockWidget(solverDock, inspectorDock);
    inspectorDock->setVisible(false);

    // Mode shape panel (right dock, below inspector)
    m_modeShapePanel = new ModeShapePanel(this);
    QDockWidget* modeShapeDock = new QDockWidget("Mode Shapes", this);
    modeShapeDock->setWidget(m_modeShapePanel);
    modeShapeDock->setAllowedAreas(Qt::RightDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, modeShapeDock);
    tabifyDockWidget(solverDock, modeShapeDock);
    modeShapeDock->setVisible(false);

    // Connect modal analysis signals
    connect(m_modeShapePanel, &ModeShapePanel::modeChanged,
            this, &MainWindow::onModeChanged);
    connect(m_modeShapePanel, &ModeShapePanel::animationToggled,
            this, &MainWindow::onModeAnimationToggled);
    connect(m_modeShapePanel, &ModeShapePanel::amplitudeChanged,
            this, &MainWindow::onModeAmplitudeChanged);
    connect(m_modeShapePanel, &ModeShapePanel::resetRequested,
            this, &MainWindow::onModeReset);

    // Parametric study panel and engine
    m_parametricEngine = new ParametricEngine();
    m_parametricPanel = new ParametricPanel(this);
    QDockWidget* parametricDock = new QDockWidget("Parametric Study", this);
    parametricDock->setWidget(m_parametricPanel);
    parametricDock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, parametricDock);
    tabifyDockWidget(solverDock, parametricDock);
    parametricDock->setVisible(false);

    connect(m_parametricPanel, &ParametricPanel::paramChanged,
            this, &MainWindow::onParamChanged);

    // Create menus, toolbars, status bar, plots
    createActions();
    createMenuBar();
    createToolbars();
    createEditorToolbar();
    createStatusBar();
    createBottomPlots();

    // Connect signals
    connect(m_solverPanel, &SolverPanel::runClicked,
            this, &MainWindow::onRunSolver);
    connect(m_solverPanel, &SolverPanel::resetClicked,
            this, &MainWindow::onResetView);
    connect(m_solverPanel, &SolverPanel::modalClicked,
            this, &MainWindow::onRunModalAnalysis);
    connectEditorSignals();
}

void MainWindow::createActions() {
    // File actions are now created directly in createMenuBar()

    // Undo/Redo actions
    m_undoAction = m_undoStack->createUndoAction(this, "Undo");
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_undoAction->setStatusTip("Undo last action (Ctrl+Z)");

    m_redoAction = m_undoStack->createRedoAction(this, "Redo");
    m_redoAction->setShortcut(QKeySequence::Redo);
    m_redoAction->setStatusTip("Redo last undone action (Ctrl+Shift+Z)");

    // View actions
    QAction* resetViewAct = new QAction("Reset View", this);
    resetViewAct->setStatusTip("Reset camera to default view");
    connect(resetViewAct, &QAction::triggered, this, &MainWindow::onResetView);

    // Help
    QAction* aboutAct = new QAction("About", this);
    aboutAct->setStatusTip("About Crucible-FEA");
    connect(aboutAct, &QAction::triggered, this, []() {
        QMessageBox::about(nullptr, "About Crucible-FEA",
            "<h2>Crucible-FEA</h2>"
            "<p>Version 1.0.0</p>"
            "<p>A 2D finite element structural solver for plane stress and plane strain problems.</p>"
            "<p>Supports Bar, Q4 (bilinear quad), Q8 (serendipity quad), and T3 (triangle) elements.</p>"
            "<p>Features: Cholesky direct solver, Conjugate Gradient iterative solver, "
            "Von Mises stress recovery, ZZ error estimator, adaptive h-refinement.</p>"
            "<p>MIT License, portfolio project for mechanical/aerospace engineering roles.</p>");
    });

    Q_UNUSED(resetViewAct) Q_UNUSED(aboutAct)
}

void MainWindow::createMenuBar() {
    QMenuBar* menuBar = this->menuBar();

    QMenu* fileMenu = menuBar->addMenu("&File");

    QAction* newAct = fileMenu->addAction("New");
    newAct->setShortcut(QKeySequence::New);
    newAct->setStatusTip("Create a new mesh");

    QAction* openAct = fileMenu->addAction("Open...");
    openAct->setShortcut(QKeySequence::Open);
    connect(openAct, &QAction::triggered, this, &MainWindow::onLoadCase);

    QAction* saveAct = fileMenu->addAction("Save...");
    saveAct->setShortcut(QKeySequence::Save);
    connect(saveAct, &QAction::triggered, this, &MainWindow::onQuickSave);

    QAction* saveAsAct = fileMenu->addAction("Save As...");
    saveAsAct->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAct, &QAction::triggered, this, &MainWindow::onSaveCase);

    fileMenu->addSeparator();

    QAction* exportPngAct = fileMenu->addAction("Export PNG");
    exportPngAct->setStatusTip("Export current view as PNG");
    connect(exportPngAct, &QAction::triggered, this, &MainWindow::onExportPNG);

    fileMenu->addSeparator();

    QAction* exitAct = fileMenu->addAction("Exit");
    exitAct->setShortcut(QKeySequence::Quit);
    connect(exitAct, &QAction::triggered, this, &QMainWindow::close);

    QMenu* editMenu = menuBar->addMenu("&Edit");
    editMenu->addAction(m_undoAction);
    editMenu->addAction(m_redoAction);

    QMenu* viewMenu = menuBar->addMenu("&View");
    viewMenu->addAction("Reset View");
    viewMenu->addSeparator();
    viewMenu->addAction(m_playAction);
    viewMenu->addAction(m_resetAnimAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_streamlineAction);

    QMenu* helpMenu = menuBar->addMenu("&Help");
    helpMenu->addAction("About");
}

void MainWindow::createToolbars() {
    QToolBar* viewToolbar = addToolBar("View");
    viewToolbar->addAction("Reset View");
    viewToolbar->addSeparator();

    QComboBox* contourCombo = new QComboBox(this);
    contourCombo->addItems({
        "Von Mises", "Sigma XX", "Sigma YY", "Sigma XY",
        "Sigma 1 (Principal)", "Sigma 2 (Principal)", "|u| (Displacement)"
    });
    connect(contourCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onContourFieldChanged);
    viewToolbar->addWidget(new QLabel("Contour:"));
    viewToolbar->addWidget(contourCombo);

    viewToolbar->addSeparator();

    QComboBox* colormapCombo = new QComboBox(this);
    colormapCombo->addItems({"Turbo", "Viridis", "Hot", "Coolwarm", "RdBu_r"});
    connect(colormapCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onColormapChanged);
    viewToolbar->addWidget(new QLabel("Colormap:"));
    viewToolbar->addWidget(colormapCombo);

    viewToolbar->addSeparator();

    QSlider* scaleSlider = new QSlider(Qt::Horizontal, this);
    scaleSlider->setRange(1, 10000);
    scaleSlider->setValue(100);
    scaleSlider->setFixedWidth(120);
    connect(scaleSlider, &QSlider::valueChanged,
            this, &MainWindow::onDisplacementScaleChanged);
    viewToolbar->addWidget(new QLabel("Scale:"));
    viewToolbar->addWidget(scaleSlider);
    m_scaleLabel = new QLabel("100x", this);
    viewToolbar->addWidget(m_scaleLabel);

    viewToolbar->addSeparator();

    // Animation controls
    m_playAction = viewToolbar->addAction("Play");
    m_playAction->setToolTip("Play/pause deformation animation (10s)");
    connect(m_playAction, &QAction::triggered, this, &MainWindow::onPlayAnimation);

    m_resetAnimAction = viewToolbar->addAction("Reset Anim");
    m_resetAnimAction->setToolTip("Reset animation to undeformed state");
    connect(m_resetAnimAction, &QAction::triggered, this, &MainWindow::onResetAnimation);

    viewToolbar->addSeparator();

    m_streamlineAction = viewToolbar->addAction("Streamlines");
    m_streamlineAction->setCheckable(true);
    m_streamlineAction->setToolTip("Toggle principal stress streamlines");
    connect(m_streamlineAction, &QAction::triggered, this, &MainWindow::onToggleStreamlines);
}

void MainWindow::createStatusBar() {
    m_statusLabel = new QLabel("Ready", this);
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedWidth(200);
    m_progressBar->setVisible(false);

    statusBar()->addWidget(m_statusLabel);
    statusBar()->addPermanentWidget(m_progressBar);
}

void MainWindow::onSolveFinished(const fea::SolveResult& result, const Mesh& mesh) {
    m_progressBar->setVisible(false);
    m_statusLabel->setText("Solver finished successfully.");

    // Switch viewport based on mesh dimension
    bool is3d = mesh.is_3d();
    switchViewport(is3d);

    if (is3d) {
        m_viewport3d->setMeshAndResults(mesh, result);
    } else {
        m_viewport->setMeshAndResults(mesh, result);
    }

    // Wire mesh nodes to tool context for node-based operations
    m_toolContext->setMeshNodes(&mesh.nodes);

    // Update geometry panel worktree with mesh info
    m_geometryPanel->updateMeshInfo(mesh.num_nodes(), mesh.num_quads() + mesh.num_tris(),
                                    static_cast<int>(mesh.dirichlet.size() + mesh.neumann.size()));
    m_geometryPanel->updateWorktree();

    m_resultModel->setData(result, mesh, ResultTableType::DISPLACEMENT);
    m_solverPanel->setResult(result);

    // Store for plot access
    m_lastMesh = mesh;
    m_lastResult = result;

    // Capture baseline for parametric study
    double baselineForce = 0.0;
    for (const auto& bc : mesh.neumann) {
        baselineForce += std::abs(bc.value);
    }
    if (baselineForce < 1e-20) baselineForce = 1.0;
    m_baselineForceScale = 1.0;
    m_parametricEngine->captureBaseline(mesh, result, m_config.E, m_config.nu, m_config.t, 1.0);
    m_parametricPanel->setInitialValues(m_config.E, m_config.nu, baselineForce, m_config.t);

    // Update stress histogram
    m_stressHist->setData(result.stresses);

    // Update energy balance
    EnergyBalanceData ed;
    ed.strain_energy = fea::compute_strain_energy(result.K_csr, result.displacement);
    ed.work_done = fea::compute_work_done(result.f, result.displacement);
    ed.valid = true;
    m_energyChart->setData(ed);

    // Update displacement line chart (2D only)
    if (!is3d) {
        m_dispLineChart->setData(mesh, result);
    }

    // Update load-displacement chart
    int dofPerNode = is3d ? 3 : 2;
    double maxDisp = 0.0;
    for (int i = 0; i < mesh.num_nodes(); ++i) {
        double ux = result.displacement[dofPerNode * i];
        double uy = result.displacement[dofPerNode * i + 1];
        double d = std::sqrt(ux * ux + uy * uy);
        if (d > maxDisp) maxDisp = d;
    }
    double totalForce = 0.0;
    for (const auto& bc : mesh.neumann) {
        totalForce += std::abs(bc.value);
    }
    m_ldChart->addPoint(totalForce, maxDisp);

    // Update analytical comparison panel
    if (!is3d) {
        auto analytical_rows = compute_analytical(m_config.case_type, mesh, result);
        m_analyticalPanel->setData(analytical_rows);
    } else {
        m_analyticalPanel->clear();
    }
}

void MainWindow::onSolverError(const QString& errorMessage) {
    m_progressBar->setVisible(false);
    m_statusLabel->setText("Solver error: " + errorMessage);
    QMessageBox::critical(this, "Solver Error", errorMessage);
}

void MainWindow::onProgress(int percent, const QString& message) {
    m_progressBar->setVisible(true);
    m_progressBar->setValue(percent);
    m_statusLabel->setText(message);
}

void MainWindow::onRunSolver() {
    // Check if we have a custom mesh from geometry editor
    if (m_currentMesh && m_currentMesh->num_nodes() > 0) {
        // Use custom mesh from geometry editor
        m_config.use_cg = m_solverPanel->useCG();
        m_config.use_adaptivity = m_solverPanel->useAdaptivity();
        m_config.adaptive_iters = m_solverPanel->adaptiveIterations();
        
        // Copy BCs from BCModel to mesh
        if (m_bcModel) {
            m_currentMesh->dirichlet.clear();
            m_currentMesh->neumann.clear();
            
            const auto& bcs = m_bcModel->bcs();
            for (const auto& bc : bcs) {
                if (bc.node_index < 0 || bc.node_index >= m_currentMesh->num_nodes()) continue;
                
                switch (bc.type) {
                case BCType::FIXED:
                    m_currentMesh->dirichlet.push_back({bc.node_index, 0, 0.0});
                    m_currentMesh->dirichlet.push_back({bc.node_index, 1, 0.0});
                    break;
                case BCType::ROLLER_X:
                    m_currentMesh->dirichlet.push_back({bc.node_index, 0, 0.0});
                    break;
                case BCType::ROLLER_Y:
                    m_currentMesh->dirichlet.push_back({bc.node_index, 1, 0.0});
                    break;
                case BCType::FORCE:
                    // Convert angle/magnitude to x/y components
                    {
                        double angle_rad = bc.angle * M_PI / 180.0;
                        double magnitude = bc.value;
                        double fx = magnitude * std::cos(angle_rad);
                        double fy = magnitude * std::sin(angle_rad);
                        if (std::abs(fx) > 1e-10) {
                            m_currentMesh->neumann.push_back({bc.node_index, 0, fx});
                        }
                        if (std::abs(fy) > 1e-10) {
                            m_currentMesh->neumann.push_back({bc.node_index, 1, fy});
                        }
                    }
                    break;
                }
            }
        }
        
        m_progressBar->setVisible(true);
        m_progressBar->setValue(0);
        m_statusLabel->setText("Running solver with custom mesh...");
        
        m_solverRunner->setConfig(m_config);
        m_solverRunner->setMesh(*m_currentMesh, m_config.use_cg);
        m_solverRunner->start();
    } else {
        // Use predefined case from mesh editor
        m_config.case_type = m_meshEditor->caseType();
        m_config.element_type = m_meshEditor->elementType();
        m_config.plane_type = m_meshEditor->planeType();
        m_config.nx = m_meshEditor->meshSizeX();
        m_config.ny = m_meshEditor->meshSizeY();
        m_config.nz = m_meshEditor->meshSizeZ();
        m_config.use_q8 = m_meshEditor->useQ8();
        m_config.is_3d = m_meshEditor->is3D();
        m_config.use_cg = m_solverPanel->useCG();
        m_config.E = m_meshEditor->youngsModulus();
        m_config.nu = m_meshEditor->poissonsRatio();
        m_config.t = m_meshEditor->thickness();
        m_config.use_adaptivity = m_solverPanel->useAdaptivity();
        m_config.adaptive_iters = m_solverPanel->adaptiveIterations();

        m_progressBar->setVisible(true);
        m_progressBar->setValue(0);
        m_statusLabel->setText("Running solver...");

        m_solverRunner->setConfig(m_config);
        m_solverRunner->start();
    }
}

void MainWindow::onLoadCase() {
    QString fileName = QFileDialog::getOpenFileName(this,
        "Open Project", "", "Crucible-FEA Project (*.cauchy)");
    if (fileName.isEmpty()) return;

    ProjectConfig config;
    if (!ProjectIO::load(fileName, config)) {
        QMessageBox::warning(this, "Load Error",
            "Could not load project file: " + QFileInfo(fileName).fileName()
            + "\n\nThe file may be corrupted or in an unsupported format.");
        return;
    }

    // Restore solver config
    m_config = config.solverConfig;

    // Restore mesh editor state from config
    // (The mesh editor combo boxes need to be set to match the loaded config)

    // Restore viewport state
    m_viewport->setContourField(static_cast<ContourField>(config.viewport.contourField));
    m_viewport->setColormap(static_cast<ColormapType>(config.viewport.colormap));
    m_viewport->setDisplacementScale(config.viewport.dispScale);
    m_viewport->toggleUndeformed(config.viewport.showUndeformed);
    m_viewport->toggleDeformed(config.viewport.showDeformed);
    m_viewport->toggleEdges(config.viewport.showEdges);
    m_viewport->toggleArrows(config.viewport.showArrows);
    m_viewport->toggleBoundary(config.viewport.showBoundary);
    m_viewport->toggleStreamlines(config.viewport.showStreamlines);
    if (m_streamlineAction) m_streamlineAction->setChecked(config.viewport.showStreamlines);

    // Restore geometry model primitives
    if (m_geometryModel) {
        m_geometryModel->clear();
        for (const auto& prim : config.geometryPrimitives) {
            m_geometryModel->addPrimitive(prim);
        }
    }

    // Restore BC model
    if (m_bcModel) {
        m_bcModel->clear();
        for (const auto& bc : config.boundaryConditions) {
            m_bcModel->addBC(bc);
        }
    }

    // Store current file path for quick-save
    m_currentFilePath = fileName;

    // If we have a mesh and results, restore them
    if (config.hasResults && config.mesh.num_nodes() > 0) {
        m_lastMesh = config.mesh;
        m_lastResult = config.result;

        // Set mesh on viewport
        bool is3d = config.mesh.is_3d();
        switchViewport(is3d);

        if (is3d) {
            m_viewport3d->setMeshAndResults(config.mesh, config.result);
        } else {
            m_viewport->setMeshAndResults(config.mesh, config.result);
        }

        // Update plots
        m_stressHist->setData(config.result.stresses);

        EnergyBalanceData ed;
        ed.strain_energy = fea::compute_strain_energy(config.result.K_csr, config.result.displacement);
        ed.work_done = fea::compute_work_done(config.result.f, config.result.displacement);
        ed.valid = true;
        m_energyChart->setData(ed);

        if (!is3d) {
            m_dispLineChart->setData(config.mesh, config.result);
        }

        m_resultModel->setData(config.result, config.mesh, ResultTableType::DISPLACEMENT);
        m_solverPanel->setResult(config.result);

        // Restore analytical comparison panel
        if (!is3d) {
            auto analytical_rows = compute_analytical(
                config.solverConfig.case_type, config.mesh, config.result);
            m_analyticalPanel->setData(analytical_rows);
        } else {
            m_analyticalPanel->clear();
        }

        m_statusLabel->setText(QString("Loaded: %1 (%2 nodes, %3 elements, %4 DOFs)")
            .arg(QFileInfo(fileName).fileName())
            .arg(config.mesh.num_nodes())
            .arg(config.mesh.num_quads() + config.mesh.num_tris())
            .arg(config.mesh.num_dofs()));
    } else {
        // Mesh-only load (no results yet)
        if (config.mesh.num_nodes() > 0) {
            m_viewport->setMesh(config.mesh);
            m_toolContext->setMeshNodes(&config.mesh.nodes);
            m_geometryPanel->updateMeshInfo(config.mesh.num_nodes(),
                                           config.mesh.num_quads() + config.mesh.num_tris(),
                                           static_cast<int>(config.mesh.dirichlet.size() + config.mesh.neumann.size()));
        }
        m_statusLabel->setText("Loaded: " + QFileInfo(fileName).fileName());
    }

    // Clear analytical panel if no results
    if (!config.hasResults) {
        m_analyticalPanel->clear();
    }

    // Update panels
    m_geometryPanel->updateWorktree();
    m_bcPanel->updateBCList();

    // Update window title
    setWindowTitle("Crucible-FEA | " + QFileInfo(fileName).fileName());
}

void MainWindow::onSaveCase() {
    QString fileName = QFileDialog::getSaveFileName(this,
        "Save Project", m_currentFilePath.isEmpty()
            ? ProjectIO::defaultProjectPath()
            : m_currentFilePath,
        "Crucible-FEA Project (*.cauchy)");
    if (fileName.isEmpty()) return;

    // Ensure .cauchy extension
    if (!fileName.endsWith(".cauchy")) {
        fileName += ".cauchy";
    }

    // Gather full project state
    ProjectConfig config;
    config.version = 2;

    // Solver config
    config.solverConfig = m_config;

    // Material (from mesh editor or geometry panel)
    config.material.E = m_config.E;
    config.material.nu = m_config.nu;
    config.material.t = m_config.t;
    config.material.rho = 7800.0; // default steel
    config.material.alpha = 0.0;

    // Mesh (from last solve or current mesh)
    if (m_lastMesh.num_nodes() > 0) {
        config.mesh = m_lastMesh;
    } else if (m_currentMesh && m_currentMesh->num_nodes() > 0) {
        config.mesh = *m_currentMesh;
    }

    // Editor boundary conditions
    if (m_bcModel) {
        config.boundaryConditions = m_bcModel->bcs();
    }

    // Geometry primitives
    if (m_geometryModel) {
        config.geometryPrimitives = m_geometryModel->primitives();
    }

    // Viewport state
    config.viewport.contourField = static_cast<int>(m_viewport->contourField());
    config.viewport.colormap = static_cast<int>(m_viewport->colormap());
    config.viewport.dispScale = m_viewport->displacementScale();
    config.viewport.showUndeformed = m_viewport->showUndeformed();
    config.viewport.showDeformed = m_viewport->showDeformed();
    config.viewport.showEdges = m_viewport->showEdges();
    config.viewport.showArrows = m_viewport->showArrows();
    config.viewport.showBoundary = m_viewport->showBoundary();
    config.viewport.showStreamlines = m_viewport->showStreamlines();
    config.viewport.panX = m_viewport->panX();
    config.viewport.panY = m_viewport->panY();
    config.viewport.zoom = m_viewport->zoomLevel();

    // Results
    config.hasResults = !m_lastResult.displacement.empty();
    if (config.hasResults) {
        config.result = m_lastResult;
    }

    // Write to file
    if (!ProjectIO::save(fileName, config)) {
        QMessageBox::warning(this, "Save Error",
            "Could not save project to: " + fileName);
        return;
    }

    // Update state for future quick-saves
    m_currentFilePath = fileName;
    setWindowTitle("Crucible-FEA | " + QFileInfo(fileName).fileName());
    m_statusLabel->setText("Saved: " + QFileInfo(fileName).fileName());
}

void MainWindow::onQuickSave() {
    if (m_currentFilePath.isEmpty()) {
        // No current file; fall through to Save As dialog
        onSaveCase();
        return;
    }

    // Gather full project state
    ProjectConfig config;
    config.version = 2;

    // Solver config
    config.solverConfig = m_config;

    // Material
    config.material.E = m_config.E;
    config.material.nu = m_config.nu;
    config.material.t = m_config.t;
    config.material.rho = 7800.0;
    config.material.alpha = 0.0;

    // Mesh
    if (m_lastMesh.num_nodes() > 0) {
        config.mesh = m_lastMesh;
    } else if (m_currentMesh && m_currentMesh->num_nodes() > 0) {
        config.mesh = *m_currentMesh;
    }

    // Editor boundary conditions
    if (m_bcModel) {
        config.boundaryConditions = m_bcModel->bcs();
    }

    // Geometry primitives
    if (m_geometryModel) {
        config.geometryPrimitives = m_geometryModel->primitives();
    }

    // Viewport state
    config.viewport.contourField = static_cast<int>(m_viewport->contourField());
    config.viewport.colormap = static_cast<int>(m_viewport->colormap());
    config.viewport.dispScale = m_viewport->displacementScale();
    config.viewport.showUndeformed = m_viewport->showUndeformed();
    config.viewport.showDeformed = m_viewport->showDeformed();
    config.viewport.showEdges = m_viewport->showEdges();
    config.viewport.showArrows = m_viewport->showArrows();
    config.viewport.showBoundary = m_viewport->showBoundary();
    config.viewport.showStreamlines = m_viewport->showStreamlines();
    config.viewport.panX = m_viewport->panX();
    config.viewport.panY = m_viewport->panY();
    config.viewport.zoom = m_viewport->zoomLevel();

    // Results
    config.hasResults = !m_lastResult.displacement.empty();
    if (config.hasResults) {
        config.result = m_lastResult;
    }

    // Write to file
    if (!ProjectIO::save(m_currentFilePath, config)) {
        QMessageBox::warning(this, "Save Error",
            "Could not save project to: " + m_currentFilePath);
        return;
    }

    m_statusLabel->setText("Saved: " + QFileInfo(m_currentFilePath).fileName());
}

void MainWindow::onExportPNG() {
    QString fileName = QFileDialog::getSaveFileName(this,
        "Export PNG", "fea_view.png", "PNG Image (*.png)");
    if (fileName.isEmpty()) return;

    QImage image = m_viewport->grab().toImage();
    image.save(fileName, "PNG");
}

void MainWindow::onResetView() {
    if (m_viewStack->currentIndex() == 1) {
        m_viewport3d->resetView();
    } else {
        m_viewport->resetView();
    }
}

void MainWindow::switchViewport(bool is3d) {
    m_viewStack->setCurrentIndex(is3d ? 1 : 0);
}

void MainWindow::onContourFieldChanged(int index) {
    ContourField field = static_cast<ContourField>(index);
    m_viewport->setContourField(field);
}

void MainWindow::onColormapChanged(int index) {
    ColormapType cmap = static_cast<ColormapType>(index);
    m_viewport->setColormap(cmap);
}

void MainWindow::onDisplacementScaleChanged(int value) {
    m_viewport->setDisplacementScale(static_cast<double>(value));
    m_scaleLabel->setText(QString("%1x").arg(value));
}

void MainWindow::onToggleUndeformed(bool checked) {
    m_viewport->toggleUndeformed(checked);
}

void MainWindow::onToggleDeformed(bool checked) {
    m_viewport->toggleDeformed(checked);
}

void MainWindow::onToggleEdges(bool checked) {
    m_viewport->toggleEdges(checked);
}

void MainWindow::onToggleArrows(bool checked) {
    m_viewport->toggleArrows(checked);
}

void MainWindow::onToggleBoundary(bool checked) {
    m_viewport->toggleBoundary(checked);
}

void MainWindow::onPlayAnimation() {
    if (!m_lastResult.displacement.empty()) {
        m_viewport->startAnimation();
    }
}

void MainWindow::onResetAnimation() {
    m_viewport->resetAnimation();
}

void MainWindow::onToggleStreamlines(bool checked) {
    m_viewport->toggleStreamlines(checked);
}

void MainWindow::createBottomPlots() {
    m_plotTabs = new QTabWidget(this);
    m_plotTabs->setMinimumHeight(220);

    m_stressHist = new StressHistogram(this);
    m_energyChart = new EnergyBalanceChart(this);
    m_dispLineChart = new DisplacementLineChart(this);
    m_ldChart = new LoadDisplacementChart(this);
    m_errorHeatmap = new ErrorHeatmap(this);
    m_convChart = new ConvergenceChart(this);
    m_analyticalPanel = new AnalyticalPanel(this);

    m_plotTabs->addTab(m_stressHist, "Stress Distribution");
    m_plotTabs->addTab(m_energyChart, "Energy Balance");
    m_plotTabs->addTab(m_dispLineChart, "Displacement Profile");
    m_plotTabs->addTab(m_ldChart, "Load-Displacement");
    m_plotTabs->addTab(m_errorHeatmap, "Error Map");
    m_plotTabs->addTab(m_convChart, "Convergence");
    m_plotTabs->addTab(m_analyticalPanel, "Validation");

    QDockWidget* plotsDock = new QDockWidget("Analysis Plots", this);
    plotsDock->setWidget(m_plotTabs);
    plotsDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    addDockWidget(Qt::BottomDockWidgetArea, plotsDock);
}

void MainWindow::createEditorToolbar() {
    m_editorToolbar = addToolBar("Editor");
    m_editorToolbar->setObjectName("EditorToolbar");

    // Undo/Redo buttons
    m_editorToolbar->addAction(m_undoAction);
    m_editorToolbar->addAction(m_redoAction);
    m_editorToolbar->addSeparator();

    // Select tool
    m_selectAction = m_editorToolbar->addAction("Select");
    m_selectAction->setCheckable(true);
    m_selectAction->setShortcut(Qt::Key_V);
    m_selectAction->setToolTip("Select and move objects (V)");

    // Drawing tools
    m_drawRectAction = m_editorToolbar->addAction("Rectangle");
    m_drawRectAction->setCheckable(true);
    m_drawRectAction->setShortcut(Qt::Key_R);
    m_drawRectAction->setToolTip("Draw rectangle (R)");

    m_drawLineAction = m_editorToolbar->addAction("Line");
    m_drawLineAction->setCheckable(true);
    m_drawLineAction->setShortcut(Qt::Key_L);
    m_drawLineAction->setToolTip("Draw line (L)");

    m_drawCircleAction = m_editorToolbar->addAction("Circle");
    m_drawCircleAction->setCheckable(true);
    m_drawCircleAction->setShortcut(Qt::Key_C);
    m_drawCircleAction->setToolTip("Draw circle (C)");

    m_editorToolbar->addSeparator();

    // BC tools
    m_fixedAction = m_editorToolbar->addAction("Fixed");
    m_fixedAction->setCheckable(true);
    m_fixedAction->setShortcut(Qt::Key_F);
    m_fixedAction->setToolTip("Apply fixed BC (F)");

    m_rollerXAction = m_editorToolbar->addAction("Roller X");
    m_rollerXAction->setCheckable(true);
    m_rollerXAction->setShortcut(Qt::Key_X);
    m_rollerXAction->setToolTip("Apply roller X BC (X)");

    m_rollerYAction = m_editorToolbar->addAction("Roller Y");
    m_rollerYAction->setCheckable(true);
    m_rollerYAction->setShortcut(Qt::Key_Y);
    m_rollerYAction->setToolTip("Apply roller Y BC (Y)");

    m_forceAction = m_editorToolbar->addAction("Force");
    m_forceAction->setCheckable(true);
    m_forceAction->setShortcut(Qt::Key_P);
    m_forceAction->setToolTip("Apply force (P)");

    m_probeAction = m_editorToolbar->addAction("Probe");
    m_probeAction->setCheckable(true);
    m_probeAction->setShortcut(Qt::Key_I);
    m_probeAction->setToolTip("Probe node/element data (I)");

    // Make actions exclusive
    QActionGroup* toolGroup = new QActionGroup(this);
    toolGroup->addAction(m_selectAction);
    toolGroup->addAction(m_drawRectAction);
    toolGroup->addAction(m_drawLineAction);
    toolGroup->addAction(m_drawCircleAction);
    toolGroup->addAction(m_fixedAction);
    toolGroup->addAction(m_rollerXAction);
    toolGroup->addAction(m_rollerYAction);
    toolGroup->addAction(m_forceAction);
    toolGroup->addAction(m_probeAction);
    toolGroup->setExclusive(true);

    // Default to select mode
    m_selectAction->setChecked(true);
}

void MainWindow::createLeftDock() {
    m_leftDock = new QDockWidget("Editor", this);
    m_leftDock->setObjectName("LeftDock");
    m_leftDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    m_leftTabs = new QTabWidget(this);
    m_leftTabs->addTab(m_geometryPanel, "Geometry");
    m_leftTabs->addTab(m_bcPanel, "Boundary Conditions");

    m_leftDock->setWidget(m_leftTabs);
    addDockWidget(Qt::LeftDockWidgetArea, m_leftDock);
}

void MainWindow::connectEditorSignals() {
    // Toolbar actions -> EditorState + ToolContext
    connect(m_selectAction, &QAction::triggered, this, [this]() {
        m_editorState->current_mode = ToolMode::SELECT;
        m_toolContext->modeChanged(ToolMode::SELECT);
        m_statusLabel->setText("Tool: Select");
    });
    connect(m_drawRectAction, &QAction::triggered, this, [this]() {
        m_editorState->current_mode = ToolMode::DRAW_RECT;
        m_toolContext->modeChanged(ToolMode::DRAW_RECT);
        m_statusLabel->setText("Tool: Draw Rectangle");
    });
    connect(m_drawLineAction, &QAction::triggered, this, [this]() {
        m_editorState->current_mode = ToolMode::DRAW_LINE;
        m_toolContext->modeChanged(ToolMode::DRAW_LINE);
        m_statusLabel->setText("Tool: Draw Line");
    });
    connect(m_drawCircleAction, &QAction::triggered, this, [this]() {
        m_editorState->current_mode = ToolMode::DRAW_CIRCLE;
        m_toolContext->modeChanged(ToolMode::DRAW_CIRCLE);
        m_statusLabel->setText("Tool: Draw Circle");
    });
    connect(m_fixedAction, &QAction::triggered, this, [this]() {
        m_editorState->current_mode = ToolMode::ASSIGN_FIXED;
        m_toolContext->modeChanged(ToolMode::ASSIGN_FIXED);
        m_statusLabel->setText("Tool: Fixed BC");
    });
    connect(m_rollerXAction, &QAction::triggered, this, [this]() {
        m_editorState->current_mode = ToolMode::ASSIGN_ROLLER_X;
        m_toolContext->modeChanged(ToolMode::ASSIGN_ROLLER_X);
        m_statusLabel->setText("Tool: Roller X BC");
    });
    connect(m_rollerYAction, &QAction::triggered, this, [this]() {
        m_editorState->current_mode = ToolMode::ASSIGN_ROLLER_Y;
        m_toolContext->modeChanged(ToolMode::ASSIGN_ROLLER_Y);
        m_statusLabel->setText("Tool: Roller Y BC");
    });
    connect(m_forceAction, &QAction::triggered, this, [this]() {
        m_editorState->current_mode = ToolMode::APPLY_FORCE;
        m_toolContext->modeChanged(ToolMode::APPLY_FORCE);
        m_statusLabel->setText("Tool: Apply Force");
    });
    connect(m_probeAction, &QAction::triggered, this, [this]() {
        m_editorState->current_mode = ToolMode::PROBE;
        m_toolContext->modeChanged(ToolMode::PROBE);
        m_statusLabel->setText("Tool: Probe");
    });

    // Probe tool connections
    connect(m_toolContext, &ToolContext::probeRequested,
            this, &MainWindow::onProbeRequested);
    connect(m_probeTool, &ProbeTool::probed,
            this, &MainWindow::onProbed);

    // BCPanel -> EditorState
    connect(m_bcPanel, &BCPanel::toolChanged, this, [this](ToolMode mode) {
        m_editorState->current_mode = mode;
        updateToolbarState();
    });

    // ToolContext -> Viewport (trigger repaint on geometry/BC changes)
    connect(m_toolContext, &ToolContext::geometryChanged,
            m_viewport, QOverload<>::of(&QWidget::update));
    connect(m_toolContext, &ToolContext::statusMessage,
            m_statusLabel, &QLabel::setText);

    // Viewport -> BCPanel (node selection feedback)
    connect(m_viewport, &ViewportWidget::nodeClicked,
            m_bcPanel, &BCPanel::onNodeSelected);

    // ToolContext -> BCPanel (node selection feedback)
    connect(m_toolContext, &ToolContext::nodeSelected,
            m_bcPanel, &BCPanel::onNodeSelected);

    // ToolContext -> GeometryPanel (primitive selection feedback)
    connect(m_toolContext, &ToolContext::primitiveSelected,
            this, &MainWindow::onPrimitiveSelected);

    // GeometryPanel -> viewport update
    connect(m_geometryPanel, &GeometryPanel::primitiveSelected,
            m_viewport, QOverload<>::of(&QWidget::update));

    // GeometryPanel mesh generation request
    connect(m_geometryPanel, &GeometryPanel::meshRequested, this, [this](int nx, int ny) {
        // Store mesh dimensions for solver
        m_config.nx = nx;
        m_config.ny = ny;
        
        // Generate mesh from geometry model
        if (m_geometryModel->primitiveCount() == 0) {
            QMessageBox::warning(this, "No Geometry", 
                "Please draw geometry before generating mesh.");
            return;
        }
        
        // Get material properties from geometry panel
        Material mat;
        mat.E = m_geometryPanel->youngsModulus();
        mat.nu = m_geometryPanel->poissonsRatio();
        mat.t = m_geometryPanel->thickness();
        PlaneType plane = m_geometryPanel->planeType();
        
        // Generate mesh
        auto mesh = m_meshGenerator->generate(*m_geometryModel, mat, nx, ny, plane);
        
        if (mesh && mesh->num_nodes() > 0) {
            // Store mesh in viewport
            m_viewport->setMesh(*mesh);
            m_toolContext->setMeshNodes(&mesh->nodes);
            
            // Update worktree with mesh info
            m_geometryPanel->updateMeshInfo(mesh->num_nodes(), 
                                           mesh->num_quads() + mesh->num_tris(),
                                           m_meshGenerator->findBoundaryNodes(*mesh).size());
            
            // Update status
            m_statusLabel->setText(QString("Mesh generated: %1 nodes, %2 elements")
                .arg(mesh->num_nodes())
                .arg(mesh->num_quads() + mesh->num_tris()));
            
            // Store mesh for solver
            m_currentMesh = std::move(mesh);
        } else {
            QMessageBox::warning(this, "Mesh Generation Failed", 
                "Failed to generate mesh from geometry.");
        }
    });

    // GeometryPanel delete request -> refresh viewport
    connect(m_geometryPanel, &GeometryPanel::deleteRequested,
            m_viewport, QOverload<>::of(&QWidget::update));

    // Viewport element double-click -> inspector panel
    connect(m_viewport, &ViewportWidget::elementDoubleClicked, this, [this](int elemIndex) {
        if (m_lastResult.displacement.empty()) return;
        m_inspectorPanel->inspectElement(elemIndex, m_lastMesh, m_lastResult);
        // Show the inspector dock if hidden
        for (QDockWidget* dock : findChildren<QDockWidget*>()) {
            if (dock->widget() == m_inspectorPanel) {
                dock->setVisible(true);
                dock->raise();
                break;
            }
        }
    });
}

void MainWindow::updateToolbarState() {
    // Sync toolbar checked state with current editor mode
    switch (m_editorState->current_mode) {
        case ToolMode::SELECT:         m_selectAction->setChecked(true); break;
        case ToolMode::DRAW_RECT:      m_drawRectAction->setChecked(true); break;
        case ToolMode::DRAW_LINE:      m_drawLineAction->setChecked(true); break;
        case ToolMode::DRAW_CIRCLE:    m_drawCircleAction->setChecked(true); break;
        case ToolMode::ASSIGN_FIXED:   m_fixedAction->setChecked(true); break;
        case ToolMode::ASSIGN_ROLLER_X: m_rollerXAction->setChecked(true); break;
        case ToolMode::ASSIGN_ROLLER_Y: m_rollerYAction->setChecked(true); break;
        case ToolMode::APPLY_FORCE:    m_forceAction->setChecked(true); break;
        case ToolMode::PROBE:          m_probeAction->setChecked(true); break;
        default:                       m_selectAction->setChecked(true); break;
    }
    m_statusLabel->setText("Tool: " + m_editorState->currentToolName());
}

void MainWindow::onUndo() {
    if (m_undoStack->canUndo()) {
        m_undoStack->undo();
        m_statusLabel->setText("Undo: " + m_undoStack->undoText());
        // Refresh editor panels after undo
        m_geometryPanel->updateWorktree();
        m_bcPanel->updateBCList();
        m_viewport->update();
    }
}

void MainWindow::onRedo() {
    if (m_undoStack->canRedo()) {
        m_undoStack->redo();
        m_statusLabel->setText("Redo: " + m_undoStack->redoText());
        // Refresh editor panels after redo
        m_geometryPanel->updateWorktree();
        m_bcPanel->updateBCList();
        m_viewport->update();
    }
}

void MainWindow::onPrimitiveSelected(int index) {
    m_geometryPanel->selectPrimitiveInTree(index);
    m_viewport->update();
}

void MainWindow::onProbeRequested(double wx, double wy) {
    if (m_lastResult.displacement.empty()) {
        m_statusLabel->setText("No solve results available. Run solver first.");
        return;
    }
    m_probeTool->probe(wx, wy, m_lastMesh, m_lastResult);
}

void MainWindow::onProbed(const ProbeResult& result) {
    if (!result.valid) {
        m_statusLabel->setText("Probe: no valid result");
        return;
    }
    QString msg = QString("Node %1 | Element %2 | sigma_vm = %3 Pa | u = (%4, %5) m")
        .arg(result.nodeId)
        .arg(result.elemId)
        .arg(result.vonMises, 0, 'g', 4)
        .arg(result.ux, 0, 'g', 4)
        .arg(result.uy, 0, 'g', 4);
    m_statusLabel->setText(msg);
}

// ------------------------------------------------------------------
// Modal analysis
// ------------------------------------------------------------------
void MainWindow::onRunModalAnalysis() {
    if (m_lastResult.K_csr.nrows == 0) {
        m_statusLabel->setText("No stiffness matrix available. Run a static solve first.");
        return;
    }
    if (m_lastMesh.num_nodes() == 0) {
        m_statusLabel->setText("No mesh available. Run a static solve first.");
        return;
    }

    m_statusLabel->setText("Running modal analysis...");
    m_modalRunner->setMesh(m_lastMesh);
    m_modalRunner->setK(m_lastResult.K_csr);
    m_modalRunner->setNumModes(10);
    m_modalRunner->start();
}

void MainWindow::onModalFinished(const dynamics::ModalResult& result) {
    m_lastModalResult = result;

    // Pass result to viewport
    m_viewport->setModalResult(result);

    // Pass result to mode shape panel
    m_modeShapePanel->setModalResult(result);

    // Show mode shape dock
    for (QDockWidget* dock : findChildren<QDockWidget*>()) {
        if (dock->widget() == m_modeShapePanel) {
            dock->setVisible(true);
            dock->raise();
            break;
        }
    }

    // Update status
    QString msg = QString("Modal analysis complete: %1 modes computed. f1 = %2 Hz")
        .arg(result.num_modes)
        .arg(result.frequencies_hz.empty() ? 0.0 : result.frequencies_hz[0], 0, 'f', 2);
    m_statusLabel->setText(msg);
}

void MainWindow::onModalError(const QString& errorMessage) {
    m_statusLabel->setText("Modal analysis error: " + errorMessage);
    QMessageBox::warning(this, "Modal Analysis Error", errorMessage);
}

void MainWindow::onModeChanged(int modeIndex) {
    if (!m_modeShapePanel) return;
    m_viewport->startModeAnimation(modeIndex);
}

void MainWindow::onModeAnimationToggled(bool playing) {
    if (playing) {
        int modeIdx = m_modeShapePanel->currentModeIndex();
        if (modeIdx >= 0 && modeIdx < m_lastModalResult.num_modes) {
            m_viewport->startModeAnimation(modeIdx);
        }
    } else {
        m_viewport->stopModeAnimation();
    }
}

void MainWindow::onModeAmplitudeChanged(double amplitude) {
    m_viewport->setModeAmplitude(amplitude);
}

void MainWindow::onModeReset() {
    m_viewport->stopModeAnimation();
}

// ------------------------------------------------------------------
// Parametric study: real-time parameter changes
// ------------------------------------------------------------------
void MainWindow::onParamChanged(ParamID param, double value) {
    if (!m_parametricEngine->isValid()) {
        m_statusLabel->setText("No baseline data. Run a solve first.");
        return;
    }

    double newE = m_parametricPanel->youngsModulus();
    double newNu = m_parametricPanel->poissonsRatio();
    double newForce = m_parametricPanel->forceMagnitude();
    double newT = m_parametricPanel->thickness();

    // Update config from panel values
    m_config.E = newE;
    m_config.nu = newNu;
    m_config.t = newT;

    if (m_parametricEngine->canRescale(newE, newNu, newT)) {
        // Fast path: scale-only rescale (no reassembly)
        m_statusLabel->setText("Parametric rescale (fast path)...");
        auto result = m_parametricEngine->rescale(newE, newForce, newT);

        if (!result.displacement.empty()) {
            m_lastResult = result;
            bool is3d = m_lastMesh.is_3d();
            if (is3d) {
                m_viewport3d->setMeshAndResults(m_lastMesh, result);
            } else {
                m_viewport->setMeshAndResults(m_lastMesh, result);
            }

            // Update plots
            m_stressHist->setData(result.stresses);

            EnergyBalanceData ed;
            ed.strain_energy = fea::compute_strain_energy(result.K_csr, result.displacement);
            ed.work_done = fea::compute_work_done(result.f, result.displacement);
            ed.valid = true;
            m_energyChart->setData(ed);

            if (!is3d) {
                m_dispLineChart->setData(m_lastMesh, result);
            }

            // Compute max displacement for result display
            int dofPerNode = is3d ? 3 : 2;
            double maxDisp = 0.0;
            for (int i = 0; i < m_lastMesh.num_nodes(); ++i) {
                double ux = result.displacement[dofPerNode * i];
                double uy = result.displacement[dofPerNode * i + 1];
                double d = std::sqrt(ux * ux + uy * uy);
                if (d > maxDisp) maxDisp = d;
            }
            double maxStress = 0.0;
            for (const auto& s : result.stresses) {
                if (s.von_mises > maxStress) maxStress = s.von_mises;
            }

            m_parametricPanel->updateFromResult(maxDisp, maxStress, result.solve_time_ms);
            m_solverPanel->setResult(result);
            m_statusLabel->setText(QString("Rescale complete (%1 ms)").arg(result.solve_time_ms, 0, 'f', 1));
        }
    } else {
        // Nu changed: need full reassembly
        m_statusLabel->setText("Nu changed: triggering full reassembly...");

        // Update material in mesh
        m_lastMesh.mat.E = newE;
        m_lastMesh.mat.nu = newNu;
        m_lastMesh.mat.t = newT;

        // Store mesh for solver
        m_currentMesh = std::make_unique<Mesh>(m_lastMesh);

        // Run full solve asynchronously
        m_config.use_cg = m_solverPanel->useCG();
        m_progressBar->setVisible(true);
        m_progressBar->setValue(0);
        m_solverRunner->setConfig(m_config);
        m_solverRunner->setMesh(m_lastMesh, m_config.use_cg);
        m_solverRunner->start();
    }
}