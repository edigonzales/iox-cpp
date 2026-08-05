#include "xtf/v24/Xtf24Dialect.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace iox {
namespace xtf {
namespace {

constexpr std::string_view iliNamespace =
    "http://www.interlis.ch/xtf/2.4/INTERLIS";
constexpr std::string_view geometryNamespace =
    "http://www.interlis.ch/geometry/1.0";
constexpr std::string_view schemaInstanceNamespace =
    "http://www.w3.org/2001/XMLSchema-instance";
constexpr std::string_view modelNamespaceBase =
    "http://www.interlis.ch/xtf/2.4/";

struct Node final {
    XmlQualifiedName name;
    std::vector<xml::XmlAttribute> attributes;
    std::string text;
    std::vector<Node> children;
    SourceLocation location;
};

bool isWhitespace(std::string_view value) {
    return std::all_of(value.begin(), value.end(), [](unsigned char character) {
        return std::isspace(character) != 0;
    });
}

std::string trimAscii(std::string_view value) {
    std::size_t first = 0;
    while (first < value.size() &&
           std::isspace(static_cast<unsigned char>(value[first])) != 0) {
        ++first;
    }
    std::size_t last = value.size();
    while (last > first &&
           std::isspace(static_cast<unsigned char>(value[last - 1U])) != 0) {
        --last;
    }
    return std::string(value.substr(first, last - first));
}

std::string upperAscii(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const auto character : value) {
        result.push_back(static_cast<char>(
            std::toupper(static_cast<unsigned char>(character))));
    }
    return result;
}

bool isControl(const XmlQualifiedName& name, std::string_view localName) {
    return name.namespaceUri == iliNamespace && name.localName == localName;
}

bool isGeometry(const XmlQualifiedName& name, std::string_view localName) {
    return name.namespaceUri == geometryNamespace &&
           name.localName == localName;
}

const xml::XmlAttribute* findIliAttribute(
    const std::vector<xml::XmlAttribute>& attributes,
    std::string_view localName) {
    for (const auto& attribute : attributes) {
        if (attribute.name.namespaceUri == iliNamespace &&
            attribute.name.localName == localName) {
            return &attribute;
        }
    }
    return nullptr;
}

const xml::XmlAttribute* findIliAttribute(const Node& node,
                                          std::string_view localName) {
    return findIliAttribute(node.attributes, localName);
}

ExtensionElement extensionFrom(const Node& node) {
    ExtensionElement result;
    result.name = node.name;
    result.text = node.text;
    result.attributes.reserve(node.attributes.size());
    for (const auto& attribute : node.attributes) {
        result.attributes.push_back({attribute.name, attribute.value});
    }
    result.children.reserve(node.children.size());
    for (const auto& child : node.children) {
        result.children.push_back(extensionFrom(child));
    }
    return result;
}

std::vector<std::string> splitWhitespace(std::string_view value) {
    std::vector<std::string> result;
    std::size_t offset = 0;
    while (offset < value.size()) {
        while (offset < value.size() &&
               std::isspace(static_cast<unsigned char>(value[offset])) != 0) {
            ++offset;
        }
        const auto begin = offset;
        while (offset < value.size() &&
               std::isspace(static_cast<unsigned char>(value[offset])) == 0) {
            ++offset;
        }
        if (offset != begin) {
            result.emplace_back(value.substr(begin, offset - begin));
        }
    }
    return result;
}

} // namespace

enum class DialectState {
    ExpectHeader,
    CapturingHeader,
    ExpectData,
    InData,
    InBasket,
    InObject,
    AfterData,
    Done,
    Failed
};

enum class CaptureKind { None, Header, Attribute };

struct Xtf24Dialect::Impl final {
    XtfReaderOptions options;
    std::vector<xml::XmlNamespaceDeclaration> rootNamespaces;
    EventHandler emitEvent;
    DiagnosticHandler addDiagnostic;
    DialectState state = DialectState::ExpectHeader;
    CaptureKind captureKind = CaptureKind::None;
    std::vector<Node> nodes;
    std::optional<IomObject> currentObject;
    XmlQualifiedName currentObjectName;
    XmlQualifiedName currentBasketName;
    IomName currentBasketTopic;
    std::unordered_map<std::string, std::string> namespaceModels;

    Impl(XtfReaderOptions value,
         std::vector<xml::XmlNamespaceDeclaration> namespaces,
         EventHandler eventHandler,
         DiagnosticHandler diagnosticHandler)
        : options(std::move(value)), rootNamespaces(std::move(namespaces)),
          emitEvent(std::move(eventHandler)),
          addDiagnostic(std::move(diagnosticHandler)) {}

    [[noreturn]] void fail(DiagnosticCode code, std::string message,
                           SourceLocation location) {
        state = DialectState::Failed;
        throw IoxError(code, std::move(message), std::move(location));
    }

    void report(DiagnosticSeverity severity, DiagnosticCode code,
                std::string message, const SourceLocation& location) {
        addDiagnostic({severity, code, std::move(message), location, {}});
    }

