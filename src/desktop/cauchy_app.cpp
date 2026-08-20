#include "cauchy_app.hpp"
#include <QPalette>
#include <QStyleFactory>
#include <QFont>

CauchyApp::CauchyApp(int& argc, char** argv)
    : QApplication(argc, argv) {
    setApplicationName("Crucible-FEA");
    setApplicationVersion("1.0.0");
    setOrganizationName("Crucible-FEA Structural Solver");

    setStyle(QStyleFactory::create("Fusion"));
    setupTheme();
}

void CauchyApp::setupTheme() {
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(0x0d, 0x11, 0x17));
    palette.setColor(QPalette::WindowText, QColor(0xc9, 0xd1, 0xd9));
    palette.setColor(QPalette::Base, QColor(0x16, 0x1b, 0x22));
    palette.setColor(QPalette::AlternateBase, QColor(0x21, 0x26, 0x2d));
    palette.setColor(QPalette::ToolTipBase, QColor(0x16, 0x1b, 0x22));
    palette.setColor(QPalette::ToolTipText, QColor(0xc9, 0xd1, 0xd9));
    palette.setColor(QPalette::Text, QColor(0xc9, 0xd1, 0xd9));
    palette.setColor(QPalette::Button, QColor(0x16, 0x1b, 0x22));
    palette.setColor(QPalette::ButtonText, QColor(0xc9, 0xd1, 0xd9));
    palette.setColor(QPalette::BrightText, QColor(0xff, 0xb3, 0x47));
    palette.setColor(QPalette::Link, QColor(0x00, 0xd4, 0xff));
    palette.setColor(QPalette::Highlight, QColor(0xcc, 0x8a, 0x2e));
    palette.setColor(QPalette::HighlightedText, QColor(0x0d, 0x11, 0x17));

    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x48, 0x4f, 0x58));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(0x48, 0x4f, 0x58));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x48, 0x4f, 0x58));

    setPalette(palette);

    // Global stylesheet for dark terminal aesthetic
    setStyleSheet(R"(
        QMainWindow {
            background-color: #0d1117;
        }
        QDockWidget {
            titlebar-close-icon: url();
            titlebar-normal-icon: url();
            color: #ffb347;
            font-weight: bold;
            font-family: 'JetBrains Mono', 'Fira Code', monospace;
        }
        QDockWidget::title {
            background: #161b22;
            padding: 6px;
            border-bottom: 1px solid #21262d;
        }
        QToolBar {
            background: #161b22;
            border-bottom: 1px solid #21262d;
            spacing: 4px;
            padding: 3px;
        }
        QToolButton {
            background: #21262d;
            color: #c9d1d9;
            border: 1px solid #30363d;
            border-radius: 4px;
            padding: 4px 8px;
            font-family: 'JetBrains Mono', 'Fira Code', monospace;
            font-size: 12px;
        }
        QToolButton:hover {
            background: #30363d;
            border-color: #ffb347;
            color: #ffb347;
        }
        QToolButton:pressed {
            background: #cc8a2e;
            color: #0d1117;
        }
        QStatusBar {
            background: #161b22;
            color: #8b949e;
            border-top: 1px solid #21262d;
            font-family: 'JetBrains Mono', 'Fira Code', monospace;
            font-size: 11px;
        }
        QMenuBar {
            background: #161b22;
            color: #c9d1d9;
            border-bottom: 1px solid #21262d;
            font-family: 'JetBrains Mono', 'Fira Code', monospace;
        }
        QMenuBar::item:selected {
            background: #21262d;
            color: #ffb347;
        }
        QMenu {
            background: #161b22;
            color: #c9d1d9;
            border: 1px solid #30363d;
            font-family: 'JetBrains Mono', 'Fira Code', monospace;
        }
        QMenu::item:selected {
            background: #21262d;
            color: #ffb347;
        }
        QGroupBox {
            border: 1px solid #30363d;
            border-radius: 4px;
            margin-top: 12px;
            font-family: 'JetBrains Mono', 'Fira Code', monospace;
            font-weight: bold;
            color: #ffb347;
            font-size: 12px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            padding: 0 4px;
        }
        QPushButton {
            background-color: #21262d;
            color: #c9d1d9;
            border: 1px solid #30363d;
            border-radius: 4px;
            padding: 6px 12px;
            font-family: 'JetBrains Mono', 'Fira Code', monospace;
            font-size: 12px;
            font-weight: 500;
        }
        QPushButton:hover {
            background-color: #30363d;
            border-color: #ffb347;
            color: #ffb347;
        }
        QPushButton:pressed {
            background-color: #cc8a2e;
            color: #0d1117;
        }
        QPushButton:disabled {
            background-color: #161b22;
            color: #484f58;
            border-color: #21262d;
        }
        QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
            background: #0d1117;
            color: #c9d1d9;
            border: 1px solid #30363d;
            border-radius: 4px;
            padding: 4px;
            font-family: 'JetBrains Mono', 'Fira Code', monospace;
            font-size: 12px;
        }
        QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {
            border-color: #00d4ff;
        }
        QTabWidget::pane {
            border: 1px solid #21262d;
            background: #161b22;
        }
        QTabBar::tab {
            background: #161b22;
            color: #8b949e;
            border: 1px solid #21262d;
            padding: 6px 12px;
            font-family: 'JetBrains Mono', 'Fira Code', monospace;
            font-size: 11px;
        }
        QTabBar::tab:selected {
            background: #21262d;
            color: #ffb347;
            border-bottom: 2px solid #ffb347;
        }
    )");
}
