#include <QApplication>
#include <QSurfaceFormat>
#include <QScreen>
#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>

#include "benchmark_gl.hpp"

// ==========================================================================
// FEA GPU Rendering Benchmark
// Validates QOpenGLWidget performance for FEA mesh rendering
// ==========================================================================

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // OpenGL 3.3 Core, vsync disabled for accurate measurement
    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    fmt.setSwapInterval(0);  // Disable vsync
    fmt.setDepthBufferSize(0);
    fmt.setStencilBufferSize(0);
    QSurfaceFormat::setDefaultFormat(fmt);

    // Create offscreen-capable widget
    GLBenchmarkWidget widget;
    widget.resize(1920, 1080);
    widget.show();

    // Process events to ensure OpenGL context is created
    QApplication::processEvents();

    // Define 8 benchmark scenarios
    std::vector<BenchmarkScenario> scenarios = {
        // 50K elements (250x200)
        {"50K edges-only",       250, 200, true,  false, false},
        {"50K filled-contours",  250, 200, false, true,  false},
        {"50K filled+edges",     250, 200, true,  true,  false},
        {"50K quality-overlay",  250, 200, true,  true,  true},

        // 100K elements (316x316)
        {"100K edges-only",      316, 316, true,  false, false},
        {"100K filled-contours", 316, 316, false, true,  false},
        {"100K filled+edges",    316, 316, true,  true,  false},
        {"100K quality-overlay", 316, 316, true,  true,  true},
    };

    std::vector<BenchmarkResult> results;
    int all_passed = 0;

    std::cout << "\n";
    std::cout << "=====================================================================\n";
    std::cout << "  Crucible-FEA QOpenGLWidget Performance Benchmark\n";
    std::cout << "  Window: 1920x1080, OpenGL 3.3 Core, VSync OFF\n";
    std::cout << "=====================================================================\n\n";

    for (const auto& scenario : scenarios) {
        std::cout << "Running: " << scenario.name.toStdString() << " ... " << std::flush;

        widget.loadScenario(scenario);
        QApplication::processEvents();

        BenchmarkResult result = widget.runBenchmark(100, 1000);
        results.push_back(result);

        if (result.passed) all_passed++;

        std::cout << "done\n";
    }

    // Print results table
    std::cout << "\n";
    std::cout << "=====================================================================\n";
    std::cout << "  RESULTS\n";
    std::cout << "=====================================================================\n\n";

    // Header
    std::cout << std::left
              << std::setw(24) << "Scenario"
              << std::right
              << std::setw(10) << "Elements"
              << std::setw(8) << "Nodes"
              << std::setw(12) << "Avg FPS"
              << std::setw(12) << "Min FPS"
              << std::setw(12) << "P95 FPS"
              << std::setw(12) << "Upload ms"
              << std::setw(12) << "Cold ms"
              << std::setw(8) << "Pass"
              << "\n";

    std::cout << std::string(110, '-') << "\n";

    for (const auto& r : results) {
        std::cout << std::left
                  << std::setw(24) << r.scenario_name.toStdString()
                  << std::right
                  << std::setw(10) << r.num_elements
                  << std::setw(8) << r.num_nodes
                  << std::setw(12) << std::fixed << std::setprecision(1) << r.avg_fps
                  << std::setw(12) << std::fixed << std::setprecision(1) << r.min_fps
                  << std::setw(12) << std::fixed << std::setprecision(1) << r.p95_fps
                  << std::setw(12) << std::fixed << std::setprecision(2) << r.mesh_upload_ms
                  << std::setw(12) << std::fixed << std::setprecision(1) << r.cold_start_ms
                  << std::setw(8) << (r.passed ? "YES" : "FAIL")
                  << "\n";
    }

    std::cout << "\n" << std::string(110, '=') << "\n";
    std::cout << "  Summary: " << all_passed << "/" << results.size() << " scenarios passed (avg FPS >= 30)\n";
    std::cout << std::string(110, '=') << "\n\n";

    return (all_passed == static_cast<int>(results.size())) ? 0 : 1;
}
