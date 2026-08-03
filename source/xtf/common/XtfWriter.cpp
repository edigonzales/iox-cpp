#include "iox/xtf/XtfWriter.h"

#include "iox/Diagnostic.h"
#include "xml/XmlWriter.h"

#include <cctype>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace iox {
namespace xtf {
namespace {

constexpr const char* GEOMETRY_NS = "http://www.interlis.ch/geometry/1.0";
constexpr const char* XTF23_NS = "http://www.interlis.ch/INTERLIS2.3";
constexpr const char* XTF24_NS = "http://www.interlis.ch/xtf/2.4/INTERLIS";

std::string lowerAscii(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const auto character : value) {
        result.push_back(static_cast<char>(
            std::tolower(static_cast<unsigned char>(character))));
    }
    return result;
}

bool isGeometryLocalName(std::string_view name) {
    const auto lower = lowerAscii(name);
    static constexpr std::string_view names[] = {
        "coord", "arc", "polyline", "multicoord", "multipolyline",
        "surface", "area", "multisurface", "multiarea", "boundary",
        "exterior", "interior", "segments", "sequence", "c1", "c2",
        "c3", "a1", "a2", "a3", "r"};
    for (const auto candidate : names) {
        if (candidate == lower) return true;
    }
    return false;
}

std::string operationName(ObjectOperation operation) {
    switch (operation) {
    case ObjectOperation::Insert: return "insert";
    case ObjectOperation::Update: return "update";
    case ObjectOperation::Delete: return "delete";
    case ObjectOperation::None: return {};
    }
    return {};
}

std::string consistencyName(Consistency consistency) {
    switch (consistency) {
    case Consistency::Complete: return "complete";
    case Consistency::Incomplete: return "incomplete";
    case Consistency::Inconsistent: return "inconsistent";
    case Consistency::Adapted: return "adapted";
    case Consistency::Unspecified: return {};
    }
    return {};
}

} // namespace

enum class WriterState {
    BeforeTransfer,
    InTransfer,
    InBasket,
    AfterTransfer,
    Closed,
    Failed
};

struct XtfWriter::Impl final {
    XtfWriterOptions options;
    std::shared_ptr<OutputSink> sink;
    std::unique_ptr<xml::XmlWriter> xml;
    WriterState state = WriterState::BeforeTransfer;
    bool dataSectionOpen = false;
    std::vector<Diagnostic> diagnostics;
    std::map<std::string, std::string> modelPrefixes;
    std::map<std::string, std::string> prefixOwners;
    std::size_t nextModelPrefix = 0;
    std::string basketTag;

    XmlQualifiedName qualifiedName(std::string_view lexical) {
        const auto separator = lexical.find(':');
        if (separator == std::string_view::npos) {
            return {{}, std::string(lexical), {}};
        }
        const auto prefix = std::string(lexical.substr(0, separator));
        const auto localName = std::string(lexical.substr(separator + 1U));
        if (prefix == "ili") {
            return {options.version == XtfVersion::V24 ? XTF24_NS : XTF23_NS,
                    localName, prefix};
        }
        if (prefix == "geom") return {GEOMETRY_NS, localName, prefix};
        const auto owner = prefixOwners.find(prefix);
        if (owner == prefixOwners.end()) {
            throw IoxError(DiagnosticCode::UnknownInterlisName,
                           "Unknown XML namespace prefix: " + prefix);
        }
        return {owner->second, localName, prefix};
    }

    void startElement(
        std::string_view name,
        const std::vector<std::pair<std::string, std::string>>& attributes = {},
        bool selfClosing = false) {
        xml->startElement(qualifiedName(name));
        for (const auto& attribute : attributes) {
            if (attribute.first == "xmlns") {
                xml->writeNamespace({}, attribute.second);
            } else if (attribute.first.rfind("xmlns:", 0) == 0) {
                xml->writeNamespace(attribute.first.substr(6), attribute.second);
            } else {
                xml->writeAttribute(qualifiedName(attribute.first),
                                    attribute.second);
            }
        }
        if (selfClosing) xml->endElement();
    }

