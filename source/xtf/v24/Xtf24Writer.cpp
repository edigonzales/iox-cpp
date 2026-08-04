#include "xtf/v24/Xtf24Writer.h"

#include "xml/XmlWriter.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace iox {
namespace xtf {
namespace {

constexpr std::string_view iliNamespace =
    "http://www.interlis.ch/xtf/2.4/INTERLIS";
constexpr std::string_view geometryNamespace =
    "http://www.interlis.ch/geometry/1.0";

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

bool isReferencePlaceholder(const IomObject& value) {
    const auto tag = upperAscii(value.tag().interlisName());
    return (tag == "REF" || tag == "REFERENCE") &&
           value.attributeCount() == 0U;
}

} // namespace

struct Xtf24Writer::Impl final {
    xml::XmlWriter& xml;
    XtfWriterOptions options;
    DiagnosticHandler addDiagnostic;
    std::map<std::string, std::string> modelNamespaces;
    std::map<std::string, std::string> modelPrefixes;
    std::map<std::string, std::string> prefixOwners;
    std::size_t nextPrefix = 0;

    Impl(xml::XmlWriter& writer, XtfWriterOptions value,
         DiagnosticHandler diagnosticHandler)
        : xml(writer), options(std::move(value)),
          addDiagnostic(std::move(diagnosticHandler)) {
        prefixOwners.emplace("ili", std::string(iliNamespace));
        prefixOwners.emplace("geom", std::string(geometryNamespace));
    }

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

    static XmlQualifiedName iliName(std::string_view localName) {
        return {std::string(iliNamespace), std::string(localName), "ili"};
    }

    static XmlQualifiedName geometryName(std::string_view localName) {
        return {std::string(geometryNamespace), std::string(localName),
                "geom"};
    }

    void startIli(std::string_view name) { xml.startElement(iliName(name)); }
    void startGeometry(std::string_view name) {
        xml.startElement(geometryName(name));
    }
    void iliAttribute(std::string_view name, std::string_view value) {
        xml.writeAttribute(iliName(name), value);
    }

    std::string uniquePrefix(std::string preferred) {
        if (!preferred.empty() && preferred != "xml" &&
            prefixOwners.count(preferred) == 0U) {
            return preferred;
        }
        std::string result;
        do {
            result = "m" + std::to_string(nextPrefix++);
        } while (prefixOwners.count(result) != 0U);
        return result;
    }

    void writeExtension(const ExtensionElement& extension) {
        if (extension.name.localName.empty()) {
            fail(DiagnosticCode::UnknownInterlisName,
                 "Extension element has no XML local name");
        }
        xml.startElement(extension.name);
        for (const auto& attribute : extension.attributes) {
            if (attribute.name.localName.empty()) {
                fail(DiagnosticCode::UnknownInterlisName,
                     "Extension attribute has no XML local name");
            }
            xml.writeAttribute(attribute.name, attribute.value);
        }
        if (!extension.text.empty()) xml.text(extension.text);
        for (const auto& child : extension.children) writeExtension(child);
        xml.endElement();
    }

    bool extensionEnabled(const ExtensionElement& extension,
                          std::string description) {
        if (options.preserveUnknownExtensions) {
            report(DiagnosticSeverity::Warning,
                   DiagnosticCode::UnknownExtensionPreserved,
                   "Unknown XTF 2.4 extension was preserved: " +
                       extension.name.localName);
            return true;
        }
        strictOrReport(DiagnosticCode::UnexpectedElement,
                       std::move(description));
        return false;
    }

    XmlQualifiedName requireXmlName(const IomName& name,
                                    std::string_view description) const {
        if (!name.hasXmlName() || name.xmlName().namespaceUri.empty() ||
            name.xmlName().localName.empty()) {
            fail(DiagnosticCode::UnknownInterlisName,
                 std::string(description) +
                     " requires an explicit XML QName in XTF 2.4");
        }
        if (name.xmlName().namespaceUri == iliNamespace ||
            name.xmlName().namespaceUri == geometryNamespace) {
            fail(DiagnosticCode::InvalidXtfNamespace,
                 std::string(description) +
                     " must use a model XML namespace");
        }
        return name.xmlName();
    }

