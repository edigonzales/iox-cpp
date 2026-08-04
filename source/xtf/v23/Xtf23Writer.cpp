#include "xtf/v23/Xtf23Writer.h"

#include "xml/XmlWriter.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace iox {
namespace xtf {
namespace {

constexpr std::string_view xtf23Namespace =
    "http://www.interlis.ch/INTERLIS2.3";

std::string upperAscii(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const auto character : value) {
        result.push_back(static_cast<char>(
            std::toupper(static_cast<unsigned char>(character))));
    }
    return result;
}

std::string join(const std::vector<std::string>& values,
                 std::string_view separator) {
    std::string result;
    for (const auto& value : values) {
        if (!result.empty()) result.append(separator);
        result.append(value);
    }
    return result;
}

std::string basketKindName(BasketKind kind) {
    switch (kind) {
    case BasketKind::Full:
    case BasketKind::Unspecified: return {};
    case BasketKind::Update: return "UPDATE";
    case BasketKind::Initial: return "INITIAL";
    }
    return {};
}

std::string operationName(ObjectOperation operation) {
    switch (operation) {
    case ObjectOperation::Insert:
    case ObjectOperation::None: return {};
    case ObjectOperation::Update: return "UPDATE";
    case ObjectOperation::Delete: return "DELETE";
    }
    return {};
}

std::string consistencyName(Consistency consistency) {
    switch (consistency) {
    case Consistency::Complete:
    case Consistency::Unspecified: return {};
    case Consistency::Incomplete: return "INCOMPLETE";
    case Consistency::Inconsistent: return "INCONSISTENT";
    case Consistency::Adapted: return "ADAPTED";
    }
    return {};
}

} // namespace

struct Xtf23Writer::Impl final {
    xml::XmlWriter& xml;
    XtfWriterOptions options;
    DiagnosticHandler addDiagnostic;
    bool dataSectionOpen = false;

    Impl(xml::XmlWriter& writer, XtfWriterOptions value,
         DiagnosticHandler diagnosticHandler)
        : xml(writer), options(std::move(value)),
          addDiagnostic(std::move(diagnosticHandler)) {}

    [[noreturn]] void fail(DiagnosticCode code, std::string message) const {
        throw IoxError(code, std::move(message));
    }

    void report(DiagnosticSeverity severity, DiagnosticCode code,
                std::string message) {
        addDiagnostic({severity, code, std::move(message), {}, {}});
    }

    void strictOrReport(DiagnosticCode code, std::string message) {
        if (options.strictness == Strictness::Strict) {
            fail(code, std::move(message));
        }
        report(DiagnosticSeverity::Error, code, std::move(message));
    }

    static XmlQualifiedName xtfName(std::string_view localName) {
        return {std::string(xtf23Namespace), std::string(localName), {}};
    }

    static XmlQualifiedName attributeName(std::string_view localName) {
        return {{}, std::string(localName), {}};
    }

    void start(std::string_view localName) {
        xml.startElement(xtfName(localName));
    }

    void attribute(std::string_view name, std::string_view value) {
        xml.writeAttribute(attributeName(name), value);
    }

    static bool isAlias(const ExtensionElement& extension) {
        return upperAscii(extension.name.localName) == "ALIAS" &&
               (extension.name.namespaceUri.empty() ||
                extension.name.namespaceUri == xtf23Namespace);
    }

    void writeExtension(const ExtensionElement& extension) {
        if (extension.name.localName.empty()) {
            fail(DiagnosticCode::UnknownInterlisName,
                 "Extension element has no XML local name");
        }
        xml.startElement(extension.name);
        if (extension.name.namespaceUri.empty()) {
            // The XTF root uses the default namespace. Preserve an explicitly
            // no-namespace extension by undeclaring it on that element.
            xml.writeNamespace({}, {});
        }
        for (const auto& item : extension.attributes) {
            if (item.name.localName.empty()) {
                fail(DiagnosticCode::UnknownInterlisName,
                     "Extension attribute has no XML local name");
            }
            xml.writeAttribute(item.name, item.value);
        }
        if (!extension.text.empty()) xml.text(extension.text);
        for (const auto& child : extension.children) writeExtension(child);
        xml.endElement();
    }