    void strictOrReport(DiagnosticCode code, std::string message,
                        const SourceLocation& location) {
        if (options.strictness == Strictness::Strict) {
            fail(code, std::move(message), location);
        }
        report(DiagnosticSeverity::Error, code, std::move(message), location);
    }

    void preserveExtension(const Node& node,
                           std::vector<ExtensionElement>& target,
                           std::string description) {
        if (options.preserveUnknownExtensions) {
            target.push_back(extensionFrom(node));
            report(DiagnosticSeverity::Warning,
                   DiagnosticCode::UnknownExtensionPreserved,
                   std::move(description), node.location);
            return;
        }
        strictOrReport(DiagnosticCode::UnexpectedElement,
                       std::move(description), node.location);
    }

    void beginCapture(CaptureKind kind,
                      const xml::XmlStartElement& element) {
        captureKind = kind;
        nodes.push_back({element.name, element.attributes, {}, {},
                         element.location});
    }

    void captureStart(const xml::XmlStartElement& element) {
        nodes.push_back({element.name, element.attributes, {}, {},
                         element.location});
    }

    std::vector<xml::XmlAttribute> unknownControlAttributes(
        const std::vector<xml::XmlAttribute>& attributes,
        const std::unordered_set<std::string>& allowed) const {
        std::vector<xml::XmlAttribute> result;
        for (const auto& attribute : attributes) {
            if (attribute.name.namespaceUri != iliNamespace ||
                allowed.count(attribute.name.localName) == 0U) {
                result.push_back(attribute);
            }
        }
        return result;
    }

    void reportUnrepresentableAttributes(
        const std::vector<xml::XmlAttribute>& attributes,
        const std::unordered_set<std::string>& allowed,
        std::string_view owner, const SourceLocation& location) {
        for (const auto& attribute :
             unknownControlAttributes(attributes, allowed)) {
            strictOrReport(
                DiagnosticCode::UnexpectedAttribute,
                "Unknown XTF 2.4 " + std::string(owner) +
                    " attribute cannot be represented on IomObject: " +
                    attribute.name.expanded(),
                location);
        }
    }

    void preserveBasketAttributes(const xml::XmlStartElement& element,
                                  BasketMetadata& basket) {
        const auto unknown = unknownControlAttributes(
            element.attributes,
            {"bid", "kind", "startstate", "endstate", "domains",
             "consistency"});
        if (unknown.empty()) return;
        Node extension;
        extension.name = element.name;
        extension.attributes = unknown;
        extension.location = element.location;
        preserveExtension(extension, basket.extensions,
                          "Unknown XTF 2.4 basket attribute was preserved");
    }

    std::optional<std::string> optionalIliAttribute(
        const std::vector<xml::XmlAttribute>& attributes,
        std::string_view name) const {
        const auto* attribute = findIliAttribute(attributes, name);
        return attribute == nullptr
                   ? std::optional<std::string>{}
                   : std::optional<std::string>{attribute->value};
    }

    std::optional<std::string> optionalIliAttribute(
        const Node& node, std::string_view name) const {
        return optionalIliAttribute(node.attributes, name);
    }

    IomName basketName(const XmlQualifiedName& name) const {
        const auto model = namespaceModels.find(name.namespaceUri);
        if (model == namespaceModels.end()) return IomName(name.localName, name);
        return IomName(model->second + "." + name.localName, name);
    }

    IomName objectName(const XmlQualifiedName& name) const {
        const auto model = namespaceModels.find(name.namespaceUri);
        if (model == namespaceModels.end()) return IomName(name.localName, name);
        if (name.localName.find('.') != std::string::npos) {
            return IomName(model->second + "." + name.localName, name);
        }
        if (currentBasketName.namespaceUri == name.namespaceUri &&
            currentBasketTopic.interlisName().rfind(model->second + ".", 0U) ==
                0U) {
            return IomName(currentBasketTopic.interlisName() + "." +
                               name.localName,
                           name);
        }
        return IomName(name.localName, name);
    }

    static IomName memberName(const XmlQualifiedName& name) {
        return IomName(name.localName, name);
    }

    void assignModelNamespaces(TransferHeader& header) {
        std::vector<xml::XmlNamespaceDeclaration> candidates;
        for (const auto& declaration : rootNamespaces) {
            if (declaration.namespaceUri.empty() ||
                declaration.namespaceUri == iliNamespace ||
                declaration.namespaceUri == geometryNamespace ||
                declaration.namespaceUri == schemaInstanceNamespace) {
                continue;
            }
            candidates.push_back(declaration);
        }

        std::unordered_set<std::string> assignedUris;
        for (auto& model : header.models) {
            const auto expected = std::string(modelNamespaceBase) + model.name;
            const auto found = std::find_if(
                candidates.begin(), candidates.end(), [&](const auto& item) {
                    return item.namespaceUri == expected;
                });
            if (found != candidates.end()) {
                model.xmlNamespace = {found->namespaceUri, model.name,
                                      found->prefix};
                namespaceModels[found->namespaceUri] = model.name;
                assignedUris.insert(found->namespaceUri);
            }
        }

        if (header.models.size() == 1U && candidates.size() == 1U &&
            assignedUris.empty()) {
            auto& model = header.models.front();
            model.xmlNamespace = {candidates.front().namespaceUri, model.name,
                                  candidates.front().prefix};
            namespaceModels[candidates.front().namespaceUri] = model.name;
        }
    }

