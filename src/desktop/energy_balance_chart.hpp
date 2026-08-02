#pragma once
#include <QWidget>
#include "fea.hpp"

struct EnergyBalanceData {
    double strain_energy = 0.0;
    double work_done = 0.0;
    bool valid = false;
};

class EnergyBalanceChart : public QWidget {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(EnergyBalanceChart)
public:
    explicit EnergyBalanceChart(QWidget* parent = nullptr);

    void setData(const EnergyBalanceData& data);
    void clear();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    EnergyBalanceData m_data;
};