    bool extensionEnabled(const ExtensionElement& extension,
                          std::string description) {
        if (isAlias(extension)) return true;
        if (options.preserveUnknownExtensions) {
            report(DiagnosticSeverity::Warning,
                   DiagnosticCode::UnknownExtensionPreserved,
                   "Unknown extension was preserved: " +
                       extension.name.localName);
            return true;
        }
        strictOrReport(DiagnosticCode::UnexpectedElement,
                       std::move(description));
        return false;
    }

    std::string interlisName(const IomName& name,
                             std::string_view description) const {
        if (!name.interlisName().empty()) return name.interlisName();
        fail(DiagnosticCode::UnknownInterlisName,
             std::string(description) + " has no INTERLIS name");
    }

    void writeStartTransfer(const StartTransferEvent& event) {
        if (event.header.version != XtfVersion::V23) {
            fail(DiagnosticCode::ModelMismatch,
                 "XTF 2.3 writer received a non-2.3 transfer header");
        }
        const auto sender = event.header.sender.empty()
                                ? (options.sender.empty() ? std::string("iox-cpp")
                                                          : options.sender)
                                : event.header.sender;

        xml.startDocument();
        start("TRANSFER");
        start("HEADERSECTION");
        attribute("VERSION", "2.3");
        attribute("SENDER", sender);

        start("MODELS");
        for (const auto& model : event.header.models) {
            if (model.name.empty()) {
                fail(DiagnosticCode::MissingModelEntry,
                     "XTF 2.3 MODEL requires NAME");
            }
            if (!model.version || model.version->empty() ||
                !model.uri || model.uri->empty()) {
                strictOrReport(
                    DiagnosticCode::MissingModelEntry,
                    "XTF 2.3 MODEL should provide VERSION and URI");
            }
            start("MODEL");
            attribute("NAME", model.name);
            if (model.version && !model.version->empty()) {
                attribute("VERSION", *model.version);
            }
            if (model.uri && !model.uri->empty()) {
                attribute("URI", *model.uri);
            }
            xml.endElement();
        }
        xml.endElement();

        for (const auto& extension : event.header.extensions) {
            if (isAlias(extension)) writeExtension(extension);
        }

        if (!event.header.oidSpaces.empty()) {
            start("OIDSPACES");
            for (const auto& space : event.header.oidSpaces) {
                if (space.name.empty() || space.domain.empty()) {
                    fail(DiagnosticCode::MissingRequiredHeader,
                         "XTF 2.3 OIDSPACE requires NAME and OIDDOMAIN");
                }
                start("OIDSPACE");
                attribute("NAME", space.name);
                attribute("OIDDOMAIN", space.domain);
                xml.endElement();
            }
            xml.endElement();
        }

        const auto comment = event.header.comment
                                 ? event.header.comment
                                 : (options.comment.empty()
                                        ? std::optional<std::string>{}
                                        : std::optional<std::string>{options.comment});
        if (comment) {
            start("COMMENT");
            xml.text(*comment);
            xml.endElement();
        }

        for (const auto& extension : event.header.extensions) {
            if (!isAlias(extension) &&
                extensionEnabled(extension,
                                 "Unknown transfer-header extension was dropped")) {
                writeExtension(extension);
            }
        }
        xml.endElement();
    }

    bool matchesBasket(const ExtensionElement& extension,
                       const BasketMetadata& basket) const {
        if (extension.name.localName == basket.topic.interlisName()) return true;
        return basket.topic.hasXmlName() &&
               extension.name == basket.topic.xmlName();
    }