    XmlQualifiedName basketXmlName(const IomName& name) const {
        if (name.hasXmlName()) return requireXmlName(name, "Basket topic");
        const auto separator = name.interlisName().find('.');
        if (separator == std::string::npos ||
            name.interlisName().find('.', separator + 1U) !=
                std::string::npos) {
            fail(DiagnosticCode::UnknownInterlisName,
                 "XTF 2.4 basket topic without a QName must be Model.Topic");
        }
        const auto model = name.interlisName().substr(0, separator);
        const auto found = modelNamespaces.find(model);
        if (found == modelNamespaces.end()) {
            fail(DiagnosticCode::UnknownInterlisName,
                 "Basket model has no explicit header XML namespace: " +
                     model);
        }
        const auto prefix = modelPrefixes.find(model);
        return {found->second,
                name.interlisName().substr(separator + 1U),
                prefix == modelPrefixes.end() ? std::string{}
                                               : prefix->second};
    }

    void writeStartTransfer(const StartTransferEvent& event) {
        if (event.header.version != XtfVersion::V24) {
            fail(DiagnosticCode::ModelMismatch,
                 "XTF 2.4 writer received a non-2.4 transfer header");
        }
        if (event.header.models.empty()) {
            fail(DiagnosticCode::MissingModelEntry,
                 "XTF 2.4 writer requires at least one model");
        }

        xml.startDocument();
        startIli("transfer");
        xml.writeNamespace("geom", geometryNamespace);

        for (const auto& model : event.header.models) {
            if (model.name.empty()) {
                fail(DiagnosticCode::MissingModelEntry,
                     "XTF 2.4 model name must not be empty");
            }
            if (model.version || model.uri) {
                strictOrReport(
                    DiagnosticCode::UnexpectedAttribute,
                    "XTF 2.4 model VERSION/URI metadata is not representable");
            }
            if (model.xmlNamespace.namespaceUri.empty()) continue;
            if (!model.xmlNamespace.localName.empty() &&
                model.xmlNamespace.localName != model.name) {
                strictOrReport(
                    DiagnosticCode::ModelMismatch,
                    "Model XML namespace name differs from model name");
            }
            const auto existing = modelNamespaces.find(model.name);
            if (existing != modelNamespaces.end() &&
                existing->second != model.xmlNamespace.namespaceUri) {
                fail(DiagnosticCode::ModelMismatch,
                     "Model name maps to multiple XML namespaces");
            }
            const auto prefix = uniquePrefix(model.xmlNamespace.prefixHint);
            modelNamespaces[model.name] = model.xmlNamespace.namespaceUri;
            modelPrefixes[model.name] = prefix;
            prefixOwners[prefix] = model.xmlNamespace.namespaceUri;
            xml.writeNamespace(prefix, model.xmlNamespace.namespaceUri);
        }

        startIli("headersection");
        startIli("models");
        for (const auto& model : event.header.models) {
            startIli("model");
            xml.text(model.name);
            xml.endElement();
        }
        xml.endElement();

        const auto sender = event.header.sender.empty()
                                ? options.sender
                                : event.header.sender;
        if (!sender.empty()) {
            startIli("sender");
            xml.text(sender);
            xml.endElement();
        }
        const auto comment = event.header.comment
                                 ? event.header.comment
                                 : (options.comment.empty()
                                        ? std::optional<std::string>{}
                                        : std::optional<std::string>{
                                              options.comment});
        if (comment) {
            startIli("comment");
            xml.text(*comment);
            xml.endElement();
        }
        if (!event.header.oidSpaces.empty()) {
            strictOrReport(DiagnosticCode::UnexpectedElement,
                           "XTF 2.4 has no OIDSPACES header element");
        }
        for (const auto& extension : event.header.extensions) {
            if (extensionEnabled(extension,
                                 "Unknown XTF 2.4 header extension was "
                                 "dropped")) {
                writeExtension(extension);
            }
        }
        xml.endElement();
        startIli("datasection");
    }

    bool matchesBasket(const ExtensionElement& extension,
                       const BasketMetadata& basket) const {
        return basket.topic.hasXmlName() &&
               extension.name == basket.topic.xmlName();
    }

