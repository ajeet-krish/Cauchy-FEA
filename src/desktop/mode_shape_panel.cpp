#include "mode_shape_panel.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QSlider>
#include <QLocale>

ModeShapePanel::ModeShapePanel(QWidget* parent)
    : QWidget(parent) {
    createUI();
}

int ModeShapePanel::currentModeIndex() const {
    return m_modeCombo->currentIndex();
}

void ModeShapePanel::createUI() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(6, 6, 6, 6);
    m_mainLayout->setSpacing(4);

    // Mode selection group
    auto* modeGroup = new QGroupBox("Mode Shape", this);
    auto* modeLayout = new QVBoxLayout(modeGroup);

    auto* comboRow = new QHBoxLayout();
    comboRow->addWidget(new QLabel("Mode:", this));
    m_modeCombo = new QComboBox(this);
    m_modeCombo->setEnabled(false);
    comboRow->addWidget(m_modeCombo, 1);
    modeLayout->addLayout(comboRow);

    m_frequencyLabel = new QLabel("-- Hz", this);
    m_frequencyLabel->setStyleSheet("font-weight: bold; color: #58a6ff;");
    modeLayout->addWidget(m_frequencyLabel);

    m_mainLayout->addWidget(modeGroup);

    // Animation controls group
    auto* animGroup = new QGroupBox("Animation", this);
    auto* animLayout = new QVBoxLayout(animGroup);

    auto* btnRow = new QHBoxLayout();
    m_playBtn = new QPushButton("Play", this);
    m_playBtn->setEnabled(false);
    m_resetBtn = new QPushButton("Reset", this);
    m_resetBtn->setEnabled(false);
    btnRow->addWidget(m_playBtn);
    btnRow->addWidget(m_resetBtn);
    animLayout->addLayout(btnRow);

    auto* ampRow = new QHBoxLayout();
    ampRow->addWidget(new QLabel("Amplitude:", this));
    m_amplitudeSlider = new QSlider(Qt::Horizontal, this);
    m_amplitudeSlider->setRange(1, 1000);
    m_amplitudeSlider->setValue(100);
    m_amplitudeSlider->setEnabled(false);
    ampRow->addWidget(m_amplitudeSlider, 1);
    m_amplitudeLabel = new QLabel("1.0x", this);
    m_amplitudeLabel->setFixedWidth(40);
    ampRow->addWidget(m_amplitudeLabel);
    animLayout->addLayout(ampRow);

    m_mainLayout->addWidget(animGroup);
    m_mainLayout->addStretch(1);

    // Connections
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ModeShapePanel::onModeComboChanged);
    connect(m_playBtn, &QPushButton::clicked,
            this, &ModeShapePanel::onPlayClicked);
    connect(m_resetBtn, &QPushButton::clicked,
            this, &ModeShapePanel::onResetClicked);
    connect(m_amplitudeSlider, &QSlider::valueChanged,
            this, &ModeShapePanel::onAmplitudeChanged);
}

void ModeShapePanel::setModalResult(const dynamics::ModalResult& result) {
    m_modalResult = result;
    m_hasResult = true;

    m_modeCombo->blockSignals(true);
    m_modeCombo->clear();
    for (int i = 0; i < result.num_modes; ++i) {
        QString label = QString("Mode %1 (%2 Hz)")
            .arg(i + 1)
            .arg(result.frequencies_hz[i], 0, 'f', 2);
        m_modeCombo->addItem(label);
    }
    m_modeCombo->setCurrentIndex(0);
    m_modeCombo->blockSignals(false);

    m_modeCombo->setEnabled(true);
    m_playBtn->setEnabled(true);
    m_resetBtn->setEnabled(true);
    m_amplitudeSlider->setEnabled(true);

    updateFrequencyLabel();
    emit modeChanged(0);
}

void ModeShapePanel::clear() {
    m_hasResult = false;
    m_isPlaying = false;
    m_modeCombo->clear();
    m_modeCombo->setEnabled(false);
    m_playBtn->setEnabled(false);
    m_resetBtn->setEnabled(false);
    m_amplitudeSlider->setEnabled(false);
    m_frequencyLabel->setText("-- Hz");
    m_playBtn->setText("Play");
}

void ModeShapePanel::onModeComboChanged(int index) {
    if (!m_hasResult || index < 0 || index >= m_modalResult.num_modes) return;
    updateFrequencyLabel();
    emit modeChanged(index);
}

void ModeShapePanel::onPlayClicked() {
    if (!m_hasResult) return;
    m_isPlaying = !m_isPlaying;
    m_playBtn->setText(m_isPlaying ? "Pause" : "Play");
    emit animationToggled(m_isPlaying);
}

void ModeShapePanel::onResetClicked() {
    m_isPlaying = false;
    m_playBtn->setText("Play");
    emit resetRequested();
}

void ModeShapePanel::onAmplitudeChanged(int value) {
    // Map 1-1000 to 0.1x-10.0x (logarithmic scale)
    double amp = 0.1 * std::exp(static_cast<double>(value) / 1000.0 * std::log(100.0));
    m_amplitudeLabel->setText(QString("%1x").arg(amp, 0, 'f', 1));
    emit amplitudeChanged(amp);
}

void ModeShapePanel::updateFrequencyLabel() {
    if (!m_hasResult) return;
    int idx = m_modeCombo->currentIndex();
    if (idx < 0 || idx >= m_modalResult.num_modes) return;

    double freq = m_modalResult.frequencies_hz[idx];
    double omega = m_modalResult.natural_frequencies[idx];

    m_frequencyLabel->setText(
        QString("f = %1 Hz (omega = %2 rad/s)")
            .arg(freq, 0, 'f', 4)
            .arg(omega, 0, 'g', 6));
}
