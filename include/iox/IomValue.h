#pragma once

#include <memory>
#include <string>

namespace iox {

class IomObject;

class IomValue final {
public:
    enum class Kind { Primitive, Object };

    static IomValue primitive(std::string value);
    static IomValue object(IomObject value);

    IomValue(const IomValue& other);
    IomValue(IomValue&& other) noexcept;
    IomValue& operator=(const IomValue& other);
    IomValue& operator=(IomValue&& other) noexcept;
    ~IomValue();

    Kind kind() const noexcept;
    bool isPrimitive() const noexcept;
    bool isObject() const noexcept;

    const std::string& primitive() const;
    const IomObject& object() const;
    IomObject& object();

    bool operator==(const IomValue& other) const;
    bool operator!=(const IomValue& other) const { return !(*this == other); }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    explicit IomValue(std::unique_ptr<Impl> impl) noexcept;
};

} // namespace iox
