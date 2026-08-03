#include "iox/Basket.h"

#include <iterator>
#include <utility>

namespace iox {

struct BasketReader::Impl {
    std::unique_ptr<Reader> reader;
    std::size_t maxObjects = 0;
    std::optional<StartTransferEvent> transferHeader;
    std::vector<Diagnostic> diagnostics;
    bool headerResolved = false;
    bool failed = false;

    explicit Impl(std::unique_ptr<Reader> source, std::size_t limit)
        : reader(std::move(source)), maxObjects(limit) {}

    void addFatal(DiagnosticCode code, std::string message) {
        diagnostics.push_back({DiagnosticSeverity::Fatal, code,
                               std::move(message), {}, {}});
        failed = true;
    }

    std::optional<IoxEvent> nextEvent() {
        if (failed || !reader) return std::nullopt;
        auto outcome = reader->next();
        if (outcome.progress == ReaderProgress::NeedInput) {
            addFatal(DiagnosticCode::BasketStateViolation,
                     "BasketReader requires a finished reader");
            return std::nullopt;
        }
        if (outcome.progress == ReaderProgress::End || !outcome.event) {
            return std::nullopt;
        }
        return std::move(outcome.event);
    }

    void resolveHeader() {
        if (headerResolved || failed) return;
        headerResolved = true;
        auto event = nextEvent();
        if (!event || !std::holds_alternative<StartTransferEvent>(*event)) {
            addFatal(DiagnosticCode::BasketStateViolation,
                     "Transfer header is missing");
            return;
        }
        transferHeader = std::get<StartTransferEvent>(*event);
    }
};

BasketReader::BasketReader(std::unique_ptr<Reader> reader,
                           std::size_t maxObjectsPerBasket)
    : impl_(std::make_unique<Impl>(std::move(reader), maxObjectsPerBasket)) {}

BasketReader::~BasketReader() = default;

const std::optional<StartTransferEvent>& BasketReader::header() {
    impl_->resolveHeader();
    return impl_->transferHeader;
}

std::optional<Basket> BasketReader::readBasket() {
    impl_->resolveHeader();
    if (impl_->failed) return std::nullopt;

    auto event = impl_->nextEvent();
    if (!event || std::holds_alternative<EndTransferEvent>(*event)) return std::nullopt;
    auto* start = std::get_if<StartBasketEvent>(&*event);
    if (!start) {
        impl_->addFatal(DiagnosticCode::BasketStateViolation,
                        "Expected StartBasketEvent or EndTransferEvent");
        return std::nullopt;
    }

    Basket result;
    result.metadata = start->basket;
    while (true) {
        event = impl_->nextEvent();
        if (!event) {
            if (!impl_->failed) {
                impl_->addFatal(DiagnosticCode::BasketStateViolation,
                                "Transfer ended before EndBasketEvent");
            }
            return std::nullopt;
        }
        if (auto* object = std::get_if<ObjectEvent>(&*event)) {
            if (impl_->maxObjects != 0 && result.objects.size() >= impl_->maxObjects) {
                impl_->addFatal(DiagnosticCode::BasketLimitExceeded,
                                "Basket object limit exceeded");
                return std::nullopt;
            }
            result.objects.push_back(object->object);
            continue;
        }
        if (std::holds_alternative<EndBasketEvent>(*event)) return result;
        impl_->addFatal(DiagnosticCode::BasketStateViolation,
                        "Unexpected event inside basket");
        return std::nullopt;
    }
}

std::vector<Diagnostic> BasketReader::takeDiagnostics() {
    auto result = std::move(impl_->diagnostics);
    if (impl_->reader) {
        auto source = impl_->reader->takeDiagnostics();
        result.insert(result.end(),
                      std::make_move_iterator(source.begin()),
                      std::make_move_iterator(source.end()));
    }
    return result;
}

} // namespace iox
