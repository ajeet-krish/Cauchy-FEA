#pragma once
#include <QWidget>
#include <vector>

struct ConvergenceSample {
    double h;
    double value;
};

struct ConvergenceData {
    std::vector<ConvergenceSample> samples;
    double analytical = 0.0;
    double gci_fine = 0.0;
    double observed_order = 0.0;
    bool is_oscillatory = false;
    QString quantity;
    QString caseName;
};

class ConvergenceChart : public QWidget {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ConvergenceChart)
public:
    explicit ConvergenceChart(QWidget* parent = nullptr);

    void setData(const ConvergenceData& data);
    void clear();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    ConvergenceData m_data;
    bool m_hasData = false;
};