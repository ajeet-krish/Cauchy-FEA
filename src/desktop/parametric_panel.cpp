#include "parametric_panel.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QFont>
#include <cmath>
#include <iomanip>
#include <sstream>

// ==========================================================================
// PARAMETRIC PANEL -- Real-time parametric study UI
// ==========================================================================

ParametricPanel::ParametricPanel(QWidget* parent)
    : QWidget(parent) {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(4);

    // ---- Material Properties Group ----
    auto* matGroup = new QGroupBox("Material Properties", this);
    auto* matLayout = new QFormLayout(matGroup);
    matLayout->setSpacing(4);

    // Young's Modulus (E): log scale
    m_ELabel = new QLabel("E (Pa):", this);
    m_ESlider = new QSlider(Qt::Horizontal, this);
    m_ESlider->setRange(0, E_STEPS);
    m_ESlider->setValue(valueToLogSlider(E_MIN, E_MAX, E_STEPS, m_initialE));
    m_ESpin = new QDoubleSpinBox(this);
    m_ESpin->setRange(E_MIN, E_MAX);
    m_ESpin->setValue(m_initialE);
    m_ESpin->setDecimals(2);
    m_ESpin->setSuffix(" Pa");
    m_ESpin->setSingleStep(1e9);

    auto* eRow = new QHBoxLayout();
    eRow->addWidget(m_ESlider, 1);
    eRow->addWidget(m_ESpin, 0);
    matLayout->addRow(m_ELabel, eRow);

    // Poisson's ratio (nu): linear
    m_nuLabel = new QLabel("nu:", this);
    m_nuSlider = new QSlider(Qt::Horizontal, this);
    m_nuSlider->setRange(0, NU_STEPS);
    m_nuSlider->setValue(valueToLinearSlider(NU_MIN, NU_MAX, NU_STEPS, m_initialNu));
    m_nuSpin = new QDoubleSpinBox(this);
    m_nuSpin->setRange(NU_MIN, NU_MAX);
    m_nuSpin->setValue(m_initialNu);
    m_nuSpin->setDecimals(3);
    m_nuSpin->setSingleStep(0.001);

    auto* nuRow = new QHBoxLayout();
    nuRow->addWidget(m_nuSlider, 1);
    nuRow->addWidget(m_nuSpin, 0);
    matLayout->addRow(m_nuLabel, nuRow);

    mainLayout->addWidget(matGroup);

    // ---- Loading Group ----
    auto* loadGroup = new QGroupBox("Loading", this);
    auto* loadLayout = new QFormLayout(loadGroup);
    loadLayout->setSpacing(4);

    // Force: linear scale relative to baseline
    m_forceLabel = new QLabel("Force (x):", this);
    m_forceSlider = new QSlider(Qt::Horizontal, this);
    m_forceSlider->setRange(0, FORCE_STEPS);
    m_forceSlider->setValue(valueToLinearSlider(FORCE_MIN_FRAC, FORCE_MAX_FRAC, FORCE_STEPS, 1.0));
    m_forceSpin = new QDoubleSpinBox(this);
    m_forceSpin->setRange(FORCE_MIN_FRAC, FORCE_MAX_FRAC);
    m_forceSpin->setValue(1.0);
    m_forceSpin->setDecimals(2);
    m_forceSpin->setSuffix("x");
    m_forceSpin->setSingleStep(0.1);

    auto* fRow = new QHBoxLayout();
    fRow->addWidget(m_forceSlider, 1);
    fRow->addWidget(m_forceSpin, 0);
    loadLayout->addRow(m_forceLabel, fRow);

    mainLayout->addWidget(loadGroup);

    // ---- Geometry Group ----
    auto* geoGroup = new QGroupBox("Geometry", this);
    auto* geoLayout = new QFormLayout(geoGroup);
    geoLayout->setSpacing(4);

    // Thickness (t): log scale
    m_tLabel = new QLabel("Thickness (m):", this);
    m_tSlider = new QSlider(Qt::Horizontal, this);
    m_tSlider->setRange(0, T_STEPS);
    m_tSlider->setValue(valueToLogSlider(T_MIN, T_MAX, T_STEPS, m_initialThickness));
    m_tSpin = new QDoubleSpinBox(this);
    m_tSpin->setRange(T_MIN, T_MAX);
    m_tSpin->setValue(m_initialThickness);
    m_tSpin->setDecimals(4);
    m_tSpin->setSuffix(" m");
    m_tSpin->setSingleStep(0.001);

    auto* tRow = new QHBoxLayout();
    tRow->addWidget(m_tSlider, 1);
    tRow->addWidget(m_tSpin, 0);
    geoLayout->addRow(m_tLabel, tRow);

    mainLayout->addWidget(geoGroup);

    // ---- Result Display ----
    auto* resultGroup = new QGroupBox("Quick Results", this);
    auto* resultLayout = new QVBoxLayout(resultGroup);
    m_resultLabel = new QLabel("Run a solve to see results.", this);
    m_resultLabel->setWordWrap(true);
    QFont monoFont("Monospace", 9);
    monoFont.setStyleHint(QFont::TypeWriter);
    m_resultLabel->setFont(monoFont);
    resultLayout->addWidget(m_resultLabel);
    mainLayout->addWidget(resultGroup);

    // ---- Reset Button ----
    auto* resetLayout = new QHBoxLayout();
    auto* resetButton = new QPushButton("Reset to Defaults", this);
    connect(resetButton, &QPushButton::clicked, this, &ParametricPanel::resetToDefaults);
    resetLayout->addStretch();
    resetLayout->addWidget(resetButton);
    resetLayout->addStretch();
    mainLayout->addLayout(resetLayout);

    mainLayout->addStretch();

    // ---- Debounce Timer ----
    m_debounceTimer = new QTimer(this);
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(100);
    connect(m_debounceTimer, &QTimer::timeout, this, &ParametricPanel::debounceFire);

    // ---- Signal/Slot Connections ----
    // Slider -> SpinBox sync + debounce
    connect(m_ESlider, &QSlider::valueChanged, this, [this](int pos) {
        double val = logSliderToValue(E_MIN, E_MAX, E_STEPS, pos);
        m_ESpin->blockSignals(true);
        m_ESpin->setValue(val);
        m_ESpin->blockSignals(false);
        m_pendingParam = ParamID::E;
        m_pendingValue = val;
        m_debounceTimer->start();
    });

    connect(m_nuSlider, &QSlider::valueChanged, this, [this](int pos) {
        double val = linearSliderToValue(NU_MIN, NU_MAX, NU_STEPS, pos);
        m_nuSpin->blockSignals(true);
        m_nuSpin->setValue(val);
        m_nuSpin->blockSignals(false);
        m_pendingParam = ParamID::NU;
        m_pendingValue = val;
        m_debounceTimer->start();
    });

    connect(m_forceSlider, &QSlider::valueChanged, this, [this](int pos) {
        double val = linearSliderToValue(FORCE_MIN_FRAC, FORCE_MAX_FRAC, FORCE_STEPS, pos);
        m_forceSpin->blockSignals(true);
        m_forceSpin->setValue(val);
        m_forceSpin->blockSignals(false);
        m_pendingParam = ParamID::FORCE;
        m_pendingValue = val;
        m_debounceTimer->start();
    });

    connect(m_tSlider, &QSlider::valueChanged, this, [this](int pos) {
        double val = logSliderToValue(T_MIN, T_MAX, T_STEPS, pos);
        m_tSpin->blockSignals(true);
        m_tSpin->setValue(val);
        m_tSpin->blockSignals(false);
        m_pendingParam = ParamID::THICKNESS;
        m_pendingValue = val;
        m_debounceTimer->start();
    });

    // SpinBox -> Slider sync + debounce
    connect(m_ESpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        m_ESlider->blockSignals(true);
        m_ESlider->setValue(valueToLogSlider(E_MIN, E_MAX, E_STEPS, val));
        m_ESlider->blockSignals(false);
        m_pendingParam = ParamID::E;
        m_pendingValue = val;
        m_debounceTimer->start();
    });

    connect(m_nuSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        m_nuSlider->blockSignals(true);
        m_nuSlider->setValue(valueToLinearSlider(NU_MIN, NU_MAX, NU_STEPS, val));
        m_nuSlider->blockSignals(false);
        m_pendingParam = ParamID::NU;
        m_pendingValue = val;
        m_debounceTimer->start();
    });

    connect(m_forceSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        m_forceSlider->blockSignals(true);
        m_forceSlider->setValue(valueToLinearSlider(FORCE_MIN_FRAC, FORCE_MAX_FRAC, FORCE_STEPS, val));
        m_forceSlider->blockSignals(false);
        m_pendingParam = ParamID::FORCE;
        m_pendingValue = val;
        m_debounceTimer->start();
    });

    connect(m_tSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        m_tSlider->blockSignals(true);
        m_tSlider->setValue(valueToLogSlider(T_MIN, T_MAX, T_STEPS, val));
        m_tSlider->blockSignals(false);
        m_pendingParam = ParamID::THICKNESS;
        m_pendingValue = val;
        m_debounceTimer->start();
    });
}

