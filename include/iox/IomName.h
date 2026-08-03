#pragma once

#include <string>
#include <utility>

namespace iox {

struct XmlQualifiedName final {
    std::string namespaceUri;
    std::string localName;
    std::string prefixHint;

    XmlQualifiedName() = default;
    XmlQualifiedName(std::string namespaceValue,
                     std::string localValue,
                     std::string prefixValue = {})
        : namespaceUri(std::move(namespaceValue)),
          localName(std::move(localValue)),
          prefixHint(std::move(prefixValue)) {}

    bool empty() const noexcept { return localName.empty(); }

    std::string expanded() const {
        return namespaceUri.empty() ? localName
                                    : "{" + namespaceUri + "}" + localName;
    }

    bool operator==(const XmlQualifiedName& other) const noexcept {
        return namespaceUri == other.namespaceUri && localName == other.localName;
    }

    bool operator!=(const XmlQualifiedName& other) const noexcept {
        return !(*this == other);
    }
};

class IomName final {
public:
    IomName() = default;
    explicit IomName(std::string interlisName)
        : interlisName_(std::move(interlisName)) {}
    IomName(std::string interlisName, XmlQualifiedName xmlName)
        : interlisName_(std::move(interlisName)),
          xmlName_(std::move(xmlName)),
          hasXmlName_(true) {}

    const std::string& interlisName() const noexcept { return interlisName_; }
    const XmlQualifiedName& xmlName() const noexcept { return xmlName_; }
    bool hasInterlisName() const noexcept { return !interlisName_.empty(); }
    bool hasXmlName() const noexcept { return hasXmlName_; }

    static IomName fromExpandedXmlName(XmlQualifiedName name) {
        IomName result;
        result.xmlName_ = std::move(name);
        result.hasXmlName_ = true;
        return result;
    }

    bool operator==(const IomName& other) const noexcept {
        return interlisName_ == other.interlisName_ &&
               hasXmlName_ == other.hasXmlName_ &&
               (!hasXmlName_ || xmlName_ == other.xmlName_);
    }

    bool operator!=(const IomName& other) const noexcept {
        return !(*this == other);
    }

private:
    std::string interlisName_;
    XmlQualifiedName xmlName_;
    bool hasXmlName_ = false;
};

} // namespace iox
