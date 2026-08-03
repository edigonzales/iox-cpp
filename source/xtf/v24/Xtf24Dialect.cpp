#include "iox/xtf/Xtf24Dialect.h"

#include <stack>
#include <string>
#include <vector>
#include <utility>
#include <cstring>
#include <stdexcept>
#include <cctype>

namespace iox {
namespace xtf {

// ============================================================================
// Helpers
// ============================================================================

namespace {

std::string lowerAscii(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (char c : value) {
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return result;
}

constexpr const char* ILI_NS = "http://www.interlis.ch/xtf/2.4/INTERLIS";
constexpr const char* LEGACY_ILI_NS = "http://www.interlis.ch/INTERLIS2.4";

bool isIliElement(std::string_view name, std::string_view local) {
    std::string expected = std::string(ILI_NS) + "\xFF" + std::string(local);
    if (name == expected) return true;
    expected = std::string(LEGACY_ILI_NS) + "\xFF" + std::string(local);
    if (name == expected) return true;
    if (lowerAscii(name) == lowerAscii(local)) return true;
    std::string prefixed = std::string("ili:") + std::string(local);
    if (name == prefixed) return true;
    return false;
}

std::string localName(std::string_view name) {
    auto sepPos = name.find('\xFF');
    if (sepPos != std::string::npos) {
        return std::string(name.substr(sepPos + 1));
    }
    std::string s(name);
    if (s.size() > 4 && s.substr(0, 4) == "ili:") {
        return s.substr(4);
    }
    return s;
}

IomName iomNameFromXml(std::string_view name) {
    const auto separator = name.find('\xFF');
    if (separator == std::string_view::npos) {
        return IomName(localName(name));
    }
    const auto uri = std::string(name.substr(0, separator));
    const auto local = std::string(name.substr(separator + 1));
    return IomName(local, XmlQualifiedName(uri, local));
}

std::string findAttr(const std::vector<std::pair<std::string_view, std::string_view>>& attrs,
                     std::string_view key) {
    for (const auto& a : attrs) {
        std::string lk = localName(a.first);
        if (lowerAscii(lk) == lowerAscii(key)) return std::string(a.second);
    }
    return "";
}

Consistency parseConsistency(std::string_view value) {
    const auto lower = lowerAscii(value);
    if (lower.empty() || lower == "complete") return Consistency::Complete;
    if (lower == "incomplete") return Consistency::Incomplete;
    if (lower == "inconsistent") return Consistency::Inconsistent;
    if (lower == "adapted") return Consistency::Adapted;
    return Consistency::Unspecified;
}

ObjectOperation parseOperation(std::string_view value) {
    const auto lower = lowerAscii(value);
    if (lower.empty() || lower == "insert") return ObjectOperation::Insert;
    if (lower == "update") return ObjectOperation::Update;
    if (lower == "delete") return ObjectOperation::Delete;
    return ObjectOperation::None;
}

} // anonymous namespace

// ============================================================================
// Element types
// ============================================================================

enum class ElemType {
    Unknown,
    Basket,
    Object,
    Attribute,
    Structure
};

// ============================================================================
// Xtf24Dialect::Impl
// ============================================================================

struct Xtf24Dialect::Impl {
    Xtf24Callbacks cb;
    bool fatal = false;

    struct ElementState {
        ElemType type = ElemType::Unknown;
        std::string name;
        std::string iliName;
        IomObject object;
        std::string textBuffer;
        std::string operation;
        bool hasChildStructure = false;
        bool attachToPolylineSegments = false;
        bool attachToMultiMember = false;
        bool attachToCustomLineMembers = false;
        std::string attachAttribute;
        ReferenceInfo reference;
        bool isReference = false;
    };
    std::stack<ElementState> stack;

