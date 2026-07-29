#pragma once

#include "iox/Events.h"
#include "iox/Diagnostic.h"
#include "iox/ByteView.h"

#include <string>
#include <string_view>
#include <vector>
#include <utility>
#include <functional>
#include <memory>

namespace iox {
namespace xtf {

class Xtf24Dialect;

/// Callbacks from the XTF 2.4 dialect to the reader.
struct Xtf24Callbacks {
    /// Produce an event to the output queue.
    std::function<void(IoxEvent)> emitEvent;

    /// Add a diagnostic.
    std::function<void(Diagnostic)> addDiagnostic;
};

/// Parses XTF 2.4 content using SAX-like callbacks.
///
/// This dialect handles:
/// - Baskets (BASKET element with BID, CONSISTENCY, OPERATION)
/// - Objects (class elements with TID, OPERATION, REF)
/// - Primitive attributes
/// - Structured attributes (nested objects)
/// - Repeated values
/// - References (REF, BID, ORDER_POS)
class Xtf24Dialect final {
public:
    explicit Xtf24Dialect(Xtf24Callbacks callbacks);
    ~Xtf24Dialect();

    // Called by the XML parser
    void onStartElement(std::string_view name,
                        const std::vector<std::pair<std::string_view, std::string_view>>& attrs);
    void onEndElement(std::string_view name);
    void onCharacterData(std::string_view data);

    /// Reset internal state (for reuse).
    void reset();

    /// Whether the dialect is in a fatal error state.
    bool isFatal() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace xtf
} // namespace iox
