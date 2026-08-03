#include "iox/IomObject.h"

#include <algorithm>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace iox {

namespace {

struct AttributeEntry final {
    IomName name;
    std::vector<IomValue> values;
};

std::string nameKey(const IomName& name) {
    if (name.hasInterlisName()) return name.interlisName();
    if (name.hasXmlName()) return name.xmlName().expanded();
    return {};
}

bool sameSemanticName(const IomName& left, const IomName& right) {
    if (left.hasInterlisName() && right.hasInterlisName()) {
        return left.interlisName() == right.interlisName();
    }
    if (left.hasXmlName() && right.hasXmlName()) {
        return left.xmlName() == right.xmlName();
    }
    return !left.hasInterlisName() && !right.hasInterlisName() &&
           !left.hasXmlName() && !right.hasXmlName();
}

} // namespace

struct IomObject::Impl final {
    IomName tag;
    std::optional<std::string> oid;
    ObjectOperation operation = ObjectOperation::None;
    Consistency consistency = Consistency::Unspecified;
    ReferenceInfo reference;
    SourceLocation location;
    std::vector<AttributeEntry> attributes;
    mutable std::unordered_map<std::string, std::size_t> lookup;
    mutable bool lookupValid = false;

    void invalidateLookup() noexcept {
        lookupValid = false;
        lookup.clear();
    }

    void buildLookup() const {
        if (lookupValid) return;
        lookup.clear();
        for (std::size_t index = 0; index < attributes.size(); ++index) {
            lookup.emplace(nameKey(attributes[index].name), index);
        }
        lookupValid = true;
    }

    std::optional<std::size_t> find(std::string_view interlisName) const {
        buildLookup();
        const auto found = lookup.find(std::string(interlisName));
        if (found == lookup.end()) return std::nullopt;
        return found->second;
    }
};

IomObject::IomObject() : impl_(std::make_shared<Impl>()) {}

IomObject::IomObject(IomName tag, std::optional<std::string> oid)
    : impl_(std::make_shared<Impl>()) {
    impl_->tag = std::move(tag);
    impl_->oid = std::move(oid);
}

IomObject::IomObject(const IomObject&) noexcept = default;
IomObject::IomObject(IomObject&&) noexcept = default;
IomObject& IomObject::operator=(const IomObject&) noexcept = default;
IomObject& IomObject::operator=(IomObject&&) noexcept = default;
IomObject::~IomObject() = default;

bool IomObject::empty() const noexcept {
    return !impl_->tag.hasInterlisName() && !impl_->tag.hasXmlName() &&
           !impl_->oid && impl_->attributes.empty() && !isReference();
}

const IomName& IomObject::tag() const { return impl_->tag; }

void IomObject::setTag(IomName tag) {
    detach();
    impl_->tag = std::move(tag);
}

const std::optional<std::string>& IomObject::oid() const noexcept { return impl_->oid; }

void IomObject::setOid(std::optional<std::string> oid) {
    detach();
    impl_->oid = std::move(oid);
}

ObjectOperation IomObject::operation() const noexcept { return impl_->operation; }

void IomObject::setOperation(ObjectOperation operation) {
    detach();
    impl_->operation = operation;
}

Consistency IomObject::consistency() const noexcept { return impl_->consistency; }

void IomObject::setConsistency(Consistency consistency) {
    detach();
    impl_->consistency = consistency;
}

const ReferenceInfo& IomObject::reference() const noexcept { return impl_->reference; }

void IomObject::setReference(ReferenceInfo reference) {
    detach();
    impl_->reference = std::move(reference);
}

bool IomObject::isReference() const noexcept {
    return impl_->reference.targetOid.has_value() ||
           impl_->reference.targetBasketId.has_value() ||
           impl_->reference.orderPosition.has_value();
}

const SourceLocation& IomObject::sourceLocation() const noexcept { return impl_->location; }

void IomObject::setSourceLocation(SourceLocation location) {
    detach();
    impl_->location = std::move(location);
}

std::size_t IomObject::attributeCount() const noexcept { return impl_->attributes.size(); }

