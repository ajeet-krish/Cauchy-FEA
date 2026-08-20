#include "project_io.hpp"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QDir>
#include <QVariant>

// ==========================================================================
// SERIALIZE to JSON
// ==========================================================================

// Material
static QJsonObject serializeMaterial(const Material& mat) {
    QJsonObject obj;
    obj["E"] = mat.E;
    obj["nu"] = mat.nu;
    obj["rho"] = mat.rho;
    obj["t"] = mat.t;
    obj["alpha"] = mat.alpha;
    return obj;
}

// ViewportState
static QJsonObject serializeViewport(const ViewportState& vp) {
    QJsonObject obj;
    obj["contourField"] = vp.contourField;
    obj["colormap"] = vp.colormap;
    obj["dispScale"] = vp.dispScale;
    obj["showUndeformed"] = vp.showUndeformed;
    obj["showDeformed"] = vp.showDeformed;
    obj["showEdges"] = vp.showEdges;
    obj["showArrows"] = vp.showArrows;
    obj["showBoundary"] = vp.showBoundary;
    obj["panX"] = vp.panX;
    obj["panY"] = vp.panY;
    obj["zoom"] = vp.zoom;
    return obj;
}

// Geometry primitives
static QJsonArray serializePrimitives(const std::vector<GeometryPrimitive>& prims) {
    QJsonArray arr;
    for (const auto& prim : prims) {
        QJsonObject obj;
        if (auto* rect = std::get_if<RectPrimitive>(&prim)) {
            obj["type"] = "rect";
            obj["x"] = rect->x;
            obj["y"] = rect->y;
            obj["width"] = rect->width;
            obj["height"] = rect->height;
            obj["label"] = rect->label;
        } else if (auto* line = std::get_if<LinePrimitive>(&prim)) {
            obj["type"] = "line";
            obj["x1"] = line->x1;
            obj["y1"] = line->y1;
            obj["x2"] = line->x2;
            obj["y2"] = line->y2;
            obj["label"] = line->label;
        } else if (auto* circle = std::get_if<CirclePrimitive>(&prim)) {
            obj["type"] = "circle";
            obj["cx"] = circle->cx;
            obj["cy"] = circle->cy;
            obj["radius"] = circle->radius;
            obj["label"] = circle->label;
        }
        arr.append(obj);
    }
    return arr;
}

// Editor BCs (BoundaryCondition struct from BCModel)
static QJsonArray serializeBoundaryConditions(const std::vector<BoundaryCondition>& bcs) {
    QJsonArray arr;
    for (const auto& bc : bcs) {
        QJsonObject obj;
        obj["node_index"] = bc.node_index;
        obj["type"] = static_cast<int>(bc.type);
        obj["value"] = bc.value;
        obj["angle"] = bc.angle;
        obj["group"] = bc.group;
        arr.append(obj);
    }
    return arr;
}

// Mesh nodes
static QJsonArray serializeNodes(const Mesh& mesh) {
    QJsonArray arr;
    for (const auto& node : mesh.nodes) {
        QJsonObject n;
        n["x"] = node.x;
        n["y"] = node.y;
        if (node.z != 0.0) n["z"] = node.z;
        arr.append(n);
    }
    return arr;
}

// Mesh connectivity
static QJsonArray serializeQ4Elements(const Mesh& mesh) {
    QJsonArray arr;
    for (const auto& elem : mesh.quad_elements) {
        QJsonObject e;
        e["n0"] = elem[0];
        e["n1"] = elem[1];
        e["n2"] = elem[2];
        e["n3"] = elem[3];
        arr.append(e);
    }
    return arr;
}

static QJsonArray serializeQ8Elements(const Mesh& mesh) {
    QJsonArray arr;
    for (const auto& elem : mesh.quad8_elements) {
        QJsonObject e;
        e["n0"] = elem[0]; e["n1"] = elem[1];
        e["n2"] = elem[2]; e["n3"] = elem[3];
        e["n4"] = elem[4]; e["n5"] = elem[5];
        e["n6"] = elem[6]; e["n7"] = elem[7];
        arr.append(e);
    }
    return arr;
}

static QJsonArray serializeT3Elements(const Mesh& mesh) {
    QJsonArray arr;
    for (const auto& elem : mesh.tri_elements) {
        QJsonObject e;
        e["n0"] = elem[0];
        e["n1"] = elem[1];
        e["n2"] = elem[2];
        arr.append(e);
    }
    return arr;
}

