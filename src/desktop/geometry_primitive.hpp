#pragma once
#include <QPointF>
#include <QString>
#include <variant>
#include <vector>

struct RectPrimitive {
    double x, y, width, height;
    QString label;
};

struct LinePrimitive {
    double x1, y1, x2, y2;
    QString label;
};

struct CirclePrimitive {
    double cx, cy, radius;
    QString label;
};

using GeometryPrimitive = std::variant<RectPrimitive, LinePrimitive, CirclePrimitive>;

// Helper functions
QPointF getRectCenter(const RectPrimitive& rect);
QPointF getLineCenter(const LinePrimitive& line);
double getLineLength(const LinePrimitive& line);
bool isPointInRect(const QPointF& p, const RectPrimitive& rect);
bool isPointOnLine(const QPointF& p, const LinePrimitive& line, double tolerance);
bool isPointOnCircle(const QPointF& p, const CirclePrimitive& circle, double tolerance);

// Generic helpers operating on GeometryPrimitive variant
QPointF getPrimitiveCenter(const GeometryPrimitive& prim);
bool isPointNearPrimitive(const QPointF& p, const GeometryPrimitive& prim, double tolerance);
double primitiveArea(const GeometryPrimitive& prim);