const IomName& IomObject::attributeName(std::size_t attributeIndex) const {
    if (attributeIndex >= impl_->attributes.size()) {
        throw IoxError(DiagnosticCode::InvalidArgument,
                       "IOM attribute index is out of range");
    }
    return impl_->attributes[attributeIndex].name;
}

bool IomObject::hasAttribute(std::string_view interlisName) const {
    return impl_->find(interlisName).has_value();
}

std::size_t IomObject::valueCount(std::string_view interlisName) const {
    const auto index = impl_->find(interlisName);
    return index ? impl_->attributes[*index].values.size() : 0;
}

const IomValue& IomObject::value(std::string_view interlisName,
                                 std::size_t valueIndex) const {
    const auto index = impl_->find(interlisName);
    if (!index || valueIndex >= impl_->attributes[*index].values.size()) {
        throw IoxError(DiagnosticCode::InvalidArgument,
                       "IOM value index is out of range");
    }
    return impl_->attributes[*index].values[valueIndex];
}

std::optional<std::string_view> IomObject::primitive(
    std::string_view interlisName,
    std::size_t valueIndex) const {
    const auto index = impl_->find(interlisName);
    if (!index || valueIndex >= impl_->attributes[*index].values.size()) {
        return std::nullopt;
    }
    const auto& entry = impl_->attributes[*index].values[valueIndex];
    return entry.isPrimitive()
               ? std::optional<std::string_view>(entry.primitive())
               : std::nullopt;
}

std::optional<IomObject> IomObject::object(std::string_view interlisName,
                                           std::size_t valueIndex) const {
    const auto index = impl_->find(interlisName);
    if (!index || valueIndex >= impl_->attributes[*index].values.size()) {
        return std::nullopt;
    }
    const auto& entry = impl_->attributes[*index].values[valueIndex];
    return entry.isObject() ? std::optional<IomObject>(entry.object()) : std::nullopt;
}

void IomObject::setPrimitive(IomName attribute, std::string value) {
    const auto key = nameKey(attribute);
    detach();
    const auto index = impl_->find(key);
    if (index) {
        auto& entry = impl_->attributes[*index];
        entry.name = std::move(attribute);
        entry.values.clear();
        entry.values.push_back(IomValue::primitive(std::move(value)));
    } else {
        AttributeEntry entry;
        entry.name = std::move(attribute);
        entry.values.push_back(IomValue::primitive(std::move(value)));
        impl_->attributes.push_back(std::move(entry));
    }
    impl_->invalidateLookup();
}

void IomObject::appendPrimitive(IomName attribute, std::string value) {
    const auto index = valueCount(nameKey(attribute));
    insertValue(std::move(attribute), index,
                IomValue::primitive(std::move(value)));
}

void IomObject::setObject(IomName attribute, IomObject value) {
    const auto key = nameKey(attribute);
    detach();
    const auto index = impl_->find(key);
    if (index) {
        auto& entry = impl_->attributes[*index];
        entry.name = std::move(attribute);
        entry.values.clear();
        entry.values.push_back(IomValue::object(std::move(value)));
    } else {
        AttributeEntry entry;
        entry.name = std::move(attribute);
        entry.values.push_back(IomValue::object(std::move(value)));
        impl_->attributes.push_back(std::move(entry));
    }
    impl_->invalidateLookup();
}

void IomObject::appendObject(IomName attribute, IomObject value) {
    const auto index = valueCount(nameKey(attribute));
    insertValue(std::move(attribute), index,
                IomValue::object(std::move(value)));
}

void IomObject::insertValue(IomName attribute,
                            std::size_t valueIndex,
                            IomValue value) {
    const auto key = nameKey(attribute);
    detach();
    const auto index = impl_->find(key);
    if (!index) {
        if (valueIndex != 0) {
            throw IoxError(DiagnosticCode::InvalidArgument,
                           "First IOM value must be inserted at index zero");
        }
        AttributeEntry entry;
        entry.name = std::move(attribute);
        entry.values.push_back(std::move(value));
        impl_->attributes.push_back(std::move(entry));
        impl_->invalidateLookup();
        return;
    }
    auto& values = impl_->attributes[*index].values;
    if (valueIndex > values.size()) {
        throw IoxError(DiagnosticCode::InvalidArgument,
                       "IOM insertion index is out of range");
    }
    values.insert(values.begin() + static_cast<std::ptrdiff_t>(valueIndex),
                  std::move(value));
}

