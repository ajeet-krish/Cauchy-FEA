#pragma once
#include "geometry_primitive.hpp"
#include <vector>
#include <optional>

class GeometryModel {
public:
    GeometryModel();
    ~GeometryModel();

    void addPrimitive(GeometryPrimitive prim);
    void removePrimitive(int index);
    void clear();

    const std::vector<GeometryPrimitive>& primitives() const;
    int primitiveCount() const;

    // Find nearest primitive to point (returns -1 if none within tolerance)
    int findNearestPrimitive(const QPointF& point, double tolerance) const;

    // Get bounding box of all primitives
    std::optional<struct BBox> boundingBox() const;

    // Check if point is inside any primitive
    bool isPointInside(const QPointF& point) const;

    // Get the index of the primitive that contains the point (-1 if none)
    int findContainingPrimitive(const QPointF& point) const;

private:
    std::vector<GeometryPrimitive> m_primitives;
};

struct BBox {
    double xmin, ymin, xmax, ymax;
};
