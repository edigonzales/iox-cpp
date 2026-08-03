#pragma once

#include "iox/Events.h"

namespace iox {
namespace xtf {

using ::iox::XtfVersion;

inline const char* toString(XtfVersion version) noexcept {
    return xtfVersionName(version);
}

} // namespace xtf
} // namespace iox
