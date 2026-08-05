#pragma once

#include "iox/IomObject.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace iox {

enum class IomPathSelectorKind {
    First,
    Index,
    All
};

struct IomPathStep final {
    std::string attribute;
    IomPathSelectorKind selector = IomPathSelectorKind::First;
    std::size_t index = 1;
};

struct IomPathMatch final {
    std::vector<std::size_t> valueIndexes;
    std::string value;
};

class IomPath final {
public:
    static IomPath parse(std::string_view expression);

    const std::string& expression() const noexcept;
    const std::vector<IomPathStep>& steps() const noexcept;
    bool containsWildcard() const noexcept;

    std::vector<IomPathMatch> primitiveMatches(
        const IomObject& object) const;

    std::string replaceSinglePrimitive(
        IomObject& object,
        std::string newValue,
        std::optional<std::string_view> expected = std::nullopt) const;

private:
    IomPath() = default;

    std::string expression_;
    std::vector<IomPathStep> steps_;
};

} // namespace iox
