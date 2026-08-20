#pragma once
#include "geometry_model.hpp"
#include "bc_model.hpp"
#include "geometry_primitive.hpp"
#include "../fea_types.hpp"
#include <QUndoCommand>
#include <QString>
#include <QPointF>
#include <memory>
#include <vector>

// ------------------------------------------------------------------
// Geometry commands
// ------------------------------------------------------------------

// Add a geometry primitive (rect, line, circle) to the model
class AddPrimitiveCommand : public QUndoCommand {
public:
    AddPrimitiveCommand(GeometryModel* model, GeometryPrimitive prim,
                        const QString& description = "Add Primitive");
    ~AddPrimitiveCommand() override;

    void undo() override;
    void redo() override;

private:
    GeometryModel* m_model;
    GeometryPrimitive m_primitive;
    int m_index;
};

// Remove a geometry primitive by index
class RemovePrimitiveCommand : public QUndoCommand {
public:
    RemovePrimitiveCommand(GeometryModel* model, int index,
                           const QString& description = "Remove Primitive");
    ~RemovePrimitiveCommand() override;

    void undo() override;
    void redo() override;

private:
    GeometryModel* m_model;
    GeometryPrimitive m_primitive;
    int m_index;
};

// ------------------------------------------------------------------
// BC commands
// ------------------------------------------------------------------

// Add (or replace) a boundary condition on a node
class AddBCCommand : public QUndoCommand {
public:
    AddBCCommand(BCModel* model, const BoundaryCondition& bc,
                 const QString& description = "Add Boundary Condition");
    ~AddBCCommand() override;

    void undo() override;
    void redo() override;

private:
    BCModel* m_model;
    BoundaryCondition m_bc;
    bool m_was_existing;
    BoundaryCondition m_old_bc;
};

// Remove a boundary condition from a node
class RemoveBCCommand : public QUndoCommand {
public:
    RemoveBCCommand(BCModel* model, int node_index,
                    const QString& description = "Remove Boundary Condition");
    ~RemoveBCCommand() override;

    void undo() override;
    void redo() override;

private:
    BCModel* m_model;
    BoundaryCondition m_bc;
    int m_node_index;
};

// ------------------------------------------------------------------
// BC Group commands
// ------------------------------------------------------------------

// Add a BC group (named collection of BCs of the same type)
class AddBCGroupCommand : public QUndoCommand {
public:
    AddBCGroupCommand(BCModel* model, const QString& name, BCType type,
                      const QString& description = "Add BC Group");
    ~AddBCGroupCommand() override;

    void undo() override;
    void redo() override;

private:
    BCModel* m_model;
    QString m_name;
    BCType m_type;
};

// Remove a BC group (removes the group and all its member BCs)
class RemoveBCGroupCommand : public QUndoCommand {
public:
    RemoveBCGroupCommand(BCModel* model, const QString& name,
                         const QString& description = "Remove BC Group");
    ~RemoveBCGroupCommand() override;

    void undo() override;
    void redo() override;

private:
    BCModel* m_model;
    QString m_name;
    BCType m_type;
    std::vector<int> m_node_indices;
};

// ------------------------------------------------------------------
// Mesh command
// ------------------------------------------------------------------

// Replace the current mesh with a newly generated one
class GenerateMeshCommand : public QUndoCommand {
public:
    GenerateMeshCommand(Mesh* mesh, std::unique_ptr<Mesh> new_mesh,
                        const QString& description = "Generate Mesh");
    ~GenerateMeshCommand() override;

    void undo() override;
    void redo() override;

private:
    Mesh* m_mesh;
    std::unique_ptr<Mesh> m_new_mesh;
    std::unique_ptr<Mesh> m_old_mesh;
};
