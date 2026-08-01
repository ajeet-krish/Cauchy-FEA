#pragma once
#include <QWidget>
#include <vector>
#include "fea.hpp"

struct DispLineSample {
    double coord;
    double value;
};

struct DispLineData {
    std::vector<DispLineSample> fea;
    std::vector<DispLineSample> analytical;
    QString axisLabel;
    QString valueLabel;
    bool valid = false;
};

class DisplacementLineChart : public QWidget {
    Q_OBJECT
public:
    explicit DisplacementLineChart(QWidget* parent = nullptr);

    void setData(const Mesh& mesh, const fea::SolveResult& result);
    void clear();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    DispLineData m_data;
};