static QJsonArray serializeBarElements(const Mesh& mesh) {
    QJsonArray arr;
    for (size_t i = 0; i < mesh.bar_elements.size(); ++i) {
        QJsonObject e;
        e["n0"] = mesh.bar_elements[i][0];
        e["n1"] = mesh.bar_elements[i][1];
        if (i < mesh.bar_areas.size()) {
            e["area"] = mesh.bar_areas[i];
        }
        arr.append(e);
    }
    return arr;
}

// Dirichlet / Neumann (mesh-level BCs)
static QJsonArray serializeDirichlet(const Mesh& mesh) {
    QJsonArray arr;
    for (const auto& bc : mesh.dirichlet) {
        QJsonObject b;
        b["node"] = bc.node;
        b["dof"] = bc.dof;
        b["value"] = bc.value;
        arr.append(b);
    }
    return arr;
}

static QJsonArray serializeNeumann(const Mesh& mesh) {
    QJsonArray arr;
    for (const auto& bc : mesh.neumann) {
        QJsonObject b;
        b["node"] = bc.node;
        b["dof"] = bc.dof;
        b["value"] = bc.value;
        arr.append(b);
    }
    return arr;
}

// Solver results (displacement + stresses only, skip K_csr and f)
static QJsonObject serializeResults(const fea::SolveResult& result) {
    QJsonObject obj;

    QJsonArray dispArr;
    for (double d : result.displacement) {
        dispArr.append(d);
    }
    obj["displacement"] = dispArr;

    QJsonArray stressArr;
    for (const auto& s : result.stresses) {
        QJsonObject st;
        st["sigma_xx"] = s.sigma_xx;
        st["sigma_yy"] = s.sigma_yy;
        st["sigma_zz"] = s.sigma_zz;
        st["sigma_xy"] = s.sigma_xy;
        st["sigma_yz"] = s.sigma_yz;
        st["sigma_xz"] = s.sigma_xz;
        st["von_mises"] = s.von_mises;
        st["sigma_1"] = s.sigma_1;
        st["sigma_2"] = s.sigma_2;
        st["sigma_3"] = s.sigma_3;
        stressArr.append(st);
    }
    obj["stresses"] = stressArr;

    obj["cg_iterations"] = result.cg_iterations;
    obj["solve_time_ms"] = result.solve_time_ms;
    obj["cg_converged"] = result.cg_converged;

    return obj;
}

// ==========================================================================
// DESERIALIZE from JSON
// ==========================================================================

// Material
static void deserializeMaterial(const QJsonObject& obj, Material& mat) {
    if (obj.contains("E")) mat.E = obj["E"].toDouble();
    if (obj.contains("nu")) mat.nu = obj["nu"].toDouble();
    if (obj.contains("rho")) mat.rho = obj["rho"].toDouble();
    if (obj.contains("t")) mat.t = obj["t"].toDouble();
    if (obj.contains("alpha")) mat.alpha = obj["alpha"].toDouble();
}

// ViewportState
static void deserializeViewport(const QJsonObject& obj, ViewportState& vp) {
    if (obj.contains("contourField")) vp.contourField = obj["contourField"].toInt();
    if (obj.contains("colormap")) vp.colormap = obj["colormap"].toInt();
    if (obj.contains("dispScale")) vp.dispScale = obj["dispScale"].toDouble();
    if (obj.contains("showUndeformed")) vp.showUndeformed = obj["showUndeformed"].toBool();
    if (obj.contains("showDeformed")) vp.showDeformed = obj["showDeformed"].toBool();
    if (obj.contains("showEdges")) vp.showEdges = obj["showEdges"].toBool();
    if (obj.contains("showArrows")) vp.showArrows = obj["showArrows"].toBool();
    if (obj.contains("showBoundary")) vp.showBoundary = obj["showBoundary"].toBool();
    if (obj.contains("panX")) vp.panX = obj["panX"].toDouble();
    if (obj.contains("panY")) vp.panY = obj["panY"].toDouble();
    if (obj.contains("zoom")) vp.zoom = obj["zoom"].toDouble();
}

// Geometry primitives
static void deserializePrimitives(const QJsonArray& arr, std::vector<GeometryPrimitive>& prims) {
    prims.clear();
    for (const auto& v : arr) {
        QJsonObject obj = v.toObject();
        QString type = obj["type"].toString();
        if (type == "rect") {
            prims.push_back(RectPrimitive{
                obj["x"].toDouble(), obj["y"].toDouble(),
                obj["width"].toDouble(), obj["height"].toDouble(),
                obj["label"].toString()
            });
        } else if (type == "line") {
            prims.push_back(LinePrimitive{
                obj["x1"].toDouble(), obj["y1"].toDouble(),
                obj["x2"].toDouble(), obj["y2"].toDouble(),
                obj["label"].toString()
            });
        } else if (type == "circle") {
            prims.push_back(CirclePrimitive{
                obj["cx"].toDouble(), obj["cy"].toDouble(),
                obj["radius"].toDouble(),
                obj["label"].toString()
            });
        }
    }
}

