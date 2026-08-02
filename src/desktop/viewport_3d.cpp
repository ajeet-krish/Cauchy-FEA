#include "viewport_3d.hpp"
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrl>

Viewport3DWidget::Viewport3DWidget(QWidget* parent)
    : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_webView = new QWebEngineView(this);
    layout->addWidget(m_webView);

    m_webView->load(QUrl("qrc:///resources/fea3d_viewer.html"));
}

void Viewport3DWidget::setMeshAndResults(const Mesh& mesh, const fea::SolveResult& result) {
    m_mesh = mesh;
    m_result = result;
    m_hasData = true;
    postMeshData();
}

void Viewport3DWidget::setContourField(const QString& field) {
    m_contourField = field;
    if (m_hasData) postMeshData();
}

void Viewport3DWidget::setDisplacementScale(double scale) {
    m_dispScale = scale;
    if (m_hasData) postMeshData();
}

void Viewport3DWidget::resetView() {
    if (!m_hasData) return;
    m_webView->page()->runJavaScript("if(typeof fitView==='function')fitView();");
}

void Viewport3DWidget::postMeshData() {
    if (!m_hasData) return;

    int dofPerNode = DOF_PER_NODE;

    // Build nodes JSON
    QJsonArray nodesArr;
    for (int i = 0; i < m_mesh.num_nodes(); ++i) {
        QJsonObject nObj;
        nObj["x"] = m_mesh.nodes[i].x;
        nObj["y"] = m_mesh.nodes[i].y;
        nObj["z"] = m_mesh.nodes[i].z;
        nodesArr.append(nObj);
    }

    // Build elements JSON (H8 and T4)
    QJsonArray elementsArr;
    for (int e = 0; e < m_mesh.num_hexes(); ++e) {
        QJsonArray conn;
        for (int i = 0; i < 8; ++i) conn.append(m_mesh.hex_elements[e][i]);
        elementsArr.append(conn);
    }
    for (int e = 0; e < m_mesh.num_tets(); ++e) {
        QJsonArray conn;
        for (int i = 0; i < 4; ++i) conn.append(m_mesh.tet_elements[e][i]);
        elementsArr.append(conn);
    }

    // Build displacement JSON
    QJsonArray dispArr;
    for (int i = 0; i < m_mesh.num_nodes(); ++i) {
        QJsonObject dObj;
        dObj["ux"] = m_result.displacement[i * dofPerNode + 0];
        dObj["uy"] = m_result.displacement[i * dofPerNode + 1];
        dObj["uz"] = (dofPerNode >= 3) ? m_result.displacement[i * dofPerNode + 2] : 0.0;
        dispArr.append(dObj);
    }

    // Build stress JSON
    QJsonObject stressObj;
    QJsonArray vmArr, sxxArr, syyArr, szzArr, s1Arr, s2Arr;
    for (size_t e = 0; e < m_result.stresses.size(); ++e) {
        const auto& s = m_result.stresses[e];
        vmArr.append(s.von_mises);
        sxxArr.append(s.sigma_xx);
        syyArr.append(s.sigma_yy);
        szzArr.append(s.sigma_zz);
        s1Arr.append(s.sigma_1);
        s2Arr.append(s.sigma_2);
    }
    stressObj["von_mises"] = vmArr;
    stressObj["sigma_xx"] = sxxArr;
    stressObj["sigma_yy"] = syyArr;
    stressObj["sigma_zz"] = szzArr;
    stressObj["sigma_1"] = s1Arr;
    stressObj["sigma_2"] = s2Arr;

    // Build complete data object
    QJsonObject dataObj;
    dataObj["nodes"] = nodesArr;
    dataObj["elements"] = elementsArr;
    dataObj["displacement"] = dispArr;
    dataObj["stress"] = stressObj;
    dataObj["contourField"] = m_contourField;
    dataObj["dispScale"] = m_dispScale;

    // Wrap in message
    QJsonObject msg;
    msg["type"] = "setData";
    msg["data"] = dataObj;

    // Post to JavaScript
    QString js = QString("window.postMessage(%1);").arg(QJsonDocument(msg).toJson(QJsonDocument::Compact));
    m_webView->page()->runJavaScript(js);
}
