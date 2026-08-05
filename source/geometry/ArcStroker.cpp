#include "ArcStroker.h"

#include "iox/Diagnostic.h"

#include <algorithm>
#include <cmath>
#include <locale>
#include <sstream>
#include <string>
#include <utility>

namespace iox::geometry::detail {
namespace {

constexpr double pi = 3.141592653589793238462643383279502884;
constexpr double twoPi = 2.0 * pi;

[[noreturn]] void arcError(std::string message) {
    throw IoxError(DiagnosticCode::InvalidGeometry, std::move(message));
}

double parseNumber(const IomObject& object, std::string_view name,
                   bool required) {
    const auto value = object.primitive(name);
    if (!value) {
        if (required) {
            arcError("ARC requires " + std::string(name));
        }
        return 0.0;
    }
    std::istringstream stream{std::string(*value)};
    stream.imbue(std::locale::classic());
    stream >> std::noskipws;
    double result = 0.0;
    char extra = '\0';
    if (!(stream >> result) || (stream >> extra) || !std::isfinite(result)) {
        arcError("ARC member is not a finite number: " + std::string(name));
    }
    return result;
}

std::optional<double> optionalNumber(const IomObject& object,
                                     std::string_view name) {
    if (!object.primitive(name)) return std::nullopt;
    return parseNumber(object, name, true);
}

double normalize(double angle) {
    angle = std::fmod(angle, twoPi);
    if (angle < 0.0) angle += twoPi;
    return angle;
}

double ccwDistance(double from, double to) {
    return normalize(to - from);
}

bool closeEnough(double left, double right) {
    return std::abs(left - right) <=
           1e-12 * std::max({1.0, std::abs(left), std::abs(right)});
}

} // namespace

std::vector<Coordinate> ArcStroker::stroke(
    const Coordinate& start, const IomObject& arc, double maxSagitta,
    std::size_t maxSegments) {
    if (!(maxSagitta > 0.0) || !std::isfinite(maxSagitta)) {
        arcError("ARC sagitta must be positive and finite");
    }
    if (maxSegments == 0U) arcError("ARC segment limit must be positive");

    const auto endC3 = optionalNumber(arc, "C3");
    const auto supportA3 = optionalNumber(arc, "A3");
    if (start.z.has_value()) {
        if (!endC3 || !supportA3) {
            arcError("3D ARC requires C3 and A3");
        }
    } else if (endC3 || supportA3) {
        arcError("2D ARC cannot contain 3D ordinates");
    }

    const Coordinate end{parseNumber(arc, "C1", true),
                         parseNumber(arc, "C2", true), endC3};
    const Coordinate support{parseNumber(arc, "A1", true),
                             parseNumber(arc, "A2", true), supportA3};
    if (const auto radius = optionalNumber(arc, "R");
        radius && (!(*radius > 0.0))) {
        arcError("ARC radius must be positive");
    }

    const double determinant =
        2.0 * (start.x * (support.y - end.y) +
               support.x * (end.y - start.y) +
               end.x * (start.y - support.y));
    const double scale = std::max({1.0, std::abs(start.x), std::abs(start.y),
                                   std::abs(support.x), std::abs(support.y),
                                   std::abs(end.x), std::abs(end.y)});
    if (std::abs(determinant) <= 1e-12 * scale * scale) {
        arcError("ARC points are collinear");
    }

    const double startSquared = start.x * start.x + start.y * start.y;
    const double supportSquared = support.x * support.x + support.y * support.y;
    const double endSquared = end.x * end.x + end.y * end.y;
    const double centerX =
        (startSquared * (support.y - end.y) +
         supportSquared * (end.y - start.y) +
         endSquared * (start.y - support.y)) /
        determinant;
    const double centerY =
        (startSquared * (end.x - support.x) +
         supportSquared * (start.x - end.x) +
         endSquared * (support.x - start.x)) /
        determinant;
    if (!std::isfinite(centerX) || !std::isfinite(centerY)) {
        arcError("ARC center is non-finite");
    }
    const double radius = std::hypot(start.x - centerX, start.y - centerY);
    if (!(radius > 0.0) || !std::isfinite(radius)) {
        arcError("ARC radius is zero or non-finite");
    }

    const double startAngle = std::atan2(start.y - centerY, start.x - centerX);
    const double supportAngle =
        std::atan2(support.y - centerY, support.x - centerX);
    const double endAngle = std::atan2(end.y - centerY, end.x - centerX);
    const double ccwSweep = ccwDistance(startAngle, endAngle);
    const double ccwSupport = ccwDistance(startAngle, supportAngle);
    const double cwSweep = ccwDistance(endAngle, startAngle);
    const double cwSupport = ccwDistance(supportAngle, startAngle);

    double sweep = 0.0;
    if (ccwSupport <= ccwSweep + 1e-10) {
        sweep = ccwSweep;
    } else if (cwSupport <= cwSweep + 1e-10) {
        sweep = -cwSweep;
    } else {
        arcError("ARC support point is not on the selected circle");
    }
    if (closeEnough(sweep, 0.0)) arcError("ARC has identical endpoints");

    const double sagittaRatio = maxSagitta / radius;
    const double acosArgument =
        std::max(-1.0, std::min(1.0, 1.0 - sagittaRatio));
    const double maximumAngle = 2.0 * std::acos(acosArgument);
    if (!(maximumAngle > 0.0) || !std::isfinite(maximumAngle)) {
        arcError("ARC sagitta produced an invalid segment angle");
    }
    const double requestedSegments =
        std::ceil(std::abs(sweep) / maximumAngle);
    if (!std::isfinite(requestedSegments) ||
        requestedSegments > static_cast<double>(maxSegments)) {
        arcError("ARC segment limit exceeded");
    }
    const auto segments = static_cast<std::size_t>(
        std::max(1.0, requestedSegments));

    std::vector<Coordinate> result;
    result.reserve(segments);
    for (std::size_t index = 1; index <= segments; ++index) {
        if (index == segments) {
            result.push_back(end);
            continue;
        }
        const double fraction = static_cast<double>(index) /
                                static_cast<double>(segments);
        const double angle = startAngle + sweep * fraction;
        Coordinate point;
        point.x = centerX + radius * std::cos(angle);
        point.y = centerY + radius * std::sin(angle);
        if (start.z) {
            point.z = *start.z + (*end.z - *start.z) * fraction;
        }
        result.push_back(point);
    }
    return result;
}

} // namespace iox::geometry::detail
