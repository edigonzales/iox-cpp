#include "iox/IomPath.h"

#include <functional>
#include <limits>
#include <utility>

namespace iox {
namespace {

[[noreturn]] void invalidPath(std::string_view expression,
                              std::string_view detail) {
    throw IoxError(DiagnosticCode::InvalidArgument,
                   "Invalid IOM path '" + std::string(expression) + "': " +
                       std::string(detail));
}

bool isIdentifierStart(char character) {
    return (character >= 'A' && character <= 'Z') ||
           (character >= 'a' && character <= 'z') || character == '_';
}

bool isIdentifierCharacter(char character) {
    return isIdentifierStart(character) ||
           (character >= '0' && character <= '9');
}

std::vector<std::size_t> selectedIndexes(const IomObject& object,
                                         const IomPathStep& step) {
    if (!object.hasAttribute(step.attribute)) {
        throw IoxError(DiagnosticCode::UnknownInterlisName,
                       "IOM path attribute does not exist: " +
                           step.attribute);
    }

    const auto count = object.valueCount(step.attribute);
    switch (step.selector) {
    case IomPathSelectorKind::First:
        return count == 0 ? std::vector<std::size_t>{}
                          : std::vector<std::size_t>{0};
    case IomPathSelectorKind::Index:
        if (step.index == 0 || step.index > count) {
            throw IoxError(DiagnosticCode::InvalidArgument,
                           "IOM path value index is out of range");
        }
        return {step.index - 1};
    case IomPathSelectorKind::All: {
        std::vector<std::size_t> result;
        result.reserve(count);
        for (std::size_t index = 0; index < count; ++index) {
            result.push_back(index);
        }
        return result;
    }
    }
    throw IoxError(DiagnosticCode::InternalError,
                   "Unknown IOM path selector");
}

struct TraversalState final {
    IomObject object;
    std::vector<std::size_t> valueIndexes;
};

} // namespace

IomPath IomPath::parse(std::string_view expression) {
    if (expression.empty()) invalidPath(expression, "path is empty");

    IomPath result;
    result.expression_ = std::string(expression);
    std::size_t offset = 0;
    while (offset < expression.size()) {
        if (!isIdentifierStart(expression[offset])) {
            invalidPath(expression, "expected an attribute identifier");
        }
        const auto identifierStart = offset++;
        while (offset < expression.size() &&
               isIdentifierCharacter(expression[offset])) {
            ++offset;
        }

        IomPathStep step;
        step.attribute = std::string(
            expression.substr(identifierStart, offset - identifierStart));
        if (offset < expression.size() && expression[offset] == '[') {
            ++offset;
            if (offset >= expression.size()) {
                invalidPath(expression, "unterminated selector");
            }
            if (expression[offset] == '*') {
                ++offset;
                if (offset >= expression.size() || expression[offset] != ']') {
                    invalidPath(expression, "wildcard selector must end with ]");
                }
                ++offset;
                step.selector = IomPathSelectorKind::All;
            } else {
                const auto digitsStart = offset;
                std::size_t value = 0;
                while (offset < expression.size() &&
                       expression[offset] >= '0' &&
                       expression[offset] <= '9') {
                    const auto digit = static_cast<std::size_t>(
                        expression[offset] - '0');
                    if (value > (std::numeric_limits<std::size_t>::max() -
                                 digit) /
                                    10) {
                        invalidPath(expression, "selector index is too large");
                    }
                    value = value * 10 + digit;
                    ++offset;
                }
                if (digitsStart == offset || offset >= expression.size() ||
                    expression[offset] != ']' || value == 0) {
                    invalidPath(expression,
                                "selector index must be a positive integer");
                }
                ++offset;
                step.selector = IomPathSelectorKind::Index;
                step.index = value;
            }
        }
        result.steps_.push_back(std::move(step));

        if (offset == expression.size()) break;
        if (expression[offset] != '.') {
            invalidPath(expression, "expected . between path steps");
        }
        ++offset;
        if (offset == expression.size()) {
            invalidPath(expression, "path cannot end with .");
        }
    }
    return result;
}

const std::string& IomPath::expression() const noexcept { return expression_; }

const std::vector<IomPathStep>& IomPath::steps() const noexcept {
    return steps_;
}

bool IomPath::containsWildcard() const noexcept {
    for (const auto& step : steps_) {
        if (step.selector == IomPathSelectorKind::All) return true;
    }
    return false;
}

std::vector<IomPathMatch> IomPath::primitiveMatches(
    const IomObject& object) const {
    std::vector<TraversalState> states;
    states.push_back({object, {}});
    for (std::size_t stepIndex = 0; stepIndex < steps_.size(); ++stepIndex) {
        const auto& step = steps_[stepIndex];
        const bool terminal = stepIndex + 1 == steps_.size();
        std::vector<TraversalState> next;
        for (const auto& state : states) {
            for (const auto valueIndex : selectedIndexes(state.object, step)) {
                auto indexes = state.valueIndexes;
                indexes.push_back(valueIndex);
                if (terminal) {
                    const auto& value = state.object.value(step.attribute,
                                                           valueIndex);
                    if (!value.isPrimitive()) {
                        throw IoxError(
                            DiagnosticCode::InvalidState,
                            "IOM path target is not primitive: " +
                                step.attribute);
                    }
                    next.push_back({state.object, std::move(indexes)});
                    continue;
                }
                const auto child = state.object.object(step.attribute,
                                                        valueIndex);
                if (!child) {
                    throw IoxError(
                        DiagnosticCode::InvalidState,
                        "IOM path step does not contain a structure: " +
                            step.attribute);
                }
                next.push_back({*child, std::move(indexes)});
            }
        }
        states = std::move(next);
    }

    std::vector<IomPathMatch> result;
    result.reserve(states.size());
    for (const auto& state : states) {
        const auto& lastStep = steps_.back();
        const auto valueIndex = state.valueIndexes.back();
        const auto value = state.object.primitive(lastStep.attribute, valueIndex);
        if (!value) {
            throw IoxError(DiagnosticCode::InvalidState,
                           "IOM path target is not primitive: " +
                               lastStep.attribute);
        }
        result.push_back({state.valueIndexes, std::string(*value)});
    }
    return result;
}

std::string IomPath::replaceSinglePrimitive(
    IomObject& object, std::string newValue,
    std::optional<std::string_view> expected) const {
    if (containsWildcard()) {
        throw IoxError(DiagnosticCode::InvalidArgument,
                       "IOM path wildcard cannot be used for replacement");
    }
    const auto matches = primitiveMatches(object);
    if (matches.size() != 1) {
        throw IoxError(DiagnosticCode::InvalidState,
                       "IOM path replacement requires exactly one match");
    }
    const auto& match = matches.front();
    if (expected && *expected != match.value) {
        throw IoxError(DiagnosticCode::ModelMismatch,
                       "IOM path expected value does not match");
    }

    std::function<void(IomObject&, std::size_t)> replace =
        [&](IomObject& current, std::size_t stepIndex) {
            const auto& step = steps_[stepIndex];
            const auto valueIndex = match.valueIndexes[stepIndex];
            if (stepIndex + 1 == steps_.size()) {
                current.replaceValue(step.attribute, valueIndex,
                                     IomValue::primitive(std::move(newValue)));
                return;
            }
            auto child = current.object(step.attribute, valueIndex);
            if (!child) {
                throw IoxError(
                    DiagnosticCode::InvalidState,
                    "IOM path step does not contain a structure: " +
                        step.attribute);
            }
            replace(*child, stepIndex + 1);
            current.replaceValue(step.attribute, valueIndex,
                                 IomValue::object(std::move(*child)));
        };
    replace(object, 0);
    return match.value;
}

} // namespace iox
