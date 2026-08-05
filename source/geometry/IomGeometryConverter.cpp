#include "iox/geometry/IomGeometryConverter.h"

#include "ArcStroker.h"
#include "GeometryTypes.h"
#include "WkbWriter.h"

#if IOX_HAS_GEOS
#include "GeosGeometryAssembler.h"
#endif

#include "iox/Diagnostic.h"

#include <algorithm>
#include <cmath>
#include <locale>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace iox::geometry {
namespace {

using detail::Coordinate;
using detail::LineString;

[[noreturn]] void geometryError(std::string message) {
    throw IoxError(DiagnosticCode::InvalidGeometry, std::move(message));
}

std::string upperAscii(std::string_view value) {
    std::string result(value);
    for (auto& character : result) {
        if (character >= 'a' && character <= 'z') {
            character = static_cast<char>(character - 'a' + 'A');
        }
    }
    return result;
}

std::string tagName(const IomObject& object) {
    if (object.tag().hasInterlisName()) return object.tag().interlisName();
    if (object.tag().hasXmlName()) return object.tag().xmlName().localName;
    return {};
}

void requireTag(const IomObject& object,
                std::initializer_list<std::string_view> expected) {
    const auto actual = upperAscii(tagName(object));
    for (const auto candidate : expected) {
        if (actual == upperAscii(candidate)) return;
    }
    geometryError("Unexpected geometry tag: " + actual);
}

double parseNumber(const IomObject& object, std::string_view name) {
    const auto value = object.primitive(name);
    if (!value) {
        geometryError("Geometry member is missing: " + std::string(name));
    }
    std::istringstream stream{std::string(*value)};
    stream.imbue(std::locale::classic());
    stream >> std::noskipws;
    double result = 0.0;
    char extra = '\0';
    if (!(stream >> result) || (stream >> extra) || !std::isfinite(result)) {
        geometryError("Geometry member is not a finite number: " +
                      std::string(name));
    }
    return result;
}

bool closeCoordinates(const Coordinate& left, const Coordinate& right,
                      double tolerance) {
    if (std::hypot(left.x - right.x, left.y - right.y) > tolerance) {
        return false;
    }
    if (left.z.has_value() != right.z.has_value()) return false;
    return !left.z || std::abs(*left.z - *right.z) <= tolerance;
}

Coordinate parseCoordinate(const IomObject& object, std::size_t dimension) {
    requireTag(object, {"COORD"});
    const auto c3 = object.primitive("C3");
    if (dimension == 3 && !c3) {
        geometryError("3D COORD requires C3");
    }
    if (dimension == 2 && c3) {
        geometryError("2D COORD cannot contain C3");
    }
    return {parseNumber(object, "C1"), parseNumber(object, "C2"),
            c3 ? std::optional<double>(parseNumber(object, "C3"))
               : std::nullopt};
}

struct ConversionContext final {
    std::size_t dimension = 2;
    double arcSagitta = 0.001;
    double endpointTolerance = 1e-9;
    std::size_t maxArcSegments = 100000;
    bool arcsApproximated = false;
};

std::optional<IomObject> firstObject(const IomObject& object,
                                     std::string_view name);
std::size_t countNamed(const IomObject& object, std::string_view name);

struct ParsedLine final {
    LineString value;
};

void appendUnique(LineString& line, const Coordinate& coordinate,
                  double tolerance) {
    if (line.empty() || !closeCoordinates(line.back(), coordinate, tolerance)) {
        line.push_back(coordinate);
    }
}

ParsedLine parsePolyline(const IomObject& object, ConversionContext& context) {
    requireTag(object, {"POLYLINE"});
    if (object.consistency() == Consistency::Incomplete ||
        object.consistency() == Consistency::Inconsistent) {
        geometryError("Incomplete or inconsistent POLYLINE is unsupported");
    }
    const auto sequence = firstObject(object, "sequence");
    const auto sequenceCount = countNamed(object, "sequence");
    if (!sequence || sequenceCount != 1U ||
        object.hasAttribute("lineattr") || object.hasAttribute("LINEATTR")) {
        geometryError("POLYLINE requires exactly one sequence");
    }
    LineString result;
    const auto segmentName = sequence->hasAttribute("segment") ? "segment"
                                                                  : "SEGMENT";
    for (std::size_t index = 0; index < sequence->valueCount(segmentName);
         ++index) {
        const auto segment = sequence->object(segmentName, index);
        if (!segment) geometryError("POLYLINE segment is not an object");
        const auto segmentTag = upperAscii(tagName(*segment));
        if (segmentTag == "COORD") {
            appendUnique(result, parseCoordinate(*segment, context.dimension),
                         context.endpointTolerance);
            continue;
        }
        if (segmentTag == "ARC") {
            if (result.empty()) {
                geometryError("POLYLINE ARC requires a preceding COORD");
            }
            const auto points = detail::ArcStroker::stroke(
                result.back(), *segment, context.arcSagitta,
                context.maxArcSegments);
            for (const auto& point : points) {
                appendUnique(result, point, context.endpointTolerance);
            }
            context.arcsApproximated = true;
            continue;
        }
        geometryError("Unsupported POLYLINE segment: " + segmentTag);
    }
    if (result.size() < 2U) {
        geometryError("POLYLINE requires at least two coordinates");
    }
    return {std::move(result)};
}

std::vector<LineString> boundaryLines(const IomObject& boundary,
                                       ConversionContext& context) {
    const auto name = boundary.hasAttribute("polyline") ? "polyline"
                                                           : "POLYLINE";
    const auto count = boundary.valueCount(name);
    if (count == 0U) geometryError("Surface boundary has no POLYLINE");
    std::vector<LineString> result;
    result.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        const auto line = boundary.object(name, index);
        if (!line) geometryError("Surface POLYLINE is not an object");
        result.push_back(parsePolyline(*line, context).value);
    }
    return result;
}

