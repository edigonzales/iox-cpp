#include "iox/IomValue.h"

#include "iox/Diagnostic.h"
#include "iox/IomObject.h"

#include <utility>
#include <variant>

namespace iox {

struct IomValue::Impl final {
    std::variant<std::string, IomObject> data;

    explicit Impl(std::string value) : data(std::move(value)) {}
    explicit Impl(IomObject value) : data(std::move(value)) {}
};

IomValue::IomValue(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl)) {}

IomValue IomValue::primitive(std::string value) {
    return IomValue(std::make_unique<Impl>(std::move(value)));
}

IomValue IomValue::object(IomObject value) {
    return IomValue(std::make_unique<Impl>(std::move(value)));
}

IomValue::IomValue(const IomValue& other)
    : impl_(std::make_unique<Impl>(*other.impl_)) {}

IomValue::IomValue(IomValue&& other) noexcept = default;

IomValue& IomValue::operator=(const IomValue& other) {
    if (this != &other) impl_ = std::make_unique<Impl>(*other.impl_);
    return *this;
}

IomValue& IomValue::operator=(IomValue&& other) noexcept = default;
IomValue::~IomValue() = default;

IomValue::Kind IomValue::kind() const noexcept {
    return std::holds_alternative<std::string>(impl_->data)
               ? Kind::Primitive
               : Kind::Object;
}

bool IomValue::isPrimitive() const noexcept { return kind() == Kind::Primitive; }
bool IomValue::isObject() const noexcept { return kind() == Kind::Object; }

const std::string& IomValue::primitive() const {
    if (!isPrimitive()) {
        throw IoxError(DiagnosticCode::InvalidState,
                       "IomValue does not contain a primitive");
    }
    return std::get<std::string>(impl_->data);
}

const IomObject& IomValue::object() const {
    if (!isObject()) {
        throw IoxError(DiagnosticCode::InvalidState,
                       "IomValue does not contain an object");
    }
    return std::get<IomObject>(impl_->data);
}

IomObject& IomValue::object() {
    if (!isObject()) {
        throw IoxError(DiagnosticCode::InvalidState,
                       "IomValue does not contain an object");
    }
    return std::get<IomObject>(impl_->data);
}

bool IomValue::operator==(const IomValue& other) const {
    if (kind() != other.kind()) return false;
    if (isPrimitive()) return primitive() == other.primitive();
    return object().semanticallyEquals(other.object());
}

} // namespace iox
