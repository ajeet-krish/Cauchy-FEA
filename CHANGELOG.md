# Changelog

## [Unreleased]
### Added
- Qt Desktop Application structure in `src/desktop/`.
- CMake configuration for Qt 6 desktop application (`CAUCHY_DESKTOP` option).

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
