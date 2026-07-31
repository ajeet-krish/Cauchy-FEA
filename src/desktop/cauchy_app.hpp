#pragma once
#include <QApplication>

class CauchyApp : public QApplication {
    Q_OBJECT
public:
    CauchyApp(int& argc, char** argv);
    ~CauchyApp() override = default;

private:
    void setupTheme();
};
