#include "benchmark_gl.hpp"
#include <QPainter>
#include <QElapsedTimer>
#include <cmath>
#include <algorithm>
#include <numeric>

// ==========================================================================
// GLBenchmarkWidget -- QOpenGLWidget GPU rendering benchmark
// ==========================================================================

// ------------------------------------------------------------------
// Turbo colormap as 1D texture (matching ViewportWidget algorithm)
// ------------------------------------------------------------------
static const int COLORMAP_SIZE = 256;

static void generateTurboColormap(unsigned char* rgba) {
    for (int i = 0; i < COLORMAP_SIZE; ++i) {
        double t = static_cast<double>(i) / (COLORMAP_SIZE - 1);
        double r = 0.0, g = 0.0, b = 0.0;

        if (t < 0.25) {
            double s = t / 0.25;
            r = 0.19 * s; g = 0.33 * s; b = 0.61 + 0.19 * s;
        } else if (t < 0.5) {
            double s = (t - 0.25) / 0.25;
            r = 0.19 + 0.76 * s; g = 0.33 + 0.65 * s; b = 0.80 - 0.59 * s;
        } else if (t < 0.75) {
            double s = (t - 0.5) / 0.25;
            r = 0.95 - 0.03 * s; g = 0.98 - 0.58 * s; b = 0.21 - 0.15 * s;
        } else {
            double s = (t - 0.75) / 0.25;
            r = 0.92 + 0.08 * s; g = 0.40 - 0.37 * s; b = 0.06 - 0.06 * s;
        }

        rgba[i * 4 + 0] = static_cast<unsigned char>(r * 255.0);
        rgba[i * 4 + 1] = static_cast<unsigned char>(g * 255.0);
        rgba[i * 4 + 2] = static_cast<unsigned char>(b * 255.0);
        rgba[i * 4 + 3] = 255;
    }
}

// ------------------------------------------------------------------
// Vertex shader
// ------------------------------------------------------------------
static const char* vertexShaderSrc = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in float aValue;
uniform mat4 uMVP;
uniform float uValueMin;
uniform float uValueRange;
out float vTexCoord;
void main() {
    gl_Position = uMVP * vec4(aPos, 0.0, 1.0);
    float t = 0.0;
    if (uValueRange > 1e-10) {
        t = clamp((aValue - uValueMin) / uValueRange, 0.0, 1.0);
    }
    vTexCoord = t;
}
)";

// ------------------------------------------------------------------
// Fragment shader with colormap lookup and optional solid-color override
// ------------------------------------------------------------------
static const char* fragmentShaderColormapSrc = R"(
#version 330 core
in float vTexCoord;
uniform sampler1D uColormap;
uniform bool u_use_override;
uniform vec4 u_override_color;
out vec4 FragColor;
void main() {
    if (u_use_override) {
        FragColor = u_override_color;
    } else {
        FragColor = texture(uColormap, vTexCoord);
    }
}
)";

// ==================================================================
// GLBenchmarkWidget implementation
// ==================================================================

GLBenchmarkWidget::GLBenchmarkWidget(QWidget* parent)
    : QOpenGLWidget(parent) {
    setFixedSize(1920, 1080);
}

GLBenchmarkWidget::~GLBenchmarkWidget() {
    makeCurrent();
    m_tri_ibo.destroy();
    m_line_ibo.destroy();
    m_position_vbo.destroy();
    m_value_vbo.destroy();
    m_filled_vao.destroy();
    m_edge_vao.destroy();
    delete m_shader;
    delete m_colormap_texture;
    doneCurrent();
}