    void writeStartBasket(const StartBasketEvent& event) {
        const auto& basket = event.basket;
        if (basket.basketId.empty()) {
            fail(DiagnosticCode::MissingBasketId,
                 "XTF 2.4 basket requires a non-empty BID");
        }
        xml.startElement(basketXmlName(basket.topic));
        iliAttribute("bid", basket.basketId);
        if (const auto kind = basketKindName(basket.kind); !kind.empty()) {
            iliAttribute("kind", kind);
            if (!basket.startState || !basket.endState) {
                strictOrReport(
                    DiagnosticCode::MissingRequiredHeader,
                    "Incremental XTF 2.4 basket requires startstate and "
                    "endstate");
            }
        }
        if (basket.startState) iliAttribute("startstate", *basket.startState);
        if (basket.endState) iliAttribute("endstate", *basket.endState);
        if (!basket.domains.empty()) {
            iliAttribute("domains", join(basket.domains, " "));
        }
        if (basket.consistency == Consistency::Incomplete) {
            iliAttribute("consistency", "INCOMPLETE");
        } else if (basket.consistency == Consistency::Inconsistent ||
                   basket.consistency == Consistency::Adapted) {
            strictOrReport(
                DiagnosticCode::UnexpectedAttribute,
                "XTF 2.4 basket consistency is not representable");
        }
        if (!basket.topics.empty()) {
            strictOrReport(DiagnosticCode::UnexpectedAttribute,
                           "XTF 2.4 has no TOPICS basket attribute");
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
            for (const auto& attribute : extension.attributes) {
                xml.writeAttribute(attribute.name, attribute.value);
            }
            if (!extension.text.empty() || !extension.children.empty()) {
                strictOrReport(
                    DiagnosticCode::UnexpectedElement,
                    "Basket attribute extension contains nested content");
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
                 "XTF 2.4 reference requires a non-empty REF");
        }
        iliAttribute("ref", *reference.targetOid);
        if (reference.targetBasketId) {
            if (reference.targetBasketId->empty()) {
                fail(DiagnosticCode::InvalidReference,
                     "Reference BID must not be empty");
            }
            iliAttribute("bid", *reference.targetBasketId);
        }
        if (reference.orderPosition) {
            if (*reference.orderPosition == 0U) {
                fail(DiagnosticCode::InvalidReference,
                     "ORDER_POS must be greater than zero");
            }
            iliAttribute("order_pos",
                         std::to_string(*reference.orderPosition));
        }
    }

    void writeNamedValue(const IomName& name, const IomValue& value);
    void writeStructured(const IomObject& value);
    void writeGenericContents(const IomObject& value);

    void writePrimitiveMember(const IomObject& value,
                              std::string_view member,
                              bool required) {
        const auto count = value.valueCount(member);
        if (count == 0U) {
            if (required) {
                fail(DiagnosticCode::InvalidGeometry,
                     value.tag().interlisName() + " requires " +
                         std::string(member));
            }
            return;
        }
        if (count != 1U || !value.value(member, 0).isPrimitive()) {
            fail(DiagnosticCode::InvalidGeometry,
                 value.tag().interlisName() + "." + std::string(member) +
                     " must be one lexical primitive");
        }
        auto localName = std::string(member);
        std::transform(localName.begin(), localName.end(), localName.begin(),
                       [](unsigned char character) {
                           return static_cast<char>(std::tolower(character));
                       });
        startGeometry(localName);
        xml.text(value.value(member, 0).primitive());
        xml.endElement();
    }

    void diagnoseUnknownGeometryMembers(
        const IomObject& value,
        const std::unordered_set<std::string>& known) {
        for (std::size_t index = 0; index < value.attributeCount(); ++index) {
            const auto& name = value.attributeName(index);
            if (known.count(upperAscii(name.interlisName())) == 0U) {
                strictOrReport(DiagnosticCode::InvalidGeometry,
                               "Unknown geometry member was not emitted: " +
                                   name.interlisName());
            }
        }
    }