// ---- Public accessors ----

void ParametricPanel::setInitialValues(double E, double nu, double force, double thickness) {
    m_initialE = E;
    m_initialNu = nu;
    m_initialForce = force;
    m_initialThickness = thickness;

    // Update spin boxes
    m_ESpin->setValue(E);
    m_nuSpin->setValue(nu);
    m_forceSpin->setValue(1.0);  // Force is always relative (1.0x)
    m_tSpin->setValue(thickness);

    // Update sliders
    m_ESlider->setValue(valueToLogSlider(E_MIN, E_MAX, E_STEPS, E));
    m_nuSlider->setValue(valueToLinearSlider(NU_MIN, NU_MAX, NU_STEPS, nu));
    m_forceSlider->setValue(valueToLinearSlider(FORCE_MIN_FRAC, FORCE_MAX_FRAC, FORCE_STEPS, 1.0));
    m_tSlider->setValue(valueToLogSlider(T_MIN, T_MAX, T_STEPS, thickness));
}

void ParametricPanel::updateFromResult(double maxDisp, double maxStress, double solveTimeMs) {
    std::ostringstream ss;
    ss << std::scientific << std::setprecision(3);
    ss << "max |u|: " << maxDisp << " m\n";
    ss << "max vm:  " << maxStress << " Pa\n";
    ss << std::fixed << std::setprecision(1);
    ss << "time:    " << solveTimeMs << " ms";
    m_resultLabel->setText(QString::fromStdString(ss.str()));
}

