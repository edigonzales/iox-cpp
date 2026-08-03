#include "iox/Events.h"
#include "iox/Writer.h"
#include "iox/xtf/XtfReader.h"
#include "iox/xtf/XtfWriter.h"
#include "iox/test/Test.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {

iox::StartTransferEvent transfer() {
    iox::StartTransferEvent result;
    result.header.version = iox::XtfVersion::V23;
    result.header.sender = "TestSender";
    return result;
}

iox::StartBasketEvent basket(std::string id,
                             iox::Consistency consistency =
                                 iox::Consistency::Unspecified) {
    iox::StartBasketEvent result;
    result.basket.topic = iox::IomName("TestModel.TopicA.DataBasket");
    result.basket.basketId = std::move(id);
    result.basket.consistency = consistency;
    result.basket.kind = iox::BasketKind::Full;
    return result;
}

iox::ObjectEvent object(std::string id, std::string value) {
    iox::ObjectEvent result;
    result.object = iox::IomObject(
        iox::IomName("TestModel.TopicA.TestClass"), std::move(id));
    result.object.setOperation(iox::ObjectOperation::Insert);
    result.object.setPrimitive(iox::IomName("Name"), std::move(value));
    return result;
}

std::string write(const std::vector<iox::IoxEvent>& events) {
    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::xtf::XtfWriterOptions options;
    options.version = iox::XtfVersion::V23;
    options.pretty = false;
    iox::xtf::XtfWriter writer(sink, options);
    for (const auto& event : events) writer.write(event);
    writer.close();
    return sink->str();
}

std::vector<iox::IoxEvent> read(const std::string& input,
                                std::size_t chunkSize = 0) {
    iox::xtf::XtfReader reader;
    if (chunkSize == 0) {
        reader.feed(iox::ByteView(input));
    } else {
        for (std::size_t offset = 0; offset < input.size(); offset += chunkSize) {
            const auto count = std::min(chunkSize, input.size() - offset);
            reader.feed(iox::ByteView(
                reinterpret_cast<const std::uint8_t*>(input.data() + offset),
                count));
        }
    }
    reader.finish();
    std::vector<iox::IoxEvent> result;
    while (true) {
        auto outcome = reader.next();
        if (outcome.progress == iox::ReaderProgress::End) break;
        IOX_CHECK_EQ(iox::ReaderProgress::Event, outcome.progress);
        result.push_back(std::move(*outcome.event));
    }
    return result;
}

std::vector<iox::IoxEvent> oneObjectEvents(iox::ObjectEvent value) {
    return {transfer(), basket("BID001"), std::move(value),
            iox::EndBasketEvent{}, iox::EndTransferEvent{}};
}

} // namespace

IOX_TEST(xtf23_simple_object_roundtrip) {
    auto value = object("TID001", "test-value");
    value.object.setPrimitive(iox::IomName("Count"), "00042");
    const auto parsed = read(write(oneObjectEvents(value)));
    IOX_CHECK_EQ(static_cast<std::size_t>(5), parsed.size());
    const auto& result = std::get<iox::ObjectEvent>(parsed[2]).object;
    IOX_CHECK_EQ(std::string("TID001"), *result.oid());
    IOX_CHECK_EQ(iox::ObjectOperation::Insert, result.operation());
    IOX_CHECK_EQ(std::string_view("test-value"), *result.primitive("Name"));
    IOX_CHECK_EQ(std::string_view("00042"), *result.primitive("Count"));
}

IOX_TEST(xtf23_multiple_objects) {
    std::vector<iox::IoxEvent> events{transfer(), basket("B1")};
    for (int i = 0; i < 10; ++i) {
        events.push_back(object("TID" + std::to_string(i),
                                std::to_string(i)));
    }
    events.push_back(iox::EndBasketEvent{});
    events.push_back(iox::EndTransferEvent{});
    const auto parsed = read(write(events));
    IOX_CHECK_EQ(static_cast<std::size_t>(14), parsed.size());
    for (int i = 0; i < 10; ++i) {
        const auto& value = std::get<iox::ObjectEvent>(parsed[2U + i]).object;
        IOX_CHECK_EQ("TID" + std::to_string(i), *value.oid());
    }
}

IOX_TEST(xtf23_object_with_structure_attribute) {
    auto value = object("TID1", "Test");
    iox::IomObject address(iox::IomName("TestModel.TopicA.Address"));
    address.setPrimitive(iox::IomName("Street"), "Main St");
    address.setPrimitive(iox::IomName("Number"), "10");
    value.object.setObject(iox::IomName("Address"), address);

    const auto parsed = read(write(oneObjectEvents(value)));
    const auto result =
        std::get<iox::ObjectEvent>(parsed[2]).object.object("Address");
    IOX_CHECK(result.has_value());
    IOX_CHECK_EQ(std::string_view("Main St"), *result->primitive("Street"));
    IOX_CHECK_EQ(std::string_view("10"), *result->primitive("Number"));
}

IOX_TEST(xtf23_basket_with_consistency) {
    const auto parsed = read(write(
        {transfer(), basket("B_INCOMPLETE", iox::Consistency::Incomplete),
         iox::EndBasketEvent{}, iox::EndTransferEvent{}}));
    const auto& metadata = std::get<iox::StartBasketEvent>(parsed[1]).basket;
    IOX_CHECK_EQ(std::string("B_INCOMPLETE"), metadata.basketId);
    IOX_CHECK_EQ(iox::Consistency::Incomplete, metadata.consistency);
}

IOX_TEST(xtf23_multiple_baskets) {
    const auto parsed = read(write(
        {transfer(), basket("B1"), iox::EndBasketEvent{}, basket("B2"),
         iox::EndBasketEvent{}, iox::EndTransferEvent{}}));
    IOX_CHECK_EQ(static_cast<std::size_t>(6), parsed.size());
    IOX_CHECK_EQ(std::string("B1"),
                 std::get<iox::StartBasketEvent>(parsed[1]).basket.basketId);
    IOX_CHECK_EQ(std::string("B2"),
                 std::get<iox::StartBasketEvent>(parsed[3]).basket.basketId);
}

IOX_TEST(xtf23_chunked_read) {
    const auto xml = write(oneObjectEvents(object("TID1", "chunk-test")));
    const auto whole = read(xml);
    const auto oneByte = read(xml, 1);
    const auto sevenBytes = read(xml, 7);
    IOX_CHECK_EQ(whole.size(), oneByte.size());
    IOX_CHECK_EQ(whole.size(), sevenBytes.size());
    IOX_CHECK_EQ(std::string_view("chunk-test"),
        *std::get<iox::ObjectEvent>(oneByte[2]).object.primitive("Name"));
}

#include "iox/test/TestMain.h"