LineString joinRing(const std::vector<LineString>& parts,
                    const ConversionContext& context) {
    if (parts.empty()) geometryError("Surface ring has no line parts");
    LineString result = parts.front();
    for (std::size_t partIndex = 1; partIndex < parts.size(); ++partIndex) {
        const auto& part = parts[partIndex];
        if (part.empty()) geometryError("Surface ring contains an empty part");
        if (closeCoordinates(result.back(), part.front(),
                             context.endpointTolerance)) {
            for (std::size_t index = 1; index < part.size(); ++index) {
                result.push_back(part[index]);
            }
        } else if (closeCoordinates(result.back(), part.back(),
                                    context.endpointTolerance)) {
            for (std::size_t index = part.size() - 1; index > 0; --index) {
                result.push_back(part[index - 1]);
            }
        } else {
            geometryError("Surface ring parts are not connectable");
        }
    }
    if (result.size() < 4U ||
        !closeCoordinates(result.front(), result.back(),
                          context.endpointTolerance)) {
        geometryError("Surface ring is not closed");
    }
    result.back() = result.front();
    return result;
}

std::optional<IomObject> firstObject(const IomObject& object,
                                     std::string_view name) {
    if (const auto result = object.object(name)) return result;
    std::string upper(name);
    for (auto& character : upper) {
        if (character >= 'a' && character <= 'z') {
            character = static_cast<char>(character - 'a' + 'A');
        }
    }
    return object.object(upper);
}

std::size_t countNamed(const IomObject& object, std::string_view name) {
    if (object.hasAttribute(name)) return object.valueCount(name);
    std::string upper(name);
    for (auto& character : upper) {
        if (character >= 'a' && character <= 'z') {
            character = static_cast<char>(character - 'a' + 'A');
        }
    }
    return object.valueCount(upper);
}

std::optional<IomObject> objectNamed(const IomObject& object,
                                     std::string_view name,
                                     std::size_t index = 0) {
    if (object.hasAttribute(name)) return object.object(name, index);
    std::string upper(name);
    for (auto& character : upper) {
        if (character >= 'a' && character <= 'z') {
            character = static_cast<char>(character - 'a' + 'A');
        }
    }
    return object.object(upper, index);
}

detail::Polygon parsePolygon(const IomObject& object,
                             ConversionContext& context) {
    requireTag(object, {"SURFACE", "AREA"});
    if (object.consistency() == Consistency::Incomplete ||
        object.consistency() == Consistency::Inconsistent ||
        object.hasAttribute("clipped") || object.hasAttribute("CLIPPED") ||
        object.hasAttribute("lineattr") || object.hasAttribute("LINEATTR")) {
        geometryError("Incomplete, clipped, or attributed surface is unsupported");
    }

    std::vector<LineString> exterior;
    std::vector<std::vector<LineString>> interiors;
    if (countNamed(object, "boundary") > 1U ||
        countNamed(object, "exterior") > 1U) {
        geometryError("Surface has multiple exterior boundaries");
    }
    if (const auto boundaryObject = firstObject(object, "boundary")) {
        exterior = boundaryLines(*boundaryObject, context);
    } else if (const auto exteriorObject = firstObject(object, "exterior")) {
        exterior = boundaryLines(*exteriorObject, context);
        const auto interiorCount = countNamed(object, "interior");
        for (std::size_t index = 0; index < interiorCount; ++index) {
            const auto interior = objectNamed(object, "interior", index);
            if (!interior) geometryError("Surface interior is not an object");
            interiors.push_back(boundaryLines(*interior, context));
        }
    } else {
        geometryError("Surface requires a boundary or exterior");
    }

    detail::Polygon result;
    result.shell = joinRing(exterior, context);
    for (const auto& parts : interiors) {
        result.holes.push_back(joinRing(parts, context));
    }
    return result;
}

detail::MultiPoint parseMultiPoint(const IomObject& object,
                                   ConversionContext& context) {
    requireTag(object, {"MULTICOORD"});
    const auto count = countNamed(object, "coord");
    if (count == 0U) geometryError("MULTICOORD requires a coordinate");
    detail::MultiPoint result;
    result.points.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        const auto coordinate = objectNamed(object, "coord", index);
        if (!coordinate) geometryError("MULTICOORD member is not an object");
        result.points.push_back(parseCoordinate(*coordinate, context.dimension));
    }
    return result;
}

