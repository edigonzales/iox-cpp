#pragma once

#include <string>
#include <string_view>
#include <optional>
#include <utility>

namespace iox {

/// An XML qualified name with namespace URI and local part.
struct XmlQualifiedName final {
    std::string namespaceUri;  // empty for unqualified
    std::string localName;
    std::string prefixHint;    // lexical prefix, never used for identity

    XmlQualifiedName() = default;
    XmlQualifiedName(std::string ns, std::string local,
                     std::string prefix = {})
        : namespaceUri(std::move(ns)), localName(std::move(local)),
          prefixHint(std::move(prefix)) {}

    bool operator==(const XmlQualifiedName& o) const noexcept {
        return namespaceUri == o.namespaceUri && localName == o.localName;
    }

    bool operator!=(const XmlQualifiedName& o) const noexcept {
        return !(*this == o);
    }
};

/// INTERLIS name information captured during parsing.
/// For model-free operation, the original XML qualified name is stored
/// alongside the derived INTERLIS name (when derivable).
class IomName final {
public:
    IomName() = default;

    /// Construct with INTERLIS scoped name only (model-free XTF 2.3 style).
    explicit IomName(std::string iliName)
        : iliName_(std::move(iliName)) {}

    /// Construct with both INTERLIS name and XML qualified name (XTF 2.4).
    IomName(std::string iliName, XmlQualifiedName xmlName)
        : iliName_(std::move(iliName)), xmlName_(std::move(xmlName)) {}

    /// The INTERLIS scoped name (e.g. "MODEL.TOPIC.Class").
    const std::string& iliName() const noexcept { return iliName_; }
    const std::string& interlisName() const noexcept { return iliName_; }
    bool hasInterlisName() const noexcept { return !iliName_.empty(); }

    /// The original XML qualified name, if captured.
    const std::optional<XmlQualifiedName>& xmlName() const noexcept {
        return xmlName_;
    }
    bool hasXmlName() const noexcept { return xmlName_.has_value(); }

    /// Set the XML qualified name (useful when adding metadata after construction).
    void setXmlName(XmlQualifiedName name) {
        xmlName_ = std::move(name);
    }

    static IomName fromExpandedXmlName(XmlQualifiedName name) {
        IomName result;
        result.xmlName_ = std::move(name);
        return result;
    }

    bool operator==(const IomName& o) const noexcept {
        return iliName_ == o.iliName_ && xmlName_ == o.xmlName_;
    }

    bool operator!=(const IomName& o) const noexcept {
        return !(*this == o);
    }

private:
    std::string iliName_;
    std::optional<XmlQualifiedName> xmlName_;
};

} // namespace iox