// ------------------------------------------------------------------
// Mesh generation from scenario parameters
// ------------------------------------------------------------------
void GLBenchmarkWidget::generateMeshData() {
    m_mesh = mesh::generate_structured_quad(1.0, 1.0, m_scenario.nx, m_scenario.ny);
    int num_nodes = m_mesh.num_nodes();
    int num_quads = m_mesh.num_quads();

    // Use size_t to avoid integer overflow for large meshes
    auto pos_count = static_cast<size_t>(num_nodes) * 2;
    auto tri_count = static_cast<size_t>(num_quads) * 6;
    auto line_count = static_cast<size_t>(num_quads) * 8;
    if (pos_count > 100000000 || tri_count > 200000000 || line_count > 200000000) {
        qWarning() << "Mesh too large for GPU buffers";
        return;
    }

    // Position buffer: x, y per node (shared vertices)
    m_positions.resize(pos_count);
    for (int i = 0; i < num_nodes; ++i) {
        m_positions[i * 2 + 0] = static_cast<float>(m_mesh.nodes[i].x);
        m_positions[i * 2 + 1] = static_cast<float>(m_mesh.nodes[i].y);
    }

    // Triangle indices: 2 tris per quad (CCW winding)
    m_tri_indices.resize(tri_count);
    for (int e = 0; e < num_quads; ++e) {
        const auto& elem = m_mesh.quad_elements[e];
        int base = e * 6;
        // Tri 1: n0, n1, n2
        m_tri_indices[base + 0] = static_cast<unsigned int>(elem[0]);
        m_tri_indices[base + 1] = static_cast<unsigned int>(elem[1]);
        m_tri_indices[base + 2] = static_cast<unsigned int>(elem[2]);
        // Tri 2: n0, n2, n3
        m_tri_indices[base + 3] = static_cast<unsigned int>(elem[0]);
        m_tri_indices[base + 4] = static_cast<unsigned int>(elem[2]);
        m_tri_indices[base + 5] = static_cast<unsigned int>(elem[3]);
    }
    m_num_tri_indices = num_quads * 6;

    // Line indices: 4 edges per quad (with duplicate removal via shared VBO)
    // Each quad has 4 edges: n0-n1, n1-n2, n2-n3, n3-n0
    m_line_indices.resize(line_count);
    for (int e = 0; e < num_quads; ++e) {
        const auto& elem = m_mesh.quad_elements[e];
        int base = e * 8;
        m_line_indices[base + 0] = static_cast<unsigned int>(elem[0]);
        m_line_indices[base + 1] = static_cast<unsigned int>(elem[1]);
        m_line_indices[base + 2] = static_cast<unsigned int>(elem[1]);
        m_line_indices[base + 3] = static_cast<unsigned int>(elem[2]);
        m_line_indices[base + 4] = static_cast<unsigned int>(elem[2]);
        m_line_indices[base + 5] = static_cast<unsigned int>(elem[3]);
        m_line_indices[base + 6] = static_cast<unsigned int>(elem[3]);
        m_line_indices[base + 7] = static_cast<unsigned int>(elem[0]);
    }
    m_num_line_indices = num_quads * 8;
}

// ------------------------------------------------------------------
// Synthetic stress field: sinusoidal pattern normalized 0-1
// ------------------------------------------------------------------
void GLBenchmarkWidget::generateSyntheticStress() {
    int num_nodes = m_mesh.num_nodes();
    m_values.resize(num_nodes);
    for (int i = 0; i < num_nodes; ++i) {
        double x = m_mesh.nodes[i].x;
        double y = m_mesh.nodes[i].y;
        m_values[i] = static_cast<float>(0.5 + 0.5 * std::sin(2.0 * M_PI * x) * std::cos(2.0 * M_PI * y));
    }
}

// ------------------------------------------------------------------
// Quality overlay: average Jacobian ratio per node
// ------------------------------------------------------------------
void GLBenchmarkWidget::generateQualityValues() {
    int num_nodes = m_mesh.num_nodes();
    m_values.resize(num_nodes, 1.0f);

    // Accumulate element quality to nodes
    std::vector<double> quality_sum(num_nodes, 0.0);
    std::vector<int> quality_count(num_nodes, 0);

    for (int e = 0; e < m_mesh.num_quads(); ++e) {
        const auto& elem = m_mesh.quad_elements[e];
        std::array<Node, 4> elem_nodes;
        for (int i = 0; i < 4; ++i) elem_nodes[i] = m_mesh.nodes[elem[i]];

        auto q = mesh::compute_q4_quality(elem_nodes);
        double val = q.jacobian_ratio;

        for (int i = 0; i < 4; ++i) {
            quality_sum[elem[i]] += val;
            quality_count[elem[i]]++;
        }
    }

    for (int i = 0; i < num_nodes; ++i) {
        if (quality_count[i] > 0) {
            m_values[i] = static_cast<float>(quality_sum[i] / quality_count[i]);
        }
    }
}

