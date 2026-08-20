#include "solver_panel.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QCheckBox>
#include <QSpinBox>
#include <QPushButton>
#include <QProgressBar>
#include <QTextEdit>
#include <QTextStream>
#include <iomanip>

SolverPanel::SolverPanel(QWidget* parent)
    : QWidget(parent) {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(6, 6, 6, 6);
    m_mainLayout->setSpacing(4);

    createSolverControls();
    createResultsDisplay();
}

void SolverPanel::createSolverControls() {
    auto* solverGroup = new QGroupBox("Solver", this);
    auto* solverLayout = new QFormLayout(solverGroup);

    m_cgCheck = new QCheckBox("Use Conjugate Gradient (CG)", this);
    m_cgCheck->setChecked(false);
    solverLayout->addRow(m_cgCheck);

    m_adaptCheck = new QCheckBox("Adaptive refinement", this);
    m_adaptCheck->setChecked(false);
    solverLayout->addRow(m_adaptCheck);

    m_adaptIters = new QSpinBox(this);
    m_adaptIters->setRange(1, 10);
    m_adaptIters->setValue(3);
    m_adaptIters->setEnabled(false);
    solverLayout->addRow("Adaptive iterations:", m_adaptIters);

    connect(m_adaptCheck, &QCheckBox::toggled,
            m_adaptIters, &QSpinBox::setEnabled);

    m_mainLayout->addWidget(solverGroup);

    auto* runLayout = new QHBoxLayout();
    m_runButton = new QPushButton("Run Solver", this);
    m_resetButton = new QPushButton("Reset View", this);
    m_modalButton = new QPushButton("Modal Analysis", this);
    runLayout->addWidget(m_runButton);
    runLayout->addWidget(m_resetButton);
    m_mainLayout->addLayout(runLayout);

    auto* modalLayout = new QHBoxLayout();
    modalLayout->addWidget(m_modalButton);
    m_mainLayout->addLayout(modalLayout);

    connect(m_runButton, &QPushButton::clicked,
            this, &SolverPanel::runClicked);
    connect(m_resetButton, &QPushButton::clicked,
            this, &SolverPanel::resetClicked);
    connect(m_modalButton, &QPushButton::clicked,
            this, &SolverPanel::modalClicked);
}

void SolverPanel::createResultsDisplay() {
    auto* resultsGroup = new QGroupBox("Results", this);
    auto* resultsLayout = new QVBoxLayout(resultsGroup);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setVisible(false);
    resultsLayout->addWidget(m_progressBar);

    m_resultsText = new QTextEdit(this);
    m_resultsText->setReadOnly(true);
    m_resultsText->setFontPointSize(10);
    m_resultsText->setMaximumHeight(200);
    resultsLayout->addWidget(m_resultsText);

    m_mainLayout->addWidget(resultsGroup);
}

bool SolverPanel::useCG() const { return m_cgCheck->isChecked(); }
bool SolverPanel::useAdaptivity() const { return m_adaptCheck->isChecked(); }
int SolverPanel::adaptiveIterations() const { return m_adaptIters->value(); }

void SolverPanel::setResult(const fea::SolveResult& result) {
    QString text;
    QTextStream stream(&text);
    stream << "Solve time: "
           << QLocale::c().toString(result.solve_time_ms, 'f', 1)
           << " ms\n";
    stream << "CG iterations: " << result.cg_iterations << "\n";
    stream << "Converged: " << (result.cg_converged ? "Yes" : "No") << "\n";
    stream << "DOFs: " << result.displacement.size() << "\n";
    int nodeCount = static_cast<int>(result.displacement.size()) / DOF_PER_NODE;
    stream << "Nodes: " << nodeCount << "\n";
    stream << "Elements: " << result.stresses.size() << "\n";
    m_resultsText->setPlainText(text);
}