double ParametricPanel::youngsModulus() const { return m_ESpin->value(); }
double ParametricPanel::poissonsRatio() const { return m_nuSpin->value(); }
double ParametricPanel::forceMagnitude() const { return m_forceSpin->value(); }
double ParametricPanel::thickness() const { return m_tSpin->value(); }

void ParametricPanel::resetToDefaults() {
    m_ESpin->setValue(m_initialE);
    m_nuSpin->setValue(m_initialNu);
    m_forceSpin->setValue(1.0);
    m_tSpin->setValue(m_initialThickness);
}

// ---- Private slots ----

void ParametricPanel::onSliderChanged() {
    // Handled by lambda connections above
}

void ParametricPanel::debounceFire() {
    emit paramChanged(m_pendingParam, m_pendingValue);
}

// ---- Static helpers ----

double ParametricPanel::logSliderToValue(double v_min, double v_max, int steps, int pos) {
    // v = v_min * (v_max / v_min)^(pos / steps)
    double fraction = static_cast<double>(pos) / static_cast<double>(steps);
    return v_min * std::pow(v_max / v_min, fraction);
}

int ParametricPanel::valueToLogSlider(double v_min, double v_max, int steps, double value) {
    // pos = steps * log(value / v_min) / log(v_max / v_min)
    if (value <= v_min) return 0;
    if (value >= v_max) return steps;
    double fraction = std::log(value / v_min) / std::log(v_max / v_min);
    return static_cast<int>(std::round(fraction * steps));
}

double ParametricPanel::linearSliderToValue(double v_min, double v_max, int steps, int pos) {
    double fraction = static_cast<double>(pos) / static_cast<double>(steps);
    return v_min + fraction * (v_max - v_min);
}

int ParametricPanel::valueToLinearSlider(double v_min, double v_max, int steps, double value) {
    if (value <= v_min) return 0;
    if (value >= v_max) return steps;
    double fraction = (value - v_min) / (v_max - v_min);
    return static_cast<int>(std::round(fraction * steps));
}
