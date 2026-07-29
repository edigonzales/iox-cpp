#pragma once

#include "iox/Events.h"
#include "iox/Reader.h"

#include <vector>
#include <memory>
#include <optional>

namespace iox {

/// Convenience: read all events from a reader until End.
/// The reader must already have all input fed and finished.
inline std::vector<IoxEvent> readAll(Reader& reader) {
    std::vector<IoxEvent> events;
    while (true) {
        auto outcome = reader.next();
        for (auto& d : outcome.diagnostics) {
            // diagnostics are accumulated; caller can use
            // reader.takeDiagnostics() separately
            (void)d;
        }
        if (outcome.status == ReadOutcome::Status::End) break;
        if (outcome.status == ReadOutcome::Status::NeedInput) break;
        if (outcome.event) {
            events.push_back(std::move(*outcome.event));
        }
    }
    return events;
}

/// Convenience: read events belonging to a single basket.
/// The reader is advanced until EndBasketEvent for the current basket.
/// Events before the first StartBasketEvent are skipped.
/// Events after EndBasketEvent remain in the reader.
inline std::vector<IoxEvent> readBasket(Reader& reader) {
    std::vector<IoxEvent> basket;
    int depth = 0;
    while (true) {
        auto outcome = reader.next();
        if (outcome.status == ReadOutcome::Status::End) break;
        if (outcome.status == ReadOutcome::Status::NeedInput) break;
        if (!outcome.event) continue;

        auto& event = *outcome.event;
        if (std::holds_alternative<StartBasketEvent>(event)) {
            ++depth;
            if (depth == 1) {
                basket.push_back(std::move(event));
                continue;
            }
        }
        if (std::holds_alternative<EndBasketEvent>(event)) {
            if (depth == 1) {
                basket.push_back(std::move(event));
                break;
            }
            --depth;
        }
        if (depth > 0) {
            basket.push_back(std::move(event));
        }
    }
    return basket;
}

/// Metadata and objects from one basket.
///
/// The event stream remains the lossless API. This value intentionally offers
/// the common object-only view for applications that process one basket at a
/// time; object identity and operation metadata remain available through the
/// event stream or `BasketReader::readEvents()`.
struct BasketMetadata final {
    IomName basketType;
    std::string bid;
    std::optional<int> oidDomain;
    std::string consistency;
    std::string operation;
    std::vector<std::string> domains;
    std::optional<std::string> startState;
    std::optional<std::string> endState;
    std::optional<std::string> kind;
};

struct Basket final {
    BasketMetadata metadata;
    std::vector<IomObject> objects;
};

/// Owns a reader and exposes one complete basket at a time.
///
/// `maxObjectsPerBasket == 0` disables the convenience-layer limit. The
/// reader itself remains the source of truth and is not modified by this
/// facade beyond consuming events.
class BasketReader final {
public:
    explicit BasketReader(std::unique_ptr<Reader> reader,
                          std::size_t maxObjectsPerBasket = 0);
    ~BasketReader();

    BasketReader(const BasketReader&) = delete;
    BasketReader& operator=(const BasketReader&) = delete;

    /// Consume events through StartTransferEvent if necessary.
    /// Returns an empty optional until a transfer header is available or when
    /// the input ended before a valid header was found.
    const std::optional<StartTransferEvent>& header();

    /// Consume exactly one complete basket, or nullopt at transfer end/error.
    std::optional<Basket> readBasket();

    /// Return diagnostics accumulated by the facade and its owned reader.
    std::vector<Diagnostic> takeDiagnostics();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace iox
