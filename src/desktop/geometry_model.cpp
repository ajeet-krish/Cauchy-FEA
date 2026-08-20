#include "geometry_model.hpp"
#include <cmath>
#include <algorithm>
#include <limits>

GeometryModel::GeometryModel() = default;
GeometryModel::~GeometryModel() = default;

void GeometryModel::addPrimitive(GeometryPrimitive prim) {
    m_primitives.push_back(std::move(prim));
}

void GeometryModel::removePrimitive(int index) {
    if (index >= 0 && index < static_cast<int>(m_primitives.size())) {
        m_primitives.erase(m_primitives.begin() + index);
    }
}

void GeometryModel::clear() {
    m_primitives.clear();
}

const std::vector<GeometryPrimitive>& GeometryModel::primitives() const {
    return m_primitives;
}

int GeometryModel::primitiveCount() const {
    return static_cast<int>(m_primitives.size());
}

int GeometryModel::findNearestPrimitive(const QPointF& point, double tolerance) const {
    int best_index = -1;
    double best_dist = tolerance;

    for (int i = 0; i < static_cast<int>(m_primitives.size()); ++i) {
        const auto& prim = m_primitives[i];
        if (isPointNearPrimitive(point, prim, tolerance)) {
            // Among matches, prefer the one whose center is closest
            QPointF center = getPrimitiveCenter(prim);
            double dx = point.x() - center.x();
            double dy = point.y() - center.y();
            double dist = std::sqrt(dx * dx + dy * dy);
            if (best_index < 0 || dist < best_dist) {
                best_index = i;
                best_dist = dist;
            }
        }
    }

    return best_index;
}

std::optional<BBox> GeometryModel::boundingBox() const {
    if (m_primitives.empty()) {
        return std::nullopt;
    }

    double xmin =  std::numeric_limits<double>::max();
    double ymin =  std::numeric_limits<double>::max();
    double xmax = -std::numeric_limits<double>::max();
    double ymax = -std::numeric_limits<double>::max();

    for (const auto& prim : m_primitives) {
        std::visit([&](const auto& p) {
            using T = std::decay_t<decltype(p)>;
            if constexpr (std::is_same_v<T, RectPrimitive>) {
                double left   = p.x;
                double right  = p.x + p.width;
                double bottom = p.y;
                double top    = p.y + p.height;
                if (left > right) std::swap(left, right);
                if (bottom > top) std::swap(bottom, top);
                xmin = std::min(xmin, left);
                xmax = std::max(xmax, right);
                ymin = std::min(ymin, bottom);
                ymax = std::max(ymax, top);
            } else if constexpr (std::is_same_v<T, LinePrimitive>) {
                xmin = std::min({xmin, p.x1, p.x2});
                xmax = std::max({xmax, p.x1, p.x2});
                ymin = std::min({ymin, p.y1, p.y2});
                ymax = std::max({ymax, p.y1, p.y2});
            } else if constexpr (std::is_same_v<T, CirclePrimitive>) {
                xmin = std::min(xmin, p.cx - p.radius);
                xmax = std::max(xmax, p.cx + p.radius);
                ymin = std::min(ymin, p.cy - p.radius);
                ymax = std::max(ymax, p.cy + p.radius);
            }
        }, prim);
    }

    return BBox{xmin, ymin, xmax, ymax};
}

bool GeometryModel::isPointInside(const QPointF& point) const {
    return findContainingPrimitive(point) >= 0;
}

int GeometryModel::findContainingPrimitive(const QPointF& point) const {
    for (int i = 0; i < static_cast<int>(m_primitives.size()); ++i) {
        if (isPointNearPrimitive(point, m_primitives[i], 0.001)) {
            return i;
        }
    }
    return -1;
}