    void writeStartBasket(const StartBasketEvent& event) {
        const auto& basket = event.basket;
        if (basket.basketId.empty()) {
            fail(DiagnosticCode::MissingBasketId,
                 "XTF 2.3 basket requires BID");
        }
        if (!dataSectionOpen) {
            start("DATASECTION");
            dataSectionOpen = true;
        }
        xml.startElement(xtfName(interlisName(basket.topic, "Basket topic")));
        attribute("BID", basket.basketId);
        if (const auto kind = basketKindName(basket.kind); !kind.empty()) {
            attribute("KIND", kind);
        }
        if (basket.startState) attribute("STARTSTATE", *basket.startState);
        if (basket.endState) attribute("ENDSTATE", *basket.endState);
        if (const auto consistency = consistencyName(basket.consistency);
            !consistency.empty()) {
            attribute("CONSISTENCY", consistency);
        }
        if (!basket.topics.empty()) attribute("TOPICS", join(basket.topics, ","));
        if (!basket.domains.empty()) {
            strictOrReport(
                DiagnosticCode::UnexpectedAttribute,
                "DOMAINS is preserved as a compatibility extension in XTF 2.3");
            attribute("DOMAINS", join(basket.domains, ","));
        }

        for (const auto& extension : basket.extensions) {
            if (!matchesBasket(extension, basket)) continue;
            if (!options.preserveUnknownExtensions) {
                strictOrReport(DiagnosticCode::UnexpectedAttribute,
                               "Unknown basket attributes were dropped");
                continue;
            }
            report(DiagnosticSeverity::Warning,
                   DiagnosticCode::UnknownExtensionPreserved,
                   "Unknown basket attributes were preserved");
            for (const auto& item : extension.attributes) {
                xml.writeAttribute(item.name, item.value);
            }
            if (!extension.text.empty() || !extension.children.empty()) {
                strictOrReport(
                    DiagnosticCode::UnexpectedElement,
                    "Basket attribute extension also contained nested content");
            }
        }
        for (const auto& extension : basket.extensions) {
            if (matchesBasket(extension, basket)) continue;
            if (extensionEnabled(extension,
                                 "Unknown basket extension was dropped")) {
                writeExtension(extension);
            }
        }
    }

    void writeReferenceAttributes(const ReferenceInfo& reference) {
        if (!reference.targetOid || reference.targetOid->empty()) {
            fail(DiagnosticCode::InvalidReference,
                 "XTF 2.3 reference requires REF");
        }
        attribute("REF", *reference.targetOid);
        if (reference.targetBasketId) {
            if (reference.targetBasketId->empty()) {
                fail(DiagnosticCode::InvalidReference,
                     "Reference BID must not be empty");
            }
            attribute("BID", *reference.targetBasketId);
        }
        if (reference.orderPosition) {
            if (*reference.orderPosition == 0U) {
                fail(DiagnosticCode::InvalidReference,
                     "ORDER_POS must be greater than zero");
            }
            attribute("ORDER_POS", std::to_string(*reference.orderPosition));
        }
    }

    bool isReferencePlaceholder(const IomObject& value) const {
        const auto tag = upperAscii(value.tag().interlisName());
        return (tag == "REF" || tag == "REFERENCE") &&
               value.attributeCount() == 0U;
    }

    bool isOidMarker(const IomObject& value) const {
        return upperAscii(value.tag().interlisName()) == "OID" &&
               value.oid().has_value() && value.attributeCount() == 0U;
    }

    void writeNamedValue(const IomName& name, const IomValue& value);
    void writeStructured(const IomObject& value);
    void writeGenericContents(const IomObject& value);

    void writePrimitiveMember(const IomObject& value,
                              std::string_view name, bool required) {
        const auto count = value.valueCount(name);
        if (count == 0U) {
            if (required) {
                fail(DiagnosticCode::InvalidGeometry,
                     value.tag().interlisName() + " requires " +
                         std::string(name));
            }
            return;
        }
        if (count != 1U || !value.value(name, 0).isPrimitive()) {
            fail(DiagnosticCode::InvalidGeometry,
                 value.tag().interlisName() + "." + std::string(name) +
                     " must be one lexical primitive");
        }
        start(name);
        xml.text(value.value(name, 0).primitive());
        xml.endElement();
    }

