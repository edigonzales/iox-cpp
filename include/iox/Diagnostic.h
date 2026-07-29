#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <optional>

namespace iox {

/// Structured diagnostic produced during reading or writing.
/// Non-fatal diagnostics accumulate; fatal errors are reported
/// through the Reader/Writer return values.
struct Diagnostic final {
    enum class Severity {
        Warning,
        Error,
        Fatal
    };

    Severity severity = Severity::Warning;
    std::string code;       // stable error code, e.g. "xtf.unknown.element"
    std::string message;    // human-readable description

    struct Location {
        std::string sourceName;
        std::uint64_t byteOffset = 0;
        int line = 0;
        int column = 0;
    };
    std::optional<Location> location;
};

/// Stable error codes for programmatic handling.
namespace ErrorCode {
    inline constexpr const char* XmlMalformed      = "xml.malformed";
    inline constexpr const char* XtfStateViolation  = "xtf.state_violation";
    inline constexpr const char* XtfUnsupportedVersion = "xtf.unsupported_version";
    inline constexpr const char* XtfUnknownElement  = "xtf.unknown_element";
    inline constexpr const char* XtfMissingName     = "xtf.missing_name";
    inline constexpr const char* IoError            = "io.error";
    inline constexpr const char* InvalidArgument    = "invalid_argument";
    inline constexpr const char* InvalidState       = "invalid_state";
    inline constexpr const char* JsonParseError     = "json.parse_error";
    inline constexpr const char* InternalError      = "internal_error";
}

} // namespace iox
