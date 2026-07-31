#include "about_dialog.hpp"
#include <QLabel>
#include <QVBoxLayout>
#include <QPushButton>

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("About Cauchy FEA");
    setModal(true);
    resize(400, 300);

    auto* layout = new QVBoxLayout(this);

    auto* title = new QLabel("<h2>Cauchy FEA</h2>", this);
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    auto* version = new QLabel("Version 1.0.0", this);
    version->setAlignment(Qt::AlignCenter);
    layout->addWidget(version);

    auto* desc = new QLabel(
        "<p>A 2D finite element structural solver for plane "
        "stress and plane strain problems.</p>"
        "<p>Supports Bar, Q4 (bilinear quad), Q8 (serendipity quad), "
        "and T3 (triangle) elements.</p>"
        "<p>Features:</p>"
        "<ul>"
        "<li>Cholesky direct solver (small/medium systems)</li>"
        "<li>Conjugate Gradient iterative solver (large sparse systems)</li>"
        "<li>Von Mises stress recovery with principal stress vectors</li>"
        "<li>ZZ error estimator-driven adaptive h-refinement</li>"
        "<li>6 validation cases with analytical benchmarks</li>"
        "<li>Mesh convergence studies with GCI</li>"
        "</ul>",
        this);
    layout->addWidget(desc);

    auto* license = new QLabel(
        "<p>MIT License -- Portfolio project for mechanical/aerospace "
        "engineering roles.</p>",
        this);
    license->setAlignment(Qt::AlignCenter);
    layout->addWidget(license);

    auto* closeBtn = new QPushButton("Close", this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    layout->addWidget(closeBtn);
}