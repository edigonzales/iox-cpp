#include "iox/geometry/IomGeometryConverter.h"
#include "iox/test/Test.h"

#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

namespace {

iox::geometry::GeometryDescriptor descriptor(
    iox::geometry::GeometryKind kind, std::size_t dimension = 2) {
    iox::geometry::GeometryDescriptor result;
    result.kind = kind;
    result.dimension = dimension;
    return result;
}

iox::IomObject coord(double x, double y,
                     std::optional<double> z = std::nullopt) {
    iox::IomObject result(iox::IomName("COORD"));
    result.setPrimitive(iox::IomName("C1"), std::to_string(x));
    result.setPrimitive(iox::IomName("C2"), std::to_string(y));
    if (z) result.setPrimitive(iox::IomName("C3"), std::to_string(*z));
    return result;
}

iox::IomObject polyline(std::vector<iox::IomObject> segments) {
    iox::IomObject sequence(iox::IomName("SEGMENTS"));
    for (auto& segment : segments) {
        sequence.appendObject(iox::IomName("segment"), std::move(segment));
    }
    iox::IomObject result(iox::IomName("POLYLINE"));
    result.setObject(iox::IomName("sequence"), std::move(sequence));
    return result;
}

iox::IomObject surface(iox::IomObject boundary) {
    iox::IomObject result(iox::IomName("SURFACE"));
    result.setObject(iox::IomName("boundary"), std::move(boundary));
    return result;
}

std::uint32_t wkbType(const std::vector<std::uint8_t>& wkb) {
    IOX_CHECK_EQ(static_cast<std::size_t>(1), wkb[0]);
    std::uint32_t value = 0;
    std::memcpy(&value, wkb.data() + 1, sizeof(value));
    return value;
}

std::uint32_t wkbCount(const std::vector<std::uint8_t>& wkb,
                       std::size_t offset) {
    std::uint32_t value = 0;
    std::memcpy(&value, wkb.data() + offset, sizeof(value));
    return value;
}

template<typename Operation>
bool throwsInvalidGeometry(const Operation& operation) {
    try {
        operation();
    } catch (const iox::IoxError& error) {
        return error.code() == iox::DiagnosticCode::InvalidGeometry;
    }
    return false;
}

} // namespace

IOX_TEST(geometry_converter_writes_2d_and_3d_points) {
    iox::geometry::IomGeometryConverter converter;
    const auto point = converter.toWkb(coord(1, 2),
                                       descriptor(iox::geometry::GeometryKind::Coord));
    IOX_CHECK_EQ(static_cast<std::size_t>(21), point.wkb.size());
    IOX_CHECK_EQ(static_cast<std::uint32_t>(1), wkbType(point.wkb));
    IOX_CHECK(!point.arcsApproximated);

    const auto point3 = converter.toWkb(
        coord(1, 2, 3), descriptor(iox::geometry::GeometryKind::Coord, 3));
    IOX_CHECK_EQ(static_cast<std::size_t>(29), point3.wkb.size());
    IOX_CHECK_EQ(static_cast<std::uint32_t>(1001), wkbType(point3.wkb));
}

IOX_TEST(geometry_converter_writes_multi_and_line_strings) {
    iox::IomObject multi(iox::IomName("MULTICOORD"));
    multi.appendObject(iox::IomName("coord"), coord(0, 0));
    multi.appendObject(iox::IomName("coord"), coord(1, 1));
    iox::geometry::IomGeometryConverter converter;
    const auto points = converter.toWkb(
        multi, descriptor(iox::geometry::GeometryKind::MultiCoord));
    IOX_CHECK_EQ(static_cast<std::uint32_t>(4), wkbType(points.wkb));
    IOX_CHECK_EQ(static_cast<std::uint32_t>(2), wkbCount(points.wkb, 5));

    auto line = polyline({coord(0, 0), coord(1, 0), coord(1, 1)});
    const auto linestring = converter.toWkb(
        line, descriptor(iox::geometry::GeometryKind::Polyline));
    IOX_CHECK_EQ(static_cast<std::uint32_t>(2), wkbType(linestring.wkb));
    IOX_CHECK_EQ(static_cast<std::uint32_t>(3), wkbCount(linestring.wkb, 5));

    iox::IomObject multiLine(iox::IomName("MULTIPOLYLINE"));
    multiLine.appendObject(iox::IomName("polyline"),
                           polyline({coord(0, 0), coord(1, 0)}));
    multiLine.appendObject(iox::IomName("polyline"),
                           polyline({coord(2, 2), coord(3, 2)}));
    const auto multiLines = converter.toWkb(
        multiLine, descriptor(iox::geometry::GeometryKind::MultiPolyline));
    IOX_CHECK_EQ(static_cast<std::uint32_t>(5), wkbType(multiLines.wkb));
    IOX_CHECK_EQ(static_cast<std::uint32_t>(2), wkbCount(multiLines.wkb, 5));
}

