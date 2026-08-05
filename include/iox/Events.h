#pragma once

#include "iox/Diagnostic.h"
#include "iox/IomName.h"
#include "iox/IomObject.h"

#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace iox {

enum class XtfVersion { V23, V24 };

inline const char* xtfVersionName(XtfVersion version) noexcept {
    return version == XtfVersion::V23 ? "2.3" : "2.4";
}

struct ModelEntry final {
    std::string name;
    std::optional<std::string> version;
    std::optional<std::string> uri;
    XmlQualifiedName xmlNamespace;
};

struct OidSpace final {
    std::string name;
    std::string domain;
};

struct ExtensionAttribute final {
    XmlQualifiedName name;
    std::string value;
};

struct ExtensionElement final {
    XmlQualifiedName name;
    std::vector<ExtensionAttribute> attributes;
    std::string text;
    std::vector<ExtensionElement> children;
};

struct TransferHeader final {
    XtfVersion version = XtfVersion::V23;
    std::string sender;
    std::optional<std::string> comment;
    std::vector<ModelEntry> models;
    std::vector<OidSpace> oidSpaces;
    std::vector<ExtensionElement> extensions;
};

enum class BasketKind { Full, Update, Initial, Unspecified };

struct BasketMetadata final {
    IomName topic;
    std::string basketId;
    BasketKind kind = BasketKind::Unspecified;
    Consistency consistency = Consistency::Unspecified;
    std::optional<std::string> startState;
    std::optional<std::string> endState;
    std::vector<std::string> domains;
    std::vector<std::string> topics;
    std::vector<ExtensionElement> extensions;
    SourceLocation location;
};

struct StartTransferEvent final { TransferHeader header; };
struct StartBasketEvent final { BasketMetadata basket; };
struct ObjectEvent final { IomObject object; };
struct EndBasketEvent final {};
struct EndTransferEvent final {};

using IoxEvent = std::variant<StartTransferEvent, StartBasketEvent,
                              ObjectEvent, EndBasketEvent, EndTransferEvent>;

enum class EventKind { StartTransfer, StartBasket, Object, EndBasket, EndTransfer };

inline EventKind eventKind(const IoxEvent& event) noexcept {
    return std::visit([](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, StartTransferEvent>) {
            return EventKind::StartTransfer;
        } else if constexpr (std::is_same_v<T, StartBasketEvent>) {
            return EventKind::StartBasket;
        } else if constexpr (std::is_same_v<T, ObjectEvent>) {
            return EventKind::Object;
        } else if constexpr (std::is_same_v<T, EndBasketEvent>) {
            return EventKind::EndBasket;
        } else {
            return EventKind::EndTransfer;
        }
    }, event);
}

inline const char* eventKindName(EventKind kind) noexcept {
    switch (kind) {
    case EventKind::StartTransfer: return "startTransfer";
    case EventKind::StartBasket: return "startBasket";
    case EventKind::Object: return "object";
    case EventKind::EndBasket: return "endBasket";
    case EventKind::EndTransfer: return "endTransfer";
    }
    return "unknown";
}

} // namespace iox
