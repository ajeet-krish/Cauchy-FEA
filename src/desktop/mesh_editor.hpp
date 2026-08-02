#pragma once
#include <QWidget>
#include <QVBoxLayout>
#include "fea_types.hpp"

class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QLabel;

class MeshEditor : public QWidget {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(MeshEditor)
public:
    explicit MeshEditor(QWidget* parent = nullptr);

    CaseType caseType() const;
    ElementType elementType() const;
    PlaneType planeType() const;
    int meshSizeX() const;
    int meshSizeY() const;
    int meshSizeZ() const;
    bool useQ8() const;
    bool is3D() const;
    double youngsModulus() const;
    double poissonsRatio() const;
    double thickness() const;

signals:
    void caseChanged();
    void materialChanged();

private:
    void createCaseControls();
    void createMaterialControls();
    void createMeshControls();

    QVBoxLayout* m_mainLayout = nullptr;
    QComboBox* m_caseCombo = nullptr;
    QComboBox* m_elemCombo = nullptr;
    QComboBox* m_planeCombo = nullptr;
    QSpinBox* m_nxSpin = nullptr;
    QSpinBox* m_nySpin = nullptr;
    QSpinBox* m_nzSpin = nullptr;
    QLabel* m_nzLabel = nullptr;
    QCheckBox* m_q8Check = nullptr;
    QDoubleSpinBox* m_ESpin = nullptr;
    QDoubleSpinBox* m_nuSpin = nullptr;
    QDoubleSpinBox* m_tSpin = nullptr;
};