#include "geometry_primitive.hpp"
#include <cmath>
#include <limits>

// ------------------------------------------------------------------
// RectPrimitive helpers
// ------------------------------------------------------------------
QPointF getRectCenter(const RectPrimitive& rect) {
    return QPointF(rect.x + rect.width / 2.0,
                   rect.y + rect.height / 2.0);
}

bool isPointInRect(const QPointF& p, const RectPrimitive& rect) {
    double left   = rect.x;
    double right  = rect.x + rect.width;
    double bottom = rect.y;
    double top    = rect.y + rect.height;

    // Handle negative width/height (user dragged left or down)
    if (left > right) std::swap(left, right);
    if (bottom > top) std::swap(bottom, top);

    return p.x() >= left && p.x() <= right &&
           p.y() >= bottom && p.y() <= top;
}

// ------------------------------------------------------------------
// LinePrimitive helpers
// ------------------------------------------------------------------
QPointF getLineCenter(const LinePrimitive& line) {
    return QPointF((line.x1 + line.x2) / 2.0,
                   (line.y1 + line.y2) / 2.0);
}

double getLineLength(const LinePrimitive& line) {
    double dx = line.x2 - line.x1;
    double dy = line.y2 - line.y1;
    return std::sqrt(dx * dx + dy * dy);
}

bool isPointOnLine(const QPointF& p, const LinePrimitive& line, double tolerance) {
    double dx = line.x2 - line.x1;
    double dy = line.y2 - line.y1;
    double len_sq = dx * dx + dy * dy;

    if (len_sq < 1e-20) {
        // Degenerate line, treat as point
        double ddx = p.x() - line.x1;
        double ddy = p.y() - line.y1;
        return std::sqrt(ddx * ddx + ddy * ddy) <= tolerance;
    }

    // Project point onto line segment
    double t = ((p.x() - line.x1) * dx + (p.y() - line.y1) * dy) / len_sq;
    t = std::max(0.0, std::min(1.0, t));

    double proj_x = line.x1 + t * dx;
    double proj_y = line.y1 + t * dy;
    double dist = std::sqrt((p.x() - proj_x) * (p.x() - proj_x) +
                            (p.y() - proj_y) * (p.y() - proj_y));

    return dist <= tolerance;
}

// ------------------------------------------------------------------
// CirclePrimitive helpers
// ------------------------------------------------------------------
bool isPointOnCircle(const QPointF& p, const CirclePrimitive& circle, double tolerance) {
    double dx = p.x() - circle.cx;
    double dy = p.y() - circle.cy;
    double dist = std::sqrt(dx * dx + dy * dy);
    return std::abs(dist - circle.radius) <= tolerance;
}

// ------------------------------------------------------------------
// Generic GeometryPrimitive helpers
// ------------------------------------------------------------------
QPointF getPrimitiveCenter(const GeometryPrimitive& prim) {
    return std::visit([](const auto& p) -> QPointF {
        using T = std::decay_t<decltype(p)>;
        if constexpr (std::is_same_v<T, RectPrimitive>) {
            return getRectCenter(p);
        } else if constexpr (std::is_same_v<T, LinePrimitive>) {
            return getLineCenter(p);
        } else if constexpr (std::is_same_v<T, CirclePrimitive>) {
            return QPointF(p.cx, p.cy);
        }
        return QPointF(0, 0);
    }, prim);
}

bool isPointNearPrimitive(const QPointF& p, const GeometryPrimitive& prim, double tolerance) {
    return std::visit([&](const auto& pr) -> bool {
        using T = std::decay_t<decltype(pr)>;
        if constexpr (std::is_same_v<T, RectPrimitive>) {
            return isPointInRect(p, pr);
        } else if constexpr (std::is_same_v<T, LinePrimitive>) {
            return isPointOnLine(p, pr, tolerance);
        } else if constexpr (std::is_same_v<T, CirclePrimitive>) {
            return isPointOnCircle(p, pr, tolerance);
        }
        return false;
    }, prim);
}

double primitiveArea(const GeometryPrimitive& prim) {
    return std::visit([](const auto& pr) -> double {
        using T = std::decay_t<decltype(pr)>;
        if constexpr (std::is_same_v<T, RectPrimitive>) {
            return std::abs(pr.width * pr.height);
        } else if constexpr (std::is_same_v<T, LinePrimitive>) {
            return getLineLength(pr);
        } else if constexpr (std::is_same_v<T, CirclePrimitive>) {
            return M_PI * pr.radius * pr.radius;
        }
        return 0.0;
    }, prim);
}
