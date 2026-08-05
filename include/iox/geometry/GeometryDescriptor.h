#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace iox::geometry {

enum class GeometryKind {
    Coord,
    MultiCoord,
    Polyline,
    DirectedPolyline,
    MultiPolyline,
    DirectedMultiPolyline,
    Surface,
    MultiSurface,
    Area,
    MultiArea
};

struct LineFormDescriptor final {
    std::string name;
    bool standardStraight = false;
    bool standardArc = false;
    std::optional<std::string> structureFqn;
};

struct GeometryDescriptor final {
    GeometryKind kind = GeometryKind::Coord;
    std::string coordinateDomainFqn;
    std::size_t dimension = 2;

    std::optional<std::string> maxOverlapLexical;
    std::optional<double> maxOverlap;

    std::vector<LineFormDescriptor> lineForms;

    bool hasStraights = false;
    bool hasArcs = false;
    bool hasCustomLineForms = false;
    bool hasLineAttributes = false;
};

} // namespace iox::geometry
