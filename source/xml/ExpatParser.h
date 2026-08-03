#pragma once

#include "iox/ByteView.h"
#include "iox/Diagnostic.h"
#include "iox/IomName.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace iox {
namespace xml {

struct XmlLimits final {
    std::size_t maxDepth = 256;
    std::size_t maxAttributesPerElement = 1024;
    std::size_t maxTextBytesPerNode = 16U * 1024U * 1024U;
    std::size_t maxTotalInputBytes = 0;
};

struct XmlAttribute final {
    XmlQualifiedName name;
    std::string value;
};

struct XmlNamespaceDeclaration final {
    std::string prefix;
    std::string namespaceUri;
};

struct XmlStartElement final {
    XmlQualifiedName name;
    std::vector<XmlAttribute> attributes;
    std::vector<XmlNamespaceDeclaration> namespaces;
    SourceLocation location;
};

struct XmlEndElement final {
    XmlQualifiedName name;
    SourceLocation location;
};

class ExpatParser final {
public:
    using StartHandler = std::function<void(const XmlStartElement&)>;
    using EndHandler = std::function<void(const XmlEndElement&)>;
    using TextHandler = std::function<void(std::string_view,
                                           const SourceLocation&)>;

    explicit ExpatParser(XmlLimits limits = {},
                         std::string sourceName = {});
    ~ExpatParser();

    ExpatParser(const ExpatParser&) = delete;
    ExpatParser& operator=(const ExpatParser&) = delete;

    void setStartHandler(StartHandler handler);
    void setEndHandler(EndHandler handler);
    void setTextHandler(TextHandler handler);

    void feed(ByteView bytes);
    void finish();
    /// Request a resumable stop from inside a parser callback.
    void suspend();
    /// Continue parsing the input buffer retained by Expat.
    void resume();
    bool suspended() const noexcept;
    bool finished() const noexcept;
    SourceLocation location() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace xml
} // namespace iox
