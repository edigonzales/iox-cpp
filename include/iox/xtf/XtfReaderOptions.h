#pragma once

#include "iox/xtf/XtfVersion.h"

#include <string>
#include <optional>
#include <cstddef>

namespace iox {
namespace xtf {

enum class Strictness { Lenient, Strict };

struct XmlLimits final {
    std::size_t maxDepth = 256;
    std::size_t maxAttributesPerElement = 1024;
    std::size_t maxTextBytesPerNode = 16U * 1024U * 1024U;
    std::size_t maxTotalInputBytes = 0;
    std::size_t maxQueuedEvents = 1024;
};

/// Options controlling XTF reader behaviour.
struct XtfReaderOptions final {
    Strictness strictness = Strictness::Lenient;

    XmlLimits xmlLimits;

    /// Name for the data source (used in diagnostics).
    std::string sourceName;

    /// If set, the reader will reject documents whose detected version
    /// does not match.
    std::optional<XtfVersion> expectedVersion;

    /// If true, unknown fachlich extension elements are preserved
    /// in the IOM structure instead of being diagnosed.
    bool preserveUnknownExtensions = true;

    bool requireAtLeastOneModel = true;

    bool allowVersionAutoDetection = true;
};

/// Options controlling XTF writer behaviour.
struct XtfWriterOptions final {
    /// The XTF version to write.
    XtfVersion version = XtfVersion::V23;

    /// Strict mode: additional validation rules.
    Strictness strictness = Strictness::Lenient;

    /// Pretty-print with indentation.
    bool pretty = true;

    /// Preserve representable extension content in the output.
    bool preserveUnknownExtensions = true;

    /// Assign stable namespace prefixes where a dialect uses prefixes.
    bool deterministicPrefixes = true;

    /// Sender string for the header.
    std::string sender;

    /// Comment string for the header.
    std::string comment;

    /// Software identifier.
    std::string software = "iox-cpp";
};

} // namespace xtf
} // namespace iox
