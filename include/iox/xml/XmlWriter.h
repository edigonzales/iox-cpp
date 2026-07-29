#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <functional>

namespace iox {
namespace xml {

/// A minimal, deterministic UTF-8 XML writer.
///
/// - UTF-8 output
/// - LF line endings
/// - Proper XML escaping (&, <, >, ", ')
/// - Configurable indentation
/// - No external dependencies
class XmlWriter final {
public:
    using WriteFunc = std::function<void(const void* data, std::size_t size)>;

    /// Construct with a write callback.
    explicit XmlWriter(WriteFunc writeFunc);

    /// Construct with indentation control.
    XmlWriter(WriteFunc writeFunc, bool pretty, int indentSpaces = 2);

    ~XmlWriter();

    // Non-copyable
    XmlWriter(const XmlWriter&) = delete;
    XmlWriter& operator=(const XmlWriter&) = delete;

    /// Write the XML declaration: <?xml version="1.0" encoding="UTF-8"?>
    void writeDeclaration();

    /// Start an element: <name attr1="val1" ...>
    /// If selfClosing is true, writes <name .../>
    void writeStartElement(std::string_view name,
                           const std::vector<std::pair<std::string, std::string>>& attributes = {},
                           bool selfClosing = false);

    /// End the current element: </name>
    void writeEndElement(std::string_view name);

    /// Write text content with proper XML escaping.
    void writeText(std::string_view text);

    /// Write a comment: <!-- ... -->
    void writeComment(std::string_view text);

    /// Flush any buffered output.
    void flush();

    /// Whether any output has been written.
    bool hasWritten() const noexcept;

    /// Current nesting depth.
    int depth() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace xml
} // namespace iox