    void parseModel(const Node& node, TransferHeader& header) {
        if (!isControl(node.name, "model")) {
            preserveExtension(node, header.extensions,
                              "Unknown element inside ili:models was preserved");
            return;
        }
        const auto name = trimAscii(node.text);
        if (name.empty()) {
            strictOrReport(DiagnosticCode::MissingModelEntry,
                           "XTF 2.4 ili:model requires a model name",
                           node.location);
            preserveExtension(node, header.extensions,
                              "Empty ili:model was preserved as an extension");
            return;
        }
        if (!node.children.empty()) {
            strictOrReport(DiagnosticCode::UnexpectedElement,
                           "XTF 2.4 ili:model must contain text only",
                           node.location);
        }
        if (!node.attributes.empty()) {
            Node extension;
            extension.name = node.name;
            extension.attributes = node.attributes;
            extension.location = node.location;
            preserveExtension(extension, header.extensions,
                              "Unknown ili:model attribute was preserved");
        }
        header.models.push_back({name, std::nullopt, std::nullopt, {}});
    }

    void parseModels(const Node& node, TransferHeader& header) {
        if (!node.attributes.empty()) {
            Node extension;
            extension.name = node.name;
            extension.attributes = node.attributes;
            extension.location = node.location;
            preserveExtension(extension, header.extensions,
                              "Unknown ili:models attribute was preserved");
        }
        if (!isWhitespace(node.text)) {
            strictOrReport(DiagnosticCode::UnexpectedElement,
                           "ili:models contains non-whitespace text",
                           node.location);
        }
        for (const auto& child : node.children) parseModel(child, header);
    }

    void parseHeader(Node headerNode) {
        TransferHeader header;
        header.version = XtfVersion::V24;
        bool modelsSeen = false;
        bool senderSeen = false;
        bool commentSeen = false;
        std::size_t ordinal = 0;
        for (const auto& child : headerNode.children) {
            if (isControl(child.name, "models")) {
                if (modelsSeen || ordinal != 0U) {
                    strictOrReport(
                        DiagnosticCode::InvalidEventOrder,
                        "ili:models must occur exactly once and first in "
                        "ili:headersection",
                        child.location);
                }
                parseModels(child, header);
                modelsSeen = true;
            } else if (isControl(child.name, "sender")) {
                if (!modelsSeen || senderSeen || commentSeen) {
                    strictOrReport(DiagnosticCode::InvalidEventOrder,
                                   "ili:sender occurs out of order",
                                   child.location);
                }
                if (!child.children.empty()) {
                    strictOrReport(DiagnosticCode::UnexpectedElement,
                                   "ili:sender must contain text only",
                                   child.location);
                }
                if (!child.attributes.empty()) {
                    Node extension;
                    extension.name = child.name;
                    extension.attributes = child.attributes;
                    extension.location = child.location;
                    preserveExtension(
                        extension, header.extensions,
                        "Unknown ili:sender attribute was preserved");
                }
                header.sender = child.text;
                senderSeen = true;
            } else if (isControl(child.name, "comment")) {
                if (!modelsSeen || commentSeen) {
                    strictOrReport(DiagnosticCode::InvalidEventOrder,
                                   "ili:comment occurs out of order",
                                   child.location);
                }
                if (!child.children.empty()) {
                    strictOrReport(DiagnosticCode::UnexpectedElement,
                                   "ili:comment must contain text only",
                                   child.location);
                }
                if (!child.attributes.empty()) {
                    Node extension;
                    extension.name = child.name;
                    extension.attributes = child.attributes;
                    extension.location = child.location;
                    preserveExtension(
                        extension, header.extensions,
                        "Unknown ili:comment attribute was preserved");
                }
                header.comment = child.text;
                commentSeen = true;
            } else if (isControl(child.name, "headersection") ||
                       isControl(child.name, "datasection")) {
                fail(DiagnosticCode::InvalidEventOrder,
                     "Nested XTF 2.4 transfer section", child.location);
            } else {
                preserveExtension(child, header.extensions,
                                  "Unknown ili:headersection element was "
                                  "preserved");
            }
            ++ordinal;
        }
        if (!modelsSeen) {
            fail(DiagnosticCode::MissingRequiredHeader,
                 "XTF 2.4 header requires ili:models", headerNode.location);
        }
        if (options.requireAtLeastOneModel && header.models.empty()) {
            fail(DiagnosticCode::MissingModelEntry,
                 "XTF 2.4 header requires at least one ili:model",
                 headerNode.location);
        }
        if (!headerNode.attributes.empty()) {
            Node extension;
            extension.name = headerNode.name;
            extension.attributes = headerNode.attributes;
            extension.location = headerNode.location;
            preserveExtension(extension, header.extensions,
                              "Unknown ili:headersection attribute was "
                              "preserved");
        }
        assignModelNamespaces(header);
        emitEvent(StartTransferEvent{std::move(header)});
    }