    void writeCoordinate(const IomObject& value) {
        startGeometry("coord");
        writePrimitiveMember(value, "C1", true);
        writePrimitiveMember(value, "C2", false);
        if (value.valueCount("C3") != 0U && value.valueCount("C2") == 0U) {
            fail(DiagnosticCode::InvalidGeometry,
                 "COORD.C3 requires COORD.C2");
        }
        writePrimitiveMember(value, "C3", false);
        diagnoseUnknownGeometryMembers(value, {"C1", "C2", "C3"});
        xml.endElement();
    }

    void writeArc(const IomObject& value) {
        startGeometry("arc");
        writePrimitiveMember(value, "C1", true);
        writePrimitiveMember(value, "C2", true);
        writePrimitiveMember(value, "C3", false);
        writePrimitiveMember(value, "A1", true);
        writePrimitiveMember(value, "A2", true);
        writePrimitiveMember(value, "R", false);
        diagnoseUnknownGeometryMembers(
            value, {"C1", "C2", "C3", "A1", "A2", "R"});
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
        diagnoseUnknownGeometryMembers(sequence, {"SEGMENT"});
    }

    void writePolyline(const IomObject& value) {
        startGeometry("polyline");
        if (value.valueCount("lineattr") != 0U) {
            strictOrReport(DiagnosticCode::InvalidGeometry,
                           "XTF 2.4 does not encode line attributes");
        }
        const auto count = value.valueCount("sequence");
        if (count != 1U) {
            fail(DiagnosticCode::InvalidGeometry,
                 "XTF 2.4 POLYLINE requires exactly one sequence; use "
                 "MULTIPOLYLINE for multiple parts");
        }
        const auto sequence = value.object("sequence");
        if (!sequence) {
            fail(DiagnosticCode::InvalidGeometry,
                 "POLYLINE sequence must be an object");
        }
        writeSegments(*sequence);
        diagnoseUnknownGeometryMembers(value, {"SEQUENCE", "LINEATTR"});
        xml.endElement();
    }

    void writeMultiCoord(const IomObject& value) {
        startGeometry("multicoord");
        const auto count = value.valueCount("coord");
        if (count == 0U) {
            fail(DiagnosticCode::InvalidGeometry,
                 "MULTICOORD requires at least one coord");
        }
        for (std::size_t index = 0; index < count; ++index) {
            const auto coord = value.object("coord", index);
            if (!coord || upperAscii(coord->tag().interlisName()) != "COORD") {
                fail(DiagnosticCode::InvalidGeometry,
                     "MULTICOORD member must be COORD");
            }
            writeCoordinate(*coord);
        }
        diagnoseUnknownGeometryMembers(value, {"COORD"});
        xml.endElement();
    }

    void writeMultiPolyline(const IomObject& value) {
        startGeometry("multipolyline");
        const auto count = value.valueCount("polyline");
        if (count == 0U) {
            fail(DiagnosticCode::InvalidGeometry,
                 "MULTIPOLYLINE requires at least one polyline");
        }
        for (std::size_t index = 0; index < count; ++index) {
            const auto polyline = value.object("polyline", index);
            if (!polyline ||
                upperAscii(polyline->tag().interlisName()) != "POLYLINE") {
                fail(DiagnosticCode::InvalidGeometry,
                     "MULTIPOLYLINE member must be POLYLINE");
            }
            writePolyline(*polyline);
        }
        diagnoseUnknownGeometryMembers(value, {"POLYLINE"});
        xml.endElement();
    }

    void writeBoundary(const IomObject& boundary,
                       std::string_view wrapper) {
        startGeometry(wrapper);
        if (upperAscii(boundary.tag().interlisName()) == "POLYLINE") {
            writePolyline(boundary);
        } else {
            const auto count = boundary.valueCount("polyline");
            if (count == 0U) {
                fail(DiagnosticCode::InvalidGeometry,
                     "Surface boundary requires a polyline");
            }
            for (std::size_t index = 0; index < count; ++index) {
                const auto polyline = boundary.object("polyline", index);
                if (!polyline) {
                    fail(DiagnosticCode::InvalidGeometry,
                         "Boundary polyline must be an object");
                }
                writePolyline(*polyline);
            }
            diagnoseUnknownGeometryMembers(boundary, {"POLYLINE"});
        }
        xml.endElement();
    }

