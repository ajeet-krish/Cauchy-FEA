# Changelog

## [Unreleased]
### Added
- Qt Desktop Application structure in `src/desktop/`.
- CMake configuration for Qt 6 desktop application (`CAUCHY_DESKTOP` option).
- macOS `.app` bundle support (`MACOSX_BUNDLE TRUE` in CMake).
- Install target for macOS (`cmake --install` copies to /Applications).
- App icon (`desktop/icon.icns`) embedded in .app bundle Resources.
- `build-desktop.sh` one-command build + install + launch script.
- CMake convenience targets: `run-desktop`, `install-desktop`.
- **StressHistogram** plot widget: element stress distribution (sigma_xx, sigma_yy, von_mises).
- **EnergyBalanceChart** plot widget: strain energy vs work done comparison bar chart.
- **DisplacementLineChart** plot widget: displacement profile along mesh edge.
- **LoadDisplacementChart** plot widget: applied force vs max displacement (accumulates across solves).
- **ErrorHeatmap** plot widget: per-element ZZ error indicator as colored overlay with pan/zoom.
- **ConvergenceChart** wired into bottom dock tab widget.
- Bottom dock QTabWidget with 6 analysis plot tabs in main window.

### Fixed
- Fixed compilation errors in `src/desktop/` components:
  - Updated `project_io.cpp` to use array indexing for element connectivity instead of struct members.
  - Replaced deprecated `BoundaryCondition` with `DirichletBC`/`NeumannBC` types.
  - Removed non-existent `theta1`/`theta2` fields from `ElementStress` serialization and usage.
  - Resolved `QPushButton`, `QCheckBox`, `QSpinBox`, `QProgressBar`, `QTextEdit` missing header inclusions.
  - Fixed `ProbeTool` const-method issue.
  - Fixed `ResultModel` syntax error (extra closing brace).
  - Corrected `SolverPanel` layout management (using member `m_mainLayout`).
  - Fixed `QTextStream` formatting in `SolverPanel` using `QLocale`.
  - Fixed `QPixmap` to `QImage` conversion in `MainWindow`.
- Configured CMake `AUTOMOC`, `AUTORCC`, `AUTOUIC` for Qt meta-object processing.