    StartBasketEvent currentBasket;

};

// ============================================================================
// Xtf24Dialect — public API
// ============================================================================

Xtf24Dialect::Xtf24Dialect(Xtf24Callbacks callbacks)
    : impl_(std::make_unique<Impl>()) {
    impl_->cb = std::move(callbacks);
}

Xtf24Dialect::~Xtf24Dialect() = default;

void Xtf24Dialect::onStartElement(
    std::string_view name,
    const std::vector<std::pair<std::string_view, std::string_view>>& attrs)
{
    if (impl_->fatal) return;

    auto local = localName(name);
    Impl::ElementState state;
    state.name = std::string(name);
    state.iliName = local;

    // --- BASKET ---
    if (isIliElement(name, "BASKET") || lowerAscii(local) == "basket" ||
        (impl_->stack.empty() && !findAttr(attrs, "BID").empty() &&
         findAttr(attrs, "TID").empty())) {
        state.type = ElemType::Basket;

        StartBasketEvent sb;
        sb.basket.basketId = findAttr(attrs, "BID");
        sb.basket.consistency = parseConsistency(findAttr(attrs, "CONSISTENCY"));
        const auto operation = parseOperation(findAttr(attrs, "OPERATION"));
        sb.basket.kind = operation == ObjectOperation::Update
                             ? BasketKind::Update
                             : operation == ObjectOperation::Insert
                                   ? BasketKind::Initial
                                   : BasketKind::Unspecified;
        sb.basket.topic = iomNameFromXml(name);
        impl_->currentBasket = sb;
        impl_->cb.emitEvent(sb);
        impl_->stack.push(std::move(state));
        return;
    }

    // --- Objects: element with TID attribute ---
    std::string tid = findAttr(attrs, "TID");
    if (!tid.empty()) {
        state.type = ElemType::Object;
        state.object = IomObject(iomNameFromXml(name), tid);
        const auto objectBid = findAttr(attrs, "BID");
        if (!objectBid.empty()) {
            ReferenceInfo reference;
            reference.targetBasketId = objectBid;
            state.object.setReference(std::move(reference));
        }
        state.object.setOperation(parseOperation(findAttr(attrs, "OPERATION")));
        state.object.setConsistency(parseConsistency(findAttr(attrs, "CONSISTENCY")));
        impl_->stack.push(std::move(state));
        return;
    }

    // --- Inside an object or structure: attribute ---
    if (!impl_->stack.empty()) {
        auto& parent = impl_->stack.top();

        if (parent.type == ElemType::Structure) {
            const auto parentTag = lowerAscii(parent.object.tag().interlisName());
            const auto childTag = lowerAscii(local);
            const auto expandedSeparator = name.find('\xFF');
            const bool geometryElement = expandedSeparator != std::string_view::npos &&
                name.substr(0, expandedSeparator) == "http://www.interlis.ch/geometry/1.0";

            // XTF 2.4 writes a polyline's COORD/ARC/custom line members
            // directly below POLYLINE. Normalize that syntax to the stable
            // IOM sequence -> SEGMENTS -> segment tree.
            if (parentTag == "polyline" &&
                (childTag == "coord" || childTag == "arc" || geometryElement)) {
                state.type = ElemType::Structure;
                state.object = IomObject(iomNameFromXml(name));
                state.attachToPolylineSegments = true;
                impl_->stack.push(std::move(state));
                return;
            }

            // Preserve supported/unknown XTF 2.4 line forms as structured
            // segment objects instead of dropping or degrading them.
            if (parentTag == "orientablecurve" &&
                (childTag == "coord" || childTag == "arc" || geometryElement)) {
                state.type = ElemType::Structure;
                state.object = IomObject(iomNameFromXml(name));
                state.attachToCustomLineMembers = true;
                impl_->stack.push(std::move(state));
                return;
            }

            const char* multiAttribute = nullptr;
            if (parentTag == "multicoord" && childTag == "coord") multiAttribute = "coord";
            if (parentTag == "multipolyline" && childTag == "polyline") multiAttribute = "polyline";
            if (parentTag == "multisurface" && childTag == "surface") multiAttribute = "surface";
            if (parentTag == "multiarea" && childTag == "area") multiAttribute = "area";
            if (multiAttribute) {
                state.type = ElemType::Structure;
                state.object = IomObject(iomNameFromXml(name));
                state.attachToMultiMember = true;
                state.attachAttribute = multiAttribute;
                impl_->stack.push(std::move(state));
                return;
            }
        }

        if (parent.type == ElemType::Object || parent.type == ElemType::Structure) {
            // Attribute of the parent object/structure
            state.type = ElemType::Attribute;
            std::string ref = findAttr(attrs, "REF");
            if (!ref.empty()) {
                state.reference.targetOid = std::move(ref);
                state.isReference = true;
            }
            std::string bid = findAttr(attrs, "BID");
            if (!bid.empty()) {
                state.reference.targetBasketId = std::move(bid);
                state.isReference = true;
            }
            std::string orderPos = findAttr(attrs, "ORDER_POS");
            if (!orderPos.empty()) {
                try {
                    state.reference.orderPosition = std::stoull(orderPos);
                    state.isReference = true;
                } catch (...) {}
            }
            impl_->stack.push(std::move(state));
            return;
        }

        if (parent.type == ElemType::Attribute) {
            // Nested structure within an attribute (e.g. COORD inside Location)
            parent.hasChildStructure = true;
            state.type = ElemType::Structure;
            state.object = IomObject(iomNameFromXml(name));
            // Immediately add this structure as a value of the parent attribute
            // The parent attribute is on the grandparent's object
            // We need to find and update the attribute on the grandparent
            // Since the attribute was pre-created on the grandparent during its onStartElement,
            // we look at the grandparent to find it.
            impl_->stack.push(std::move(state));
            return;
        }
    }

    // Unknown — push as unknown
    state.type = ElemType::Unknown;
    impl_->stack.push(std::move(state));
}

void Xtf24Dialect::onEndElement(std::string_view /*name*/) {
    if (impl_->fatal || impl_->stack.empty()) return;

    auto state = std::move(impl_->stack.top());
    impl_->stack.pop();

    switch (state.type) {
    case ElemType::Basket: {
        impl_->cb.emitEvent(EndBasketEvent{});
        break;
    }
    case ElemType::Object: {
        ObjectEvent objEvent;
        objEvent.object = std::move(state.object);
        impl_->cb.emitEvent(objEvent);
        break;
    }
    case ElemType::Attribute: {
        // The attribute entry was created during onStartElement.
        // Now fill in the value from text content or nested structures.
        if (!impl_->stack.empty()) {
            auto& parent = impl_->stack.top();
            if (parent.type == ElemType::Object || parent.type == ElemType::Structure) {
                if (!state.hasChildStructure) {
                    const auto attributeName = iomNameFromXml(state.name);
                    if (state.isReference) {
                        IomObject reference(attributeName);
                        reference.setReference(std::move(state.reference));
                        parent.object.appendObject(attributeName,
                                                   std::move(reference));
                    } else {
                        parent.object.appendPrimitive(attributeName,
                                                      std::move(state.textBuffer));
                    }
                }
            }
        }
        break;
    }
    case ElemType::Structure: {
        if (!impl_->stack.empty()) {
            auto& parent = impl_->stack.top();
            if (state.attachToPolylineSegments && parent.type == ElemType::Structure) {
                IomObject segments(IomName("SEGMENTS"));
                const auto sequenceCount = parent.object.valueCount("sequence");
                if (sequenceCount != 0) {
                    segments = *parent.object.object("sequence", sequenceCount - 1);
                }
                segments.appendObject(IomName("segment"), std::move(state.object));
                if (sequenceCount == 0) {
                    parent.object.appendObject(IomName("sequence"), std::move(segments));
                } else {
                    parent.object.replaceValue("sequence", sequenceCount - 1,
                                               IomValue::object(std::move(segments)));
                }
                break;
            }
            if (state.attachToMultiMember && parent.type == ElemType::Structure) {
                parent.object.appendObject(IomName(state.attachAttribute),
                                           std::move(state.object));
                break;
            }
            if (state.attachToCustomLineMembers && parent.type == ElemType::Structure) {
                parent.object.appendObject(IomName("segment"),
                                           std::move(state.object));
                break;
            }
            if (parent.type == ElemType::Attribute) {
                // Structure inside attribute — pop parent to reach grandparent
                std::string parentIliName = parent.iliName; // save before move
                auto parentState = std::move(impl_->stack.top());
                impl_->stack.pop();

                if (!impl_->stack.empty()) {
                    auto& grandparent = impl_->stack.top();
                    if (grandparent.type == ElemType::Object || grandparent.type == ElemType::Structure) {
                        grandparent.object.appendObject(IomName(parentIliName),
                                                        std::move(state.object));
                    }
                }

                impl_->stack.push(std::move(parentState));
            } else if (parent.type == ElemType::Object || parent.type == ElemType::Structure) {
                parent.object.appendObject(IomName(state.iliName),
                                           std::move(state.object));
            }
        }
        break;
    }
    default:
        break;
    }
}

void Xtf24Dialect::onCharacterData(std::string_view data) {
    if (impl_->fatal || impl_->stack.empty()) return;

    auto& state = impl_->stack.top();
    if (state.type == ElemType::Attribute) {
        state.textBuffer.append(data.data(), data.size());
    }
}

void Xtf24Dialect::reset() {
    while (!impl_->stack.empty()) impl_->stack.pop();
    impl_->fatal = false;
}

bool Xtf24Dialect::isFatal() const noexcept {
    return impl_->fatal;
}

} // namespace xtf
} // namespace iox