    void writeSurface(const IomObject& value) {
        startGeometry("surface");
        const auto exteriorCount = value.valueCount("exterior");
        const auto directCount = value.valueCount("boundary");
        if (exteriorCount != 0U && directCount != 0U) {
            fail(DiagnosticCode::InvalidGeometry,
                 "SURFACE cannot mix exterior and boundary forms");
        }
        if (exteriorCount != 0U) {
            if (exteriorCount != 1U) {
                fail(DiagnosticCode::InvalidGeometry,
                     "SURFACE requires exactly one exterior");
            }
            const auto exterior = value.object("exterior");
            if (!exterior) {
                fail(DiagnosticCode::InvalidGeometry,
                     "SURFACE exterior must be an object");
            }
            writeBoundary(*exterior, "exterior");
            const auto interiorCount = value.valueCount("interior");
            for (std::size_t index = 0; index < interiorCount; ++index) {
                const auto interior = value.object("interior", index);
                if (!interior) {
                    fail(DiagnosticCode::InvalidGeometry,
                         "SURFACE interior must be an object");
                }
                writeBoundary(*interior, "interior");
            }
            diagnoseUnknownGeometryMembers(
                value, {"EXTERIOR", "INTERIOR"});
        } else {
            if (directCount == 0U) {
                fail(DiagnosticCode::InvalidGeometry,
                     "SURFACE requires an exterior boundary");
            }
            for (std::size_t index = 0; index < directCount; ++index) {
                const auto boundary = value.object("boundary", index);
                if (!boundary) {
                    fail(DiagnosticCode::InvalidGeometry,
                         "SURFACE boundary must be an object");
                }
                writeBoundary(*boundary,
                              index == 0U ? "exterior" : "interior");
            }
            diagnoseUnknownGeometryMembers(value, {"BOUNDARY"});
        }
        xml.endElement();
    }

    void writeMultiSurface(const IomObject& value) {
        startGeometry("multisurface");
        auto member = std::string("surface");
        auto count = value.valueCount(member);
        if (count == 0U) {
            member = "area";
            count = value.valueCount(member);
        }
        if (count == 0U) {
            fail(DiagnosticCode::InvalidGeometry,
                 "MULTISURFACE requires at least one surface");
        }
        for (std::size_t index = 0; index < count; ++index) {
            const auto surface = value.object(member, index);
            if (!surface) {
                fail(DiagnosticCode::InvalidGeometry,
                     "MULTISURFACE member must be an object");
            }
            writeSurface(*surface);
        }
        diagnoseUnknownGeometryMembers(value, {upperAscii(member)});
        xml.endElement();
    }

    void writeObject(const ObjectEvent& event) {
        const auto& object = event.object;
        const auto tag = upperAscii(object.tag().interlisName());
        const bool deletion =
            tag == "DELETE" ||
            (object.tag().hasXmlName() &&
             object.tag().xmlName().namespaceUri == iliNamespace &&
             object.tag().xmlName().localName == "delete");
        if (deletion) startIli("delete");
        else xml.startElement(requireXmlName(object.tag(), "Object"));

        if (object.oid()) {
            if (object.oid()->empty()) {
                fail(DiagnosticCode::MissingObjectId,
                     "Object TID must not be empty");
            }
            iliAttribute("tid", *object.oid());
        }
        if (object.reference().targetBasketId) {
            strictOrReport(DiagnosticCode::UnexpectedAttribute,
                           "Object-level BID is a non-normative extension");
            iliAttribute("bid", *object.reference().targetBasketId);
        }
        if (object.reference().targetOid || object.reference().orderPosition) {
            fail(DiagnosticCode::InvalidReference,
                 "Object-level reference only permits BID");
        }
        if (deletion) {
            if (object.operation() != ObjectOperation::Delete &&
                object.operation() != ObjectOperation::None) {
                strictOrReport(DiagnosticCode::InvalidEventOrder,
                               "ili:delete has a conflicting operation");
            }
        } else if (const auto operation = operationName(object.operation());
                   !operation.empty()) {
            iliAttribute("operation", operation);
        }
        if (object.consistency() != Consistency::Complete &&
            object.consistency() != Consistency::Unspecified) {
            strictOrReport(DiagnosticCode::UnexpectedAttribute,
                           "XTF 2.4 object consistency is not representable");
        }
        writeGenericContents(object);
        xml.endElement();
    }