    Consistency parseConsistency(std::optional<std::string> value,
                                 const SourceLocation& location) {
        if (!value) return Consistency::Complete;
        const auto normalized = upperAscii(*value);
        if (*value != normalized) {
            if (options.strictness == Strictness::Strict) {
                fail(DiagnosticCode::UnexpectedAttribute,
                     "XTF 2.4 consistency values are case-sensitive",
                     location);
            }
            report(DiagnosticSeverity::Warning,
                   DiagnosticCode::UnexpectedAttribute,
                   "Accepted non-canonical consistency casing", location);
        }
        if (normalized == "COMPLETE") return Consistency::Complete;
        if (normalized == "INCOMPLETE") return Consistency::Incomplete;
        strictOrReport(DiagnosticCode::UnexpectedAttribute,
                       "Unknown XTF 2.4 consistency value: " + *value,
                       location);
        return Consistency::Unspecified;
    }

    BasketKind parseBasketKind(std::optional<std::string> value,
                               const SourceLocation& location) {
        if (!value) return BasketKind::Full;
        const auto normalized = upperAscii(*value);
        if (*value != normalized) {
            if (options.strictness == Strictness::Strict) {
                fail(DiagnosticCode::UnexpectedAttribute,
                     "XTF 2.4 kind values are case-sensitive", location);
            }
            report(DiagnosticSeverity::Warning,
                   DiagnosticCode::UnexpectedAttribute,
                   "Accepted non-canonical kind casing", location);
        }
        if (normalized == "FULL") return BasketKind::Full;
        if (normalized == "UPDATE") return BasketKind::Update;
        if (normalized == "INITIAL") return BasketKind::Initial;
        strictOrReport(DiagnosticCode::UnexpectedAttribute,
                       "Unknown XTF 2.4 basket kind: " + *value, location);
        return BasketKind::Unspecified;
    }

    ObjectOperation parseOperation(std::optional<std::string> value,
                                   const SourceLocation& location) {
        if (!value) return ObjectOperation::Insert;
        const auto normalized = upperAscii(*value);
        if (*value != normalized) {
            if (options.strictness == Strictness::Strict) {
                fail(DiagnosticCode::UnexpectedAttribute,
                     "XTF 2.4 operation values are case-sensitive",
                     location);
            }
            report(DiagnosticSeverity::Warning,
                   DiagnosticCode::UnexpectedAttribute,
                   "Accepted non-canonical operation casing", location);
        }
        if (normalized == "INSERT") return ObjectOperation::Insert;
        if (normalized == "UPDATE") return ObjectOperation::Update;
        if (normalized == "DELETE") return ObjectOperation::Delete;
        strictOrReport(DiagnosticCode::UnexpectedAttribute,
                       "Unknown XTF 2.4 object operation: " + *value,
                       location);
        return ObjectOperation::None;
    }

    void beginBasket(const xml::XmlStartElement& element) {
        if (element.name.namespaceUri.empty() ||
            element.name.namespaceUri == iliNamespace ||
            element.name.namespaceUri == geometryNamespace) {
            fail(DiagnosticCode::InvalidXtfNamespace,
                 "XTF 2.4 basket requires a model namespace",
                 element.location);
        }
        const auto bid = optionalIliAttribute(element.attributes, "bid");
        if (!bid || bid->empty()) {
            fail(DiagnosticCode::MissingBasketId,
                 "XTF 2.4 basket requires a non-empty ili:bid",
                 element.location);
        }
        StartBasketEvent event;
        event.basket.topic = basketName(element.name);
        event.basket.basketId = *bid;
        event.basket.kind = parseBasketKind(
            optionalIliAttribute(element.attributes, "kind"),
            element.location);
        event.basket.startState =
            optionalIliAttribute(element.attributes, "startstate");
        event.basket.endState =
            optionalIliAttribute(element.attributes, "endstate");
        if ((event.basket.kind == BasketKind::Update ||
             event.basket.kind == BasketKind::Initial) &&
            (!event.basket.startState || !event.basket.endState)) {
            strictOrReport(
                DiagnosticCode::MissingRequiredHeader,
                "Incremental XTF 2.4 basket requires ili:startstate and "
                "ili:endstate",
                element.location);
        }
        event.basket.consistency = parseConsistency(
            optionalIliAttribute(element.attributes, "consistency"),
            element.location);
        if (const auto domains =
                optionalIliAttribute(element.attributes, "domains")) {
            event.basket.domains = splitWhitespace(*domains);
        }
        event.basket.location = element.location;
        preserveBasketAttributes(element, event.basket);
        currentBasketName = element.name;
        currentBasketTopic = event.basket.topic;
        emitEvent(std::move(event));
        state = DialectState::InBasket;
    }