    void writeUnknownGeometryMembers(
        const IomObject& value,
        const std::unordered_set<std::string>& known) {
        for (std::size_t attributeIndex = 0;
             attributeIndex < value.attributeCount(); ++attributeIndex) {
            const auto& name = value.attributeName(attributeIndex);
            if (known.count(upperAscii(name.interlisName())) != 0U) continue;
            strictOrReport(DiagnosticCode::InvalidGeometry,
                           "Unknown geometry member was preserved: " +
                               name.interlisName());
            const auto count = value.valueCount(name.interlisName());
            for (std::size_t index = 0; index < count; ++index) {
                writeNamedValue(name, value.value(name.interlisName(), index));
            }
        }
    }

    void writeCoordinate(const IomObject& value) {
        start("COORD");
        writePrimitiveMember(value, "C1", true);
        writePrimitiveMember(value, "C2", false);
        writePrimitiveMember(value, "C3", false);
        writeUnknownGeometryMembers(value, {"C1", "C2", "C3"});
        xml.endElement();
    }

    void writeArc(const IomObject& value) {
        start("ARC");
        writePrimitiveMember(value, "C1", true);
        writePrimitiveMember(value, "C2", true);
        writePrimitiveMember(value, "C3", false);
        writePrimitiveMember(value, "A1", true);
        writePrimitiveMember(value, "A2", true);
        writePrimitiveMember(value, "R", false);
        writeUnknownGeometryMembers(value,
                                    {"C1", "C2", "C3", "A1", "A2", "R"});
        xml.endElement();
    }

    void writeSegments(const IomObject& sequence) {
        const auto count = sequence.valueCount("segment");
        if (count == 0U) {
            fail(DiagnosticCode::InvalidGeometry,
                 "POLYLINE sequence requires at least one segment");
        }
        for (std::size_t index = 0; index < count; ++index) {
            const auto segment = sequence.object("segment", index);
            if (!segment) {
                fail(DiagnosticCode::InvalidGeometry,
                     "POLYLINE segment must be an object");
            }
            writeStructured(*segment);
        }
        if (sequence.attributeCount() != 1U ||
            upperAscii(sequence.attributeName(0).interlisName()) != "SEGMENT") {
            strictOrReport(DiagnosticCode::InvalidGeometry,
                           "Unknown SEGMENTS members cannot be transferred");
        }
    }

    void writePolyline(const IomObject& value) {
        start("POLYLINE");
        const auto lineAttrCount = value.valueCount("lineattr");
        if (lineAttrCount > 1U) {
            fail(DiagnosticCode::InvalidGeometry,
                 "POLYLINE permits at most one lineattr");
        }
        if (lineAttrCount == 1U) {
            const auto lineAttr = value.object("lineattr");
            if (!lineAttr) {
                fail(DiagnosticCode::InvalidGeometry,
                     "POLYLINE lineattr must be an object");
            }
            start("LINEATTR");
            writeStructured(*lineAttr);
            xml.endElement();
        }

        const auto sequenceCount = value.valueCount("sequence");
        if (sequenceCount == 0U) {
            fail(DiagnosticCode::InvalidGeometry,
                 "POLYLINE requires at least one sequence");
        }
        const bool clipped = value.consistency() == Consistency::Incomplete;
        if (!clipped && sequenceCount != 1U) {
            fail(DiagnosticCode::InvalidGeometry,
                 "An unclipped POLYLINE requires exactly one sequence");
        }
        for (std::size_t index = 0; index < sequenceCount; ++index) {
            const auto sequence = value.object("sequence", index);
            if (!sequence) {
                fail(DiagnosticCode::InvalidGeometry,
                     "POLYLINE sequence must be an object");
            }
            if (clipped) start("CLIPPED");
            writeSegments(*sequence);
            if (clipped) xml.endElement();
        }
        writeUnknownGeometryMembers(value, {"LINEATTR", "SEQUENCE"});
        xml.endElement();
    }

