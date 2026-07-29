#include "iox/xtf/XtfWriter.h"
#include "iox/xml/XmlWriter.h"

#include <string>
#include <vector>
#include <utility>
#include <stdexcept>
#include <map>
#include <cctype>

namespace iox {
namespace xtf {

namespace {

constexpr const char* GEOMETRY_NS = "http://www.interlis.ch/geometry/1.0";

std::string lowerAscii(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const auto character : value) {
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
    }
    return result;
}

bool isGeometryLocalName(std::string_view name) {
    std::string lower;
    lower.reserve(name.size());
    for (const auto character : name) {
        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
    }
    static constexpr std::string_view geometryNames[] = {
        "coord", "arc", "polyline", "multicoord", "multipolyline",
        "surface", "area", "multisurface", "multiarea", "boundary",
        "exterior", "interior", "segments", "sequence", "c1", "c2",
        "c3", "a1", "a2", "a3", "r"
    };
    for (const auto candidate : geometryNames) {
        if (candidate == lower) return true;
    }
    return false;
}

}

// ============================================================================
// Writer event state machine
// ============================================================================

enum class WriterState {
    BeforeTransfer,
    InTransfer,
    InBasket,
    AfterBasket,
    AfterTransfer,
    Closed
};

// ============================================================================
// XtfWriter::Impl
// ============================================================================

struct XtfWriter::Impl {
    XtfWriterOptions options;
    std::shared_ptr<OutputSink> sink;
    std::unique_ptr<xml::XmlWriter> xml;
    WriterState state = WriterState::BeforeTransfer;
    bool closed_ = false;
    bool dataSectionOpen = false;
    std::vector<Diagnostic> diagnostics;
    std::map<std::string, std::string> modelPrefixes;
    std::map<std::string, std::string> prefixOwners;
    std::size_t nextModelPrefix = 0;
    std::string basketTag;

    std::string elementName(const IomName& name) {
        if (options.version != XtfVersion::Xtf24) {
            return name.iliName();
        }
        if (!name.xmlName()) {
            if (isGeometryLocalName(name.iliName())) {
                return "geom:" + lowerAscii(name.iliName());
            }
            return name.iliName();
        }
        const auto& xmlName = *name.xmlName();
        if (xmlName.namespaceUri == "http://www.interlis.ch/xtf/2.4/INTERLIS") {
            return "ili:" + xmlName.localName;
        }
        if (xmlName.namespaceUri == GEOMETRY_NS) {
            return "geom:" + xmlName.localName;
        }
        auto found = modelPrefixes.find(xmlName.namespaceUri);
        if (found == modelPrefixes.end()) {
            std::string prefix = xmlName.prefixHint;
            if (prefix.empty() || prefix == "ili" || prefix == "geom" ||
                (prefixOwners.count(prefix) && prefixOwners.at(prefix) != xmlName.namespaceUri)) {
                do {
                    prefix = "m" + std::to_string(nextModelPrefix++);
                } while (prefixOwners.count(prefix));
            }
            prefixOwners.emplace(prefix, xmlName.namespaceUri);
            found = modelPrefixes.emplace(xmlName.namespaceUri, std::move(prefix)).first;
        }
        return found->second + ":" + xmlName.localName;
    }

    void addNamespaceBinding(std::vector<std::pair<std::string, std::string>>& attrs,
                             const IomName& name) {
        if (options.version != XtfVersion::Xtf24 || !name.xmlName()) return;
        const auto& xmlName = *name.xmlName();
        if (xmlName.namespaceUri.empty() ||
            xmlName.namespaceUri == "http://www.interlis.ch/xtf/2.4/INTERLIS" ||
            xmlName.namespaceUri == GEOMETRY_NS) return;
        const auto lexical = elementName(name);
        const auto separator = lexical.find(':');
        if (separator != std::string::npos) {
            attrs.push_back({"xmlns:" + lexical.substr(0, separator), xmlName.namespaceUri});
        }
    }