// ------------------------------------------------------------------
// Upload mesh data to GPU
// ------------------------------------------------------------------
void GLBenchmarkWidget::uploadMeshToGPU() {
    QElapsedTimer timer;
    timer.start();

    // Position VBO
    m_position_vbo.bind();
    m_position_vbo.allocate(m_positions.data(), static_cast<int>(m_positions.size() * sizeof(float)));
    m_position_vbo.release();

    // Triangle IBO
    m_tri_ibo.bind();
    m_tri_ibo.allocate(m_tri_indices.data(), static_cast<int>(m_tri_indices.size() * sizeof(unsigned int)));
    m_tri_ibo.release();

    // Line IBO
    m_line_ibo.bind();
    m_line_ibo.allocate(m_line_indices.data(), static_cast<int>(m_line_indices.size() * sizeof(unsigned int)));
    m_line_ibo.release();

    m_mesh_upload_ms = timer.elapsed();
}

// ------------------------------------------------------------------
// Upload color data to GPU
// ------------------------------------------------------------------
void GLBenchmarkWidget::uploadColorsToGPU() {
    QElapsedTimer timer;
    timer.start();

    m_value_vbo.bind();
    m_value_vbo.allocate(m_values.data(), static_cast<int>(m_values.size() * sizeof(float)));
    m_value_vbo.release();

    m_color_upload_ms = timer.elapsed();
}

// ------------------------------------------------------------------
// Create colormap texture
// ------------------------------------------------------------------
void GLBenchmarkWidget::createColormapTexture() {
    unsigned char rgba[COLORMAP_SIZE * 4];
    generateTurboColormap(rgba);

    m_colormap_texture = new QOpenGLTexture(QOpenGLTexture::Target1D);
    m_colormap_texture->create();
    m_colormap_texture->setSize(COLORMAP_SIZE, 1, 1);
    m_colormap_texture->setFormat(QOpenGLTexture::RGBA8_UNorm);
    m_colormap_texture->allocateStorage();
    m_colormap_texture->setData(0, 0, 0, COLORMAP_SIZE, 1, 1,
                                QOpenGLTexture::RGBA, QOpenGLTexture::UInt8, rgba);
    m_colormap_texture->setMinificationFilter(QOpenGLTexture::Linear);
    m_colormap_texture->setMagnificationFilter(QOpenGLTexture::Linear);
    m_colormap_texture->setWrapMode(QOpenGLTexture::ClampToEdge);
}

// ------------------------------------------------------------------
// Compile shaders
// ------------------------------------------------------------------
void GLBenchmarkWidget::createShaders() {
    m_shader = new QOpenGLShaderProgram(this);

    // Vertex shader
    m_shader->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSrc);

    // We need both fragment shaders; create two programs
    // For simplicity, use the colormap fragment shader as default
    // and switch uniform for solid color mode
    m_shader->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderColormapSrc);

    m_shader->link();
    if (!m_shader->isLinked()) {
        qWarning() << "Shader link failed:" << m_shader->log();
        delete m_shader;
        m_shader = nullptr;
        return;
    }
}

