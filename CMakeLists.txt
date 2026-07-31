cmake_minimum_required(VERSION 3.15)
project(Cauchy VERSION 1.0.0 LANGUAGES CXX)

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

add_executable(FEA_Tests src/fea_test.cpp ${FEA_HEADERS})
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
