#pragma once
#include <vector>
#include <QString>

struct MaterialPreset {
    QString name;
    double E;       // Pa
    double nu;
    double rho;     // kg/m3
    double t;       // m
    double alpha;   // 1/K
};

inline std::vector<MaterialPreset> getMaterialLibrary() {
    return {
        {"Steel (A36)",          200.0e9,  0.30, 7800.0, 0.01, 12.0e-6},
        {"Aluminum (6061-T6)",    68.9e9,  0.33, 2700.0, 0.01, 23.6e-6},
        {"Titanium (Ti-6Al-4V)", 113.8e9,  0.34, 4430.0, 0.01,  8.6e-6},
        {"Copper (C11000)",       117.0e9,  0.34, 8960.0, 0.01, 17.0e-6},
        {"Concrete (C30)",         30.0e9,  0.20, 2400.0, 0.01, 10.0e-6},
        {"Custom",                     0.0,  0.0,    0.0, 0.01,  0.0e-6},
    };
}
