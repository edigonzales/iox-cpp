#pragma once

#include "iox/IomName.h"
#include "iox/IomValue.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <utility>
#include <cstdint>

namespace iox {

// Forward declarations
class IomObject;

namespace detail {
class IomObjectImpl;
} // namespace detail

/// A single attribute entry in an IomObject.
/// Attributes are ordered; each attribute may have multiple values
/// (repeated, analogous to LIST/BAG in transfer encoding).
struct IomAttribute final {
    IomName name;

    /// Each value is either a primitive or a nested IomObject.
    using AttrValue = std::variant<IomValue, IomObject>;
    std::vector<AttrValue> values;

    // Reference metadata (REF, BID, ORDER_POS)
    std::optional<std::string> ref;
    std::optional<std::string> bid;
    std::optional<std::int64_t> orderPos;
};

/// Copy-on-write handle for an INTERLIS IOM object.
///
/// Publicly this is a small, copyable value type. Internally it
/// wraps a std::shared_ptr<Impl>. Mutating operations call detach()
/// to ensure copy-on-write semantics.
class IomObject final {
public:
    /// Construct an empty object with the given tag (class name).
    IomObject(IomName tag = IomName{});
    ~IomObject();

    // Copy / move
    IomObject(const IomObject&) = default;
    IomObject(IomObject&&) noexcept = default;
    IomObject& operator=(const IomObject&) = default;
    IomObject& operator=(IomObject&&) noexcept = default;

    // --- Tag (INTERLIS class name) ---

    const IomName& tag() const;
    void setTag(IomName tag);

    // --- Attributes (ordered) ---

    /// Number of attributes.
    std::size_t attributeCount() const;

    /// Get attribute by index (0-based).
    const IomAttribute& attributeAt(std::size_t index) const;

    /// Get all attributes (ordered).
    const std::vector<IomAttribute>& attributes() const;

    /// Find attribute by name. Returns nullptr if not found.
    const IomAttribute* findAttribute(std::string_view iliName) const;

    /// Add or replace an attribute. If an attribute with the same
    /// iliName already exists, its values are replaced.
    /// Returns a reference to the (new or updated) attribute.
    IomAttribute& setAttribute(IomName name);

    /// Remove an attribute by iliName. Returns true if removed.
    bool removeAttribute(std::string_view iliName);

    // --- Convenience: single-valued primitive attributes ---

    /// Get a single primitive value by attribute name.
    /// Returns nullopt if the attribute does not exist or has
    /// multiple values or is structured.
    std::optional<IomValue> getPrimitive(std::string_view attrName) const;

    /// Set a single primitive value. Replaces any existing values.
    void setPrimitive(std::string_view attrName, IomValue value);

    // --- Convenience: structured sub-objects ---

    /// Get the first structured sub-object for an attribute.
    /// Returns a default (empty) IomObject if not found.
    IomObject getStructure(std::string_view attrName) const;

    /// Set a structured sub-object as the only value.
    void setStructure(std::string_view attrName, IomObject obj);

    // --- Reference metadata on this object ---

    const std::optional<std::string>& ref() const;
    const std::optional<std::string>& bid() const;
    const std::optional<std::int64_t>& orderPos() const;

    void setRef(std::string ref);
    void setBid(std::string bid);
    void setOrderPos(std::int64_t pos);

    // --- COW operations ---

    /// Ensure exclusive ownership of the implementation.
    /// Called automatically by all mutating operations.
    void detach();

    /// Recursive deep copy. After this, no mutation to either
    /// the original or the copy affects the other at any depth.
    /// Detects cycles and returns an empty object if one is found.
    IomObject deepCopy() const;

    /// Number of shared owners of the internal state.
    /// For testing and debugging only.
    long useCount() const noexcept;

    // --- Comparison ---

    bool operator==(const IomObject& o) const;
    bool operator!=(const IomObject& o) const { return !(*this == o); }

private:
    std::shared_ptr<detail::IomObjectImpl> impl_;
};

} // namespace iox