    void writeBoundaryContents(const IomObject& boundary) {
        const auto count = boundary.valueCount("polyline");
        if (count == 0U) {
            fail(DiagnosticCode::InvalidGeometry,
                 "BOUNDARY requires at least one polyline");
        }
        for (std::size_t index = 0; index < count; ++index) {
            const auto polyline = boundary.object("polyline", index);
            if (!polyline) {
                fail(DiagnosticCode::InvalidGeometry,
                     "BOUNDARY polyline must be an object");
            }
            writePolyline(*polyline);
        }
        writeUnknownGeometryMembers(boundary, {"POLYLINE"});
    }

    void writeBoundary(const IomObject& boundary) {
        start("BOUNDARY");
        writeBoundaryContents(boundary);
        xml.endElement();
    }

    void writeSurfaceBoundaries(const IomObject& surface) {
        const auto count = surface.valueCount("boundary");
        if (count == 0U) {
            fail(DiagnosticCode::InvalidGeometry,
                 "SURFACE group requires at least one boundary");
        }
        for (std::size_t index = 0; index < count; ++index) {
            const auto boundary = surface.object("boundary", index);
            if (!boundary) {
                fail(DiagnosticCode::InvalidGeometry,
                     "SURFACE boundary must be an object");
            }
            writeBoundary(*boundary);
        }
    }

    void writeSurface(const IomObject& value) {
        start("SURFACE");
        const auto tag = upperAscii(value.tag().interlisName());
        if (tag == "MULTISURFACE") {
            const auto count = value.valueCount("surface");
            if (count == 0U) {
                fail(DiagnosticCode::InvalidGeometry,
                     "MULTISURFACE requires at least one surface");
            }
            const bool clipped = value.consistency() == Consistency::Incomplete;
            if (!clipped && count != 1U) {
                fail(DiagnosticCode::InvalidGeometry,
                     "Unclipped MULTISURFACE requires one surface");
            }
            for (std::size_t index = 0; index < count; ++index) {
                const auto surface = value.object("surface", index);
                if (!surface) {
                    fail(DiagnosticCode::InvalidGeometry,
                         "MULTISURFACE member must be an object");
                }
                if (clipped) start("CLIPPED");
                writeSurfaceBoundaries(*surface);
                writeUnknownGeometryMembers(*surface, {"BOUNDARY"});
                if (clipped) xml.endElement();
            }
            writeUnknownGeometryMembers(value, {"SURFACE"});
        } else {
            const auto clippedCount = value.valueCount("clipped");
            const auto boundaryCount = value.valueCount("boundary");
            if (clippedCount != 0U && boundaryCount != 0U) {
                fail(DiagnosticCode::InvalidGeometry,
                     "SURFACE cannot mix direct and clipped boundaries");
            }
            if (clippedCount != 0U) {
                for (std::size_t index = 0; index < clippedCount; ++index) {
                    const auto clipped = value.object("clipped", index);
                    if (!clipped) {
                        fail(DiagnosticCode::InvalidGeometry,
                             "SURFACE clipped group must be an object");
                    }
                    start("CLIPPED");
                    writeSurfaceBoundaries(*clipped);
                    writeUnknownGeometryMembers(*clipped, {"BOUNDARY"});
                    xml.endElement();
                }
            } else if (value.consistency() == Consistency::Incomplete) {
                start("CLIPPED");
                writeSurfaceBoundaries(value);
                xml.endElement();
            } else {
                writeSurfaceBoundaries(value);
            }
            writeUnknownGeometryMembers(value, {"BOUNDARY", "CLIPPED"});
        }
        xml.endElement();
    }

