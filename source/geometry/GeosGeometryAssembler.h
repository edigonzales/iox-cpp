#pragma once

#include "GeosContext.h"

#include <cstdint>
#include <vector>

namespace iox::geometry::detail {

class GeosGeometryAssembler final {
public:
    GeosGeometryAssembler() = default;
    ~GeosGeometryAssembler() = default;

    void validate(const std::vector<std::uint8_t>& wkb) const;

private:
    GeosContext context_;
};

} // namespace iox::geometry::detail