// ------------------------------------------------------------------
// Create VAOs for filled and edge passes
// ------------------------------------------------------------------
void GLBenchmarkWidget::createVAOs() {
    if (!m_shader) return;

    // Filled VAO (triangles with colormap)
    m_filled_vao.create();
    m_filled_vao.bind();

    m_position_vbo.bind();
    m_shader->enableAttributeArray(0);
    m_shader->setAttributeBuffer(0, GL_FLOAT, 0, 2, sizeof(float) * 2);
    m_position_vbo.release();

    m_value_vbo.bind();
    m_shader->enableAttributeArray(1);
    m_shader->setAttributeBuffer(1, GL_FLOAT, 0, 1, sizeof(float));
    m_value_vbo.release();

    m_tri_ibo.bind();

    m_filled_vao.release();

    // Edge VAO (lines with solid color)
    m_edge_vao.create();
    m_edge_vao.bind();

    m_position_vbo.bind();
    m_shader->enableAttributeArray(0);
    m_shader->setAttributeBuffer(0, GL_FLOAT, 0, 2, sizeof(float) * 2);
    m_position_vbo.release();

    // No value attribute needed for edges, but layout requires it
    // Bind dummy
    m_value_vbo.bind();
    m_shader->enableAttributeArray(1);
    m_shader->setAttributeBuffer(1, GL_FLOAT, 0, 1, sizeof(float));
    m_value_vbo.release();

    m_line_ibo.bind();

    m_edge_vao.release();
}

// ------------------------------------------------------------------
// Compute model-view-projection matrix (orthographic)
// ------------------------------------------------------------------
QMatrix4x4 GLBenchmarkWidget::computeMVP() const {
    float half_range = 0.6f / m_zoom;
    float aspect = (height() > 0) ? static_cast<float>(width()) / static_cast<float>(height()) : 1.0f;
    float left   = m_pan_x - half_range * aspect;
    float right  = m_pan_x + half_range * aspect;
    float bottom = m_pan_y - half_range;
    float top    = m_pan_y + half_range;

    QMatrix4x4 proj;
    proj.ortho(left, right, bottom, top, -1.0f, 1.0f);
    return proj;
}

// ------------------------------------------------------------------
// OpenGL initialization
// ------------------------------------------------------------------
void GLBenchmarkWidget::initializeGL() {
    QElapsedTimer timer;
    timer.start();

    initializeOpenGLFunctions();

    glClearColor(0.05f, 0.06f, 0.09f, 1.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    createShaders();
    createColormapTexture();

    // Create GPU buffers before VAO setup
    m_position_vbo.create();
    m_value_vbo.create();
    m_tri_ibo.create();
    m_line_ibo.create();

    createVAOs();

    m_cold_start_ms = timer.elapsed();
}

// ------------------------------------------------------------------
// Resize
// ------------------------------------------------------------------
void GLBenchmarkWidget::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
}

// ------------------------------------------------------------------
// Paint: auto-pan animation + mesh rendering
// ------------------------------------------------------------------
void GLBenchmarkWidget::paintGL() {
    // Auto-pan: sinusoidal, 10s period
    m_frame_count++;
    double t = static_cast<double>(m_frame_count) / 600.0 * (2.0 * M_PI);
    m_pan_x = 0.5f + 0.1f * static_cast<float>(std::sin(t));
    m_pan_y = 0.5f + 0.1f * static_cast<float>(std::cos(t * 0.7));

    glClear(GL_COLOR_BUFFER_BIT);

    if (!m_data_loaded || !m_shader) return;

    m_shader->bind();

    QMatrix4x4 mvp = computeMVP();
    m_shader->setUniformValue("uMVP", mvp);

    float v_min = 0.0f;
    float v_max = 1.0f;
    if (!m_values.empty()) {
        v_min = *std::min_element(m_values.begin(), m_values.end());
        v_max = *std::max_element(m_values.begin(), m_values.end());
    }
    m_shader->setUniformValue("uValueMin", v_min);
    m_shader->setUniformValue("uValueRange", v_max - v_min);

    // Bind colormap texture
    if (m_colormap_texture) {
        m_colormap_texture->bind(0);
        m_shader->setUniformValue("uColormap", 0);
    }

    // Pass 1: Filled contours
    if (m_scenario.show_filled) {
        m_shader->setUniformValue("u_use_override", false);
        m_filled_vao.bind();
        glDrawElements(GL_TRIANGLES, m_num_tri_indices, GL_UNSIGNED_INT, nullptr);
        m_filled_vao.release();
    }

    // Pass 2: Edges
    if (m_scenario.show_edges) {
        // Override colormap with semi-transparent white for edges
        m_shader->setUniformValue("u_use_override", true);
        m_shader->setUniformValue("u_override_color", QVector4D(1.0f, 1.0f, 1.0f, 0.15f));
        m_edge_vao.bind();
        glLineWidth(1.0f);
        glDrawElements(GL_LINES, m_num_line_indices, GL_UNSIGNED_INT, nullptr);
        m_edge_vao.release();
    }

    m_shader->release();
}

