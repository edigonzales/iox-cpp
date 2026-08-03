#include "xtf/v23/Xtf23Dialect.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace iox {
namespace xtf {
namespace {

constexpr std::string_view xtf23Namespace =
    "http://www.interlis.ch/INTERLIS2.3";

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

std::string upperAscii(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const auto character : value) {
        result.push_back(static_cast<char>(
            std::toupper(static_cast<unsigned char>(character))));
    }
    return result;
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

bool isControl(const XmlQualifiedName& name, std::string_view localName) {
    return name.namespaceUri == xtf23Namespace && name.localName == localName;
}

const xml::XmlAttribute* findAttribute(const Node& node,
                                       std::string_view localName) {
    for (const auto& attribute : node.attributes) {
        if (attribute.name.namespaceUri.empty() &&
            attribute.name.localName == localName) {
            return &attribute;
        }
    }
    return nullptr;
}

const xml::XmlAttribute* findAttribute(
    const xml::XmlStartElement& element, std::string_view localName) {
    for (const auto& attribute : element.attributes) {
        if (attribute.name.namespaceUri.empty() &&
            attribute.name.localName == localName) {
            return &attribute;
        }
    }
    return nullptr;
}

IomName iomName(const XmlQualifiedName& name) {
    return IomName(name.localName, name);
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

std::vector<std::string> splitList(std::string_view value) {
    std::vector<std::string> result;
    std::size_t start = 0;
    while (start <= value.size()) {
        const auto separator = value.find(',', start);
        const auto item = trimAscii(value.substr(
            start, separator == std::string_view::npos
                       ? std::string_view::npos
                       : separator - start));
        if (!item.empty()) result.push_back(item);
        if (separator == std::string_view::npos) break;
        start = separator + 1U;
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

struct Xtf23Dialect::Impl final {
    XtfReaderOptions options;
    EventHandler emitEvent;
    DiagnosticHandler addDiagnostic;
    DialectState state = DialectState::ExpectHeader;
    CaptureKind captureKind = CaptureKind::None;
    std::vector<Node> nodes;
    std::optional<IomObject> currentObject;
    XmlQualifiedName currentObjectName;
    XmlQualifiedName currentBasketName;

    Impl(XtfReaderOptions value, EventHandler eventHandler,
         DiagnosticHandler diagnosticHandler)
        : options(std::move(value)), emitEvent(std::move(eventHandler)),
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

    static std::unordered_set<std::string> attributeNames(
        std::initializer_list<const char*> values) {
        std::unordered_set<std::string> result;
        for (const auto* value : values) result.emplace(value);
        return result;
    }

    std::vector<xml::XmlAttribute> unknownAttributes(
        const Node& node,
        const std::unordered_set<std::string>& known) const {
        std::vector<xml::XmlAttribute> result;
        for (const auto& attribute : node.attributes) {
            if (!attribute.name.namespaceUri.empty() ||
                known.count(attribute.name.localName) == 0U) {
                result.push_back(attribute);
            }
        }
        return result;
    }

    void handleUnknownAttributes(
        const Node& node, const std::unordered_set<std::string>& known,
        std::vector<ExtensionElement>& target, std::string owner) {
        const auto unknown = unknownAttributes(node, known);
        if (unknown.empty()) return;
        Node extension;
        extension.name = node.name;
        extension.attributes = unknown;
        extension.location = node.location;
        preserveExtension(extension, target,
                          "Unknown XTF 2.3 " + owner +
                              " attribute was preserved");
    }

    std::optional<std::string> optionalAttribute(
        const Node& node, std::string_view name) const {
        const auto* attribute = findAttribute(node, name);
        if (attribute == nullptr) return std::nullopt;
        return attribute->value;
    }

    void parseModel(const Node& node, TransferHeader& header) {
        if (!isControl(node.name, "MODEL")) {
            preserveExtension(node, header.extensions,
                              "Unknown element inside MODELS was preserved");
            return;
        }
        const auto name = optionalAttribute(node, "NAME");
        if (!name || name->empty()) {
            strictOrReport(DiagnosticCode::MissingModelEntry,
                           "XTF 2.3 MODEL requires NAME", node.location);
            preserveExtension(node, header.extensions,
                              "MODEL without NAME was preserved as an extension");
            return;
        }
        ModelEntry model;
        model.name = *name;
        model.version = optionalAttribute(node, "VERSION");
        model.uri = optionalAttribute(node, "URI");
        if (options.strictness == Strictness::Strict &&
            (!model.version || model.version->empty() ||
             !model.uri || model.uri->empty())) {
            fail(DiagnosticCode::MissingModelEntry,
                 "Strict XTF 2.3 MODEL requires VERSION and URI",
                 node.location);
        }
        handleUnknownAttributes(
            node, attributeNames({"NAME", "VERSION", "URI"}),
            header.extensions, "MODEL");
        if (!node.children.empty() || !isWhitespace(node.text)) {
            strictOrReport(DiagnosticCode::UnexpectedElement,
                           "MODEL must not contain nested data", node.location);
        }
        header.models.push_back(std::move(model));
    }

    void validateAlias(const Node& alias) {
        for (const auto& entries : alias.children) {
            if (!isControl(entries.name, "ENTRIES")) {
                strictOrReport(DiagnosticCode::UnexpectedElement,
                               "ALIAS only permits ENTRIES children",
                               entries.location);
                continue;
            }
            const auto entryFor = optionalAttribute(entries, "FOR");
            if (!entryFor || entryFor->empty()) {
                strictOrReport(DiagnosticCode::MissingRequiredHeader,
                               "ALIAS ENTRIES requires FOR",
                               entries.location);
            }
            for (const auto& entry : entries.children) {
                const auto local = entry.name.localName;
                std::vector<std::string_view> required;
                if (isControl(entry.name, "TAGENTRY")) {
                    required = {"FROM", "TO"};
                } else if (isControl(entry.name, "VALENTRY")) {
                    required = {"TAG", "ATTR", "FROM", "TO"};
                } else if (isControl(entry.name, "DELENTRY")) {
                    required = {"TAG"};
                } else {
                    strictOrReport(DiagnosticCode::UnexpectedElement,
                                   "Unknown ALIAS entry: " + local,
                                   entry.location);
                    continue;
                }
                for (const auto requiredName : required) {
                    const auto value = optionalAttribute(entry, requiredName);
                    if (!value || value->empty()) {
                        strictOrReport(
                            DiagnosticCode::MissingRequiredHeader,
                            local + " requires " + std::string(requiredName),
                            entry.location);
                    }
                }
            }
        }
    }

    void parseModels(const Node& node, TransferHeader& header,
                     bool duplicate) {
        if (duplicate) {
            strictOrReport(DiagnosticCode::UnexpectedElement,
                           "HEADERSECTION contains multiple MODELS elements",
                           node.location);
        }
        for (const auto& child : node.children) parseModel(child, header);
        if (!isWhitespace(node.text)) {
            strictOrReport(DiagnosticCode::UnexpectedElement,
                           "MODELS contains text content", node.location);
        }
        handleUnknownAttributes(node, attributeNames({}), header.extensions,
                                "MODELS");
    }

    void parseOidSpaces(const Node& node, TransferHeader& header) {
        for (const auto& child : node.children) {
            if (!isControl(child.name, "OIDSPACE")) {
                preserveExtension(child, header.extensions,
                                  "Unknown element inside OIDSPACES was preserved");
                continue;
            }
            const auto name = optionalAttribute(child, "NAME");
            const auto domain = optionalAttribute(child, "OIDDOMAIN");
            if (!name || name->empty() || !domain || domain->empty()) {
                strictOrReport(DiagnosticCode::MissingRequiredHeader,
                               "OIDSPACE requires NAME and OIDDOMAIN",
                               child.location);
                preserveExtension(child, header.extensions,
                                  "Incomplete OIDSPACE was preserved as an extension");
                continue;
            }
            header.oidSpaces.push_back({*name, *domain});
            handleUnknownAttributes(
                child, attributeNames({"NAME", "OIDDOMAIN"}),
                header.extensions, "OIDSPACE");
        }
        if (node.children.empty()) {
            strictOrReport(DiagnosticCode::MissingRequiredHeader,
                           "OIDSPACES requires at least one OIDSPACE",
                           node.location);
        }
    }

    void parseHeader(Node headerNode) {
        TransferHeader header;
        header.version = XtfVersion::V23;
        const auto senderAttribute = optionalAttribute(headerNode, "SENDER");
        const auto versionAttribute = optionalAttribute(headerNode, "VERSION");
        if (senderAttribute) header.sender = *senderAttribute;

        std::optional<std::string> elementVersion;
        bool modelsSeen = false;
        for (const auto& child : headerNode.children) {
            if (isControl(child.name, "MODELS")) {
                parseModels(child, header, modelsSeen);
                modelsSeen = true;
            } else if (isControl(child.name, "ALIAS")) {
                // The public schema intentionally reuses the lossless extension
                // tree instead of adding an alias-specific class hierarchy.
                validateAlias(child);
                header.extensions.push_back(extensionFrom(child));
            } else if (isControl(child.name, "OIDSPACES")) {
                parseOidSpaces(child, header);
            } else if (isControl(child.name, "COMMENT")) {
                if (!child.children.empty()) {
                    strictOrReport(DiagnosticCode::UnexpectedElement,
                                   "COMMENT must contain text only",
                                   child.location);
                }
                header.comment = child.text;
            } else if (isControl(child.name, "SENDER")) {
                // Accepted for compatibility with the pre-0.2 writer.
                strictOrReport(
                    DiagnosticCode::UnexpectedElement,
                    "SENDER must be a HEADERSECTION attribute in XTF 2.3",
                    child.location);
                header.sender = child.text;
            } else if (isControl(child.name, "VERSION")) {
                strictOrReport(
                    DiagnosticCode::UnexpectedElement,
                    "VERSION must be a HEADERSECTION attribute in XTF 2.3",
                    child.location);
                elementVersion = trimAscii(child.text);
            } else if (isControl(child.name, "HEADERSECTION") ||
                       isControl(child.name, "DATASECTION")) {
                fail(DiagnosticCode::InvalidEventOrder,
                     "Nested transfer control section inside HEADERSECTION",
                     child.location);
            } else {
                preserveExtension(child, header.extensions,
                                  "Unknown HEADERSECTION element was preserved");
            }
        }

        const auto version = versionAttribute
                                 ? std::optional<std::string>(*versionAttribute)
                                 : elementVersion;
        if (!version || version->empty()) {
            strictOrReport(DiagnosticCode::MissingRequiredHeader,
                           "XTF 2.3 HEADERSECTION requires VERSION",
                           headerNode.location);
        } else if (*version != "2.3") {
            fail(DiagnosticCode::UnsupportedXtfVersion,
                 "Unsupported HEADERSECTION VERSION: " + *version,
                 headerNode.location);
        }
        if (header.sender.empty()) {
            strictOrReport(DiagnosticCode::MissingRequiredHeader,
                           "XTF 2.3 HEADERSECTION requires SENDER",
                           headerNode.location);
        }
        if (options.requireAtLeastOneModel && header.models.empty()) {
            fail(DiagnosticCode::MissingModelEntry,
                 "XTF 2.3 HEADERSECTION requires at least one MODEL",
                 headerNode.location);
        }
        handleUnknownAttributes(
            headerNode, attributeNames({"SENDER", "VERSION"}),
            header.extensions, "HEADERSECTION");
        emitEvent(StartTransferEvent{std::move(header)});
    }

    Consistency parseConsistency(std::optional<std::string> value,
                                 const SourceLocation& location) {
        if (!value) return Consistency::Complete;
        const auto normalized = upperAscii(*value);
        if (options.strictness == Strictness::Strict && *value != normalized) {
            fail(DiagnosticCode::UnexpectedAttribute,
                 "CONSISTENCY values are case-sensitive", location);
        }
        if (*value != normalized) {
            report(DiagnosticSeverity::Warning,
                   DiagnosticCode::UnexpectedAttribute,
                   "Accepted non-canonical CONSISTENCY casing", location);
        }
        if (normalized == "COMPLETE") return Consistency::Complete;
        if (normalized == "INCOMPLETE") return Consistency::Incomplete;
        if (normalized == "INCONSISTENT") return Consistency::Inconsistent;
        if (normalized == "ADAPTED") return Consistency::Adapted;
        strictOrReport(DiagnosticCode::UnexpectedAttribute,
                       "Unknown CONSISTENCY value: " + *value, location);
        return Consistency::Unspecified;
    }

    BasketKind parseBasketKind(std::optional<std::string> value,
                               const SourceLocation& location) {
        if (!value) return BasketKind::Full;
        const auto normalized = upperAscii(*value);
        if (options.strictness == Strictness::Strict && *value != normalized) {
            fail(DiagnosticCode::UnexpectedAttribute,
                 "KIND values are case-sensitive", location);
        }
        if (*value != normalized) {
            report(DiagnosticSeverity::Warning,
                   DiagnosticCode::UnexpectedAttribute,
                   "Accepted non-canonical KIND casing", location);
        }
        if (normalized == "FULL") return BasketKind::Full;
        if (normalized == "UPDATE") return BasketKind::Update;
        if (normalized == "INITIAL") return BasketKind::Initial;
        strictOrReport(DiagnosticCode::UnexpectedAttribute,
                       "Unknown basket KIND value: " + *value, location);
        return BasketKind::Unspecified;
    }

    ObjectOperation parseOperation(std::optional<std::string> value,
                                   const SourceLocation& location) {
        if (!value) return ObjectOperation::Insert;
        const auto normalized = upperAscii(*value);
        if (options.strictness == Strictness::Strict && *value != normalized) {
            fail(DiagnosticCode::UnexpectedAttribute,
                 "OPERATION values are case-sensitive", location);
        }
        if (*value != normalized) {
            report(DiagnosticSeverity::Warning,
                   DiagnosticCode::UnexpectedAttribute,
                   "Accepted non-canonical OPERATION casing", location);
        }
        if (normalized == "INSERT") return ObjectOperation::Insert;
        if (normalized == "UPDATE") return ObjectOperation::Update;
        if (normalized == "DELETE") return ObjectOperation::Delete;
        strictOrReport(DiagnosticCode::UnexpectedAttribute,
                       "Unknown object OPERATION value: " + *value, location);
        return ObjectOperation::None;
    }

    void beginBasket(const xml::XmlStartElement& element) {
        const auto* bid = findAttribute(element, "BID");
        if (bid == nullptr || bid->value.empty()) {
            fail(DiagnosticCode::MissingBasketId,
                 "XTF 2.3 basket requires a non-empty BID", element.location);
        }
        StartBasketEvent event;
        event.basket.topic = iomName(element.name);
        event.basket.basketId = bid->value;
        event.basket.kind = parseBasketKind(
            findAttribute(element, "KIND") == nullptr
                ? std::nullopt
                : std::optional<std::string>(
                      findAttribute(element, "KIND")->value),
            element.location);
        event.basket.consistency = parseConsistency(
            findAttribute(element, "CONSISTENCY") == nullptr
                ? std::nullopt
                : std::optional<std::string>(
                      findAttribute(element, "CONSISTENCY")->value),
            element.location);
        if (const auto* value = findAttribute(element, "STARTSTATE")) {
            event.basket.startState = value->value;
        }
        if (const auto* value = findAttribute(element, "ENDSTATE")) {
            event.basket.endState = value->value;
        }
        if (const auto* value = findAttribute(element, "DOMAINS")) {
            event.basket.domains = splitList(value->value);
        }
        if (const auto* value = findAttribute(element, "TOPICS")) {
            event.basket.topics = splitList(value->value);
        }
        event.basket.location = element.location;

        Node basketNode{element.name, element.attributes, {}, {},
                        element.location};
        handleUnknownAttributes(
            basketNode,
            attributeNames({"BID", "KIND", "CONSISTENCY", "STARTSTATE",
                            "ENDSTATE", "DOMAINS", "TOPICS"}),
            event.basket.extensions, "basket");
        currentBasketName = element.name;
        emitEvent(std::move(event));
        state = DialectState::InBasket;
    }

    void beginObject(const xml::XmlStartElement& element) {
        const bool deletion = isControl(element.name, "DELETE");
        const auto* tid = findAttribute(element, "TID");
        if (deletion && (tid == nullptr || tid->value.empty())) {
            fail(DiagnosticCode::MissingObjectId,
                 "XTF 2.3 DELETE requires a non-empty TID", element.location);
        }
        if (!deletion && (tid == nullptr || tid->value.empty())) {
            strictOrReport(DiagnosticCode::MissingObjectId,
                           "XTF 2.3 object has no TID", element.location);
        }
        IomObject object(iomName(element.name),
                         tid == nullptr || tid->value.empty()
                             ? std::nullopt
                             : std::optional<std::string>(tid->value));
        object.setSourceLocation(element.location);
        object.setOperation(deletion
                                ? ObjectOperation::Delete
                                : parseOperation(
                                      findAttribute(element, "OPERATION") == nullptr
                                          ? std::nullopt
                                          : std::optional<std::string>(
                                                findAttribute(element, "OPERATION")->value),
                                      element.location));
        object.setConsistency(parseConsistency(
            findAttribute(element, "CONSISTENCY") == nullptr
                ? std::nullopt
                : std::optional<std::string>(
                      findAttribute(element, "CONSISTENCY")->value),
            element.location));
        if (const auto* bid = findAttribute(element, "BID")) {
            ReferenceInfo reference;
            reference.targetBasketId = bid->value;
            object.setReference(std::move(reference));
        }
        Node objectNode{element.name, element.attributes, {}, {},
                        element.location};
        const auto unknown = unknownAttributes(
            objectNode, attributeNames(
                            {"TID", "BID", "OPERATION", "CONSISTENCY"}));
        if (!unknown.empty()) {
            strictOrReport(DiagnosticCode::UnexpectedAttribute,
                           "Unknown XTF 2.3 object control attribute",
                           element.location);
        }
        currentObject = std::move(object);
        currentObjectName = element.name;
        state = DialectState::InObject;
    }

    ReferenceInfo parseReference(const Node& node) {
        ReferenceInfo result;
        if (const auto value = optionalAttribute(node, "REF")) {
            if (value->empty()) {
                strictOrReport(DiagnosticCode::InvalidReference,
                               "REF must not be empty", node.location);
            } else {
                result.targetOid = *value;
            }
        }
        if (const auto value = optionalAttribute(node, "BID")) {
            if (value->empty()) {
                strictOrReport(DiagnosticCode::InvalidReference,
                               "Reference BID must not be empty", node.location);
            } else {
                result.targetBasketId = *value;
            }
        }
        if (const auto value = optionalAttribute(node, "ORDER_POS")) {
            try {
                if (value->empty() ||
                    !std::all_of(value->begin(), value->end(),
                                 [](unsigned char character) {
                                     return std::isdigit(character) != 0;
                                 })) {
                    throw std::invalid_argument("not unsigned digits");
                }
                std::size_t consumed = 0;
                const auto parsed = std::stoull(*value, &consumed);
                if (consumed != value->size() || parsed == 0U) {
                    throw std::invalid_argument("out of range");
                }
                result.orderPosition = parsed;
            } catch (const std::exception&) {
                strictOrReport(DiagnosticCode::InvalidReference,
                               "ORDER_POS must be an integer greater than zero",
                               node.location);
            }
        }
        return result;
    }

    bool hasReference(const Node& node) const {
        return findAttribute(node, "REF") != nullptr ||
               findAttribute(node, "BID") != nullptr ||
               findAttribute(node, "ORDER_POS") != nullptr;
    }

    IomObject nodeToObject(const Node& node);
    IomObject sequenceFrom(const Node& node);

    void appendGenericAttribute(IomObject& owner, const Node& attribute) {
        const auto name = iomName(attribute.name);
        if (const auto oid = optionalAttribute(attribute, "OID")) {
            if (oid->empty()) {
                strictOrReport(DiagnosticCode::UnexpectedAttribute,
                               "OID must not be empty", attribute.location);
            }
            if (!unknownAttributes(attribute, attributeNames({"OID"})).empty()) {
                strictOrReport(DiagnosticCode::UnexpectedAttribute,
                               "OID attribute contains unknown XML attributes",
                               attribute.location);
            }
            if (!attribute.children.empty() || !isWhitespace(attribute.text)) {
                strictOrReport(DiagnosticCode::UnexpectedElement,
                               "OID attribute must not contain element or text data",
                               attribute.location);
            }
            IomObject oidValue(IomName("OID"), *oid);
            oidValue.setSourceLocation(attribute.location);
            owner.appendObject(name, std::move(oidValue));
            return;
        }
        if (hasReference(attribute)) {
            if (!isWhitespace(attribute.text)) {
                strictOrReport(DiagnosticCode::UnexpectedElement,
                               "Reference attribute must not contain text data",
                               attribute.location);
            }
            IomObject reference = attribute.children.empty()
                                      ? IomObject(IomName("REF"))
                                      : nodeToObject(attribute.children.front());
            reference.setReference(parseReference(attribute));
            owner.appendObject(name, std::move(reference));
            if (attribute.children.size() > 1U) {
                strictOrReport(DiagnosticCode::InvalidReference,
                               "Reference attribute contains multiple objects",
                               attribute.location);
                for (std::size_t index = 1; index < attribute.children.size();
                     ++index) {
                    owner.appendObject(name,
                                       nodeToObject(attribute.children[index]));
                }
            }
            return;
        }
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

    void finishAttribute(Node node) {
        if (!currentObject) {
            fail(DiagnosticCode::InvalidEventOrder,
                 "Attribute ended without an active object", node.location);
        }
        appendGenericAttribute(*currentObject, node);
    }

    void finishCapture(const xml::XmlEndElement& element) {
        if (nodes.empty()) {
            fail(DiagnosticCode::InternalError,
                 "XTF capture stack underflow", element.location);
        }
        if (nodes.back().name != element.name) {
            fail(DiagnosticCode::InvalidEventOrder,
                 "Mismatched XTF element while capturing data",
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
            if (!isControl(element.name, "HEADERSECTION")) {
                fail(DiagnosticCode::InvalidEventOrder,
                     "XTF 2.3 requires HEADERSECTION immediately after TRANSFER",
                     element.location);
            }
            state = DialectState::CapturingHeader;
            beginCapture(CaptureKind::Header, element);
            return;
        case DialectState::ExpectData:
            if (!isControl(element.name, "DATASECTION")) {
                fail(DiagnosticCode::InvalidEventOrder,
                     "XTF 2.3 requires DATASECTION after HEADERSECTION",
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
                 "Unexpected XTF 2.3 element in the current reader state",
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
            if (element.name != currentObjectName || !currentObject) {
                fail(DiagnosticCode::InvalidEventOrder,
                     "Unexpected object end element", element.location);
            }
            emitEvent(ObjectEvent{std::move(*currentObject)});
            currentObject.reset();
            currentObjectName = {};
            state = DialectState::InBasket;
            return;
        case DialectState::InBasket:
            if (element.name != currentBasketName) {
                fail(DiagnosticCode::InvalidEventOrder,
                     "Unexpected basket end element", element.location);
            }
            emitEvent(EndBasketEvent{});
            currentBasketName = {};
            state = DialectState::InData;
            return;
        case DialectState::InData:
            if (!isControl(element.name, "DATASECTION")) {
                fail(DiagnosticCode::InvalidEventOrder,
                     "Unexpected element ending in DATASECTION",
                     element.location);
            }
            state = DialectState::AfterData;
            return;
        default:
            fail(DiagnosticCode::InvalidEventOrder,
                 "Unexpected XTF 2.3 end element", element.location);
        }
    }

    void text(std::string_view data, const SourceLocation& location) {
        if (captureKind != CaptureKind::None) {
            if (nodes.empty()) {
                fail(DiagnosticCode::InternalError,
                     "Text capture has no active element", location);
            }
            nodes.back().text.append(data.data(), data.size());
            return;
        }
        if (!isWhitespace(data)) {
            fail(DiagnosticCode::UnexpectedElement,
                 "Non-whitespace text occurred between XTF control elements",
                 location);
        }
    }
};

IomObject Xtf23Dialect::Impl::sequenceFrom(const Node& node) {
    IomObject sequence(IomName("SEGMENTS"));
    const auto append = [&](const Node& child, auto&& self) -> void {
        const auto local = upperAscii(child.name.localName);
        if (local == "COORD" || local == "ARC") {
            sequence.appendObject(IomName("segment"), nodeToObject(child));
            return;
        }
        if (local == "SEGMENT" || local == "SEGMENTS" ||
            local == "SEQUENCE" || local == "CLIPPED") {
            for (const auto& nested : child.children) self(nested, self);
            return;
        }
        // XTF 2.3 permits model-defined line forms as segment objects.
        sequence.appendObject(IomName("segment"), nodeToObject(child));
    };
    for (const auto& child : node.children) append(child, append);
    return sequence;
}

IomObject Xtf23Dialect::Impl::nodeToObject(const Node& node) {
    const auto local = upperAscii(node.name.localName);
    IomObject result(iomName(node.name));
    result.setSourceLocation(node.location);

    if (local == "POLYLINE") {
        bool added = false;
        bool sawClipped = false;
        bool sawDirect = false;
        bool sawLineAttr = false;
        std::vector<Node> direct;
        for (const auto& child : node.children) {
            const auto childLocal = upperAscii(child.name.localName);
            if (childLocal == "LINEATTR") {
                if (sawLineAttr || added || !direct.empty() ||
                    child.children.size() != 1U) {
                    strictOrReport(
                        DiagnosticCode::InvalidGeometry,
                        "LINEATTR must occur once before the POLYLINE segments "
                        "and contain exactly one structure",
                        child.location);
                }
                sawLineAttr = true;
                if (!child.children.empty()) {
                    result.appendObject(IomName("lineattr"),
                                        nodeToObject(child.children.front()));
                }
            } else if (childLocal == "CLIPPED") {
                if (sawDirect || !direct.empty()) {
                    strictOrReport(
                        DiagnosticCode::InvalidGeometry,
                        "POLYLINE cannot mix direct and CLIPPED sequences",
                        child.location);
                }
                sawClipped = true;
                auto sequence = sequenceFrom(child);
                if (sequence.valueCount("segment") != 0U) {
                    result.appendObject(IomName("sequence"),
                                        std::move(sequence));
                    added = true;
                } else {
                    strictOrReport(DiagnosticCode::InvalidGeometry,
                                   "CLIPPED requires at least one segment",
                                   child.location);
                }
            } else if (childLocal == "SEQUENCE" ||
                       childLocal == "SEGMENTS") {
                sawDirect = true;
                auto sequence = sequenceFrom(child);
                if (sequence.valueCount("segment") != 0U) {
                    result.appendObject(IomName("sequence"),
                                        std::move(sequence));
                    added = true;
                }
            } else {
                sawDirect = true;
                direct.push_back(child);
            }
        }
        if (!direct.empty()) {
            Node wrapper;
            wrapper.children = std::move(direct);
            result.appendObject(IomName("sequence"), sequenceFrom(wrapper));
            added = true;
        }
        if (sawClipped) result.setConsistency(Consistency::Incomplete);
        if (!added) {
            strictOrReport(DiagnosticCode::InvalidGeometry,
                           "POLYLINE requires at least one segment",
                           node.location);
        }
        return result;
    }

    if (local == "SURFACE" || local == "AREA") {
        bool sawClipped = false;
        bool sawDirect = false;
        for (const auto& child : node.children) {
            const auto childLocal = upperAscii(child.name.localName);
            if (child.name.localName == "boundary" &&
                child.children.size() == 1U) {
                sawDirect = true;
                result.appendObject(IomName("boundary"),
                                    nodeToObject(child.children.front()));
            } else if (childLocal == "BOUNDARY") {
                sawDirect = true;
                result.appendObject(IomName("boundary"),
                                    nodeToObject(child));
            } else if (childLocal == "EXTERIOR" ||
                       childLocal == "INTERIOR") {
                result.appendObject(IomName(child.name.localName),
                                    nodeToObject(child));
            } else if (childLocal == "BOUNDARIES") {
                sawDirect = true;
                for (const auto& nested : child.children) {
                    result.appendObject(IomName("boundary"),
                                        nodeToObject(nested));
                }
            } else if (childLocal == "CLIPPED") {
                if (sawDirect) {
                    strictOrReport(
                        DiagnosticCode::InvalidGeometry,
                        "SURFACE cannot mix direct and CLIPPED boundaries",
                        child.location);
                }
                sawClipped = true;
                IomObject clipped(IomName("BOUNDARIES"));
                clipped.setSourceLocation(child.location);
                for (const auto& nested : child.children) {
                    if (upperAscii(nested.name.localName) != "BOUNDARY") {
                        strictOrReport(
                            DiagnosticCode::InvalidGeometry,
                            "CLIPPED surface content must be BOUNDARY elements",
                            nested.location);
                        appendGenericAttribute(clipped, nested);
                    } else {
                        clipped.appendObject(IomName("boundary"),
                                             nodeToObject(nested));
                    }
                }
                if (clipped.valueCount("boundary") == 0U) {
                    strictOrReport(
                        DiagnosticCode::InvalidGeometry,
                        "CLIPPED surface requires at least one BOUNDARY",
                        child.location);
                } else {
                    result.appendObject(IomName("clipped"),
                                        std::move(clipped));
                }
            } else {
                appendGenericAttribute(result, child);
            }
        }
        if (sawClipped) result.setConsistency(Consistency::Incomplete);
        if (result.valueCount("boundary") == 0U &&
            result.valueCount("exterior") == 0U &&
            result.valueCount("clipped") == 0U) {
            strictOrReport(DiagnosticCode::InvalidGeometry,
                           "SURFACE requires at least one BOUNDARY",
                           node.location);
        }
        return result;
    }

    if (local == "BOUNDARY") {
        for (const auto& child : node.children) {
            const auto childLocal = upperAscii(child.name.localName);
            if (child.name.localName == "polyline" &&
                child.children.size() == 1U) {
                result.appendObject(IomName("polyline"),
                                    nodeToObject(child.children.front()));
            } else if (childLocal == "POLYLINE") {
                result.appendObject(IomName("polyline"),
                                    nodeToObject(child));
            } else if (childLocal == "POLYLINES") {
                for (const auto& nested : child.children) {
                    result.appendObject(IomName("polyline"),
                                        nodeToObject(nested));
                }
            } else {
                appendGenericAttribute(result, child);
            }
        }
        if (result.valueCount("polyline") == 0U) {
            strictOrReport(DiagnosticCode::InvalidGeometry,
                           "BOUNDARY requires at least one POLYLINE",
                           node.location);
        }
        return result;
    }

    for (const auto& child : node.children) {
        appendGenericAttribute(result, child);
    }
    if ((local == "COORD" || local == "ARC") &&
        result.attributeCount() == 0U) {
        strictOrReport(DiagnosticCode::InvalidGeometry,
                       local + " requires coordinate values",
                       node.location);
    }
    return result;
}

Xtf23Dialect::Xtf23Dialect(XtfReaderOptions options,
                           EventHandler emitEvent,
                           DiagnosticHandler addDiagnostic)
    : impl_(std::make_unique<Impl>(std::move(options),
                                   std::move(emitEvent),
                                   std::move(addDiagnostic))) {}

Xtf23Dialect::~Xtf23Dialect() = default;

void Xtf23Dialect::onStartElement(const xml::XmlStartElement& element) {
    impl_->start(element);
}

void Xtf23Dialect::onEndElement(const xml::XmlEndElement& element) {
    impl_->end(element);
}

void Xtf23Dialect::onText(std::string_view data,
                          const SourceLocation& location) {
    impl_->text(data, location);
}

void Xtf23Dialect::onRootClosed(const SourceLocation& location) {
    if (impl_->state != DialectState::AfterData) {
        impl_->fail(DiagnosticCode::InvalidEventOrder,
                    "TRANSFER ended before a complete DATASECTION",
                    location);
    }
    impl_->emitEvent(EndTransferEvent{});
    impl_->state = DialectState::Done;
}

bool Xtf23Dialect::finished() const noexcept {
    return impl_->state == DialectState::Done;
}

} // namespace xtf
} // namespace iox
