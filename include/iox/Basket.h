#pragma once

#include "iox/Events.h"
#include "iox/Reader.h"

#include <vector>
#include <memory>

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

} // namespace iox