// ------------------------------------------------------------------
// Load a scenario and upload to GPU
// ------------------------------------------------------------------
void GLBenchmarkWidget::loadScenario(const BenchmarkScenario& scenario) {
    makeCurrent();

    if (scenario.nx <= 0 || scenario.ny <= 0 || scenario.nx > 10000 || scenario.ny > 10000) {
        qWarning() << "Invalid mesh dimensions:" << scenario.nx << "x" << scenario.ny;
        doneCurrent();
        return;
    }

    m_scenario = scenario;
    m_frame_count = 0;
    m_frame_times_ms.clear();

    generateMeshData();

    if (m_scenario.show_quality) {
        generateQualityValues();
    } else {
        generateSyntheticStress();
    }

    uploadMeshToGPU();
    uploadColorsToGPU();

    m_data_loaded = true;
    doneCurrent();
}

// ------------------------------------------------------------------
// Run benchmark: warmup + measurement frames
// ------------------------------------------------------------------
BenchmarkResult GLBenchmarkWidget::runBenchmark(int warmup_frames, int measure_frames) {
    BenchmarkResult result;
    result.scenario_name = m_scenario.name;
    result.num_elements = m_mesh.num_quads();
    result.num_nodes = m_mesh.num_nodes();
    result.mesh_upload_ms = m_mesh_upload_ms;
    result.color_upload_ms = m_color_upload_ms;
    result.cold_start_ms = m_cold_start_ms;

    // Ensure the OpenGL context is current for manual paintGL calls
    makeCurrent();

    m_frame_times_ms.clear();
    m_frame_count = 0;

    // Warmup: render frames to stabilize GPU/caches
    QElapsedTimer warmup_timer;
    warmup_timer.start();
    for (int i = 0; i < warmup_frames; ++i) {
        paintGL();
        glFinish();
    }
    double warmup_total_ms = warmup_timer.elapsed();
    (void)warmup_total_ms;

    // Reset for measurement
    m_frame_times_ms.clear();
    m_frame_count = 0;

    // Measurement: render frames and record per-frame times
    QElapsedTimer total_timer;
    total_timer.start();

    for (int i = 0; i < measure_frames; ++i) {
        QElapsedTimer frame_timer;
        frame_timer.start();

        paintGL();
        glFinish();

        m_frame_times_ms.push_back(frame_timer.elapsed());
    }

    double total_ms = total_timer.elapsed();

    // Release GL context
    doneCurrent();

    // Compute FPS statistics from frame times
    if (!m_frame_times_ms.empty()) {
        // Convert frame times to FPS
        std::vector<double> fps_values;
        fps_values.reserve(m_frame_times_ms.size());
        for (double ft : m_frame_times_ms) {
            if (ft > 0.0) {
                fps_values.push_back(1000.0 / ft);
            } else {
                // Sub-millisecond frame; use total time for accurate FPS
                fps_values.push_back(1000.0 * measure_frames / total_ms);
            }
        }

        if (!fps_values.empty()) {
            result.avg_fps = std::accumulate(fps_values.begin(), fps_values.end(), 0.0) / fps_values.size();
            result.min_fps = *std::min_element(fps_values.begin(), fps_values.end());

            // P95 (95th percentile): sort ascending, take 5th percentile
            std::sort(fps_values.begin(), fps_values.end());
            int p95_idx = static_cast<int>(fps_values.size() * 0.05);
            result.p95_fps = fps_values[p95_idx];
        }
    } else {
        // Fallback: compute from total time
        result.avg_fps = static_cast<double>(measure_frames) / (total_ms / 1000.0);
        result.min_fps = result.avg_fps;
        result.p95_fps = result.avg_fps;
    }

    result.passed = (result.avg_fps >= 30.0);
    return result;
}