    void beginObject(const xml::XmlStartElement& element) {
        const bool deletion = isControl(element.name, "delete");
        if (!deletion &&
            (element.name.namespaceUri.empty() ||
             element.name.namespaceUri == iliNamespace ||
             element.name.namespaceUri == geometryNamespace)) {
            fail(DiagnosticCode::InvalidXtfNamespace,
                 "XTF 2.4 object requires a model namespace",
                 element.location);
        }
        const auto tid = optionalIliAttribute(element.attributes, "tid");
        if (tid && tid->empty()) {
            strictOrReport(DiagnosticCode::MissingObjectId,
                           "XTF 2.4 ili:tid must not be empty",
                           element.location);
        }
        IomName tag = deletion ? IomName("DELETE", element.name)
                               : objectName(element.name);
        currentObject.emplace(
            std::move(tag),
            tid && !tid->empty() ? std::optional<std::string>(*tid)
                                 : std::optional<std::string>{});
        currentObject->setSourceLocation(element.location);
        currentObject->setOperation(
            deletion ? ObjectOperation::Delete
                     : parseOperation(optionalIliAttribute(
                                          element.attributes, "operation"),
                                      element.location));
        if (const auto bid = optionalIliAttribute(element.attributes, "bid")) {
            if (bid->empty()) {
                strictOrReport(DiagnosticCode::InvalidReference,
                               "Object-level ili:bid must not be empty",
                               element.location);
            } else {
                strictOrReport(
                    DiagnosticCode::UnexpectedAttribute,
                    "Object-level ili:bid is a non-normative extension",
                    element.location);
                auto reference = currentObject->reference();
                reference.targetBasketId = *bid;
                currentObject->setReference(std::move(reference));
            }
        }
        if (deletion && optionalIliAttribute(element.attributes, "operation")) {
            strictOrReport(DiagnosticCode::UnexpectedAttribute,
                           "ili:delete must not have ili:operation",
                           element.location);
        }
        if (const auto consistency =
                optionalIliAttribute(element.attributes, "consistency")) {
            strictOrReport(
                DiagnosticCode::UnexpectedAttribute,
                "ili:consistency is not defined on XTF 2.4 objects",
                element.location);
            currentObject->setConsistency(
                parseConsistency(consistency, element.location));
        }
        reportUnrepresentableAttributes(
            element.attributes, {"tid", "bid", "operation", "consistency"},
            deletion ? "delete" : "object", element.location);
        currentObjectName = element.name;
        state = DialectState::InObject;
    }

    ReferenceInfo parseReference(const Node& node) {
        ReferenceInfo result;
        const auto ref = optionalIliAttribute(node, "ref");
        if (!ref || ref->empty()) {
            strictOrReport(DiagnosticCode::InvalidReference,
                           "XTF 2.4 reference requires non-empty ili:ref",
                           node.location);
        } else {
            result.targetOid = *ref;
        }
        if (const auto bid = optionalIliAttribute(node, "bid")) {
            if (bid->empty()) {
                strictOrReport(DiagnosticCode::InvalidReference,
                               "Reference ili:bid must not be empty",
                               node.location);
            } else {
                result.targetBasketId = *bid;
            }
        }
        if (const auto order = optionalIliAttribute(node, "order_pos")) {
            try {
                if (order->empty() ||
                    !std::all_of(order->begin(), order->end(),
                                 [](unsigned char value) {
                                     return std::isdigit(value) != 0;
                                 })) {
                    throw std::invalid_argument("not unsigned digits");
                }
                std::size_t consumed = 0;
                const auto parsed = std::stoull(*order, &consumed);
                if (consumed != order->size() || parsed == 0U) {
                    throw std::invalid_argument("out of range");
                }
                result.orderPosition = parsed;
            } catch (const std::exception&) {
                strictOrReport(
                    DiagnosticCode::InvalidReference,
                    "ili:order_pos must be an integer greater than zero",
                    node.location);
            }
        }
        return result;
    }

    bool hasReference(const Node& node) const {
        return findIliAttribute(node, "ref") != nullptr ||
               findIliAttribute(node, "bid") != nullptr ||
               findIliAttribute(node, "order_pos") != nullptr;
    }

    void reportNodeAttributes(
        const Node& node, const std::unordered_set<std::string>& allowed,
        std::string_view owner) {
        reportUnrepresentableAttributes(node.attributes, allowed, owner,
                                        node.location);
    }

    IomObject nodeToObject(const Node& node);

    void appendGenericAttribute(IomObject& owner, const Node& attribute) {
        const auto name = memberName(attribute.name);
        if (hasReference(attribute)) {
            reportNodeAttributes(attribute, {"ref", "bid", "order_pos"},
                                 "reference");
            if (!isWhitespace(attribute.text)) {
                strictOrReport(DiagnosticCode::UnexpectedElement,
                               "Reference attribute contains text",
                               attribute.location);
            }
            IomObject reference = attribute.children.empty()
                                      ? IomObject(IomName("REF"))
                                      : nodeToObject(attribute.children.front());
            reference.setSourceLocation(attribute.location);
            reference.setReference(parseReference(attribute));
            owner.appendObject(name, std::move(reference));
            for (std::size_t index = 1; index < attribute.children.size();
                 ++index) {
                strictOrReport(
                    DiagnosticCode::InvalidReference,
                    "Reference attribute contains multiple embedded objects",
                    attribute.children[index].location);
                owner.appendObject(name,
                                   nodeToObject(attribute.children[index]));
            }
            return;
        }

        reportNodeAttributes(attribute, {}, "attribute");
        if (attribute.children.empty()) {
            owner.appendPrimitive(name, attribute.text);
            return;
        }
        if (!isWhitespace(attribute.text)) {
            owner.appendPrimitive(name, attribute.text);
            report(DiagnosticSeverity::Warning,
                   DiagnosticCode::UnknownExtensionPreserved,
                   "Mixed text in a structured value was preserved",
                   attribute.location);
        }
        for (const auto& child : attribute.children) {
            owner.appendObject(name, nodeToObject(child));
        }
    }