void IomObject::replaceValue(std::string_view interlisName,
                             std::size_t valueIndex,
                             IomValue value) {
    detach();
    const auto index = impl_->find(interlisName);
    if (!index || valueIndex >= impl_->attributes[*index].values.size()) {
        throw IoxError(DiagnosticCode::InvalidArgument,
                       "IOM replacement index is out of range");
    }
    impl_->attributes[*index].values[valueIndex] = std::move(value);
}

void IomObject::eraseValue(std::string_view interlisName,
                           std::size_t valueIndex) {
    detach();
    const auto index = impl_->find(interlisName);
    if (!index || valueIndex >= impl_->attributes[*index].values.size()) {
        throw IoxError(DiagnosticCode::InvalidArgument,
                       "IOM erase index is out of range");
    }
    auto& values = impl_->attributes[*index].values;
    values.erase(values.begin() + static_cast<std::ptrdiff_t>(valueIndex));
    if (values.empty()) {
        impl_->attributes.erase(impl_->attributes.begin() +
                                static_cast<std::ptrdiff_t>(*index));
        impl_->invalidateLookup();
    }
}

void IomObject::eraseAttribute(std::string_view interlisName) {
    detach();
    const auto index = impl_->find(interlisName);
    if (!index) return;
    impl_->attributes.erase(impl_->attributes.begin() +
                            static_cast<std::ptrdiff_t>(*index));
    impl_->invalidateLookup();
}

void IomObject::clearAttributes() {
    detach();
    impl_->attributes.clear();
    impl_->invalidateLookup();
}

void IomObject::detach() {
    if (impl_.use_count() != 1) {
        impl_ = std::make_shared<Impl>(*impl_);
        impl_->invalidateLookup();
    }
}

IomObject IomObject::deepCopy() const {
    std::unordered_set<const Impl*> path;
    std::function<IomObject(const IomObject&)> copy = [&](const IomObject& source) {
        if (!path.insert(source.impl_.get()).second) {
            throw IoxError(DiagnosticCode::IomCycle,
                           "Cycle detected in IOM object graph");
        }
        IomObject result(source.impl_->tag, source.impl_->oid);
        result.impl_->operation = source.impl_->operation;
        result.impl_->consistency = source.impl_->consistency;
        result.impl_->reference = source.impl_->reference;
        result.impl_->location = source.impl_->location;
        for (const auto& attribute : source.impl_->attributes) {
            for (const auto& item : attribute.values) {
                if (item.isPrimitive()) {
                    result.appendPrimitive(attribute.name, item.primitive());
                } else {
                    result.appendObject(attribute.name, copy(item.object()));
                }
            }
        }
        path.erase(source.impl_.get());
        return result;
    };
    return copy(*this);
}

bool IomObject::semanticallyEquals(const IomObject& other) const {
    if (impl_ == other.impl_) return true;
    if (!sameSemanticName(impl_->tag, other.impl_->tag) ||
        impl_->oid != other.impl_->oid ||
        impl_->operation != other.impl_->operation ||
        impl_->consistency != other.impl_->consistency ||
        !(impl_->reference == other.impl_->reference) ||
        impl_->attributes.size() != other.impl_->attributes.size()) {
        return false;
    }
    for (std::size_t attrIndex = 0; attrIndex < impl_->attributes.size(); ++attrIndex) {
        const auto& left = impl_->attributes[attrIndex];
        const auto& right = other.impl_->attributes[attrIndex];
        if (!sameSemanticName(left.name, right.name) ||
            left.values.size() != right.values.size()) {
            return false;
        }
        for (std::size_t valueIndex = 0; valueIndex < left.values.size(); ++valueIndex) {
            if (left.values[valueIndex] != right.values[valueIndex]) return false;
        }
    }
    return true;
}

} // namespace iox