    void writeObject(const ObjectEvent& event) {
        const auto& object = event.object;
        const auto tag = upperAscii(object.tag().interlisName());
        const bool deleteElement = tag == "DELETE";
        if (!object.oid() || object.oid()->empty()) {
            fail(DiagnosticCode::MissingObjectId,
                 "XTF 2.3 top-level object requires a non-empty TID");
        }
        xml.startElement(xtfName(deleteElement
                                     ? std::string("DELETE")
                                     : interlisName(object.tag(), "Object")));
        attribute("TID", *object.oid());
        if (object.reference().targetBasketId) {
            if (deleteElement) {
                fail(DiagnosticCode::InvalidReference,
                     "DELETE does not permit an object-level BID");
            }
            attribute("BID", *object.reference().targetBasketId);
        }
        if (object.reference().targetOid || object.reference().orderPosition) {
            fail(DiagnosticCode::InvalidReference,
                 "Object-level reference only permits BID");
        }
        if (!deleteElement) {
            if (const auto operation = operationName(object.operation());
                !operation.empty()) {
                attribute("OPERATION", operation);
            }
            if (const auto consistency = consistencyName(object.consistency());
                !consistency.empty()) {
                attribute("CONSISTENCY", consistency);
            }
        }
        writeGenericContents(object);
        xml.endElement();
    }

    void writeEndBasket() { xml.endElement(); }

    void writeEndTransfer() {
        if (!dataSectionOpen) {
            start("DATASECTION");
            dataSectionOpen = true;
        }
        xml.endElement();
        dataSectionOpen = false;
        xml.endElement();
    }
};

void Xtf23Writer::Impl::writeNamedValue(const IomName& name,
                                        const IomValue& value) {
    const auto lexicalName = interlisName(name, "Attribute");
    xml.startElement(xtfName(lexicalName));
    if (value.isPrimitive()) {
        xml.text(value.primitive());
    } else if (isOidMarker(value.object())) {
        if (value.object().oid()->empty()) {
            fail(DiagnosticCode::UnexpectedAttribute,
                 "OID attribute value must not be empty");
        }
        attribute("OID", *value.object().oid());
    } else if (value.object().isReference()) {
        writeReferenceAttributes(value.object().reference());
        if (!isReferencePlaceholder(value.object())) {
            writeStructured(value.object());
        }
    } else {
        writeStructured(value.object());
    }
    xml.endElement();
}

void Xtf23Writer::Impl::writeGenericContents(const IomObject& value) {
    for (std::size_t attributeIndex = 0;
         attributeIndex < value.attributeCount(); ++attributeIndex) {
        const auto& name = value.attributeName(attributeIndex);
        const auto count = value.valueCount(name.interlisName());
        for (std::size_t index = 0; index < count; ++index) {
            writeNamedValue(name, value.value(name.interlisName(), index));
        }
    }
}

void Xtf23Writer::Impl::writeStructured(const IomObject& value) {
    const auto tag = upperAscii(value.tag().interlisName());
    if (tag == "COORD") {
        writeCoordinate(value);
    } else if (tag == "ARC") {
        writeArc(value);
    } else if (tag == "POLYLINE") {
        writePolyline(value);
    } else if (tag == "SURFACE" || tag == "AREA" ||
               tag == "MULTISURFACE") {
        writeSurface(value);
    } else if (tag == "BOUNDARY") {
        writeBoundary(value);
    } else {
        xml.startElement(xtfName(interlisName(value.tag(),
                                              "Structured value")));
        writeGenericContents(value);
        xml.endElement();
    }
}

Xtf23Writer::Xtf23Writer(xml::XmlWriter& xmlWriter,
                         XtfWriterOptions options,
                         DiagnosticHandler addDiagnostic)
    : impl_(std::make_unique<Impl>(xmlWriter, std::move(options),
                                   std::move(addDiagnostic))) {}

Xtf23Writer::~Xtf23Writer() = default;

void Xtf23Writer::writeStartTransfer(const StartTransferEvent& event) {
    impl_->writeStartTransfer(event);
}

void Xtf23Writer::writeStartBasket(const StartBasketEvent& event) {
    impl_->writeStartBasket(event);
}

void Xtf23Writer::writeObject(const ObjectEvent& event) {
    impl_->writeObject(event);
}

void Xtf23Writer::writeEndBasket(const EndBasketEvent&) {
    impl_->writeEndBasket();
}

void Xtf23Writer::writeEndTransfer(const EndTransferEvent&) {
    impl_->writeEndTransfer();
}

} // namespace xtf
} // namespace iox
