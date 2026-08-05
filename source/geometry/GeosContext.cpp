#include "GeosContext.h"

#include "iox/Diagnostic.h"

namespace iox::geometry::detail {

GeosContext::GeosContext() : handle_(GEOS_init_r()) {
    if (handle_ == nullptr) {
        throw IoxError(DiagnosticCode::InvalidGeometry,
                       "Could not create a GEOS context");
    }
}

GeosContext::~GeosContext() {
    if (handle_ != nullptr) GEOS_finish_r(handle_);
}

GEOSContextHandle_t GeosContext::get() const noexcept { return handle_; }

} // namespace iox::geometry::detail
