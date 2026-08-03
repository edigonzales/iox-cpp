#pragma once

#include "iox/Diagnostic.h"
#include "iox/IomName.h"
#include "iox/IomValue.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace iox {

enum class ObjectOperation {
    Insert,
    Update,
    Delete,
    None
};

enum class Consistency {
    Complete,
    Incomplete,
    Inconsistent,
    Adapted,
    Unspecified
};

struct ReferenceInfo final {
    std::optional<std::string> targetOid;
    std::optional<std::string> targetBasketId;
    std::optional<std::uint64_t> orderPosition;

    bool operator==(const ReferenceInfo& other) const noexcept {
        return targetOid == other.targetOid &&
               targetBasketId == other.targetBasketId &&
               orderPosition == other.orderPosition;
    }
};

class IomObject final {
public:
    IomObject();
    explicit IomObject(IomName tag,
                       std::optional<std::string> oid = std::nullopt);

    IomObject(const IomObject&) noexcept;
    IomObject(IomObject&&) noexcept;
    IomObject& operator=(const IomObject&) noexcept;
    IomObject& operator=(IomObject&&) noexcept;
    ~IomObject();

    bool empty() const noexcept;

    const IomName& tag() const;
    void setTag(IomName tag);

    const std::optional<std::string>& oid() const noexcept;
    void setOid(std::optional<std::string> oid);

    ObjectOperation operation() const noexcept;
    void setOperation(ObjectOperation operation);

    Consistency consistency() const noexcept;
    void setConsistency(Consistency consistency);

    const ReferenceInfo& reference() const noexcept;
    void setReference(ReferenceInfo reference);
    bool isReference() const noexcept;

    const SourceLocation& sourceLocation() const noexcept;
    void setSourceLocation(SourceLocation location);

    std::size_t attributeCount() const noexcept;
    const IomName& attributeName(std::size_t attributeIndex) const;
    bool hasAttribute(std::string_view interlisName) const;

    std::size_t valueCount(std::string_view interlisName) const;
    const IomValue& value(std::string_view interlisName,
                          std::size_t valueIndex) const;

    std::optional<std::string_view> primitive(
        std::string_view interlisName,
        std::size_t valueIndex = 0) const;

    std::optional<IomObject> object(
        std::string_view interlisName,
        std::size_t valueIndex = 0) const;

    void setPrimitive(IomName attribute, std::string value);
    void appendPrimitive(IomName attribute, std::string value);
    void setObject(IomName attribute, IomObject value);
    void appendObject(IomName attribute, IomObject value);

    void insertValue(IomName attribute,
                     std::size_t valueIndex,
                     IomValue value);
    void replaceValue(std::string_view interlisName,
                      std::size_t valueIndex,
                      IomValue value);
    void eraseValue(std::string_view interlisName,
                    std::size_t valueIndex);
    void eraseAttribute(std::string_view interlisName);
    void clearAttributes();

    IomObject deepCopy() const;
    bool semanticallyEquals(const IomObject& other) const;

private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
    void detach();
};

} // namespace iox
