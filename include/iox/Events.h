#pragma once

#include "iox/IomObject.h"
#include "iox/IomName.h"
#include "iox/Diagnostic.h"

#include <string>
#include <vector>
#include <variant>
#include <optional>

namespace iox {

// ============================================================================
// Event Types
// ============================================================================

/// Signals the start of an INTERLIS transfer.
struct StartTransferEvent final {
    std::string sender;         // XTF header: SENDER
    std::string comment;        // XTF header: COMMENT
    std::string iliVersion;     // INTERLIS version string
    std::string software;       // generating software
    std::string date;           // transfer date
    std::optional<int> version; // XTF version (23 or 24), nullopt until detected
};

/// Signals the start of a basket.
struct StartBasketEvent final {
    IomName basketType;              // e.g. "MODEL.TOPIC.Data"
    std::string bid;                 // basket ID (BID)
    std::optional<int> oidDomain;    // OID domain
    std::string consistency;         // "complete", "incomplete", etc.
    std::string operation;           // "insert", "update", "delete"
    std::vector<std::string> domains; // user-defined domains
    std::optional<std::string> startState;
    std::optional<std::string> endState;
    std::optional<std::string> kind; // XTF 2.4: Kind
};

/// A data object within a basket.
struct ObjectEvent final {
    IomObject object;                // the IOM object (class tag, attributes, values)

    std::string operation;           // "insert", "update", "delete"
    std::optional<std::string> consistency;

    // Object identity
    std::string objectId;            // TID
    std::optional<std::string> refBid;      // REF BID
    std::optional<std::string> refOrderPos; // REF ORDER_POS
};

/// Signals the end of a basket.
struct EndBasketEvent final {
    std::string bid;  // basket ID being closed
};

/// Signals the end of an INTERLIS transfer.
struct EndTransferEvent final {
    // Currently no additional fields.
};

// ============================================================================
// IoxEvent — the normative ordered event variant
// ============================================================================

using IoxEvent = std::variant<
    StartTransferEvent,
    StartBasketEvent,
    ObjectEvent,
    EndBasketEvent,
    EndTransferEvent
>;

// ============================================================================
// Event type helpers
// ============================================================================

/// Return a human-readable event type name for debugging.
inline const char* eventTypeName(const IoxEvent& event) {
    return std::visit([](const auto& e) -> const char* {
        using T = std::decay_t<decltype(e)>;
        if constexpr (std::is_same_v<T, StartTransferEvent>) return "StartTransfer";
        else if constexpr (std::is_same_v<T, StartBasketEvent>)  return "StartBasket";
        else if constexpr (std::is_same_v<T, ObjectEvent>)        return "Object";
        else if constexpr (std::is_same_v<T, EndBasketEvent>)     return "EndBasket";
        else if constexpr (std::is_same_v<T, EndTransferEvent>)   return "EndTransfer";
    }, event);
}

} // namespace iox
