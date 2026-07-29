#pragma once

#include "iox/ByteView.h"
#include "iox/Diagnostic.h"

#include <memory>
#include <functional>
#include <string>
#include <vector>

namespace iox {
namespace xml {

/// Callbacks for the Expat-based incremental XML parser.
struct ExpatCallbacks {
    /// Called when an opening tag is encountered.
    /// name: element name (namespace|local for XTF 2.4, local for 2.3)
    /// attributes: alternating name/value pairs (borrowed, valid only during callback)
    std::function<void(std::string_view name,
                       const std::vector<std::pair<std::string_view, std::string_view>>& attrs)>
        onStartElement;

    /// Called when a closing tag is encountered.
    std::function<void(std::string_view name)> onEndElement;

    /// Called for character data content (may be called multiple times per element).
    std::function<void(std::string_view data)> onCharacterData;

    /// Called when a processing instruction is encountered.
    std::function<void(std::string_view target, std::string_view data)> onProcessingInstruction;

    /// Called for XML comments (informational; not preserved for roundtrip).
    std::function<void(std::string_view comment)> onComment;

    /// Called when the XML declaration is parsed.
    std::function<void(std::string_view version, std::string_view encoding,
                       bool standalone)> onXmlDeclaration;
};

/// Configuration limits for the XML parser.
struct ExpatLimits {
    std::size_t maxElementDepth = 256;
    std::size_t maxAttributeCount = 512;
    std::size_t maxElementNameLength = 1024;
    std::size_t maxAttributeValueLength = 65536;
};

/// Secure incremental XML parser built on Expat.
///
/// - No external entity resolution
/// - No DTD processing (DTD is rejected)
/// - Chunk-based feeding via feed()
/// - byteOffset/line/column tracking
/// - Configurable limits (depth, size, attributes)
class ExpatParser final {
public:
    explicit ExpatParser(ExpatCallbacks callbacks, ExpatLimits limits = {});
    ~ExpatParser();

    // Non-copyable, non-movable (Expat parser is not move-safe)
    ExpatParser(const ExpatParser&) = delete;
    ExpatParser& operator=(const ExpatParser&) = delete;

    /// Feed a chunk of XML data. May fire callbacks synchronously.
    /// Returns false on fatal error (diagnostics available via takeDiagnostics).
    bool feed(ByteView data);

    /// Signal end of document. Returns false on fatal error.
    bool finish();

    /// Extract accumulated diagnostics.
    std::vector<Diagnostic> takeDiagnostics();

    /// Current byte offset from start of all fed data.
    std::uint64_t byteOffset() const noexcept;

    /// Current line number (1-based).
    int line() const noexcept;

    /// Current column number (1-based).
    int column() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace xml
} // namespace iox