    void writeEndBasket() { xml.endElement(); }

    void writeEndTransfer() {
        xml.endElement();
        xml.endElement();
    }
};

void Xtf24Writer::Impl::writeNamedValue(const IomName& name,
                                        const IomValue& value) {
    if (value.isObject() && name.hasXmlName() &&
        value.object().tag().hasXmlName() &&
        name.xmlName() == value.object().tag().xmlName() &&
        name.xmlName().namespaceUri == geometryNamespace) {
        writeStructured(value.object());
        return;
    }
    xml.startElement(requireXmlName(name, "Attribute"));
    if (value.isPrimitive()) {
        xml.text(value.primitive());
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

void Xtf24Writer::Impl::writeGenericContents(const IomObject& value) {
    for (std::size_t attributeIndex = 0;
         attributeIndex < value.attributeCount(); ++attributeIndex) {
        const auto& name = value.attributeName(attributeIndex);
        const auto count = value.valueCount(name.interlisName());
        for (std::size_t index = 0; index < count; ++index) {
            if (!name.hasXmlName() && name.interlisName() == "text" &&
                value.value(name.interlisName(), index).isPrimitive()) {
                xml.text(value.value(name.interlisName(), index).primitive());
                continue;
            }
            writeNamedValue(name, value.value(name.interlisName(), index));
        }
    }
}

void Xtf24Writer::Impl::writeStructured(const IomObject& value) {
    const auto tag = upperAscii(value.tag().interlisName());
    if (tag == "COORD") writeCoordinate(value);
    else if (tag == "ARC") writeArc(value);
    else if (tag == "POLYLINE") writePolyline(value);
    else if (tag == "MULTICOORD") writeMultiCoord(value);
    else if (tag == "MULTIPOLYLINE") writeMultiPolyline(value);
    else if (tag == "SURFACE" || tag == "AREA") writeSurface(value);
    else if (tag == "MULTISURFACE" || tag == "MULTIAREA") {
        writeMultiSurface(value);
    } else {
        if (!value.tag().hasXmlName() ||
            value.tag().xmlName().namespaceUri.empty() ||
            value.tag().xmlName().localName.empty()) {
            fail(DiagnosticCode::UnknownInterlisName,
                 "Structured value requires an explicit XML QName");
        }
        if (value.tag().xmlName().namespaceUri == geometryNamespace) {
            if (!options.preserveUnknownExtensions) {
                strictOrReport(DiagnosticCode::InvalidGeometry,
                               "Unknown geometry value was not emitted");
                return;
            }
            report(DiagnosticSeverity::Warning,
                   DiagnosticCode::UnknownExtensionPreserved,
                   "Unknown geometry value was preserved: " +
                       value.tag().xmlName().localName);
        } else if (value.tag().xmlName().namespaceUri == iliNamespace) {
            fail(DiagnosticCode::InvalidXtfNamespace,
                 "Structured model value must not use the ili namespace");
        }
        xml.startElement(value.tag().xmlName());
        writeGenericContents(value);
        xml.endElement();
    }
}

Xtf24Writer::Xtf24Writer(xml::XmlWriter& xmlWriter,
                         XtfWriterOptions options,
                         DiagnosticHandler addDiagnostic)
    : impl_(std::make_unique<Impl>(xmlWriter, std::move(options),
                                   std::move(addDiagnostic))) {}

Xtf24Writer::~Xtf24Writer() = default;

void Xtf24Writer::writeStartTransfer(const StartTransferEvent& event) {
    impl_->writeStartTransfer(event);
}

void Xtf24Writer::writeStartBasket(const StartBasketEvent& event) {
    impl_->writeStartBasket(event);
}

void Xtf24Writer::writeObject(const ObjectEvent& event) {
    impl_->writeObject(event);
}

void Xtf24Writer::writeEndBasket(const EndBasketEvent&) {
    impl_->writeEndBasket();
}

void Xtf24Writer::writeEndTransfer(const EndTransferEvent&) {
    impl_->writeEndTransfer();
}

} // namespace xtf
} // namespace iox
