#include "main_window.hpp"
#include <QMenuBar>
#include <QMenu>
#include <QAction>
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
#include <QApplication>
#include <QClipboard>
#include <QImageWriter>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle("Cauchy FEA -- 2D Finite Element Structural Solver");
    resize(1400, 900);

    // Central viewport
    m_viewport = new ViewportWidget(this);
    setCentralWidget(m_viewport);

    // Solver runner
    m_solverRunner = new SolverRunner(this);
    connect(m_solverRunner, &SolverRunner::finished,
            this, &MainWindow::onSolveFinished);
    connect(m_solverRunner, &SolverRunner::error,
            this, &MainWindow::onSolverError);
    connect(m_solverRunner, &SolverRunner::progress,
            this, &MainWindow::onProgress);

    // Result model
    m_resultModel = new ResultModel(this);

    // Create dock widgets
    m_meshEditor = new MeshEditor(this);
    m_solverPanel = new SolverPanel(this);

    QDockWidget* meshDock = new QDockWidget("Mesh & Material", this);
    meshDock->setWidget(m_meshEditor);
    meshDock->setAllowedAreas(Qt::LeftDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, meshDock);

    QDockWidget* solverDock = new QDockWidget("Solver & Results", this);
    solverDock->setWidget(m_solverPanel);
    solverDock->setAllowedAreas(Qt::RightDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, solverDock);

    // Create menus, toolbars, status bar
    createActions();
    createMenuBar();
    createToolbars();
    createStatusBar();
    createBottomPlots();

    // Connect solver panel signals
    connect(m_solverPanel, &SolverPanel::runClicked,
            this, &MainWindow::onRunSolver);
    connect(m_solverPanel, &SolverPanel::resetClicked,
            this, &MainWindow::onResetView);

    // Connect viewport signals
    Q_UNUSED(m_viewport)
}

void MainWindow::createActions() {
    // File actions
    QAction* newAct = new QAction("New", this);
    newAct->setShortcut(QKeySequence::New);
    newAct->setStatusTip("Create a new mesh");

    QAction* openAct = new QAction("Open...", this);
    openAct->setShortcut(QKeySequence::Open);
    openAct->setStatusTip("Open a project file");

    QAction* saveAct = new QAction("Save As...", this);
    saveAct->setShortcut(QKeySequence::SaveAs);
    saveAct->setStatusTip("Save project to file");

    QAction* exportPngAct = new QAction("Export PNG", this);
    exportPngAct->setStatusTip("Export current view as PNG");
    connect(exportPngAct, &QAction::triggered, this, &MainWindow::onExportPNG);

    QAction* exitAct = new QAction("Exit", this);
    exitAct->setShortcut(QKeySequence::Quit);
    connect(exitAct, &QAction::triggered, this, &QMainWindow::close);

    // View actions
    QAction* resetViewAct = new QAction("Reset View", this);
    resetViewAct->setStatusTip("Reset camera to default view");
    connect(resetViewAct, &QAction::triggered, this, &MainWindow::onResetView);

    // Help
    QAction* aboutAct = new QAction("About", this);
    aboutAct->setStatusTip("About Cauchy FEA");
    connect(aboutAct, &QAction::triggered, this, []() {
        QMessageBox::about(nullptr, "About Cauchy FEA",
            "<h2>Cauchy FEA</h2>"
            "<p>Version 1.0.0</p>"
            "<p>A 2D finite element structural solver for plane stress and plane strain problems.</p>"
            "<p>Supports Bar, Q4 (bilinear quad), Q8 (serendipity quad), and T3 (triangle) elements.</p>"
            "<p>Features: Cholesky direct solver, Conjugate Gradient iterative solver, "
            "Von Mises stress recovery, ZZ error estimator, adaptive h-refinement.</p>"
            "<p>MIT License -- Portfolio project for mechanical/aerospace engineering roles.</p>");
    });

    // Store actions for menu building
    Q_UNUSED(newAct) Q_UNUSED(openAct) Q_UNUSED(saveAct) Q_UNUSED(exitAct)
    Q_UNUSED(resetViewAct) Q_UNUSED(aboutAct)
}