    IomObject parseCoordinate(const Node& node, bool arc) {
        IomObject result(IomName(arc ? "ARC" : "COORD", node.name));
        result.setSourceLocation(node.location);
        const std::unordered_set<std::string> allowed =
            arc ? std::unordered_set<std::string>{"c1", "c2", "c3", "a1",
                                                        "a2", "r"}
                : std::unordered_set<std::string>{"c1", "c2", "c3"};
        std::unordered_set<std::string> seen;
        for (const auto& child : node.children) {
            if (child.name.namespaceUri != geometryNamespace ||
                allowed.count(child.name.localName) == 0U) {
                strictOrReport(DiagnosticCode::InvalidGeometry,
                               "Unknown coordinate member was preserved: " +
                                   child.name.expanded(),
                               child.location);
                appendGenericAttribute(result, child);
                continue;
            }
            if (!seen.insert(child.name.localName).second ||
                !child.children.empty() || !child.attributes.empty()) {
                strictOrReport(DiagnosticCode::InvalidGeometry,
                               "Coordinate member must occur once as text",
                               child.location);
            }
            result.appendPrimitive(
                IomName(upperAscii(child.name.localName), child.name),
                child.text);
        }
        const auto require = [&](std::string_view name) {
            if (seen.count(std::string(name)) == 0U) {
                strictOrReport(DiagnosticCode::InvalidGeometry,
                               std::string(arc ? "ARC" : "COORD") +
                                   " requires geom:" + std::string(name),
                               node.location);
            }
        };
        require("c1");
        if (arc) {
            require("c2");
            require("a1");
            require("a2");
        }
        if (seen.count("c3") != 0U && seen.count("c2") == 0U) {
            strictOrReport(DiagnosticCode::InvalidGeometry,
                           "geom:c3 requires geom:c2", node.location);
        }
        reportNodeAttributes(node, {}, arc ? "arc" : "coord");
        return result;
    }

    IomObject parsePolyline(const Node& node) {
        IomObject result(IomName("POLYLINE", node.name));
        result.setSourceLocation(node.location);
        IomObject sequence(IomName("SEGMENTS"));
        for (const auto& child : node.children) {
            if (isGeometry(child.name, "coord")) {
                sequence.appendObject(IomName("segment"),
                                      parseCoordinate(child, false));
            } else if (isGeometry(child.name, "arc")) {
                sequence.appendObject(IomName("segment"),
                                      parseCoordinate(child, true));
            } else if (child.name.namespaceUri == iliNamespace) {
                strictOrReport(DiagnosticCode::InvalidGeometry,
                               "Unexpected INTERLIS control element in "
                               "geom:polyline",
                               child.location);
            } else {
                sequence.appendObject(IomName("segment"),
                                      nodeToObject(child));
            }
        }
        if (sequence.valueCount("segment") == 0U) {
            strictOrReport(DiagnosticCode::InvalidGeometry,
                           "geom:polyline requires at least one segment",
                           node.location);
        } else {
            result.appendObject(IomName("sequence"), std::move(sequence));
        }
        reportNodeAttributes(node, {}, "polyline");
        return result;
    }

    IomObject parseBoundary(const Node& node) {
        IomObject boundary(IomName("BOUNDARY", node.name));
        boundary.setSourceLocation(node.location);
        for (const auto& child : node.children) {
            if (isGeometry(child.name, "polyline")) {
                boundary.appendObject(IomName("polyline"),
                                      parsePolyline(child));
            } else {
                strictOrReport(DiagnosticCode::InvalidGeometry,
                               "Boundary member was preserved: " +
                                   child.name.expanded(),
                               child.location);
                boundary.appendObject(IomName("polyline"),
                                      nodeToObject(child));
            }
        }
        if (boundary.valueCount("polyline") == 0U) {
            strictOrReport(DiagnosticCode::InvalidGeometry,
                           "Surface boundary requires a geom:polyline",
                           node.location);
        }
        reportNodeAttributes(node, {}, "surface boundary");
        return boundary;
    }

