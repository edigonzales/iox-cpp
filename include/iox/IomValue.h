#pragma once

#include <string>
#include <variant>
#include <cstdint>
#include <type_traits>

namespace iox {

/// Union of primitive INTERLIS transfer values.
/// All numeric values are stored in their canonical types;
/// the core never stores numeric values as strings.
class IomValue final {
public:
    enum class Kind {
        Null,
        Text,
        Integer,
        Decimal,
        Boolean
    };

    IomValue() noexcept : data_(NullTag{}) {}

    static IomValue text(std::string value) { return IomValue(std::move(value)); }
    static IomValue integer(std::int64_t value) { return IomValue(value); }
    static IomValue decimal(double value) { return IomValue(value); }
    static IomValue boolean(bool value) { return IomValue(value); }
    static IomValue null() { return IomValue(); }

    Kind kind() const noexcept {
        return static_cast<Kind>(data_.index());
    }

    bool isNull() const noexcept { return kind() == Kind::Null; }

    const std::string& asText() const {
        return std::get<std::string>(data_);
    }

    std::int64_t asInteger() const {
        return std::get<std::int64_t>(data_);
    }

    double asDecimal() const {
        return std::get<double>(data_);
    }

    bool asBoolean() const {
        return std::get<bool>(data_);
    }

    /// Return the value as its original string form for transfer.
    /// Text values are returned verbatim; numeric values are
    /// formatted to string.
    std::string toTransferString() const;

    bool operator==(const IomValue& o) const noexcept {
        return data_ == o.data_;
    }

    bool operator!=(const IomValue& o) const noexcept {
        return !(*this == o);
    }

private:
    struct NullTag {
        bool operator==(const NullTag&) const noexcept { return true; }
    };
    using Data = std::variant<NullTag, std::string, std::int64_t, double, bool>;
    Data data_;

    explicit IomValue(std::string s) : data_(std::move(s)) {}
    explicit IomValue(std::int64_t i) : data_(i) {}
    explicit IomValue(double d) : data_(d) {}
    explicit IomValue(bool b) : data_(b) {}
};

} // namespace iox
