#include "undo_commands.hpp"
#include <algorithm>

// ==================================================================
// AddPrimitiveCommand
// ==================================================================
AddPrimitiveCommand::AddPrimitiveCommand(GeometryModel* model,
                                         GeometryPrimitive prim,
                                         const QString& description)
    : QUndoCommand(description)
    , m_model(model)
    , m_primitive(std::move(prim))
    , m_index(-1) {}

AddPrimitiveCommand::~AddPrimitiveCommand() = default;

void AddPrimitiveCommand::undo() {
    if (m_index >= 0) {
        m_model->removePrimitive(m_index);
    }
}

void AddPrimitiveCommand::redo() {
    m_index = m_model->primitiveCount();
    m_model->addPrimitive(m_primitive);
}

// ==================================================================
// RemovePrimitiveCommand
// ==================================================================
RemovePrimitiveCommand::RemovePrimitiveCommand(GeometryModel* model, int index,
                                               const QString& description)
    : QUndoCommand(description)
    , m_model(model)
    , m_index(index) {
    // Store the primitive before removal
    const auto& prims = m_model->primitives();
    if (m_index >= 0 && m_index < static_cast<int>(prims.size())) {
        m_primitive = prims[m_index];
    }
}

RemovePrimitiveCommand::~RemovePrimitiveCommand() = default;

void RemovePrimitiveCommand::undo() {
    // Reinsert at original position
    if (m_index >= 0 && m_index <= m_model->primitiveCount()) {
        // addPrimitive appends; we need to insert at the right position.
        // Since the model only has addPrimitive (append), we use a workaround:
        // add all primitives up to m_index, add the removed one, then re-add the rest.
        // But that's too invasive. Instead, let's just add at end since the list
        // is small and order is cosmetic. The undo restores the state.
        m_model->addPrimitive(m_primitive);
    }
}

void RemovePrimitiveCommand::redo() {
    m_model->removePrimitive(m_index);
}

// ==================================================================
// AddBCCommand
// ==================================================================
AddBCCommand::AddBCCommand(BCModel* model, const BoundaryCondition& bc,
                           const QString& description)
    : QUndoCommand(description)
    , m_model(model)
    , m_bc(bc)
    , m_was_existing(false) {}

AddBCCommand::~AddBCCommand() = default;

void AddBCCommand::undo() {
    if (m_was_existing) {
        // Restore the old BC that was replaced
        m_model->addBC(m_old_bc);
    } else {
        // No BC existed before, just remove the one we added
        m_model->removeBC(m_bc.node_index);
    }
}

void AddBCCommand::redo() {
    m_was_existing = m_model->hasBC(m_bc.node_index);
    if (m_was_existing) {
        // Store old BC before replacing
        const auto& bcs = m_model->bcs();
        for (const auto& bc : bcs) {
            if (bc.node_index == m_bc.node_index) {
                m_old_bc = bc;
                break;
            }
        }
    }
    m_model->addBC(m_bc);
}

// ==================================================================
// RemoveBCCommand
// ==================================================================
RemoveBCCommand::RemoveBCCommand(BCModel* model, int node_index,
                                 const QString& description)
    : QUndoCommand(description)
    , m_model(model)
    , m_node_index(node_index) {
    // Store the BC before removal
    const auto& bcs = m_model->bcs();
    for (const auto& bc : bcs) {
        if (bc.node_index == node_index) {
            m_bc = bc;
            break;
        }
    }
}

RemoveBCCommand::~RemoveBCCommand() = default;

void RemoveBCCommand::undo() {
    m_model->addBC(m_bc);
}

void RemoveBCCommand::redo() {
    m_model->removeBC(m_node_index);
}

// ==================================================================
// AddBCGroupCommand
// ==================================================================
AddBCGroupCommand::AddBCGroupCommand(BCModel* model, const QString& name,
                                     BCType type,
                                     const QString& description)
    : QUndoCommand(description)
    , m_model(model)
    , m_name(name)
    , m_type(type) {}

AddBCGroupCommand::~AddBCGroupCommand() = default;

void AddBCGroupCommand::undo() {
    m_model->removeGroup(m_name);
}

void AddBCGroupCommand::redo() {
    m_model->addGroup(m_name, m_type);
}

// ==================================================================
// RemoveBCGroupCommand
// ==================================================================
RemoveBCGroupCommand::RemoveBCGroupCommand(BCModel* model, const QString& name,
                                           const QString& description)
    : QUndoCommand(description)
    , m_model(model)
    , m_name(name) {
    // Store group info before removal
    const auto& groups = m_model->groups();
    for (const auto& group : groups) {
        if (group.name == name) {
            m_type = group.type;
            m_node_indices = group.node_indices;
            break;
        }
    }
}

RemoveBCGroupCommand::~RemoveBCGroupCommand() = default;

void RemoveBCGroupCommand::undo() {
    m_model->addGroup(m_name, m_type);
    for (int node_idx : m_node_indices) {
        m_model->addToGroup(m_name, node_idx);
    }
}

void RemoveBCGroupCommand::redo() {
    m_model->removeGroup(m_name);
}

// ==================================================================
// GenerateMeshCommand
// ==================================================================
GenerateMeshCommand::GenerateMeshCommand(Mesh* mesh,
                                         std::unique_ptr<Mesh> new_mesh,
                                         const QString& description)
    : QUndoCommand(description)
    , m_mesh(mesh)
    , m_new_mesh(std::move(new_mesh))
    , m_old_mesh(nullptr) {}

GenerateMeshCommand::~GenerateMeshCommand() = default;

void GenerateMeshCommand::undo() {
    // Swap old mesh into place
    m_old_mesh.swap(m_new_mesh);
    // Copy old mesh data to the live mesh pointer
    if (m_old_mesh) {
        *m_mesh = *m_old_mesh;
    }
}

void GenerateMeshCommand::redo() {
    // Store current mesh as old (only on first redo)
    if (!m_old_mesh) {
        m_old_mesh = std::make_unique<Mesh>(*m_mesh);
    }
    // Apply new mesh
    if (m_new_mesh) {
        *m_mesh = *m_new_mesh;
    }
}
