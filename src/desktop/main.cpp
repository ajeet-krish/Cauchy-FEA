#include <QApplication>
#include "cauchy_app.hpp"
#include "main_window.hpp"

int main(int argc, char* argv[]) {
    CauchyApp app(argc, argv);

    MainWindow window;
    window.show();

    return app.exec();
}
