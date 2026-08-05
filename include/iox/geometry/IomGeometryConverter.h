#pragma once

#include "iox/geometry/GeometryDescriptor.h"
#include "iox/IomObject.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#ifndef IOX_HAS_GEOS
#define IOX_HAS_GEOS 0
#endif

namespace iox::geometry {

#if IOX_HAS_GEOS
namespace detail {
class GeosGeometryAssembler;
}
#endif

struct GeometryConversionOptions final {
    std::optional<double> arcToleranceOverride;
    double defaultArcTolerance = 0.001;
    double endpointTolerance = 1e-9;
    std::size_t maxArcSegments = 100000;
};

struct GeometryConversionResult final {
    std::vector<std::uint8_t> wkb;
    bool arcsApproximated = false;
    std::optional<double> arcToleranceUsed;
};

class IomGeometryConverter final {
public:
    explicit IomGeometryConverter(GeometryConversionOptions options = {});
    ~IomGeometryConverter();

    GeometryConversionResult toWkb(
        const IomObject& geometry,
        const GeometryDescriptor& descriptor) const;

private:
    GeometryConversionOptions options_;
#if IOX_HAS_GEOS
    std::unique_ptr<detail::GeosGeometryAssembler> geos_;
#endif
};

} // namespace iox::geometry
