#pragma once
#include <QWidget>
#include "fea.hpp"

class QCheckBox;
class QSpinBox;
class QDoubleSpinBox;
class QPushButton;
class QLabel;
class QTextEdit;
class QProgressBar;
class QVBoxLayout;

class SolverPanel : public QWidget {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SolverPanel)
public:
    explicit SolverPanel(QWidget* parent = nullptr);

    bool useCG() const;
    bool useAdaptivity() const;
    int adaptiveIterations() const;
    void setResult(const fea::SolveResult& result);

signals:
    void runClicked();
    void resetClicked();
    void modalClicked();

private:
    void createSolverControls();
    void createResultsDisplay();

    QVBoxLayout* m_mainLayout = nullptr;
    QCheckBox* m_cgCheck = nullptr;
    QCheckBox* m_adaptCheck = nullptr;
    QSpinBox* m_adaptIters = nullptr;
    QPushButton* m_runButton = nullptr;
    QPushButton* m_resetButton = nullptr;
    QPushButton* m_modalButton = nullptr;
    QTextEdit* m_resultsText = nullptr;
    QProgressBar* m_progressBar = nullptr;
};