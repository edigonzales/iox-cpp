#pragma once

#include "iox/xtf/XtfVersion.h"

#include <string>
#include <optional>

namespace iox {
namespace xtf {

/// Options controlling XTF reader behaviour.
struct XtfReaderOptions final {
    /// If true, certain non-fatal issues become fatal.
    bool strict = false;

    /// Name for the data source (used in diagnostics).
    std::string sourceName;

    /// If set, the reader will reject documents whose detected version
    /// does not match.
    std::optional<XtfVersion> expectedVersion;

    /// If true, unknown fachlich extension elements are preserved
    /// in the IOM structure instead of being diagnosed.
    bool preserveUnknownExtensions = false;
};

/// Options controlling XTF writer behaviour.
struct XtfWriterOptions final {
    /// The XTF version to write.
    XtfVersion version = XtfVersion::Xtf24;

    /// Strict mode: additional validation rules.
    bool strict = false;

    /// Pretty-print with indentation.
    bool pretty = true;

    /// Sender string for the header.
    std::string sender;

    /// Comment string for the header.
    std::string comment;

    /// Software identifier.
    std::string software = "iox-cpp";
};

} // namespace xtf
} // namespace iox
