#include "GeosGeometryAssembler.h"

#include "GeosContext.h"
#include "iox/Diagnostic.h"

#include <geos_c.h>

namespace iox::geometry::detail {

void GeosGeometryAssembler::validate(
    const std::vector<std::uint8_t>& wkb) const {
    GEOSWKBReader* reader = GEOSWKBReader_create_r(context_.get());
    if (reader == nullptr) {
        throw IoxError(DiagnosticCode::InvalidGeometry,
                       "Could not create a GEOS WKB reader");
    }
    GEOSGeometry* geometry = GEOSWKBReader_read_r(
        context_.get(), reader, wkb.data(), wkb.size());
    GEOSWKBReader_destroy_r(context_.get(), reader);
    if (geometry == nullptr) {
        throw IoxError(DiagnosticCode::InvalidGeometry,
                       "GEOS rejected projected WKB");
    }
    const char valid = GEOSisValid_r(context_.get(), geometry);
    GEOSGeom_destroy_r(context_.get(), geometry);
    if (valid == 0) {
        throw IoxError(DiagnosticCode::InvalidGeometry,
                       "GEOS rejected invalid projected geometry");
    }
}

} // namespace iox::geometry::detail
