#pragma once
#include <QWidget>
#include <vector>
#include "fea.hpp"

class StressHistogram : public QWidget {
    Q_OBJECT
public:
    explicit StressHistogram(QWidget* parent = nullptr);

    void setData(const std::vector<postprocess::ElementStress>& stresses);
    void clear();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    std::vector<double> m_sigma_xx;
    std::vector<double> m_sigma_yy;
    std::vector<double> m_von_mises;
    bool m_hasData = false;

    static constexpr int NUM_BINS = 40;
};
