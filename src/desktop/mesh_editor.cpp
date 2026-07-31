#include "mesh_editor.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QGroupBox>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>

MeshEditor::MeshEditor(QWidget* parent)
    : QWidget(parent) {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(6, 6, 6, 6);
    m_mainLayout->setSpacing(4);

    createCaseControls();
    createMaterialControls();
    createMeshControls();

    connect(m_caseCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MeshEditor::caseChanged);
    connect(m_elemCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MeshEditor::caseChanged);
    connect(m_planeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MeshEditor::caseChanged);
    connect(m_ESpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MeshEditor::materialChanged);
    connect(m_nuSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MeshEditor::materialChanged);
    connect(m_tSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MeshEditor::materialChanged);
}

void MeshEditor::createCaseControls() {
    auto* caseGroup = new QGroupBox("Case", this);
    auto* caseLayout = new QVBoxLayout(caseGroup);

    m_caseCombo = new QComboBox(this);
    m_caseCombo->addItems({
        "Cantilever Beam",
        "Cook's Membrane",
        "L-Bracket",
        "Patch Test",
        "Plate with Hole",
        "Michell Truss",
        "Thick Cylinder"
    });
    caseLayout->addWidget(m_caseCombo);

    m_mainLayout->addWidget(caseGroup);
}

void MeshEditor::createMaterialControls() {
    auto* matGroup = new QGroupBox("Material", this);
    auto* matLayout = new QFormLayout(matGroup);

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
    matLayout->addRow("Poisson's Ratio (nu):", m_nuSpin);

    m_tSpin = new QDoubleSpinBox(this);
    m_tSpin->setRange(0.001, 10.0);
    m_tSpin->setValue(0.01);
    m_tSpin->setDecimals(4);
    m_tSpin->setSingleStep(0.001);
    m_tSpin->setSuffix(" m");
    matLayout->addRow("Thickness (t):", m_tSpin);

    m_mainLayout->addWidget(matGroup);
}

void MeshEditor::createMeshControls() {
    auto* meshGroup = new QGroupBox("Mesh", this);
    auto* meshLayout = new QFormLayout(meshGroup);

    m_nxSpin = new QSpinBox(this);
    m_nxSpin->setRange(2, 256);
    m_nxSpin->setValue(32);
    meshLayout->addRow("NX:", m_nxSpin);

    m_nySpin = new QSpinBox(this);
    m_nySpin->setRange(2, 256);
    m_nySpin->setValue(8);
    meshLayout->addRow("NY:", m_nySpin);

    m_elemCombo = new QComboBox(this);
    m_elemCombo->addItems({"Q4 (Bilinear Quad)", "Q8 (Serendipity)", "T3 (Triangle)", "Bar (Truss)"});
    meshLayout->addRow("Element Type:", m_elemCombo);

    m_planeCombo = new QComboBox(this);
    m_planeCombo->addItems({"Plane Stress", "Plane Strain"});
    meshLayout->addRow("Plane Type:", m_planeCombo);

    m_q8Check = new QCheckBox("Use Q8 elements", this);
    meshLayout->addRow(m_q8Check);

    m_mainLayout->addWidget(meshGroup);
}

CaseType MeshEditor::caseType() const {
    switch (m_caseCombo->currentIndex()) {
    case 0: return CaseType::CANTILEVER;
    case 1: return CaseType::COOK;
    case 2: return CaseType::LBRACKET;
    case 3: return CaseType::PATCH;
    case 4: return CaseType::PLATE_HOLE;
    case 5: return CaseType::MICHELL;
    case 6: return CaseType::THERMAL_CYLINDER;
    default: return CaseType::CANTILEVER;
    }
}

ElementType MeshEditor::elementType() const {
    switch (m_elemCombo->currentIndex()) {
    case 2: return ElementType::T3;
    case 3: return ElementType::BAR;
    default: return ElementType::Q4;
    }
}

PlaneType MeshEditor::planeType() const {
    return m_planeCombo->currentIndex() == 1
        ? PlaneType::STRAIN
        : PlaneType::STRESS;
}

int MeshEditor::meshSizeX() const { return m_nxSpin->value(); }
int MeshEditor::meshSizeY() const { return m_nySpin->value(); }
bool MeshEditor::useQ8() const { return m_q8Check->isChecked(); }
double MeshEditor::youngsModulus() const { return m_ESpin->value(); }
double MeshEditor::poissonsRatio() const { return m_nuSpin->value(); }
double MeshEditor::thickness() const { return m_tSpin->value(); }