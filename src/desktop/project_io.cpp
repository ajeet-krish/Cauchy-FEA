#include "project_io.hpp"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QDir>

bool ProjectIO::save(const QString& filePath, const ProjectConfig& config) {
    QJsonObject root;

    // Solver config
    QJsonObject cfgObj;
    cfgObj["case_type"] = static_cast<int>(config.solverConfig.case_type);
    cfgObj["element_type"] = static_cast<int>(config.solverConfig.element_type);
    cfgObj["plane_type"] = static_cast<int>(config.solverConfig.plane_type);
    cfgObj["nx"] = config.solverConfig.nx;
    cfgObj["ny"] = config.solverConfig.ny;
    cfgObj["use_q8"] = config.solverConfig.use_q8;
    cfgObj["use_cg"] = config.solverConfig.use_cg;
    cfgObj["E"] = config.solverConfig.E;
    cfgObj["nu"] = config.solverConfig.nu;
    cfgObj["t"] = config.solverConfig.t;
    root["solver_config"] = cfgObj;

    // Mesh nodes
    QJsonArray nodesArr;
    for (const auto& node : config.mesh.nodes) {
        QJsonObject n;
        n["x"] = node.x;
        n["y"] = node.y;
        nodesArr.append(n);
    }
    root["nodes"] = nodesArr;

    // Mesh elements (Q4)
    QJsonArray quadArr;
    for (const auto& elem : config.mesh.quad_elements) {
        QJsonObject e;
        e["n0"] = elem[0];
        e["n1"] = elem[1];
        e["n2"] = elem[2];
        e["n3"] = elem[3];
        quadArr.append(e);
    }
    root["quad_elements"] = quadArr;

    // Mesh elements (T3)
    QJsonArray triArr;
    for (const auto& elem : config.mesh.tri_elements) {
        QJsonObject e;
        e["n0"] = elem[0];
        e["n1"] = elem[1];
        e["n2"] = elem[2];
        triArr.append(e);
    }
    root["tri_elements"] = triArr;

    // Dirichlet BCs
    QJsonArray dirArr;
    for (const auto& bc : config.mesh.dirichlet) {
        QJsonObject b;
        b["node"] = bc.node;
        b["dof"] = bc.dof;
        b["value"] = bc.value;
        dirArr.append(b);
    }
    root["dirichlet"] = dirArr;

    // Neumann BCs
    QJsonArray neumArr;
    for (const auto& bc : config.mesh.neumann) {
        QJsonObject b;
        b["node"] = bc.node;
        b["dof"] = bc.dof;
        b["value"] = bc.value;
        neumArr.append(b);
    }
    root["neumann"] = neumArr;

    // Results
    QJsonArray dispArr;
    for (double d : config.result.displacement) {
        dispArr.append(d);
    }
    root["displacement"] = dispArr;

    QJsonArray stressArr;
    for (const auto& s : config.result.stresses) {
        QJsonObject st;
        st["von_mises"] = s.von_mises;
        st["sigma_xx"] = s.sigma_xx;
        st["sigma_yy"] = s.sigma_yy;
        st["sigma_xy"] = s.sigma_xy;
        st["sigma_1"] = s.sigma_1;
        st["sigma_2"] = s.sigma_2;
        stressArr.append(st);
    }
    root["stresses"] = stressArr;

    root["cg_iterations"] = config.result.cg_iterations;
    root["solve_time_ms"] = config.result.solve_time_ms;
    root["cg_converged"] = config.result.cg_converged;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

bool ProjectIO::load(const QString& filePath, ProjectConfig& config) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) {
        return false;
    }

    QJsonObject root = doc.object();

    // Solver config
    if (root.contains("solver_config") && root["solver_config"].isObject()) {
        QJsonObject cfgObj = root["solver_config"].toObject();
        config.solverConfig.case_type = static_cast<CaseType>(cfgObj["case_type"].toInt());
        config.solverConfig.element_type = static_cast<ElementType>(cfgObj["element_type"].toInt());
        config.solverConfig.plane_type = static_cast<PlaneType>(cfgObj["plane_type"].toInt());
        config.solverConfig.nx = cfgObj["nx"].toInt();
        config.solverConfig.ny = cfgObj["ny"].toInt();
        config.solverConfig.use_q8 = cfgObj["use_q8"].toBool();
        config.solverConfig.use_cg = cfgObj["use_cg"].toBool();
        config.solverConfig.E = cfgObj["E"].toDouble();
        config.solverConfig.nu = cfgObj["nu"].toDouble();
        config.solverConfig.t = cfgObj["t"].toDouble();
    }

    // Mesh nodes
    if (root.contains("nodes") && root["nodes"].isArray()) {
        config.mesh.nodes.clear();
        for (const auto& v : root["nodes"].toArray()) {
            QJsonObject n = v.toObject();
            Node node;
            node.x = n["x"].toDouble();
            node.y = n["y"].toDouble();
            config.mesh.nodes.push_back(node);
        }
    }

    // Q4 elements
    if (root.contains("quad_elements") && root["quad_elements"].isArray()) {
        config.mesh.quad_elements.clear();
        for (const auto& v : root["quad_elements"].toArray()) {
            QJsonObject e = v.toObject();
            config.mesh.quad_elements.push_back(
                {e["n0"].toInt(), e["n1"].toInt(), e["n2"].toInt(), e["n3"].toInt()});
        }
    }

    // T3 elements
    if (root.contains("tri_elements") && root["tri_elements"].isArray()) {
        config.mesh.tri_elements.clear();
        for (const auto& v : root["tri_elements"].toArray()) {
            QJsonObject e = v.toObject();
            config.mesh.tri_elements.push_back(
                {e["n0"].toInt(), e["n1"].toInt(), e["n2"].toInt()});
        }
    }

    // Dirichlet BCs
    if (root.contains("dirichlet") && root["dirichlet"].isArray()) {
        config.mesh.dirichlet.clear();
        for (const auto& v : root["dirichlet"].toArray()) {
            QJsonObject b = v.toObject();
            DirichletBC bc;
            bc.node = b["node"].toInt();
            bc.dof = b["dof"].toInt();
            bc.value = b["value"].toDouble();
            config.mesh.dirichlet.push_back(bc);
        }
    }

    // Neumann BCs
    if (root.contains("neumann") && root["neumann"].isArray()) {
        config.mesh.neumann.clear();
        for (const auto& v : root["neumann"].toArray()) {
            QJsonObject b = v.toObject();
            NeumannBC bc;
            bc.node = b["node"].toInt();
            bc.dof = b["dof"].toInt();
            bc.value = b["value"].toDouble();
            config.mesh.neumann.push_back(bc);
        }
    }

    // Displacement results
    if (root.contains("displacement") && root["displacement"].isArray()) {
        config.result.displacement.clear();
        for (const auto& v : root["displacement"].toArray()) {
            config.result.displacement.push_back(v.toDouble());
        }
    }

    // Stress results
    if (root.contains("stresses") && root["stresses"].isArray()) {
        config.result.stresses.clear();
        for (const auto& v : root["stresses"].toArray()) {
            QJsonObject st = v.toObject();
            postprocess::ElementStress s;
            s.von_mises = st["von_mises"].toDouble();
            s.sigma_xx = st["sigma_xx"].toDouble();
            s.sigma_yy = st["sigma_yy"].toDouble();
            s.sigma_xy = st["sigma_xy"].toDouble();
            s.sigma_1 = st["sigma_1"].toDouble();
            s.sigma_2 = st["sigma_2"].toDouble();
            config.result.stresses.push_back(s);
        }
    }

    config.result.cg_iterations = root["cg_iterations"].toInt();
    config.result.solve_time_ms = root["solve_time_ms"].toDouble();
    config.result.cg_converged = root["cg_converged"].toBool();

    return true;
}

QString ProjectIO::defaultProjectPath() {
    return QDir::homePath() + "/cauchy_project.cauchy";
}