    IomObject parseSurface(const Node& node) {
        IomObject result(IomName(isGeometry(node.name, "area") ? "AREA"
                                                               : "SURFACE",
                                 node.name));
        result.setSourceLocation(node.location);
        bool exteriorSeen = false;
        bool interiorSeen = false;
        for (const auto& child : node.children) {
            if (isGeometry(child.name, "exterior")) {
                if (exteriorSeen || interiorSeen) {
                    strictOrReport(
                        DiagnosticCode::InvalidGeometry,
                        "geom:exterior must occur once before geom:interior",
                        child.location);
                }
                exteriorSeen = true;
                result.appendObject(IomName("exterior", child.name),
                                    parseBoundary(child));
            } else if (isGeometry(child.name, "interior")) {
                if (!exteriorSeen) {
                    strictOrReport(DiagnosticCode::InvalidGeometry,
                                   "geom:interior requires a preceding "
                                   "geom:exterior",
                                   child.location);
                }
                interiorSeen = true;
                result.appendObject(IomName("interior", child.name),
                                    parseBoundary(child));
            } else {
                strictOrReport(DiagnosticCode::InvalidGeometry,
                               "Unknown surface member was preserved: " +
                                   child.name.expanded(),
                               child.location);
                appendGenericAttribute(result, child);
            }
        }
        if (!exteriorSeen) {
            strictOrReport(DiagnosticCode::InvalidGeometry,
                           "geom:surface requires geom:exterior",
                           node.location);
        }
        reportNodeAttributes(node, {}, "surface");
        return result;
    }

    IomObject parseMulti(const Node& node, std::string tag,
                         std::string member,
                         std::string_view childName) {
        IomObject result(IomName(std::move(tag), node.name));
        result.setSourceLocation(node.location);
        for (const auto& child : node.children) {
            if (!isGeometry(child.name, childName)) {
                strictOrReport(DiagnosticCode::InvalidGeometry,
                               "Unknown multi-geometry member was preserved: " +
                                   child.name.expanded(),
                               child.location);
            }
            result.appendObject(IomName(member), nodeToObject(child));
        }
        if (result.valueCount(member) == 0U) {
            strictOrReport(DiagnosticCode::InvalidGeometry,
                           "Multi-geometry requires at least one member",
                           node.location);
        }
        reportNodeAttributes(node, {}, "multi-geometry");
        return result;
    }

    IomObject genericObject(const Node& node) {
        IomObject result(IomName(node.name.localName, node.name));
        result.setSourceLocation(node.location);
        reportNodeAttributes(node, {}, "structured value");
        if (!isWhitespace(node.text)) {
            result.appendPrimitive(IomName("text"), node.text);
            report(DiagnosticSeverity::Warning,
                   DiagnosticCode::UnknownExtensionPreserved,
                   "Mixed structure text was preserved", node.location);
        }
        for (const auto& child : node.children) {
            if (node.name.namespaceUri == geometryNamespace) {
                result.appendObject(memberName(child.name),
                                    nodeToObject(child));
            } else {
                appendGenericAttribute(result, child);
            }
        }
        return result;
    }

    void finishAttribute(Node node) {
        if (!currentObject) {
            fail(DiagnosticCode::InvalidEventOrder,
                 "Attribute ended without an active XTF 2.4 object",
                 node.location);
        }
        appendGenericAttribute(*currentObject, node);
    }

    void finishCapture(const xml::XmlEndElement& element) {
        if (nodes.empty()) {
            fail(DiagnosticCode::InternalError,
                 "XTF 2.4 capture stack underflow", element.location);
        }
        if (nodes.back().name != element.name) {
            fail(DiagnosticCode::InvalidEventOrder,
                 "Mismatched XTF 2.4 element while capturing data",
                 element.location);
        }
        Node completed = std::move(nodes.back());
        nodes.pop_back();
        if (!nodes.empty()) {
            nodes.back().children.push_back(std::move(completed));
            return;
        }
        const auto completedKind = captureKind;
        captureKind = CaptureKind::None;
        if (completedKind == CaptureKind::Header) {
            parseHeader(std::move(completed));
            state = DialectState::ExpectData;
        } else if (completedKind == CaptureKind::Attribute) {
            finishAttribute(std::move(completed));
        }
    }

    void start(const xml::XmlStartElement& element) {
        if (captureKind != CaptureKind::None) {
            captureStart(element);
            return;
        }
        switch (state) {
        case DialectState::ExpectHeader:
            if (!isControl(element.name, "headersection")) {
                fail(DiagnosticCode::InvalidEventOrder,
                     "XTF 2.4 requires ili:headersection immediately "
                     "after ili:transfer",
                     element.location);
            }
            state = DialectState::CapturingHeader;
            beginCapture(CaptureKind::Header, element);
            return;
        case DialectState::ExpectData:
            if (!isControl(element.name, "datasection")) {
                fail(DiagnosticCode::InvalidEventOrder,
                     "XTF 2.4 requires ili:datasection after the header",
                     element.location);
            }
            state = DialectState::InData;
            return;
        case DialectState::InData:
            beginBasket(element);
            return;
        case DialectState::InBasket:
            beginObject(element);
            return;
        case DialectState::InObject:
            beginCapture(CaptureKind::Attribute, element);
            return;
        default:
            fail(DiagnosticCode::InvalidEventOrder,
                 "Unexpected XTF 2.4 element in the current reader state",
                 element.location);
        }
    }

