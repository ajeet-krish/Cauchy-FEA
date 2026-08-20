#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QElapsedTimer>
#include <memory>
#include <vector>

#include "fea_types.hpp"
#include "mesh.hpp"

struct BenchmarkScenario {
    QString name;
    int nx;
    int ny;
    bool show_edges;
    bool show_filled;
    bool show_quality;
};

struct BenchmarkResult {
    QString scenario_name;
    int num_elements;
    int num_nodes;
    double avg_fps;
    double min_fps;
    double p95_fps;
    double mesh_upload_ms;
    double color_upload_ms;
    double cold_start_ms;
    bool passed;
};

class GLBenchmarkWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core {
    Q_OBJECT
public:
    explicit GLBenchmarkWidget(QWidget* parent = nullptr);
    ~GLBenchmarkWidget() override;

    void loadScenario(const BenchmarkScenario& scenario);
    BenchmarkResult runBenchmark(int warmup_frames = 100, int measure_frames = 1000);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    // Mesh data (CPU side)
    Mesh m_mesh;
    std::vector<float> m_positions;
    std::vector<float> m_values;
    std::vector<unsigned int> m_tri_indices;
    std::vector<unsigned int> m_line_indices;
    int m_num_tri_indices = 0;
    int m_num_line_indices = 0;

    // OpenGL resources
    QOpenGLVertexArrayObject m_filled_vao;
    QOpenGLVertexArrayObject m_edge_vao;
    QOpenGLBuffer m_position_vbo{QOpenGLBuffer::VertexBuffer};
    QOpenGLBuffer m_value_vbo{QOpenGLBuffer::VertexBuffer};
    QOpenGLBuffer m_tri_ibo{QOpenGLBuffer::IndexBuffer};
    QOpenGLBuffer m_line_ibo{QOpenGLBuffer::IndexBuffer};
    std::unique_ptr<QOpenGLShaderProgram> m_shader;
    std::unique_ptr<QOpenGLTexture> m_colormap_texture;

    // Benchmark state
    BenchmarkScenario m_scenario;
    bool m_data_loaded = false;
    int m_frame_count = 0;
    std::vector<double> m_frame_times_ms;

    // Camera
    float m_pan_x = 0.5f;
    float m_pan_y = 0.5f;
    float m_zoom = 1.0f;

    // Timing
    double m_mesh_upload_ms = 0.0;
    double m_color_upload_ms = 0.0;
    double m_cold_start_ms = 0.0;
    QElapsedTimer m_frame_timer;

    // Internal methods
    void generateMeshData();
    void generateSyntheticStress();
    void generateQualityValues();
    void uploadMeshToGPU();
    void uploadColorsToGPU();
    void createColormapTexture();
    void createShaders();
    void createVAOs();
    QMatrix4x4 computeMVP() const;
};