    std::string elementName(const IomName& name) {
        if (options.version != XtfVersion::V24) return name.interlisName();
        if (!name.hasXmlName()) {
            if (isGeometryLocalName(name.interlisName())) {
                return "geom:" + lowerAscii(name.interlisName());
            }
            return name.interlisName();
        }
        const auto& xmlName = name.xmlName();
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
                (prefixOwners.count(prefix) != 0 &&
                 prefixOwners.at(prefix) != xmlName.namespaceUri)) {
                do {
                    prefix = "m" + std::to_string(nextModelPrefix++);
                } while (prefixOwners.count(prefix) != 0);
            }
            prefixOwners.emplace(prefix, xmlName.namespaceUri);
            found = modelPrefixes.emplace(xmlName.namespaceUri, std::move(prefix)).first;
        }
        return found->second + ":" + xmlName.localName;
    }

    void addNamespaceBinding(
        std::vector<std::pair<std::string, std::string>>& attributes,
        const IomName& name) {
        if (options.version != XtfVersion::V24 || !name.hasXmlName()) return;
        const auto& xmlName = name.xmlName();
        if (xmlName.namespaceUri.empty() ||
            xmlName.namespaceUri == "http://www.interlis.ch/xtf/2.4/INTERLIS" ||
            xmlName.namespaceUri == GEOMETRY_NS) {
            return;
        }
        const auto lexical = elementName(name);
        const auto separator = lexical.find(':');
        if (separator != std::string::npos) {
            attributes.push_back({"xmlns:" + lexical.substr(0, separator),
                                  xmlName.namespaceUri});
        }
    }

    void validate(EventKind kind) {
        const bool valid =
            (state == WriterState::BeforeTransfer && kind == EventKind::StartTransfer) ||
            (state == WriterState::InTransfer &&
             (kind == EventKind::StartBasket || kind == EventKind::EndTransfer)) ||
            (state == WriterState::InBasket &&
             (kind == EventKind::Object || kind == EventKind::EndBasket));
        if (!valid) {
            state = WriterState::Failed;
            throw IoxError(DiagnosticCode::InvalidEventOrder,
                           "Invalid event order in XTF writer");
        }
    }

    void writeStartTransfer(const StartTransferEvent& event) {
        xml->startDocument();
        std::vector<std::pair<std::string, std::string>> attributes;
        if (options.version == XtfVersion::V24) {
            attributes.push_back({"xmlns:ili", XTF24_NS});
            attributes.push_back({"xmlns:geom", GEOMETRY_NS});
        } else {
            attributes.push_back({"xmlns:ili", XTF23_NS});
        }
        startElement("ili:TRANSFER", attributes);
        const auto writeTextElement = [this](const char* tag, const std::string& value) {
            if (value.empty()) return;
            startElement(tag);
            xml->text(value);
            xml->endElement();
        };
        const auto sender = event.header.sender.empty() ? options.sender
                                                        : event.header.sender;
        if (options.version == XtfVersion::V24) {
            startElement("ili:headersection");
            writeTextElement("ili:sender", sender);
            if (event.header.comment) writeTextElement("ili:comment", *event.header.comment);
            xml->endElement();
        } else {
            startElement("ili:HEADERSECTION");
            writeTextElement("ili:SENDER", sender);
            if (event.header.comment) writeTextElement("ili:COMMENT", *event.header.comment);
            xml->endElement();
        }
        state = WriterState::InTransfer;
    }

    void writeStartBasket(const StartBasketEvent& event) {
        if (!dataSectionOpen) {
            startElement(options.version == XtfVersion::V24
                             ? "ili:datasection" : "ili:DATASECTION");
            dataSectionOpen = true;
        }
        std::vector<std::pair<std::string, std::string>> attributes;
        const auto prefix = options.version == XtfVersion::V24 ? "ili:" : "";
        attributes.push_back({std::string(prefix) +
                                  (options.version == XtfVersion::V24 ? "bid" : "BID"),
                              event.basket.basketId});
        const auto consistency = consistencyName(event.basket.consistency);
        if (!consistency.empty() && consistency != "complete") {
            attributes.push_back({std::string(prefix) +
                                      (options.version == XtfVersion::V24
                                           ? "consistency" : "CONSISTENCY"),
                                  consistency});
        }
        basketTag = elementName(event.basket.topic);
        if (basketTag.empty()) {
            basketTag = options.version == XtfVersion::V24 ? "ili:basket" : "ili:BASKET";
        }
        addNamespaceBinding(attributes, event.basket.topic);
        startElement(basketTag, attributes);
        state = WriterState::InBasket;
    }

    void writeReferenceAttributes(
        std::vector<std::pair<std::string, std::string>>& attributes,
        const ReferenceInfo& reference) {
        const auto prefix = options.version == XtfVersion::V24 ? "ili:" : "";
        if (reference.targetOid) {
            attributes.push_back({std::string(prefix) +
                                      (options.version == XtfVersion::V24 ? "ref" : "REF"),
                                  *reference.targetOid});
        }
        if (reference.targetBasketId) {
            attributes.push_back({std::string(prefix) +
                                      (options.version == XtfVersion::V24 ? "bid" : "BID"),
                                  *reference.targetBasketId});
        }
        if (reference.orderPosition) {
            attributes.push_back({std::string(prefix) +
                                      (options.version == XtfVersion::V24
                                           ? "order_pos" : "ORDER_POS"),
                                  std::to_string(*reference.orderPosition)});
        }
    }

    void writeObject(const ObjectEvent& event) {
        const auto& object = event.object;
        std::vector<std::pair<std::string, std::string>> attributes;
        const auto prefix = options.version == XtfVersion::V24 ? "ili:" : "";
        if (object.oid()) {
            attributes.push_back({std::string(prefix) +
                                      (options.version == XtfVersion::V24 ? "tid" : "TID"),
                                  *object.oid()});
        }
        const auto operation = operationName(object.operation());
        if (!operation.empty() && operation != "insert") {
            attributes.push_back({std::string(prefix) +
                                      (options.version == XtfVersion::V24
                                           ? "operation" : "OPERATION"),
                                  operation});
        }
        writeReferenceAttributes(attributes, object.reference());
        auto tag = elementName(object.tag());
        if (tag.empty()) {
            state = WriterState::Failed;
            throw IoxError(DiagnosticCode::UnknownInterlisName,
                           "Object has no writable class name");
        }
        addNamespaceBinding(attributes, object.tag());
        startElement(tag, attributes);

        std::function<void(const IomObject&)> writeContents;
        std::function<void(const IomObject&)> writeStructured;

        writeStructured = [this, &writeContents](const IomObject& value) {
            auto nestedTag = elementName(value.tag());
            if (nestedTag.empty()) {
                throw IoxError(DiagnosticCode::UnknownInterlisName,
                               "Structured value has no writable name");
            }
            std::vector<std::pair<std::string, std::string>> nestedAttributes;
            addNamespaceBinding(nestedAttributes, value.tag());
            startElement(nestedTag, nestedAttributes);
            writeContents(value);
            xml->endElement();
        };

        writeContents = [this, &writeStructured](const IomObject& owner) {
            for (std::size_t attributeIndex = 0;
                 attributeIndex < owner.attributeCount(); ++attributeIndex) {
                const auto& name = owner.attributeName(attributeIndex);
                const auto ownerTag = lowerAscii(owner.tag().interlisName());
                const auto attributeName = lowerAscii(name.interlisName());
                const auto count = owner.valueCount(name.interlisName());

                if (options.version == XtfVersion::V24 &&
                    ownerTag == "polyline" && attributeName == "sequence") {
                    for (std::size_t index = 0; index < count; ++index) {
                        const auto sequence = owner.object(name.interlisName(), index);
                        if (!sequence) continue;
                        const auto segmentCount = sequence->valueCount("segment");
                        for (std::size_t segment = 0; segment < segmentCount; ++segment) {
                            const auto member = sequence->object("segment", segment);
                            if (member) writeStructured(*member);
                        }
                    }
                    continue;
                }

                const bool directMulti = options.version == XtfVersion::V24 &&
                    ((ownerTag == "multicoord" && attributeName == "coord") ||
                     (ownerTag == "multipolyline" && attributeName == "polyline") ||
                     (ownerTag == "multisurface" && attributeName == "surface") ||
                     (ownerTag == "multiarea" && attributeName == "area"));
                if (directMulti) {
                    for (std::size_t index = 0; index < count; ++index) {
                        const auto member = owner.object(name.interlisName(), index);
                        if (member) writeStructured(*member);
                    }
                    continue;
                }

                auto lexicalName = elementName(name);
                if (lexicalName.empty()) {
                    throw IoxError(DiagnosticCode::UnknownInterlisName,
                                   "Attribute has no writable name");
                }
                for (std::size_t index = 0; index < count; ++index) {
                    const auto& value = owner.value(name.interlisName(), index);
                    std::vector<std::pair<std::string, std::string>> valueAttributes;
                    addNamespaceBinding(valueAttributes, name);
                    if (value.isObject() && value.object().isReference()) {
                        writeReferenceAttributes(valueAttributes, value.object().reference());
                        startElement(lexicalName, valueAttributes, true);
                        continue;
                    }
                    startElement(lexicalName, valueAttributes);
                    if (value.isPrimitive()) xml->text(value.primitive());
                    else writeStructured(value.object());
                    xml->endElement();
                }
            }
        };

        writeContents(object);
        xml->endElement();
    }

    void writeEndBasket() {
        xml->endElement();
        basketTag.clear();
        state = WriterState::InTransfer;
    }

    void writeEndTransfer() {
        if (dataSectionOpen) {
            xml->endElement();
            dataSectionOpen = false;
        }
        xml->endElement();
        state = WriterState::AfterTransfer;
    }
};

