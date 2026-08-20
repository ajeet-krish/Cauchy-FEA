#pragma once
#include <QWidget>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QTimer>

// ==========================================================================
// PARAMETRIC PANEL -- Sliders for E, nu, force, thickness with debounce
// ==========================================================================

enum class ParamID { E, NU, FORCE, THICKNESS };

class ParametricPanel : public QWidget {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ParametricPanel)
public:
    explicit ParametricPanel(QWidget* parent = nullptr);

    void setInitialValues(double E, double nu, double force, double thickness);
    void updateFromResult(double maxDisp, double maxStress, double solveTimeMs);

    double youngsModulus() const;
    double poissonsRatio() const;
    double forceMagnitude() const;
    double thickness() const;

    void resetToDefaults();

signals:
    void paramChanged(ParamID param, double value);

private slots:
    void onSliderChanged();
    void debounceFire();

private:
    // E slider: log scale [1e9, 1e12], 1000 steps
    QSlider* m_ESlider = nullptr;
    QDoubleSpinBox* m_ESpin = nullptr;
    QLabel* m_ELabel = nullptr;

    // nu slider: linear [0.0, 0.499], 500 steps
    QSlider* m_nuSlider = nullptr;
    QDoubleSpinBox* m_nuSpin = nullptr;
    QLabel* m_nuLabel = nullptr;

    // Force slider: linear [0.1x, 10.0x], 100 steps
    QSlider* m_forceSlider = nullptr;
    QDoubleSpinBox* m_forceSpin = nullptr;
    QLabel* m_forceLabel = nullptr;

    // Thickness slider: log scale [1e-4, 1.0], 1000 steps
    QSlider* m_tSlider = nullptr;
    QDoubleSpinBox* m_tSpin = nullptr;
    QLabel* m_tLabel = nullptr;

    // Result display
    QLabel* m_resultLabel = nullptr;

    // Debounce timer (100ms)
    QTimer* m_debounceTimer = nullptr;

    // Initial reference values
    double m_initialE = 210e9;
    double m_initialNu = 0.3;
    double m_initialForce = 1000.0;
    double m_initialThickness = 0.01;

    // Track which param changed
    ParamID m_pendingParam = ParamID::E;
    double m_pendingValue = 0.0;

    // Logarithmic slider helpers
    static constexpr int E_STEPS = 1000;
    static constexpr double E_MIN = 1e9;
    static constexpr double E_MAX = 1e12;

    static constexpr int NU_STEPS = 500;
    static constexpr double NU_MIN = 0.0;
    static constexpr double NU_MAX = 0.499;

    static constexpr int FORCE_STEPS = 100;
    static constexpr double FORCE_MIN_FRAC = 0.1;
    static constexpr double FORCE_MAX_FRAC = 10.0;

    static constexpr int T_STEPS = 1000;
    static constexpr double T_MIN = 1e-4;
    static constexpr double T_MAX = 1.0;

    static double logSliderToValue(double v_min, double v_max, int steps, int pos);
    static int valueToLogSlider(double v_min, double v_max, int steps, double value);
    static double linearSliderToValue(double v_min, double v_max, int steps, int pos);
    static int valueToLinearSlider(double v_min, double v_max, int steps, double value);
};
