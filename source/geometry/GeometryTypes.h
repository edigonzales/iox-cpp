#pragma once

#include <optional>
#include <vector>

namespace iox::geometry::detail {

struct Coordinate final {
    double x = 0;
    double y = 0;
    std::optional<double> z;
};

using LineString = std::vector<Coordinate>;

struct Polygon final {
    LineString shell;
    std::vector<LineString> holes;
};

struct MultiPoint final {
    std::vector<Coordinate> points;
};

struct MultiLineString final {
    std::vector<LineString> lines;
};

struct MultiPolygon final {
    std::vector<Polygon> polygons;
};

} // namespace iox::geometry::detail
