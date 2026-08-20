#include "bc_model.hpp"
#include <algorithm>

BCModel::BCModel() = default;
BCModel::~BCModel() = default;

// ------------------------------------------------------------------
// Individual BC management
// ------------------------------------------------------------------
void BCModel::addBC(const BoundaryCondition& bc) {
    // Replace if node already has a BC
    int idx = findBCIndex(bc.node_index);
    if (idx >= 0) {
        m_bcs[idx] = bc;
    } else {
        m_bcs.push_back(bc);
    }
}

void BCModel::removeBC(int node_index) {
    int idx = findBCIndex(node_index);
    if (idx >= 0) {
        m_bcs.erase(m_bcs.begin() + idx);
    }
}

void BCModel::clear() {
    m_bcs.clear();
    m_groups.clear();
}

// ------------------------------------------------------------------
// BC groups
// ------------------------------------------------------------------
void BCModel::addGroup(const QString& name, BCType type) {
    // Don't duplicate
    for (const auto& g : m_groups) {
        if (g.name == name) return;
    }
    BCGroup group;
    group.name = name;
    group.type = type;
    group.force_value = 0.0;
    group.force_angle = 0.0;
    m_groups.push_back(std::move(group));
}

void BCModel::removeGroup(const QString& name) {
    m_groups.erase(
        std::remove_if(m_groups.begin(), m_groups.end(),
                        [&name](const BCGroup& g) { return g.name == name; }),
        m_groups.end());
}

void BCModel::addToGroup(const QString& name, int node_index) {
    BCGroup* group = findGroup(name);
    if (!group) return;

    // Avoid duplicates
    for (int ni : group->node_indices) {
        if (ni == node_index) return;
    }
    group->node_indices.push_back(node_index);

    // Also add the BC itself
    BoundaryCondition bc;
    bc.node_index = node_index;
    bc.type = group->type;
    bc.value = group->force_value;
    bc.group = name;
    addBC(bc);
}

void BCModel::removeFromGroup(const QString& name, int node_index) {
    BCGroup* group = findGroup(name);
    if (!group) return;

    group->node_indices.erase(
        std::remove(group->node_indices.begin(), group->node_indices.end(), node_index),
        group->node_indices.end());

    // Remove the BC if it belongs to this group
    int idx = findBCIndex(node_index);
    if (idx >= 0 && m_bcs[idx].group == name) {
        m_bcs.erase(m_bcs.begin() + idx);
    }
}

const std::vector<BCGroup>& BCModel::groups() const {
    return m_groups;
}

BCGroup* BCModel::findGroup(const QString& name) {
    for (auto& g : m_groups) {
        if (g.name == name) return &g;
    }
    return nullptr;
}

const BCGroup* BCModel::findGroup(const QString& name) const {
    for (const auto& g : m_groups) {
        if (g.name == name) return &g;
    }
    return nullptr;
}

// ------------------------------------------------------------------
// Query
// ------------------------------------------------------------------
const std::vector<BoundaryCondition>& BCModel::bcs() const {
    return m_bcs;
}

bool BCModel::hasBC(int node_index) const {
    return findBCIndex(node_index) >= 0;
}

BCType BCModel::getBCType(int node_index) const {
    int idx = findBCIndex(node_index);
    if (idx >= 0) {
        return m_bcs[idx].type;
    }
    // Return FIXED as a safe default (never reached if hasBC check done first)
    return BCType::FIXED;
}

std::vector<int> BCModel::getNodesForType(BCType type) const {
    std::vector<int> result;
    for (const auto& bc : m_bcs) {
        if (bc.type == type) {
            result.push_back(bc.node_index);
        }
    }
    return result;
}

int BCModel::bcCount() const {
    return static_cast<int>(m_bcs.size());
}

int BCModel::groupCount() const {
    return static_cast<int>(m_groups.size());
}

// ------------------------------------------------------------------
// Private
// ------------------------------------------------------------------
int BCModel::findBCIndex(int node_index) const {
    for (int i = 0; i < static_cast<int>(m_bcs.size()); ++i) {
        if (m_bcs[i].node_index == node_index) {
            return i;
        }
    }
    return -1;
}

double BCModel::getGroupForceMagnitude(const QString& group_name) const {
    for (const auto& group : m_groups) {
        if (group.name == group_name) {
            return group.force_value;
        }
    }
    return 0.0;
}
