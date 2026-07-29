#include "iox/Basket.h"

#include <iterator>
#include <utility>

namespace iox {

namespace {

BasketMetadata basketMetadata(const StartBasketEvent& event) {
    BasketMetadata result;
    result.basketType = event.basketType;
    result.bid = event.bid;
    result.oidDomain = event.oidDomain;
    result.consistency = event.consistency;
    result.operation = event.operation;
    result.domains = event.domains;
    result.startState = event.startState;
    result.endState = event.endState;
    result.kind = event.kind;
    return result;
}

} // namespace

struct BasketReader::Impl {
    std::unique_ptr<Reader> reader;
    std::size_t maxObjects = 0;
    std::optional<StartTransferEvent> transferHeader;
    std::optional<IoxEvent> pending;
    std::vector<Diagnostic> diagnostics;
    bool headerResolved = false;
    bool failed = false;

    explicit Impl(std::unique_ptr<Reader> source, std::size_t limit)
        : reader(std::move(source)), maxObjects(limit) {}

    void addFatal(const char* code, std::string message) {
        diagnostics.push_back({Diagnostic::Severity::Fatal, code,
                               std::move(message), std::nullopt});
        failed = true;
    }

    std::optional<IoxEvent> nextEvent() {
        if (failed || !reader) return std::nullopt;
        auto outcome = reader->next();
        for (auto& diagnostic : outcome.diagnostics) {
            diagnostics.push_back(std::move(diagnostic));
        }
        if (outcome.status == ReadOutcome::Status::NeedInput) {
            addFatal(ErrorCode::BasketStateViolation,
                     "BasketReader requires a finished reader");
            return std::nullopt;
        }
        if (outcome.status == ReadOutcome::Status::End || !outcome.event) {
            return std::nullopt;
        }
        return std::move(outcome.event);
    }

    void resolveHeader() {
        if (headerResolved || failed) return;
        headerResolved = true;
        while (true) {
            auto event = nextEvent();
            if (!event) {
                if (!failed && !transferHeader) {
                    addFatal(ErrorCode::BasketStateViolation,
                             "Transfer header is missing");
                }
                return;
            }
            if (auto* start = std::get_if<StartTransferEvent>(&*event)) {
                transferHeader = *start;
                return;
            }
            if (std::holds_alternative<StartBasketEvent>(*event)) {
                pending = std::move(*event);
                addFatal(ErrorCode::BasketStateViolation,
                         "StartBasketEvent appeared before StartTransferEvent");
                return;
            }
            addFatal(ErrorCode::BasketStateViolation,
                     "Unexpected event before StartTransferEvent");
            return;
        }
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

    std::optional<IoxEvent> event;
    if (impl_->pending) {
        event = std::move(impl_->pending);
        impl_->pending.reset();
    } else {
        event = impl_->nextEvent();
    }
    if (!event) return std::nullopt;

    auto* start = std::get_if<StartBasketEvent>(&*event);
    if (!start) {
        if (std::holds_alternative<EndTransferEvent>(*event)) return std::nullopt;
        impl_->addFatal(ErrorCode::BasketStateViolation,
                        "Expected StartBasketEvent or EndTransferEvent");
        return std::nullopt;
    }

    Basket result;
    result.metadata = basketMetadata(*start);
    while (true) {
        event = impl_->nextEvent();
        if (!event) {
            if (!impl_->failed) {
                impl_->addFatal(ErrorCode::BasketStateViolation,
                                "Transfer ended before EndBasketEvent");
            }
            return std::nullopt;
        }

        if (auto* object = std::get_if<ObjectEvent>(&*event)) {
            if (impl_->maxObjects != 0 &&
                result.objects.size() >= impl_->maxObjects) {
                impl_->addFatal(
                    ErrorCode::BasketLimitExceeded,
                    "Basket object limit exceeded (limit=" +
                        std::to_string(impl_->maxObjects) + ")");
                return std::nullopt;
            }
            result.objects.push_back(object->object);
            continue;
        }
        if (const auto* end = std::get_if<EndBasketEvent>(&*event)) {
            if (!end->bid.empty() && !result.metadata.bid.empty() &&
                end->bid != result.metadata.bid) {
                impl_->addFatal(ErrorCode::BasketStateViolation,
                                "EndBasketEvent BID does not match basket BID");
                return std::nullopt;
            }
            return result;
        }
        impl_->addFatal(ErrorCode::BasketStateViolation,
                        "Unexpected event inside basket");
        return std::nullopt;
    }
}

std::vector<Diagnostic> BasketReader::takeDiagnostics() {
    auto result = std::move(impl_->diagnostics);
    if (impl_->reader) {
        auto readerDiagnostics = impl_->reader->takeDiagnostics();
        result.insert(result.end(),
                      std::make_move_iterator(readerDiagnostics.begin()),
                      std::make_move_iterator(readerDiagnostics.end()));
    }
    return result;
}

} // namespace iox
