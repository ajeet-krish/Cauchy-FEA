#pragma once
#include <QApplication>

class CauchyApp : public QApplication {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(CauchyApp)
public:
    CauchyApp(int& argc, char** argv);
    ~CauchyApp() override = default;

private:
    void setupTheme();
};
