#include "iox/xtf/Xtf23Dialect.h"

#include <stack>
#include <string>
#include <vector>
#include <utility>
#include <cstring>
#include <stdexcept>

namespace iox {
namespace xtf {

// ============================================================================
// Helpers
// ============================================================================

namespace {

constexpr const char* ILI_NS = "http://www.interlis.ch/INTERLIS2.3";

bool isIliElement(std::string_view name, std::string_view local) {
    std::string expected = std::string(ILI_NS) + "\xFF" + std::string(local);
    if (name == expected) return true;
    if (name == local) return true;
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

std::string findAttr(const std::vector<std::pair<std::string_view, std::string_view>>& attrs,
                     std::string_view key) {
    for (const auto& a : attrs) {
        std::string lk = localName(a.first);
        if (lk == key) return std::string(a.second);
    }
    return "";
}

Consistency parseConsistency(std::string_view value) {
    if (value == "COMPLETE" || value == "complete" || value.empty()) return Consistency::Complete;
    if (value == "INCOMPLETE" || value == "incomplete") return Consistency::Incomplete;
    if (value == "INCONSISTENT" || value == "inconsistent") return Consistency::Inconsistent;
    if (value == "ADAPTED" || value == "adapted") return Consistency::Adapted;
    return Consistency::Unspecified;
}

ObjectOperation parseOperation(std::string_view value) {
    if (value == "INSERT" || value == "insert" || value.empty()) return ObjectOperation::Insert;
    if (value == "UPDATE" || value == "update") return ObjectOperation::Update;
    if (value == "DELETE" || value == "delete") return ObjectOperation::Delete;
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
// Xtf23Dialect::Impl
// ============================================================================

struct Xtf23Dialect::Impl {
    Xtf23Callbacks cb;
    bool fatal = false;

    struct ElementState {
        ElemType type = ElemType::Unknown;
        std::string name;
        std::string iliName;
        IomObject object;
        std::string textBuffer;
        std::string operation;
        bool hasChildStructure = false;
        ReferenceInfo reference;
        bool isReference = false;
    };
    std::stack<ElementState> stack;

    StartBasketEvent currentBasket;

};

// ============================================================================
// Xtf23Dialect — public API
// ============================================================================

Xtf23Dialect::Xtf23Dialect(Xtf23Callbacks callbacks)
    : impl_(std::make_unique<Impl>()) {
    impl_->cb = std::move(callbacks);
}

Xtf23Dialect::~Xtf23Dialect() = default;

void Xtf23Dialect::onStartElement(
    std::string_view name,
    const std::vector<std::pair<std::string_view, std::string_view>>& attrs)
{
    if (impl_->fatal) return;

    auto local = localName(name);
    Impl::ElementState state;
    state.name = std::string(name);
    state.iliName = local;

    // --- BASKET ---
    if (isIliElement(name, "BASKET") ||
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
        sb.basket.topic = IomName(local);
        impl_->currentBasket = sb;
        impl_->cb.emitEvent(sb);
        impl_->stack.push(std::move(state));
        return;
    }

    // --- Objects: element with TID attribute ---
    std::string tid = findAttr(attrs, "TID");
    if (!tid.empty()) {
        state.type = ElemType::Object;
        state.object = IomObject(IomName(local), tid);
        state.object.setOperation(parseOperation(findAttr(attrs, "OPERATION")));
        state.object.setConsistency(parseConsistency(findAttr(attrs, "CONSISTENCY")));
        // Capture object-level BID if present
        std::string objBid = findAttr(attrs, "BID");
        if (!objBid.empty()) {
            ReferenceInfo reference;
            reference.targetBasketId = std::move(objBid);
            state.object.setReference(std::move(reference));
        }
        impl_->stack.push(std::move(state));
        return;
    }

    // --- Inside an object or structure: attribute ---
    if (!impl_->stack.empty()) {
        auto& parent = impl_->stack.top();

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
            state.object = IomObject(IomName(local));
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

void Xtf23Dialect::onEndElement(std::string_view /*name*/) {
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
                    if (state.isReference) {
                        IomObject reference(IomName(state.iliName));
                        reference.setReference(std::move(state.reference));
                        parent.object.appendObject(IomName(state.iliName),
                                                   std::move(reference));
                    } else {
                        parent.object.appendPrimitive(IomName(state.iliName),
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

void Xtf23Dialect::onCharacterData(std::string_view data) {
    if (impl_->fatal || impl_->stack.empty()) return;

    auto& state = impl_->stack.top();
    if (state.type == ElemType::Attribute) {
        state.textBuffer.append(data.data(), data.size());
    }
}

void Xtf23Dialect::reset() {
    while (!impl_->stack.empty()) impl_->stack.pop();
    impl_->fatal = false;
}

bool Xtf23Dialect::isFatal() const noexcept {
    return impl_->fatal;
}

} // namespace xtf
} // namespace iox
