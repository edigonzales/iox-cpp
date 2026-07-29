#include "iox/Basket.h"
#include "iox/json/JsonEventReader.h"
#include "iox/json/JsonEventWriter.h"
#include "iox/test/Test.h"

#include <memory>
#include <string>
#include <vector>

namespace {

std::string makeEvents(int objectCount, int basketCount = 1) {
    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::json::JsonEventWriter writer(sink);
    writer.write(iox::StartTransferEvent{});
    for (int basketIndex = 0; basketIndex < basketCount; ++basketIndex) {
        iox::StartBasketEvent basket;
        basket.basketType = iox::IomName("Model.Topic.Basket");
        basket.bid = "B" + std::to_string(basketIndex + 1);
        writer.write(basket);
        for (int objectIndex = 0; objectIndex < objectCount; ++objectIndex) {
            iox::ObjectEvent object;
            object.objectId = "T" + std::to_string(objectIndex + 1);
            object.object = iox::IomObject(iox::IomName("Model.Topic.Class"));
            object.object.setPrimitive("Name", iox::IomValue::text("value"));
            writer.write(object);
        }
        writer.write(iox::EndBasketEvent{"B" + std::to_string(basketIndex + 1)});
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

} // namespace

IOX_TEST(basket_reader_reads_header_and_basket) {
    iox::BasketReader reader(makeReader(makeEvents(2)), 0);
    const auto& header = reader.header();
    IOX_CHECK(header.has_value());

    auto basket = reader.readBasket();
    IOX_CHECK(basket.has_value());
    IOX_CHECK_EQ(std::string("B1"), basket->metadata.bid);
    IOX_CHECK_EQ(static_cast<std::size_t>(2), basket->objects.size());
    IOX_CHECK_EQ(std::string("value"),
                 basket->objects[0].getPrimitive("Name")->asText());

    IOX_CHECK(!reader.readBasket().has_value());
    IOX_CHECK(reader.takeDiagnostics().empty());
}

IOX_TEST(basket_reader_reads_multiple_baskets) {
    iox::BasketReader reader(makeReader(makeEvents(1, 2)));
    auto first = reader.readBasket();
    auto second = reader.readBasket();
    IOX_CHECK(first.has_value());
    IOX_CHECK(second.has_value());
    IOX_CHECK_EQ(std::string("B1"), first->metadata.bid);
    IOX_CHECK_EQ(std::string("B2"), second->metadata.bid);
}

IOX_TEST(basket_reader_reports_object_limit) {
    iox::BasketReader reader(makeReader(makeEvents(2)), 1);
    IOX_CHECK(!reader.readBasket().has_value());

    bool found = false;
    for (const auto& diagnostic : reader.takeDiagnostics()) {
        if (diagnostic.code == iox::ErrorCode::BasketLimitExceeded) found = true;
    }
    IOX_CHECK(found);
}

#include "iox/test/TestMain.h"