    void end(const xml::XmlEndElement& element) {
        if (captureKind != CaptureKind::None) {
            finishCapture(element);
            return;
        }
        switch (state) {
        case DialectState::InObject:
            if (!currentObject || element.name != currentObjectName) {
                fail(DiagnosticCode::InvalidEventOrder,
                     "Unexpected XTF 2.4 object end element",
                     element.location);
            }
            emitEvent(ObjectEvent{std::move(*currentObject)});
            currentObject.reset();
            currentObjectName = {};
            state = DialectState::InBasket;
            return;
        case DialectState::InBasket:
            if (element.name != currentBasketName) {
                fail(DiagnosticCode::InvalidEventOrder,
                     "Unexpected XTF 2.4 basket end element",
                     element.location);
            }
            emitEvent(EndBasketEvent{});
            currentBasketName = {};
            currentBasketTopic = {};
            state = DialectState::InData;
            return;
        case DialectState::InData:
            if (!isControl(element.name, "datasection")) {
                fail(DiagnosticCode::InvalidEventOrder,
                     "Unexpected element ending in ili:datasection",
                     element.location);
            }
            state = DialectState::AfterData;
            return;
        default:
            fail(DiagnosticCode::InvalidEventOrder,
                 "Unexpected XTF 2.4 end element", element.location);
        }
    }

    void text(std::string_view data, const SourceLocation& location) {
        if (captureKind != CaptureKind::None) {
            if (nodes.empty()) {
                fail(DiagnosticCode::InternalError,
                     "XTF 2.4 text capture has no active element",
                     location);
            }
            nodes.back().text.append(data.data(), data.size());
            return;
        }
        if (!isWhitespace(data)) {
            fail(DiagnosticCode::UnexpectedElement,
                 "Non-whitespace text occurred between XTF 2.4 control "
                 "elements",
                 location);
        }
    }
};

IomObject Xtf24Dialect::Impl::nodeToObject(const Node& node) {
    if (isGeometry(node.name, "coord")) return parseCoordinate(node, false);
    if (isGeometry(node.name, "arc")) return parseCoordinate(node, true);
    if (isGeometry(node.name, "polyline")) return parsePolyline(node);
    if (isGeometry(node.name, "multicoord")) {
        return parseMulti(node, "MULTICOORD", "coord", "coord");
    }
    if (isGeometry(node.name, "multipolyline")) {
        return parseMulti(node, "MULTIPOLYLINE", "polyline", "polyline");
    }
    if (isGeometry(node.name, "surface") ||
        isGeometry(node.name, "area")) {
        return parseSurface(node);
    }
    if (isGeometry(node.name, "multisurface")) {
        return parseMulti(node, "MULTISURFACE", "surface", "surface");
    }
    if (isGeometry(node.name, "multiarea")) {
        IomObject result(IomName("MULTIAREA", node.name));
        result.setSourceLocation(node.location);
        for (const auto& child : node.children) {
            if (!isGeometry(child.name, "surface") &&
                !isGeometry(child.name, "area")) {
                strictOrReport(
                    DiagnosticCode::InvalidGeometry,
                    "Unknown multi-area member was preserved: " +
                        child.name.expanded(),
                    child.location);
            }
            result.appendObject(IomName("area"), nodeToObject(child));
        }
        if (result.valueCount("area") == 0U) {
            strictOrReport(DiagnosticCode::InvalidGeometry,
                           "geom:multiarea requires at least one member",
                           node.location);
        }
        reportNodeAttributes(node, {}, "multi-area");
        return result;
    }
    if (node.name.namespaceUri == geometryNamespace) {
        report(DiagnosticSeverity::Warning,
               DiagnosticCode::UnknownExtensionPreserved,
               "Unknown XTF 2.4 geometry was preserved: " +
                   node.name.localName,
               node.location);
    }
    return genericObject(node);
}

Xtf24Dialect::Xtf24Dialect(
    XtfReaderOptions options,
    std::vector<xml::XmlNamespaceDeclaration> rootNamespaces,
    EventHandler emitEvent,
    DiagnosticHandler addDiagnostic)
    : impl_(std::make_unique<Impl>(std::move(options),
                                   std::move(rootNamespaces),
                                   std::move(emitEvent),
                                   std::move(addDiagnostic))) {}

Xtf24Dialect::~Xtf24Dialect() = default;

void Xtf24Dialect::onStartElement(const xml::XmlStartElement& element) {
    impl_->start(element);
}

void Xtf24Dialect::onEndElement(const xml::XmlEndElement& element) {
    impl_->end(element);
}

void Xtf24Dialect::onText(std::string_view data,
                          const SourceLocation& location) {
    impl_->text(data, location);
}

void Xtf24Dialect::onRootClosed(const SourceLocation& location) {
    if (impl_->state != DialectState::AfterData) {
        impl_->fail(DiagnosticCode::InvalidEventOrder,
                    "ili:transfer ended before a complete ili:datasection",
                    location);
    }
    impl_->emitEvent(EndTransferEvent{});
    impl_->state = DialectState::Done;
}

bool Xtf24Dialect::finished() const noexcept {
    return impl_->state == DialectState::Done;
}

} // namespace xtf
} // namespace iox