XtfWriter::XtfWriter(std::shared_ptr<OutputSink> output,
                     XtfWriterOptions options)
    : impl_(std::make_unique<Impl>()) {
    if (!output) throw IoxError(DiagnosticCode::InvalidArgument,
                                "XtfWriter requires an output sink");
    impl_->options = std::move(options);
    impl_->sink = std::move(output);
    xml::XmlWriterOptions xmlOptions;
    xmlOptions.pretty = impl_->options.pretty;
    impl_->xml = std::make_unique<xml::XmlWriter>(impl_->sink,
                                                  std::move(xmlOptions));
}

XtfWriter::~XtfWriter() = default;

void XtfWriter::write(const IoxEvent& event) {
    if (impl_->state == WriterState::Closed || impl_->state == WriterState::Failed) {
        throw IoxError(DiagnosticCode::WriterStateError,
                       "XTF writer is closed or failed");
    }
    const auto kind = eventKind(event);
    impl_->validate(kind);
    try {
        std::visit([this](const auto& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, StartTransferEvent>) {
                impl_->writeStartTransfer(value);
            } else if constexpr (std::is_same_v<T, StartBasketEvent>) {
                impl_->writeStartBasket(value);
            } else if constexpr (std::is_same_v<T, ObjectEvent>) {
                impl_->writeObject(value);
            } else if constexpr (std::is_same_v<T, EndBasketEvent>) {
                impl_->writeEndBasket();
            } else {
                impl_->writeEndTransfer();
            }
        }, event);
    } catch (...) {
        impl_->state = WriterState::Failed;
        throw;
    }
}

void XtfWriter::flush() {
    if (impl_->state == WriterState::Failed) {
        throw IoxError(DiagnosticCode::WriterStateError,
                       "XTF writer is in a failed state");
    }
    impl_->xml->flush();
    impl_->sink->flush();
}

void XtfWriter::close() {
    if (impl_->state == WriterState::Closed) return;
    if (impl_->state != WriterState::AfterTransfer) {
        impl_->state = WriterState::Failed;
        throw IoxError(DiagnosticCode::InvalidEventOrder,
                       "XTF writer closed before EndTransferEvent");
    }
    impl_->xml->endDocument();
    impl_->sink->close();
    impl_->state = WriterState::Closed;
}

bool XtfWriter::isClosed() const noexcept {
    return impl_->state == WriterState::Closed;
}

std::vector<Diagnostic> XtfWriter::takeDiagnostics() {
    auto result = std::move(impl_->diagnostics);
    impl_->diagnostics.clear();
    return result;
}

} // namespace xtf
} // namespace iox