// Editor BCs
static void deserializeBoundaryConditions(const QJsonArray& arr, std::vector<BoundaryCondition>& bcs) {
    bcs.clear();
    for (const auto& v : arr) {
        QJsonObject obj = v.toObject();
        BoundaryCondition bc;
        bc.node_index = obj["node_index"].toInt();
        bc.type = static_cast<BCType>(obj["type"].toInt());
        bc.value = obj["value"].toDouble();
        bc.angle = obj["angle"].toDouble();
        bc.group = obj["group"].toString();
        bcs.push_back(bc);
    }
}

// Mesh nodes
static void deserializeNodes(const QJsonArray& arr, Mesh& mesh) {
    mesh.nodes.clear();
    for (const auto& v : arr) {
        QJsonObject n = v.toObject();
        Node node;
        node.x = n["x"].toDouble();
        node.y = n["y"].toDouble();
        node.z = n.contains("z") ? n["z"].toDouble() : 0.0;
        mesh.nodes.push_back(node);
    }
}

// Q4 elements
static void deserializeQ4(const QJsonArray& arr, Mesh& mesh) {
    mesh.quad_elements.clear();
    for (const auto& v : arr) {
        QJsonObject e = v.toObject();
        mesh.quad_elements.push_back(
            {e["n0"].toInt(), e["n1"].toInt(), e["n2"].toInt(), e["n3"].toInt()});
    }
}

// Q8 elements
static void deserializeQ8(const QJsonArray& arr, Mesh& mesh) {
    mesh.quad8_elements.clear();
    for (const auto& v : arr) {
        QJsonObject e = v.toObject();
        mesh.quad8_elements.push_back({
            e["n0"].toInt(), e["n1"].toInt(), e["n2"].toInt(), e["n3"].toInt(),
            e["n4"].toInt(), e["n5"].toInt(), e["n6"].toInt(), e["n7"].toInt()
        });
    }
}

// T3 elements
static void deserializeT3(const QJsonArray& arr, Mesh& mesh) {
    mesh.tri_elements.clear();
    for (const auto& v : arr) {
        QJsonObject e = v.toObject();
        mesh.tri_elements.push_back(
            {e["n0"].toInt(), e["n1"].toInt(), e["n2"].toInt()});
    }
}

// Bar elements
static void deserializeBar(const QJsonArray& arr, Mesh& mesh) {
    mesh.bar_elements.clear();
    mesh.bar_areas.clear();
    for (const auto& v : arr) {
        QJsonObject e = v.toObject();
        mesh.bar_elements.push_back({e["n0"].toInt(), e["n1"].toInt()});
        mesh.bar_areas.push_back(e["area"].toDouble());
    }
}

// Dirichlet BCs
static void deserializeDirichlet(const QJsonArray& arr, Mesh& mesh) {
    mesh.dirichlet.clear();
    for (const auto& v : arr) {
        QJsonObject b = v.toObject();
        DirichletBC bc;
        bc.node = b["node"].toInt();
        bc.dof = b["dof"].toInt();
        bc.value = b["value"].toDouble();
        mesh.dirichlet.push_back(bc);
    }
}

// Neumann BCs
static void deserializeNeumann(const QJsonArray& arr, Mesh& mesh) {
    mesh.neumann.clear();
    for (const auto& v : arr) {
        QJsonObject b = v.toObject();
        NeumannBC bc;
        bc.node = b["node"].toInt();
        bc.dof = b["dof"].toInt();
        bc.value = b["value"].toDouble();
        mesh.neumann.push_back(bc);
    }
}

// Solver results
static void deserializeResults(const QJsonObject& obj, fea::SolveResult& result) {
    if (obj.contains("displacement") && obj["displacement"].isArray()) {
        result.displacement.clear();
        for (const auto& v : obj["displacement"].toArray()) {
            result.displacement.push_back(v.toDouble());
        }
    }
    if (obj.contains("stresses") && obj["stresses"].isArray()) {
        result.stresses.clear();
        for (const auto& v : obj["stresses"].toArray()) {
            QJsonObject st = v.toObject();
            postprocess::ElementStress s;
            s.sigma_xx = st["sigma_xx"].toDouble();
            s.sigma_yy = st["sigma_yy"].toDouble();
            s.sigma_zz = st.contains("sigma_zz") ? st["sigma_zz"].toDouble() : 0.0;
            s.sigma_xy = st["sigma_xy"].toDouble();
            s.sigma_yz = st.contains("sigma_yz") ? st["sigma_yz"].toDouble() : 0.0;
            s.sigma_xz = st.contains("sigma_xz") ? st["sigma_xz"].toDouble() : 0.0;
            s.von_mises = st["von_mises"].toDouble();
            s.sigma_1 = st["sigma_1"].toDouble();
            s.sigma_2 = st["sigma_2"].toDouble();
            s.sigma_3 = st.contains("sigma_3") ? st["sigma_3"].toDouble() : 0.0;
            result.stresses.push_back(s);
        }
    }
    result.cg_iterations = obj.value("cg_iterations").toInt();
    result.solve_time_ms = obj.value("solve_time_ms").toDouble();
    result.cg_converged = obj.value("cg_converged").toBool();
}

