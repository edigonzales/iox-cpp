#pragma once

namespace iox {
namespace xtf {

/// Detected or expected XTF version.
enum class XtfVersion {
    Unknown,
    Xtf23,
    Xtf24
};

/// Return a string representation of the version.
inline const char* toString(XtfVersion v) {
    switch (v) {
    case XtfVersion::Unknown: return "unknown";
    case XtfVersion::Xtf23:   return "2.3";
    case XtfVersion::Xtf24:   return "2.4";
    }
    return "?";
}

} // namespace xtf
} // namespace iox