    void validateEvent(const IoxEvent& event);
    void writeStartTransfer(const StartTransferEvent& e);
    void writeStartBasket(const StartBasketEvent& e);
    void writeObject(const ObjectEvent& e);
    void writeEndBasket(const EndBasketEvent& e);
    void writeEndTransfer(const EndTransferEvent& e);
};

void XtfWriter::Impl::validateEvent(const IoxEvent& event) {
    // Check event ordering
    std::visit([this](const auto& e) {
        using T = std::decay_t<decltype(e)>;

        if constexpr (std::is_same_v<T, StartTransferEvent>) {
            if (state != WriterState::BeforeTransfer) {
                diagnostics.push_back({Diagnostic::Severity::Error,
                    ErrorCode::XtfStateViolation,
                    "StartTransferEvent must be first"});
            }
        } else if constexpr (std::is_same_v<T, StartBasketEvent>) {
            if (state != WriterState::InTransfer &&
                state != WriterState::AfterBasket) {
                diagnostics.push_back({Diagnostic::Severity::Error,
                    ErrorCode::XtfStateViolation,
                    "StartBasketEvent must follow StartTransfer or EndBasket"});
            }
        } else if constexpr (std::is_same_v<T, ObjectEvent>) {
            if (state != WriterState::InBasket) {
                diagnostics.push_back({Diagnostic::Severity::Error,
                    ErrorCode::XtfStateViolation,
                    "ObjectEvent must be inside a basket"});
            }
        } else if constexpr (std::is_same_v<T, EndBasketEvent>) {
            if (state != WriterState::InBasket) {
                diagnostics.push_back({Diagnostic::Severity::Error,
                    ErrorCode::XtfStateViolation,
                    "EndBasketEvent must close an open basket"});
            }
        } else if constexpr (std::is_same_v<T, EndTransferEvent>) {
            if (state != WriterState::AfterBasket &&
                state != WriterState::InTransfer) {
                diagnostics.push_back({Diagnostic::Severity::Error,
                    ErrorCode::XtfStateViolation,
                    "EndTransferEvent must be last"});
            }
        }
    }, event);
}

void XtfWriter::Impl::writeStartTransfer(const StartTransferEvent& e) {
    state = WriterState::InTransfer;

    xml->writeDeclaration();

    std::vector<std::pair<std::string, std::string>> attrs;

    if (options.version == XtfVersion::Xtf24) {
        attrs.push_back({"xmlns:ili", "http://www.interlis.ch/xtf/2.4/INTERLIS"});
        attrs.push_back({"xmlns:geom", "http://www.interlis.ch/geometry/1.0"});
        attrs.push_back({"xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance"});
        xml->writeStartElement("ili:TRANSFER", attrs);
    } else {
        attrs.push_back({"xmlns:ili", "http://www.interlis.ch/INTERLIS2.3"});
        xml->writeStartElement("ili:TRANSFER", attrs);
    }

    // Write header elements
    auto writeHeaderElement = [this](const char* tag, const std::string& value) {
        if (value.empty()) return;
        xml->writeStartElement(tag);
        xml->writeText(value);
        xml->writeEndElement(tag);
    };

    if (options.version == XtfVersion::Xtf24) {
        writeHeaderElement("ili:HEADERSECTION", "");
        xml->writeStartElement("ili:headersection");
        writeHeaderElement("ili:sender", e.sender.empty() ? options.sender : e.sender);
        writeHeaderElement("ili:comment", e.comment.empty() ? options.comment : e.comment);
        writeHeaderElement("ili:version", e.iliVersion);
        writeHeaderElement("ili:software", options.software);
        writeHeaderElement("ili:date", e.date);
        xml->writeEndElement("ili:headersection");
    } else {
        // XTF 2.3 header
        xml->writeStartElement("ili:HEADERSECTION");
        writeHeaderElement("ili:SENDER", e.sender.empty() ? options.sender : e.sender);
        writeHeaderElement("ili:COMMENT", e.comment.empty() ? options.comment : e.comment);
        writeHeaderElement("ili:VERSION", e.iliVersion);
        writeHeaderElement("ili:SOFTWARE", options.software);
        writeHeaderElement("ili:DATE", e.date);
        xml->writeEndElement("ili:HEADERSECTION");
    }
}

void XtfWriter::Impl::writeStartBasket(const StartBasketEvent& e) {
    state = WriterState::InBasket;

    if (!dataSectionOpen) {
        xml->writeStartElement(options.version == XtfVersion::Xtf24
                                   ? "ili:datasection" : "ili:DATASECTION");
        dataSectionOpen = true;
    }

    std::vector<std::pair<std::string, std::string>> attrs;
    const auto controlPrefix = options.version == XtfVersion::Xtf24 ? "ili:" : "";
    attrs.push_back({std::string(controlPrefix) + (options.version == XtfVersion::Xtf24 ? "bid" : "BID"), e.bid});
    if (e.consistency == "incomplete") {
        attrs.push_back({std::string(controlPrefix) + (options.version == XtfVersion::Xtf24 ? "consistency" : "CONSISTENCY"), "incomplete"});
    }
    if (!e.operation.empty() && e.operation != "insert") {
        attrs.push_back({std::string(controlPrefix) + (options.version == XtfVersion::Xtf24 ? "operation" : "OPERATION"), e.operation});
    }

    basketTag = elementName(e.basketType);
    if (basketTag.empty()) basketTag = options.version == XtfVersion::Xtf24 ? "ili:basket" : "ili:BASKET";
    addNamespaceBinding(attrs, e.basketType);
    xml->writeStartElement(basketTag, attrs);
}

void XtfWriter::Impl::writeObject(const ObjectEvent& e) {
    state = WriterState::InBasket;

    std::vector<std::pair<std::string, std::string>> attrs;
    const auto controlPrefix = options.version == XtfVersion::Xtf24 ? "ili:" : "";
    attrs.push_back({std::string(controlPrefix) + (options.version == XtfVersion::Xtf24 ? "tid" : "TID"), e.objectId});
    if (!e.operation.empty() && e.operation != "insert") {
        attrs.push_back({std::string(controlPrefix) + (options.version == XtfVersion::Xtf24 ? "operation" : "OPERATION"), e.operation});
    }
    if (e.refBid) {
        attrs.push_back({std::string(controlPrefix) + (options.version == XtfVersion::Xtf24 ? "ref" : "REF"), *e.refBid});
    }
    if (e.object.bid()) {
        attrs.push_back({std::string(controlPrefix) + (options.version == XtfVersion::Xtf24 ? "bid" : "BID"), *e.object.bid()});
    }

    auto tagName = elementName(e.object.tag());
    if (tagName.empty()) tagName = "DataObject";
    addNamespaceBinding(attrs, e.object.tag());

    xml->writeStartElement(tagName, attrs);

    // Geometry has a compact XTF 2.4 encoding. The public IOM tree remains
    // canonical (sequence/SEGMENTS/segment and multi-member attributes), so
    // this writer flattens only the XML representation at the dialect edge.
    std::function<void(const IomObject&)> writeObjectElement;
    std::function<void(const IomObject&)> writeContents;
    std::function<void(const IomAttribute&)> writeAttribute;

    writeObjectElement = [this, &writeObjectElement, &writeContents](const IomObject& obj) {
        auto tag = elementName(obj.tag());
        if (tag.empty()) tag = "Structure";
        std::vector<std::pair<std::string, std::string>> attrs;
        addNamespaceBinding(attrs, obj.tag());
        xml->writeStartElement(tag, attrs);
        writeContents(obj);
        xml->writeEndElement(tag);
    };

    writeAttribute = [this, &writeObjectElement](const IomAttribute& attr) {
        auto attrName = elementName(attr.name);
        if (attrName.empty()) return;

        std::vector<std::pair<std::string, std::string>> refAttrs;
        if (attr.ref) refAttrs.push_back({"REF", *attr.ref});
        if (attr.bid) refAttrs.push_back({"BID", *attr.bid});
        if (attr.orderPos) refAttrs.push_back({"ORDER_POS", std::to_string(*attr.orderPos)});
        addNamespaceBinding(refAttrs, attr.name);

        if (attr.values.empty()) {
            xml->writeStartElement(attrName, refAttrs, true);
            return;
        }
        for (const auto& val : attr.values) {
            xml->writeStartElement(attrName, refAttrs);
            if (const auto* prim = std::get_if<IomValue>(&val)) {
                xml->writeText(prim->toTransferString());
            } else if (const auto* sub = std::get_if<IomObject>(&val)) {
                writeObjectElement(*sub);
            }
            xml->writeEndElement(attrName);
        }
    };

    writeContents = [this, &writeObjectElement, &writeAttribute](const IomObject& obj) {
        for (std::size_t i = 0; i < obj.attributeCount(); ++i) {
            const auto& attr = obj.attributeAt(i);
            const auto objectTag = lowerAscii(obj.tag().iliName());
            const auto attributeName = lowerAscii(attr.name.iliName());

            if (options.version == XtfVersion::Xtf24 &&
                objectTag == "polyline" && attributeName == "sequence") {
                for (const auto& sequenceValue : attr.values) {
                    const auto* sequence = std::get_if<IomObject>(&sequenceValue);
                    if (!sequence) continue;
                    const auto* segments = sequence->findAttribute("segment");
                    if (!segments) continue;
                    for (const auto& segmentValue : segments->values) {
                        if (const auto* segment = std::get_if<IomObject>(&segmentValue)) {
                            writeObjectElement(*segment);
                        }
                    }
                }
                continue;
            }

            const bool directMultiMember = options.version == XtfVersion::Xtf24 &&
                ((objectTag == "multicoord" && attributeName == "coord") ||
                 (objectTag == "multipolyline" && attributeName == "polyline") ||
                 (objectTag == "multisurface" && attributeName == "surface") ||
                 (objectTag == "multiarea" && attributeName == "area"));
            if (directMultiMember) {
                for (const auto& memberValue : attr.values) {
                    if (const auto* member = std::get_if<IomObject>(&memberValue)) {
                        writeObjectElement(*member);
                    }
                }
                continue;
            }

            writeAttribute(attr);
        }
    };

    writeContents(e.object);
    xml->writeEndElement(tagName);
}

void XtfWriter::Impl::writeEndBasket(const EndBasketEvent& e) {
    (void)e;
    state = WriterState::AfterBasket;
    xml->writeEndElement(basketTag.empty() ? (options.version == XtfVersion::Xtf24 ? "ili:basket" : "ili:BASKET") : basketTag);
    basketTag.clear();
}

void XtfWriter::Impl::writeEndTransfer(const EndTransferEvent& e) {
    (void)e;
    state = WriterState::AfterTransfer;
    if (dataSectionOpen) {
        xml->writeEndElement(options.version == XtfVersion::Xtf24
                                 ? "ili:datasection" : "ili:DATASECTION");
        dataSectionOpen = false;
    }
    xml->writeEndElement(options.version == XtfVersion::Xtf24
                         ? "ili:TRANSFER" : "ili:TRANSFER");
}

// ============================================================================
// XtfWriter — public API
// ============================================================================

XtfWriter::XtfWriter(std::shared_ptr<OutputSink> output,
                     XtfWriterOptions options)
    : impl_(std::make_unique<Impl>()) {
    impl_->options = std::move(options);
    impl_->sink = std::move(output);

    auto writeFunc = [this](const void* data, std::size_t size) {
        impl_->sink->write(data, size);
    };

    impl_->xml = std::make_unique<xml::XmlWriter>(
        std::move(writeFunc),
        impl_->options.pretty,
        2);
}

XtfWriter::~XtfWriter() {
    if (!impl_->closed_) {
        close();
    }
}

void XtfWriter::write(const IoxEvent& event) {
    if (impl_->closed_) return;

    impl_->validateEvent(event);

    std::visit([this](const auto& e) {
        using T = std::decay_t<decltype(e)>;
        if constexpr (std::is_same_v<T, StartTransferEvent>) {
            impl_->writeStartTransfer(e);
        } else if constexpr (std::is_same_v<T, StartBasketEvent>) {
            impl_->writeStartBasket(e);
        } else if constexpr (std::is_same_v<T, ObjectEvent>) {
            impl_->writeObject(e);
        } else if constexpr (std::is_same_v<T, EndBasketEvent>) {
            impl_->writeEndBasket(e);
        } else if constexpr (std::is_same_v<T, EndTransferEvent>) {
            impl_->writeEndTransfer(e);
        }
    }, event);
}

void XtfWriter::flush() {
    if (impl_->xml) impl_->xml->flush();
    if (impl_->sink) impl_->sink->flush();
}

void XtfWriter::close() {
    if (!impl_->closed_) {
        impl_->closed_ = true;
        if (impl_->sink) impl_->sink->close();
    }
}

bool XtfWriter::isClosed() const noexcept {
    return impl_->closed_;
}

std::vector<Diagnostic> XtfWriter::takeDiagnostics() {
    auto diags = std::move(impl_->diagnostics);
    impl_->diagnostics.clear();
    return diags;
}

} // namespace xtf
} // namespace iox
