#pragma once
#include <QWidget>
#include <QVBoxLayout>
#include "fea.hpp"

class QWebEngineView;

class Viewport3DWidget : public QWidget {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Viewport3DWidget)
public:
    explicit Viewport3DWidget(QWidget* parent = nullptr);
    ~Viewport3DWidget() override = default;

    void setMeshAndResults(const Mesh& mesh, const fea::SolveResult& result);
    void setContourField(const QString& field);
    void setDisplacementScale(double scale);
    void resetView();

signals:
    void pointProbed(int nodeId, double x, double y, double z);

private:
    QWebEngineView* m_webView = nullptr;
    Mesh m_mesh;
    fea::SolveResult m_result;
    bool m_hasData = false;
    QString m_contourField = "von_mises";
    double m_dispScale = 100.0;

    void postMeshData();
};
