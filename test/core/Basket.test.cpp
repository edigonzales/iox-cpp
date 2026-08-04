#include "iox/Basket.h"
#include "iox/json/JsonEventReader.h"
#include "iox/json/JsonEventWriter.h"
#include "iox/test/Test.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

std::string makeEvents(int objectCount, int basketCount = 1) {
    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::json::JsonEventWriter writer(sink);
    iox::StartTransferEvent transfer;
    transfer.header.sender = "basket-test";
    writer.write(transfer);
    for (int basketIndex = 0; basketIndex < basketCount; ++basketIndex) {
        iox::StartBasketEvent basket;
        basket.basket.topic = iox::IomName("Model.Topic");
        basket.basket.basketId = "B" + std::to_string(basketIndex + 1);
        writer.write(basket);
        for (int objectIndex = 0; objectIndex < objectCount; ++objectIndex) {
            iox::ObjectEvent object;
            object.object = iox::IomObject(
                iox::IomName("Model.Topic.Class"),
                "T" + std::to_string(objectIndex + 1));
            object.object.setPrimitive(iox::IomName("Name"), "value");
            writer.write(object);
        }
        writer.write(iox::EndBasketEvent{});
    }
    writer.write(iox::EndTransferEvent{});
    writer.close();
    return sink->str();
}

std::unique_ptr<iox::Reader> makeReader(const std::string& data) {
    auto reader = std::make_unique<iox::json::JsonEventReader>();
    reader->feed(iox::ByteView(data));
    reader->finish();
    return reader;
}

class OutcomeReader final : public iox::Reader {
public:
    explicit OutcomeReader(std::vector<iox::ReadOutcome> outcomes)
        : outcomes_(std::move(outcomes)) {}
    iox::ReadOutcome next() override {
        if (position_ == outcomes_.size()) {
            return {iox::ReaderProgress::End, std::nullopt};
        }
        return std::move(outcomes_[position_++]);
    }
    void feed(iox::ByteView) override {}
    void finish() override {}
    bool isFinished() const noexcept override { return true; }
    std::vector<iox::Diagnostic> takeDiagnostics() override {
        return {{iox::DiagnosticSeverity::Warning,
                 iox::DiagnosticCode::UnexpectedElement,
                 "source diagnostic", {}, {}}};
    }

private:
    std::vector<iox::ReadOutcome> outcomes_;
    std::size_t position_ = 0;
};

iox::ReadOutcome outcome(iox::IoxEvent event) {
    return {iox::ReaderProgress::Event, std::move(event)};
}

} // namespace

IOX_TEST(basket_reader_reads_header_and_basket) {
    iox::BasketReader reader(makeReader(makeEvents(2)), 0);
    IOX_CHECK(reader.header().has_value());
    IOX_CHECK_EQ(std::string("basket-test"), reader.header()->header.sender);

    const auto basket = reader.readBasket();
    IOX_CHECK(basket.has_value());
    IOX_CHECK_EQ(std::string("B1"), basket->metadata.basketId);
    IOX_CHECK_EQ(static_cast<std::size_t>(2), basket->objects.size());
    IOX_CHECK_EQ(std::string_view("value"),
                 *basket->objects[0].primitive("Name"));
    IOX_CHECK(!reader.readBasket().has_value());
    IOX_CHECK(reader.takeDiagnostics().empty());
}

IOX_TEST(basket_reader_reads_multiple_baskets) {
    iox::BasketReader reader(makeReader(makeEvents(1, 2)));
    const auto first = reader.readBasket();
    const auto second = reader.readBasket();
    IOX_CHECK(first.has_value());
    IOX_CHECK(second.has_value());
    IOX_CHECK_EQ(std::string("B1"), first->metadata.basketId);
    IOX_CHECK_EQ(std::string("B2"), second->metadata.basketId);
}

IOX_TEST(basket_reader_reports_object_limit) {
    iox::BasketReader reader(makeReader(makeEvents(2)), 1);
    IOX_CHECK(!reader.readBasket().has_value());
    bool found = false;
    for (const auto& diagnostic : reader.takeDiagnostics()) {
        if (diagnostic.code == iox::DiagnosticCode::BasketLimitExceeded) {
            found = true;
        }
    }
    IOX_CHECK(found);
}

IOX_TEST(basket_reader_reports_incomplete_and_invalid_event_sequences) {
    auto needInput = std::make_unique<OutcomeReader>(
        std::vector<iox::ReadOutcome>{{iox::ReaderProgress::NeedInput,
                                       std::nullopt}});
    iox::BasketReader unfinished(std::move(needInput));
    IOX_CHECK(!unfinished.header().has_value());
    IOX_CHECK(!unfinished.takeDiagnostics().empty());

    iox::ObjectEvent object;
    object.object = iox::IomObject(iox::IomName("M.T.C"), "o1");
    iox::BasketReader wrongStart(std::make_unique<OutcomeReader>(
        std::vector<iox::ReadOutcome>{outcome(iox::StartTransferEvent{}),
                                      outcome(object)}));
    IOX_CHECK(!wrongStart.readBasket().has_value());

    iox::StartBasketEvent basket;
    basket.basket.topic = iox::IomName("M.T");
    basket.basket.basketId = "b1";
    iox::BasketReader interrupted(std::make_unique<OutcomeReader>(
        std::vector<iox::ReadOutcome>{outcome(iox::StartTransferEvent{}),
                                      outcome(basket),
                                      outcome(iox::EndTransferEvent{})}));
    IOX_CHECK(!interrupted.readBasket().has_value());

    iox::BasketReader truncated(std::make_unique<OutcomeReader>(
        std::vector<iox::ReadOutcome>{outcome(iox::StartTransferEvent{}),
                                      outcome(basket)}));
    IOX_CHECK(!truncated.readBasket().has_value());
    IOX_CHECK(!truncated.takeDiagnostics().empty());
}

#include "iox/test/TestMain.h"
