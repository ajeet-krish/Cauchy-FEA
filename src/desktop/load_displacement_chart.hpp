#pragma once
#include <QWidget>
#include <vector>

struct LDPoint {
    double force;
    double max_disp;
};

class LoadDisplacementChart : public QWidget {
    Q_OBJECT
public:
    explicit LoadDisplacementChart(QWidget* parent = nullptr);

    void addPoint(double force, double max_disp);
    void clear();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    std::vector<LDPoint> m_points;
};
