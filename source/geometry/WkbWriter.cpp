#include "WkbWriter.h"

#include "iox/Diagnostic.h"

#include <cstring>
#include <limits>

namespace iox::geometry::detail {
namespace {

void appendUInt32(std::vector<std::uint8_t>& output, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8) {
        output.push_back(static_cast<std::uint8_t>((value >> shift) & 0xffU));
    }
}

void appendDouble(std::vector<std::uint8_t>& output, double value) {
    static_assert(sizeof(double) == sizeof(std::uint64_t));
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    for (unsigned shift = 0; shift < 64; shift += 8) {
        output.push_back(static_cast<std::uint8_t>((bits >> shift) & 0xffU));
    }
}

void appendCoordinate(std::vector<std::uint8_t>& output,
                      const Coordinate& coordinate, std::size_t dimension) {
    appendDouble(output, coordinate.x);
    appendDouble(output, coordinate.y);
    if (dimension == 3) {
        if (!coordinate.z) {
            throw IoxError(DiagnosticCode::InvalidGeometry,
                           "3D WKB coordinate is missing C3");
        }
        appendDouble(output, *coordinate.z);
    }
}

void appendLineBody(std::vector<std::uint8_t>& output,
                    const LineString& line, std::size_t dimension) {
    if (line.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw IoxError(DiagnosticCode::InvalidGeometry,
                       "WKB line has too many coordinates");
    }
    appendUInt32(output, static_cast<std::uint32_t>(line.size()));
    for (const auto& coordinate : line) {
        appendCoordinate(output, coordinate, dimension);
    }
}

} // namespace

WkbWriter::WkbWriter(std::size_t dimension) : dimension_(dimension) {
    if (dimension_ != 2 && dimension_ != 3) {
        throw IoxError(DiagnosticCode::InvalidGeometry,
                       "WKB supports only 2D and 3D coordinates");
    }
}

std::vector<std::uint8_t> WkbWriter::point(
    const Coordinate& coordinate) const {
    std::vector<std::uint8_t> output;
    output.reserve(1U + 4U + dimension_ * 8U);
    output.push_back(1U);
    appendUInt32(output, static_cast<std::uint32_t>(1000U * (dimension_ == 3) + 1U));
    appendCoordinate(output, coordinate, dimension_);
    return output;
}

std::vector<std::uint8_t> WkbWriter::multiPoint(
    const MultiPoint& geometry) const {
    if (geometry.points.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw IoxError(DiagnosticCode::InvalidGeometry,
                       "WKB multipoint has too many points");
    }
    std::vector<std::uint8_t> output;
    output.push_back(1U);
    appendUInt32(output,
                 static_cast<std::uint32_t>(1000U * (dimension_ == 3) + 4U));
    appendUInt32(output, static_cast<std::uint32_t>(geometry.points.size()));
    for (const auto& pointValue : geometry.points) {
        const auto encoded = point(pointValue);
        output.insert(output.end(), encoded.begin(), encoded.end());
    }
    return output;
}

std::vector<std::uint8_t> WkbWriter::lineString(
    const LineString& line) const {
    std::vector<std::uint8_t> output;
    output.push_back(1U);
    appendUInt32(output,
                 static_cast<std::uint32_t>(1000U * (dimension_ == 3) + 2U));
    appendLineBody(output, line, dimension_);
    return output;
}

std::vector<std::uint8_t> WkbWriter::multiLineString(
    const MultiLineString& geometry) const {
    if (geometry.lines.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw IoxError(DiagnosticCode::InvalidGeometry,
                       "WKB multilinestring has too many lines");
    }
    std::vector<std::uint8_t> output;
    output.push_back(1U);
    appendUInt32(output,
                 static_cast<std::uint32_t>(1000U * (dimension_ == 3) + 5U));
    appendUInt32(output, static_cast<std::uint32_t>(geometry.lines.size()));
    for (const auto& line : geometry.lines) {
        const auto encoded = lineString(line);
        output.insert(output.end(), encoded.begin(), encoded.end());
    }
    return output;
}

std::vector<std::uint8_t> WkbWriter::polygon(
    const Polygon& geometry) const {
    if (geometry.shell.empty()) {
        throw IoxError(DiagnosticCode::InvalidGeometry,
                       "WKB polygon has no exterior ring");
    }
    const auto ringCount = geometry.holes.size() + 1U;
    if (ringCount > std::numeric_limits<std::uint32_t>::max()) {
        throw IoxError(DiagnosticCode::InvalidGeometry,
                       "WKB polygon has too many rings");
    }
    std::vector<std::uint8_t> output;
    output.push_back(1U);
    appendUInt32(output,
                 static_cast<std::uint32_t>(1000U * (dimension_ == 3) + 3U));
    appendUInt32(output, static_cast<std::uint32_t>(ringCount));
    appendLineBody(output, geometry.shell, dimension_);
    for (const auto& hole : geometry.holes) {
        appendLineBody(output, hole, dimension_);
    }
    return output;
}

std::vector<std::uint8_t> WkbWriter::multiPolygon(
    const MultiPolygon& geometry) const {
    if (geometry.polygons.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw IoxError(DiagnosticCode::InvalidGeometry,
                       "WKB multipolygon has too many polygons");
    }
    std::vector<std::uint8_t> output;
    output.push_back(1U);
    appendUInt32(output,
                 static_cast<std::uint32_t>(1000U * (dimension_ == 3) + 6U));
    appendUInt32(output, static_cast<std::uint32_t>(geometry.polygons.size()));
    for (const auto& polygonValue : geometry.polygons) {
        const auto encoded = polygon(polygonValue);
        output.insert(output.end(), encoded.begin(), encoded.end());
    }
    return output;
}

} // namespace iox::geometry::detail
