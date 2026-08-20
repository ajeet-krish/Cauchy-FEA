#pragma once
#include <QWidget>
#include "dynamics.hpp"

class QComboBox;
class QLabel;
class QPushButton;
class QSlider;
class QVBoxLayout;

class ModeShapePanel : public QWidget {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ModeShapePanel)
public:
    explicit ModeShapePanel(QWidget* parent = nullptr);

public:
    int currentModeIndex() const;

public slots:
    void setModalResult(const dynamics::ModalResult& result);
    void clear();

signals:
    void modeChanged(int modeIndex);
    void animationToggled(bool playing);
    void amplitudeChanged(double amplitude);
    void resetRequested();

private slots:
    void onModeComboChanged(int index);
    void onPlayClicked();
    void onResetClicked();
    void onAmplitudeChanged(int value);

private:
    void createUI();
    void updateFrequencyLabel();

    QVBoxLayout* m_mainLayout = nullptr;
    QComboBox* m_modeCombo = nullptr;
    QLabel* m_frequencyLabel = nullptr;
    QLabel* m_amplitudeLabel = nullptr;
    QPushButton* m_playBtn = nullptr;
    QPushButton* m_resetBtn = nullptr;
    QSlider* m_amplitudeSlider = nullptr;

    dynamics::ModalResult m_modalResult;
    bool m_hasResult = false;
    bool m_isPlaying = false;
};