IOX_TEST(geometry_converter_strokes_arcs_and_preserves_endpoint) {
    iox::IomObject arc(iox::IomName("ARC"));
    arc.setPrimitive(iox::IomName("C1"), "-1");
    arc.setPrimitive(iox::IomName("C2"), "0");
    arc.setPrimitive(iox::IomName("A1"), "0");
    arc.setPrimitive(iox::IomName("A2"), "1");
    auto line = polyline({coord(1, 0), std::move(arc)});

    iox::geometry::GeometryConversionOptions options;
    options.defaultArcTolerance = 0.1;
    const iox::geometry::IomGeometryConverter converter(options);
    const auto result = converter.toWkb(
        line, descriptor(iox::geometry::GeometryKind::Polyline));
    IOX_CHECK(result.arcsApproximated);
    IOX_CHECK(result.arcToleranceUsed.has_value());
    IOX_CHECK_EQ(0.1, *result.arcToleranceUsed);
    IOX_CHECK(wkbCount(result.wkb, 5) > 2U);
}

IOX_TEST(geometry_converter_writes_polygon_hole_and_multi_polygon) {
    const auto shell = polyline({coord(0, 0), coord(10, 0), coord(10, 10),
                                 coord(0, 10), coord(0, 0)});
    iox::IomObject boundary(iox::IomName("BOUNDARY"));
    boundary.setObject(iox::IomName("polyline"), shell);
    const auto polygon = iox::geometry::IomGeometryConverter().toWkb(
        surface(std::move(boundary)),
        descriptor(iox::geometry::GeometryKind::Surface));
    IOX_CHECK_EQ(static_cast<std::uint32_t>(3), wkbType(polygon.wkb));
    IOX_CHECK_EQ(static_cast<std::uint32_t>(1), wkbCount(polygon.wkb, 5));

    iox::IomObject exterior(iox::IomName("BOUNDARY"));
    exterior.setObject(
        iox::IomName("polyline"),
        polyline({coord(0, 0), coord(10, 0), coord(10, 10), coord(0, 10),
                  coord(0, 0)}));
    iox::IomObject interior(iox::IomName("BOUNDARY"));
    interior.setObject(
        iox::IomName("polyline"),
        polyline({coord(2, 2), coord(8, 2), coord(8, 8), coord(2, 8),
                  coord(2, 2)}));
    iox::IomObject area(iox::IomName("AREA"));
    area.setObject(iox::IomName("exterior"), std::move(exterior));
    area.setObject(iox::IomName("interior"), std::move(interior));
    const auto polygonWithHole = iox::geometry::IomGeometryConverter().toWkb(
        std::move(area), descriptor(iox::geometry::GeometryKind::Area));
    IOX_CHECK_EQ(static_cast<std::uint32_t>(2), wkbCount(polygonWithHole.wkb, 5));

    iox::IomObject first(iox::IomName("SURFACE"));
    iox::IomObject firstBoundary(iox::IomName("BOUNDARY"));
    firstBoundary.setObject(
        iox::IomName("polyline"),
        polyline({coord(0, 0), coord(2, 0), coord(2, 2), coord(0, 2),
                  coord(0, 0)}));
    first.setObject(iox::IomName("boundary"), std::move(firstBoundary));
    iox::IomObject multi(iox::IomName("MULTISURFACE"));
    multi.appendObject(iox::IomName("surface"), std::move(first));
    const auto multipolygon = iox::geometry::IomGeometryConverter().toWkb(
        multi, descriptor(iox::geometry::GeometryKind::MultiSurface));
    IOX_CHECK_EQ(static_cast<std::uint32_t>(6), wkbType(multipolygon.wkb));
    IOX_CHECK_EQ(static_cast<std::uint32_t>(1), wkbCount(multipolygon.wkb, 5));
}

IOX_TEST(geometry_converter_rejects_unsupported_and_malformed_values) {
    iox::IomObject line = polyline({coord(0, 0), coord(1, 1)});
    auto custom = descriptor(iox::geometry::GeometryKind::Polyline);
    custom.hasCustomLineForms = true;
    IOX_CHECK(throwsInvalidGeometry([&] {
        (void)iox::geometry::IomGeometryConverter().toWkb(line, custom);
    }));

    auto lineAttributes = descriptor(iox::geometry::GeometryKind::Polyline);
    lineAttributes.hasLineAttributes = true;
    IOX_CHECK(throwsInvalidGeometry([&] {
        (void)iox::geometry::IomGeometryConverter().toWkb(line, lineAttributes);
    }));

    line.setConsistency(iox::Consistency::Incomplete);
    IOX_CHECK(throwsInvalidGeometry([&] {
        (void)iox::geometry::IomGeometryConverter().toWkb(
            line, descriptor(iox::geometry::GeometryKind::Polyline));
    }));

    iox::IomObject oneDimension = coord(1, 2);
    IOX_CHECK(throwsInvalidGeometry([&] {
        (void)iox::geometry::IomGeometryConverter().toWkb(
            oneDimension, descriptor(iox::geometry::GeometryKind::Coord, 1));
    }));
    IOX_CHECK(throwsInvalidGeometry([&] {
        (void)iox::geometry::IomGeometryConverter().toWkb(
            oneDimension, descriptor(iox::geometry::GeometryKind::Coord, 4));
    }));
    IOX_CHECK(throwsInvalidGeometry([&] {
        (void)iox::geometry::IomGeometryConverter().toWkb(
            coord(1, 2), descriptor(iox::geometry::GeometryKind::Coord, 3));
    }));
}

#include "iox/test/TestMain.h"
