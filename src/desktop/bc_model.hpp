#pragma once
#include <QPointF>
#include <QString>
#include <vector>

enum class BCType {
    FIXED,
    ROLLER_X,  // Fixed in X, free in Y
    ROLLER_Y,  // Fixed in Y, free in X
    FORCE
};

struct BoundaryCondition {
    int node_index;
    BCType type;
    double value;   // For forces: magnitude in N
    double angle;   // For forces: angle in degrees (0 = +X, 90 = +Y)
    QString group;  // BC group name
};

struct BCGroup {
    QString name;
    BCType type;
    std::vector<int> node_indices;
    double force_value;   // For force groups (N)
    double force_angle;   // For force groups (degrees, 0 = +X)
};

class BCModel {
public:
    BCModel();
    ~BCModel();

    // Add/remove individual BCs
    void addBC(const BoundaryCondition& bc);
    void removeBC(int node_index);
    void clear();

    // BC groups
    void addGroup(const QString& name, BCType type);
    void removeGroup(const QString& name);
    void addToGroup(const QString& name, int node_index);
    void removeFromGroup(const QString& name, int node_index);
    const std::vector<BCGroup>& groups() const;
    BCGroup* findGroup(const QString& name);
    const BCGroup* findGroup(const QString& name) const;

    // Query
    const std::vector<BoundaryCondition>& bcs() const;
    bool hasBC(int node_index) const;
    BCType getBCType(int node_index) const;

    // Get all nodes for a specific BC type
    std::vector<int> getNodesForType(BCType type) const;

    // Count
    int bcCount() const;
    int groupCount() const;
    
    // Get force magnitude for a group
    double getGroupForceMagnitude(const QString& group_name) const;

private:
    std::vector<BoundaryCondition> m_bcs;
    std::vector<BCGroup> m_groups;

    int findBCIndex(int node_index) const;
};
