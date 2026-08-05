#pragma once

#include "GeometryTypes.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace iox::geometry::detail {

class WkbWriter final {
public:
    explicit WkbWriter(std::size_t dimension);

    std::vector<std::uint8_t> point(const Coordinate&) const;
    std::vector<std::uint8_t> multiPoint(const MultiPoint&) const;
    std::vector<std::uint8_t> lineString(const LineString&) const;
    std::vector<std::uint8_t> multiLineString(
        const MultiLineString&) const;
    std::vector<std::uint8_t> polygon(const Polygon&) const;
    std::vector<std::uint8_t> multiPolygon(
        const MultiPolygon&) const;

private:
    std::size_t dimension_;
};

} // namespace iox::geometry::detail
