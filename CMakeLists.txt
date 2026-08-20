cmake_minimum_required(VERSION 3.15)
project(CrucibleFEA VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Homebrew libomp path on Apple Silicon
if(APPLE)
  set(OpenMP_ROOT "/opt/homebrew/opt/libomp")
endif()

find_package(OpenMP REQUIRED)

# ------------------------------------------------------------------
# Common include directory
# ------------------------------------------------------------------
set(FEA_HEADERS
  src/fea_types.hpp
  src/elements.hpp
  src/sparse.hpp
  src/solver.hpp
  src/mesh.hpp
  src/postprocess.hpp
  src/convergence.hpp
  src/adaptivity.hpp
  src/fea.hpp
)

# ------------------------------------------------------------------
# Validation case executables
# ------------------------------------------------------------------
add_executable(FEA_Cantilever src/main_cantilever.cpp ${FEA_HEADERS})
target_include_directories(FEA_Cantilever PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(FEA_Cantilever PRIVATE OpenMP::OpenMP_CXX)

add_executable(FEA_Michell src/main_michell.cpp ${FEA_HEADERS})
target_include_directories(FEA_Michell PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(FEA_Michell PRIVATE OpenMP::OpenMP_CXX)

add_executable(FEA_Cook src/main_cook.cpp ${FEA_HEADERS})
target_include_directories(FEA_Cook PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(FEA_Cook PRIVATE OpenMP::OpenMP_CXX)

add_executable(FEA_LBracket src/main_lbracket.cpp ${FEA_HEADERS})
target_include_directories(FEA_LBracket PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(FEA_LBracket PRIVATE OpenMP::OpenMP_CXX)

add_executable(FEA_Patch src/main_patch.cpp ${FEA_HEADERS})
target_include_directories(FEA_Patch PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(FEA_Patch PRIVATE OpenMP::OpenMP_CXX)

add_executable(FEA_PlateHole src/main_plate_hole.cpp ${FEA_HEADERS})
target_include_directories(FEA_PlateHole PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(FEA_PlateHole PRIVATE OpenMP::OpenMP_CXX)

add_executable(FEA_AdaptHole src/main_adapt_hole.cpp ${FEA_HEADERS})
target_include_directories(FEA_AdaptHole PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(FEA_AdaptHole PRIVATE OpenMP::OpenMP_CXX)

add_executable(FEA_ThermalCylinder src/main_thermal.cpp ${FEA_HEADERS})
target_include_directories(FEA_ThermalCylinder PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(FEA_ThermalCylinder PRIVATE OpenMP::OpenMP_CXX)

add_executable(FEA_Cantilever3D src/main_cantilever_3d.cpp ${FEA_HEADERS})
target_include_directories(FEA_Cantilever3D PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(FEA_Cantilever3D PRIVATE OpenMP::OpenMP_CXX)

add_executable(FEA_PlateHole3D src/main_plate_hole_3d.cpp ${FEA_HEADERS})
target_include_directories(FEA_PlateHole3D PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(FEA_PlateHole3D PRIVATE OpenMP::OpenMP_CXX)

add_executable(FEA_Lame3D src/main_lame_3d.cpp ${FEA_HEADERS})
target_include_directories(FEA_Lame3D PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(FEA_Lame3D PRIVATE OpenMP::OpenMP_CXX)

# ------------------------------------------------------------------
# Unit tests (Google Test)
# ------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(FEA_Tests
  src/fea_test.cpp
  ${FEA_HEADERS}
)
target_include_directories(FEA_Tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(FEA_Tests PRIVATE GTest::gtest_main OpenMP::OpenMP_CXX)

# ------------------------------------------------------------------
# Custom targets
# ------------------------------------------------------------------
add_custom_target(cantilever
  COMMAND FEA_Cantilever
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_custom_target(cook
  COMMAND FEA_Cook
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_custom_target(patch
  COMMAND FEA_Patch
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_custom_target(serve-docs
  COMMAND python3 -m http.server 8765 -d ${CMAKE_SOURCE_DIR}/docs
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# ------------------------------------------------------------------
# Desktop Application (Qt 6, optional)
# ------------------------------------------------------------------
option(CRUCIBLE_FEA_DESKTOP "Build the Qt 6 desktop FEA application" OFF)

if(CRUCIBLE_FEA_DESKTOP)
  find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGLWidgets WebEngineWidgets)

  set(CMAKE_AUTOMOC ON)
  set(CMAKE_AUTORCC ON)
  set(CMAKE_AUTOUIC ON)

  set(DESKTOP_SOURCES
    src/desktop/main.cpp
    src/desktop/cauchy_app.cpp
    src/desktop/main_window.cpp
    src/desktop/mesh_editor.cpp
    src/desktop/solver_panel.cpp
    src/desktop/viewport_widget.cpp
    src/desktop/viewport_3d.cpp
    src/desktop/result_model.cpp
    src/desktop/solver_runner.cpp
    src/desktop/project_io.cpp
    src/desktop/convergence_chart.cpp
    src/desktop/probe_tool.cpp
    src/desktop/mesh_quality_overlay.cpp
    src/desktop/about_dialog.cpp
    src/desktop/stress_histogram.cpp
    src/desktop/energy_balance_chart.cpp
    src/desktop/displacement_line_chart.cpp
    src/desktop/load_displacement_chart.cpp
    src/desktop/error_heatmap.cpp
    src/desktop/analytical_panel.cpp
    src/desktop/editor_state.cpp
    src/desktop/geometry_primitive.cpp
    src/desktop/geometry_model.cpp
    src/desktop/bc_model.cpp
    src/desktop/selection_model.cpp
    src/desktop/tool_context.cpp
    src/desktop/geometry_panel.cpp
    src/desktop/bc_panel.cpp
    src/desktop/mesh_generator.cpp
    src/desktop/undo_commands.cpp
    src/desktop/element_inspector_panel.cpp
    src/desktop/modal_runner.cpp
    src/desktop/mode_shape_panel.cpp
    src/desktop/parametric_panel.cpp
    src/desktop/parametric_engine.cpp
  )

  set(DESKTOP_HEADERS
    src/desktop/cauchy_app.hpp
    src/desktop/main_window.hpp
    src/desktop/mesh_editor.hpp
    src/desktop/solver_panel.hpp
    src/desktop/viewport_widget.hpp
    src/desktop/viewport_3d.hpp
    src/desktop/result_model.hpp
    src/desktop/solver_runner.hpp
    src/desktop/project_io.hpp
    src/desktop/convergence_chart.hpp
    src/desktop/probe_tool.hpp
    src/desktop/mesh_quality_overlay.hpp
    src/desktop/about_dialog.hpp
    src/desktop/stress_histogram.hpp
    src/desktop/energy_balance_chart.hpp
    src/desktop/displacement_line_chart.hpp
    src/desktop/load_displacement_chart.hpp
    src/desktop/error_heatmap.hpp
    src/desktop/analytical.hpp
    src/desktop/analytical_panel.hpp
    src/desktop/editor_state.hpp
    src/desktop/geometry_primitive.hpp
    src/desktop/geometry_model.hpp
    src/desktop/bc_model.hpp
    src/desktop/selection_model.hpp
    src/desktop/tool_context.hpp
    src/desktop/geometry_panel.hpp
    src/desktop/material_library.hpp
    src/desktop/bc_panel.hpp
    src/desktop/mesh_generator.hpp
    src/desktop/undo_commands.hpp
    src/desktop/element_inspector_panel.hpp
    src/desktop/modal_runner.hpp
    src/desktop/mode_shape_panel.hpp
    src/desktop/parametric_panel.hpp
    src/desktop/parametric_engine.hpp
  )

  set(DESKTOP_RESOURCES src/desktop/resources.qrc)

  add_executable(crucible-fea-desktop ${DESKTOP_SOURCES} ${DESKTOP_HEADERS} ${DESKTOP_RESOURCES})
  target_include_directories(crucible-fea-desktop PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(crucible-fea-desktop PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGLWidgets Qt6::WebEngineWidgets OpenMP::OpenMP_CXX
  )

  # macOS bundle
  if(APPLE)
    set_target_properties(crucible-fea-desktop PROPERTIES
      MACOSX_BUNDLE TRUE
      MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/desktop/Info.plist
      MACOSX_BUNDLE_BUNDLE_NAME "Crucible-FEA"
      MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
      MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
      MACOSX_BUNDLE_GUI_IDENTIFIER "com.crucible.fea2d"
      MACOSX_BUNDLE_ICON_FILE "icon.icns"
    )
    install(TARGETS crucible-fea-desktop BUNDLE DESTINATION /Applications)

    # Copy icon into the .app bundle Resources folder
    add_custom_command(TARGET crucible-fea-desktop POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/desktop/icon.icns
        $<TARGET_FILE_DIR:crucible-fea-desktop>/../Resources/icon.icns
      COMMENT "Copying app icon to bundle Resources"
    )
  endif()

  # Linux .desktop file
  if(UNIX AND NOT APPLE)
    install(FILES desktop/crucible-fea.desktop
      DESTINATION share/applications
    )
    install(TARGETS crucible-fea-desktop DESTINATION bin)
  endif()

  # Convenience targets
  add_custom_target(run-desktop
    COMMAND ./crucible-fea-desktop
    DEPENDS crucible-fea-desktop
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running Crucible-FEA desktop app"
  )

  add_custom_target(install-desktop
    COMMAND cmake --install ${CMAKE_BINARY_DIR} --prefix /Applications
    DEPENDS crucible-fea-desktop
    COMMENT "Installing Crucible-FEA to /Applications"
  )

  # ------------------------------------------------------------------
  # QOpenGLWidget Performance Benchmark
  # ------------------------------------------------------------------
  add_executable(FEA_GLBenchmark
    src/desktop/benchmark_main.cpp
    src/desktop/benchmark_gl.cpp
    ${FEA_HEADERS}
  )
  target_include_directories(FEA_GLBenchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(FEA_GLBenchmark PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGLWidgets OpenMP::OpenMP_CXX
  )

  add_custom_target(run-benchmark
    COMMAND FEA_GLBenchmark
    DEPENDS FEA_GLBenchmark
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running QOpenGLWidget performance benchmark"
  )
endif()
