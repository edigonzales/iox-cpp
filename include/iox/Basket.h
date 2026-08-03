#pragma once

#include "iox/Events.h"
#include "iox/Reader.h"

#include <memory>
#include <optional>
#include <vector>

namespace iox {

inline std::vector<IoxEvent> readAll(Reader& reader) {
    std::vector<IoxEvent> events;
    while (true) {
        auto outcome = reader.next();
        if (outcome.progress != ReaderProgress::Event) break;
        events.push_back(std::move(*outcome.event));
    }
    return events;
}

inline std::vector<IoxEvent> readBasket(Reader& reader) {
    std::vector<IoxEvent> basket;
    bool inBasket = false;
    while (true) {
        auto outcome = reader.next();
        if (outcome.progress != ReaderProgress::Event) break;
        auto event = std::move(*outcome.event);
        if (std::holds_alternative<StartBasketEvent>(event)) inBasket = true;
        const bool ended = std::holds_alternative<EndBasketEvent>(event);
        if (inBasket) basket.push_back(std::move(event));
        if (ended) break;
    }
    return basket;
}

struct Basket final {
    BasketMetadata metadata;
    std::vector<IomObject> objects;
};

class BasketReader final {
public:
    explicit BasketReader(std::unique_ptr<Reader> reader,
                          std::size_t maxObjectsPerBasket = 0);
    ~BasketReader();

    BasketReader(const BasketReader&) = delete;
    BasketReader& operator=(const BasketReader&) = delete;

    const std::optional<StartTransferEvent>& header();
    std::optional<Basket> readBasket();
    std::vector<Diagnostic> takeDiagnostics();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace iox