// ==========================================================================
// SAVE
// ==========================================================================

bool ProjectIO::save(const QString& filePath, const ProjectConfig& config) {
    QJsonObject root;

    // Version
    root["version"] = 2;

    // Solver config
    QJsonObject cfgObj;
    cfgObj["case_type"] = static_cast<int>(config.solverConfig.case_type);
    cfgObj["element_type"] = static_cast<int>(config.solverConfig.element_type);
    cfgObj["plane_type"] = static_cast<int>(config.solverConfig.plane_type);
    cfgObj["nx"] = config.solverConfig.nx;
    cfgObj["ny"] = config.solverConfig.ny;
    cfgObj["nz"] = config.solverConfig.nz;
    cfgObj["use_q8"] = config.solverConfig.use_q8;
    cfgObj["use_cg"] = config.solverConfig.use_cg;
    cfgObj["use_adaptivity"] = config.solverConfig.use_adaptivity;
    cfgObj["adaptive_iters"] = config.solverConfig.adaptive_iters;
    cfgObj["is_3d"] = config.solverConfig.is_3d;
    root["solver_config"] = cfgObj;

    // Material
    root["material"] = serializeMaterial(config.material);

    // Mesh
    QJsonObject meshObj;
    meshObj["nodes"] = serializeNodes(config.mesh);
    meshObj["quad_elements"] = serializeQ4Elements(config.mesh);
    meshObj["quad8_elements"] = serializeQ8Elements(config.mesh);
    meshObj["tri_elements"] = serializeT3Elements(config.mesh);
    meshObj["bar_elements"] = serializeBarElements(config.mesh);
    meshObj["dirichlet"] = serializeDirichlet(config.mesh);
    meshObj["neumann"] = serializeNeumann(config.mesh);
    root["mesh"] = meshObj;

    // Editor boundary conditions
    root["boundary_conditions"] = serializeBoundaryConditions(config.boundaryConditions);

    // Geometry primitives
    root["geometry_primitives"] = serializePrimitives(config.geometryPrimitives);

    // Viewport state
    root["viewport"] = serializeViewport(config.viewport);

    // Results
    root["has_results"] = config.hasResults;
    if (config.hasResults) {
        root["results"] = serializeResults(config.result);
    }

    // Write to file
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

// ==========================================================================
// LOAD
// ==========================================================================

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

    // Version detection (v1 files have no version key)
    config.version = root.contains("version") ? root["version"].toInt() : 1;

    // Solver config
    if (root.contains("solver_config") && root["solver_config"].isObject()) {
        QJsonObject cfgObj = root["solver_config"].toObject();
        config.solverConfig.case_type = static_cast<CaseType>(cfgObj["case_type"].toInt());
        config.solverConfig.element_type = static_cast<ElementType>(cfgObj["element_type"].toInt());
        config.solverConfig.plane_type = static_cast<PlaneType>(cfgObj["plane_type"].toInt());
        config.solverConfig.nx = cfgObj["nx"].toInt();
        config.solverConfig.ny = cfgObj["ny"].toInt();
        if (cfgObj.contains("nz")) config.solverConfig.nz = cfgObj["nz"].toInt();
        config.solverConfig.use_q8 = cfgObj["use_q8"].toBool();
        config.solverConfig.use_cg = cfgObj["use_cg"].toBool();
        config.solverConfig.E = cfgObj["E"].toDouble();
        config.solverConfig.nu = cfgObj["nu"].toDouble();
        config.solverConfig.t = cfgObj["t"].toDouble();
        config.solverConfig.use_adaptivity = cfgObj.contains("use_adaptivity") ? cfgObj["use_adaptivity"].toBool() : false;
        config.solverConfig.adaptive_iters = cfgObj.contains("adaptive_iters") ? cfgObj["adaptive_iters"].toInt() : 3;
        config.solverConfig.is_3d = cfgObj.contains("is_3d") ? cfgObj["is_3d"].toBool() : false;
    }

    // Material (v2)
    if (root.contains("material") && root["material"].isObject()) {
        deserializeMaterial(root["material"].toObject(), config.material);
    }

    // Mesh (v2 nested format or v1 flat format)
    if (root.contains("mesh") && root["mesh"].isObject()) {
        QJsonObject meshObj = root["mesh"].toObject();

        if (meshObj.contains("nodes") && meshObj["nodes"].isArray()) {
            deserializeNodes(meshObj["nodes"].toArray(), config.mesh);
        }
        if (meshObj.contains("quad_elements") && meshObj["quad_elements"].isArray()) {
            deserializeQ4(meshObj["quad_elements"].toArray(), config.mesh);
        }
        if (meshObj.contains("quad8_elements") && meshObj["quad8_elements"].isArray()) {
            deserializeQ8(meshObj["quad8_elements"].toArray(), config.mesh);
        }
        if (meshObj.contains("tri_elements") && meshObj["tri_elements"].isArray()) {
            deserializeT3(meshObj["tri_elements"].toArray(), config.mesh);
        }
        if (meshObj.contains("bar_elements") && meshObj["bar_elements"].isArray()) {
            deserializeBar(meshObj["bar_elements"].toArray(), config.mesh);
        }
        if (meshObj.contains("dirichlet") && meshObj["dirichlet"].isArray()) {
            deserializeDirichlet(meshObj["dirichlet"].toArray(), config.mesh);
        }
        if (meshObj.contains("neumann") && meshObj["neumann"].isArray()) {
            deserializeNeumann(meshObj["neumann"].toArray(), config.mesh);
        }
    } else {
        // v1 flat format fallback
        if (root.contains("nodes") && root["nodes"].isArray()) {
            deserializeNodes(root["nodes"].toArray(), config.mesh);
        }
        if (root.contains("quad_elements") && root["quad_elements"].isArray()) {
            deserializeQ4(root["quad_elements"].toArray(), config.mesh);
        }
        if (root.contains("tri_elements") && root["tri_elements"].isArray()) {
            deserializeT3(root["tri_elements"].toArray(), config.mesh);
        }
        if (root.contains("dirichlet") && root["dirichlet"].isArray()) {
            deserializeDirichlet(root["dirichlet"].toArray(), config.mesh);
        }
        if (root.contains("neumann") && root["neumann"].isArray()) {
            deserializeNeumann(root["neumann"].toArray(), config.mesh);
        }
    }

    // Editor boundary conditions (v2)
    if (root.contains("boundary_conditions") && root["boundary_conditions"].isArray()) {
        deserializeBoundaryConditions(root["boundary_conditions"].toArray(),
                                      config.boundaryConditions);
    }

    // Geometry primitives (v2)
    if (root.contains("geometry_primitives") && root["geometry_primitives"].isArray()) {
        deserializePrimitives(root["geometry_primitives"].toArray(),
                              config.geometryPrimitives);
    }

    // Viewport state (v2)
    if (root.contains("viewport") && root["viewport"].isObject()) {
        deserializeViewport(root["viewport"].toObject(), config.viewport);
    }

    // Results
    config.hasResults = root.contains("has_results") ? root["has_results"].toBool() : false;
    if (config.hasResults && root.contains("results") && root["results"].isObject()) {
        deserializeResults(root["results"].toObject(), config.result);
    } else if (!config.hasResults) {
        // v1 fallback: results were stored at top level
        bool has_disp = root.contains("displacement") && root["displacement"].isArray();
        bool has_stress = root.contains("stresses") && root["stresses"].isArray();
        if (has_disp || has_stress) {
            config.hasResults = true;
            // Reconstruct v1 result at top level
            if (has_disp) {
                config.result.displacement.clear();
                for (const auto& v : root["displacement"].toArray()) {
                    config.result.displacement.push_back(v.toDouble());
                }
            }
            if (has_stress) {
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
            config.result.cg_iterations = root.contains("cg_iterations") ? root["cg_iterations"].toInt() : 0;
            config.result.solve_time_ms = root.contains("solve_time_ms") ? root["solve_time_ms"].toDouble() : 0.0;
            config.result.cg_converged = root.contains("cg_converged") ? root["cg_converged"].toBool() : false;
        }
    }

    // Propagate mesh material from config
    config.mesh.mat = config.material;
    config.mesh.plane = config.solverConfig.plane_type;

    return true;
}

QString ProjectIO::defaultProjectPath() {
    return QDir::homePath() + "/cauchy_project.cauchy";
}
