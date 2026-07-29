#include "iox/IomObject.h"

#include <algorithm>
#include <set>
#include <stdexcept>

namespace iox {
namespace detail {

// ============================================================================
// IomObjectImpl — internal state for IomObject
// ============================================================================

class IomObjectImpl final {
public:
    IomName tag;
    std::vector<IomAttribute> attributes;

    // Reference metadata for this object (REF, BID, ORDER_POS)
    std::optional<std::string> refMeta;
    std::optional<std::string> bidMeta;
    std::optional<std::int64_t> orderPosMeta;

    // Mutable index for fast attribute lookup by iliName.
    // Invalidated on any mutation; rebuilt lazily.
    struct AttrIndex {
        std::vector<std::size_t> indices;
    };
    mutable bool indexValid_ = false;
    mutable AttrIndex index_;

    IomObjectImpl() = default;
    explicit IomObjectImpl(IomName t) : tag(std::move(t)) {}

    void invalidateIndex() {
        indexValid_ = false;
    }

    const AttrIndex& getIndex() const {
        if (!indexValid_) {
            index_.indices.clear();
            index_.indices.reserve(attributes.size());
            for (std::size_t i = 0; i < attributes.size(); ++i) {
                index_.indices.push_back(i);
            }
            std::sort(index_.indices.begin(), index_.indices.end(),
                [this](std::size_t a, std::size_t b) {
                    return attributes[a].name.iliName() < attributes[b].name.iliName();
                });
            indexValid_ = true;
        }
        return index_;
    }
};

} // namespace detail

// ============================================================================
// IomObject — public methods (in namespace iox)
// ============================================================================

IomObject::IomObject(IomName tag)
    : impl_(std::make_shared<detail::IomObjectImpl>(std::move(tag))) {}

IomObject::~IomObject() = default;

const IomName& IomObject::tag() const {
    return impl_->tag;
}

void IomObject::setTag(IomName tag) {
    detach();
    impl_->tag = std::move(tag);
}

std::size_t IomObject::attributeCount() const {
    return impl_->attributes.size();
}

const IomAttribute& IomObject::attributeAt(std::size_t index) const {
    return impl_->attributes.at(index);
}

const std::vector<IomAttribute>& IomObject::attributes() const {
    return impl_->attributes;
}

const IomAttribute* IomObject::findAttribute(std::string_view iliName) const {
    auto& idx = impl_->getIndex();
    auto it = std::lower_bound(
        idx.indices.begin(), idx.indices.end(), iliName,
        [this](std::size_t pos, std::string_view name) {
            return impl_->attributes[pos].name.iliName() < name;
        });
    if (it != idx.indices.end() &&
        impl_->attributes[*it].name.iliName() == iliName) {
        return &impl_->attributes[*it];
    }
    return nullptr;
}

IomAttribute& IomObject::setAttribute(IomName name) {
    detach();
    for (auto& attr : impl_->attributes) {
        if (attr.name.iliName() == name.iliName()) {
            attr.name = std::move(name);
            attr.values.clear();
            attr.ref.reset();
            attr.bid.reset();
            attr.orderPos.reset();
            impl_->invalidateIndex();
            return attr;
        }
    }
    impl_->attributes.push_back(IomAttribute{std::move(name)});
    impl_->invalidateIndex();
    return impl_->attributes.back();
}

bool IomObject::removeAttribute(std::string_view iliName) {
    detach();
    auto it = std::find_if(
        impl_->attributes.begin(), impl_->attributes.end(),
        [iliName](const IomAttribute& a) {
            return a.name.iliName() == iliName;
        });
    if (it != impl_->attributes.end()) {
        impl_->attributes.erase(it);
        impl_->invalidateIndex();
        return true;
    }
    return false;
}

std::optional<IomValue> IomObject::getPrimitive(std::string_view attrName) const {
    auto* attr = findAttribute(attrName);
    if (!attr || attr->values.empty()) return std::nullopt;
    if (attr->values.size() > 1) return std::nullopt;
    if (auto* val = std::get_if<IomValue>(&attr->values[0])) {
        return *val;
    }
    return std::nullopt;
}

void IomObject::setPrimitive(std::string_view attrName, IomValue value) {
    auto& attr = setAttribute(IomName(std::string(attrName)));
    attr.values.push_back(std::move(value));
}

IomObject IomObject::getStructure(std::string_view attrName) const {
    auto* attr = findAttribute(attrName);
    if (!attr || attr->values.empty()) return IomObject{};
    if (auto* obj = std::get_if<IomObject>(&attr->values[0])) {
        return *obj;
    }
    return IomObject{};
}

void IomObject::setStructure(std::string_view attrName, IomObject obj) {
    auto& attr = setAttribute(IomName(std::string(attrName)));
    attr.values.clear();
    attr.values.push_back(std::move(obj));
}

const std::optional<std::string>& IomObject::ref() const {
    return impl_->refMeta;
}

const std::optional<std::string>& IomObject::bid() const {
    return impl_->bidMeta;
}

const std::optional<std::int64_t>& IomObject::orderPos() const {
    return impl_->orderPosMeta;
}

void IomObject::setRef(std::string ref) {
    detach();
    impl_->refMeta = std::move(ref);
}

void IomObject::setBid(std::string bid) {
    detach();
    impl_->bidMeta = std::move(bid);
}

void IomObject::setOrderPos(std::int64_t pos) {
    detach();
    impl_->orderPosMeta = pos;
}

void IomObject::detach() {
    if (impl_.use_count() > 1) {
        auto newImpl = std::make_shared<detail::IomObjectImpl>(*impl_);
        newImpl->indexValid_ = false;
        impl_ = std::move(newImpl);
    }
}

IomObject IomObject::deepCopy() const {
    // Use iterative approach with cycle detection
    // For simplicity: do a recursive copy without cycle detection
    // (deep cycles are extremely rare in INTERLIS data)
    auto newImpl = std::make_shared<detail::IomObjectImpl>();
    newImpl->tag = impl_->tag;
    newImpl->refMeta = impl_->refMeta;
    newImpl->bidMeta = impl_->bidMeta;
    newImpl->orderPosMeta = impl_->orderPosMeta;

    for (const auto& attr : impl_->attributes) {
        IomAttribute attrCopy;
        attrCopy.name = attr.name;
        attrCopy.ref = attr.ref;
        attrCopy.bid = attr.bid;
        attrCopy.orderPos = attr.orderPos;

        for (const auto& val : attr.values) {
            if (auto* prim = std::get_if<IomValue>(&val)) {
                attrCopy.values.push_back(*prim);
            } else if (auto* obj = std::get_if<IomObject>(&val)) {
                attrCopy.values.push_back(obj->deepCopy());
            }
        }

        newImpl->attributes.push_back(std::move(attrCopy));
    }

    IomObject result;
    result.impl_ = std::move(newImpl);
    return result;
}

long IomObject::useCount() const noexcept {
    return impl_.use_count();
}

bool IomObject::operator==(const IomObject& o) const {
    if (impl_ == o.impl_) return true;
    if (impl_->tag != o.impl_->tag) return false;
    if (impl_->refMeta != o.impl_->refMeta) return false;
    if (impl_->bidMeta != o.impl_->bidMeta) return false;
    if (impl_->orderPosMeta != o.impl_->orderPosMeta) return false;
    if (impl_->attributes.size() != o.impl_->attributes.size()) return false;

    for (std::size_t i = 0; i < impl_->attributes.size(); ++i) {
        const auto& a = impl_->attributes[i];
        const auto& b = o.impl_->attributes[i];
        if (a.name != b.name) return false;
        if (a.ref != b.ref) return false;
        if (a.bid != b.bid) return false;
        if (a.orderPos != b.orderPos) return false;
        if (a.values.size() != b.values.size()) return false;

        for (std::size_t j = 0; j < a.values.size(); ++j) {
            if (a.values[j] != b.values[j]) return false;
        }
    }
    return true;
}

} // namespace iox
