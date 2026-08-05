#pragma once

#include "GeometryTypes.h"
#include "iox/IomObject.h"

#include <cstddef>
#include <vector>

namespace iox::geometry::detail {

class ArcStroker final {
public:
    static std::vector<Coordinate> stroke(
        const Coordinate& start,
        const IomObject& arc,
        double maxSagitta,
        std::size_t maxSegments);
};

} // namespace iox::geometry::detail