void MainWindow::createMenuBar() {
    QMenuBar* menuBar = this->menuBar();

    QMenu* fileMenu = menuBar->addMenu("&File");
    fileMenu->addAction("New");
    fileMenu->addAction("Open...");
    fileMenu->addAction("Save As...");
    fileMenu->addSeparator();
    fileMenu->addAction("Export PNG");
    fileMenu->addSeparator();
    fileMenu->addAction("Exit");

    QMenu* viewMenu = menuBar->addMenu("&View");
    viewMenu->addAction("Reset View");

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

    m_viewport->setMeshAndResults(mesh, result);
    m_resultModel->setData(result, mesh, ResultTableType::DISPLACEMENT);
    m_solverPanel->setResult(result);

    // Store for plot access
    m_lastMesh = mesh;
    m_lastResult = result;

    // Update stress histogram
    m_stressHist->setData(result.stresses);

    // Update energy balance
    EnergyBalanceData ed;
    ed.strain_energy = fea::compute_strain_energy(result.K_csr, result.displacement);
    ed.work_done = fea::compute_work_done(result.f, result.displacement);
    ed.valid = true;
    m_energyChart->setData(ed);

    // Update displacement line chart
    m_dispLineChart->setData(mesh, result);

    // Update load-displacement chart
    double maxDisp = 0.0;
    for (int i = 0; i < mesh.num_nodes(); ++i) {
        double ux = result.displacement[2 * i];
        double uy = result.displacement[2 * i + 1];
        double d = std::sqrt(ux * ux + uy * uy);
        if (d > maxDisp) maxDisp = d;
    }
    double totalForce = 0.0;
    for (const auto& bc : mesh.neumann) {
        totalForce += std::abs(bc.value);
    }
    m_ldChart->addPoint(totalForce, maxDisp);
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
    m_config.case_type = m_meshEditor->caseType();
    m_config.element_type = m_meshEditor->elementType();
    m_config.plane_type = m_meshEditor->planeType();
    m_config.nx = m_meshEditor->meshSizeX();
    m_config.ny = m_meshEditor->meshSizeY();
    m_config.use_q8 = m_meshEditor->useQ8();
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

void MainWindow::onLoadCase() {
    QString fileName = QFileDialog::getOpenFileName(this,
        "Open Project", "", "Cauchy Project (*.cauchy)");
    if (fileName.isEmpty()) return;

    // Load project file (JSON-based)
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Load Error",
            "Could not open file: " + fileName);
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) {
        QMessageBox::warning(this, "Load Error", "Invalid project file format.");
        return;
    }

    QJsonObject obj = doc.object();
    // TODO: Restore mesh, material, BC, and results from JSON
    QMessageBox::information(this, "Load Project",
        "Project loaded: " + QFileInfo(fileName).fileName());
}

void MainWindow::onExportPNG() {
    QString fileName = QFileDialog::getSaveFileName(this,
        "Export PNG", "fea_view.png", "PNG Image (*.png)");
    if (fileName.isEmpty()) return;

    QImage image = m_viewport->grab().toImage();
    image.save(fileName, "PNG");
}

void MainWindow::onResetView() {
    m_viewport->resetView();
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
    // TODO: Implement deformation animation
}

void MainWindow::onResetAnimation() {
    // TODO: Reset animation
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

    m_plotTabs->addTab(m_stressHist, "Stress Distribution");
    m_plotTabs->addTab(m_energyChart, "Energy Balance");
    m_plotTabs->addTab(m_dispLineChart, "Displacement Profile");
    m_plotTabs->addTab(m_ldChart, "Load-Displacement");
    m_plotTabs->addTab(m_errorHeatmap, "Error Map");
    m_plotTabs->addTab(m_convChart, "Convergence");

    QDockWidget* plotsDock = new QDockWidget("Analysis Plots", this);
    plotsDock->setWidget(m_plotTabs);
    plotsDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    addDockWidget(Qt::BottomDockWidgetArea, plotsDock);
}