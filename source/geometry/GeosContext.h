#pragma once

#include <geos_c.h>

namespace iox::geometry::detail {

class GeosContext final {
public:
    GeosContext();
    ~GeosContext();

    GeosContext(const GeosContext&) = delete;
    GeosContext& operator=(const GeosContext&) = delete;

    GEOSContextHandle_t get() const noexcept;

private:
    GEOSContextHandle_t handle_ = nullptr;
};

} // namespace iox::geometry::detail