detail::MultiLineString parseMultiLine(const IomObject& object,
                                       ConversionContext& context) {
    requireTag(object, {"MULTIPOLYLINE"});
    const auto count = countNamed(object, "polyline");
    if (count == 0U) geometryError("MULTIPOLYLINE requires a polyline");
    detail::MultiLineString result;
    result.lines.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        const auto line = objectNamed(object, "polyline", index);
        if (!line) geometryError("MULTIPOLYLINE member is not an object");
        result.lines.push_back(parsePolyline(*line, context).value);
    }
    return result;
}

detail::MultiPolygon parseMultiPolygon(const IomObject& object,
                                       ConversionContext& context,
                                       bool area) {
    requireTag(object, area ? std::initializer_list<std::string_view>{"MULTIAREA", "MULTISURFACE"}
                            : std::initializer_list<std::string_view>{"MULTISURFACE"});
    const auto name = area ? "area" : "surface";
    const auto count = countNamed(object, name);
    if (count == 0U && area) {
        const auto alternate = countNamed(object, "surface");
        if (alternate != 0U) {
            detail::MultiPolygon result;
            result.polygons.reserve(alternate);
            for (std::size_t index = 0; index < alternate; ++index) {
                const auto child = objectNamed(object, "surface", index);
                if (!child) geometryError("MULTIAREA member is not an object");
                result.polygons.push_back(parsePolygon(*child, context));
            }
            return result;
        }
    }
    if (count == 0U) geometryError("Multi-surface requires a member");
    detail::MultiPolygon result;
    result.polygons.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        const auto child = objectNamed(object, name, index);
        if (!child) geometryError("Multi-surface member is not an object");
        result.polygons.push_back(parsePolygon(*child, context));
    }
    return result;
}

} // namespace

IomGeometryConverter::IomGeometryConverter(GeometryConversionOptions options)
    : options_(std::move(options))
#if IOX_HAS_GEOS
      , geos_(std::make_unique<detail::GeosGeometryAssembler>())
#endif
{}

IomGeometryConverter::~IomGeometryConverter() = default;

GeometryConversionResult IomGeometryConverter::toWkb(
    const IomObject& geometry, const GeometryDescriptor& descriptor) const {
    if (descriptor.dimension != 2U && descriptor.dimension != 3U) {
        geometryError("Only 2D and 3D geometry projection is supported");
    }
    if (!std::isfinite(options_.endpointTolerance) ||
        options_.endpointTolerance < 0.0 ||
        !std::isfinite(options_.defaultArcTolerance) ||
        options_.defaultArcTolerance <= 0.0 ||
        (options_.arcToleranceOverride &&
         (!std::isfinite(*options_.arcToleranceOverride) ||
          *options_.arcToleranceOverride <= 0.0))) {
        throw IoxError(DiagnosticCode::InvalidArgument,
                       "Invalid geometry conversion tolerance");
    }
    if (descriptor.hasCustomLineForms) {
        geometryError("Custom line forms are unsupported by native projection");
    }
    if (descriptor.hasLineAttributes) {
        geometryError("Line attributes are unsupported by native projection");
    }
    if (geometry.consistency() == Consistency::Incomplete ||
        geometry.consistency() == Consistency::Inconsistent) {
        geometryError("Incomplete or inconsistent geometry is unsupported");
    }

    ConversionContext context;
    context.dimension = descriptor.dimension;
    context.arcSagitta = options_.arcToleranceOverride.value_or(
        descriptor.maxOverlap.value_or(options_.defaultArcTolerance));
    context.endpointTolerance = options_.endpointTolerance;
    context.maxArcSegments = options_.maxArcSegments;
    detail::WkbWriter writer(descriptor.dimension);
    std::vector<std::uint8_t> wkb;
    switch (descriptor.kind) {
    case GeometryKind::Coord:
        wkb = writer.point(parseCoordinate(geometry, descriptor.dimension));
        break;
    case GeometryKind::MultiCoord:
        wkb = writer.multiPoint(parseMultiPoint(geometry, context));
        break;
    case GeometryKind::Polyline:
    case GeometryKind::DirectedPolyline:
        wkb = writer.lineString(parsePolyline(geometry, context).value);
        break;
    case GeometryKind::MultiPolyline:
    case GeometryKind::DirectedMultiPolyline:
        wkb = writer.multiLineString(parseMultiLine(geometry, context));
        break;
    case GeometryKind::Surface:
    case GeometryKind::Area:
        wkb = writer.polygon(parsePolygon(geometry, context));
        break;
    case GeometryKind::MultiSurface:
        wkb = writer.multiPolygon(parseMultiPolygon(geometry, context, false));
        break;
    case GeometryKind::MultiArea:
        wkb = writer.multiPolygon(parseMultiPolygon(geometry, context, true));
        break;
    }

#if IOX_HAS_GEOS
    geos_->validate(wkb);
#endif

    GeometryConversionResult result;
    result.wkb = std::move(wkb);
    result.arcsApproximated = context.arcsApproximated;
    if (context.arcsApproximated) result.arcToleranceUsed = context.arcSagitta;
    return result;
}

} // namespace iox::geometry
