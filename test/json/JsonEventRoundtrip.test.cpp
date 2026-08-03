#include "iox/Events.h"
#include "iox/Writer.h"
#include "iox/json/JsonEventReader.h"
#include "iox/json/JsonEventWriter.h"
#include "iox/test/Test.h"

#include <memory>
#include <string>
#include <vector>

namespace {

std::string writeEvents(const std::vector<iox::IoxEvent>& events) {
    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::json::JsonEventWriter writer(sink);
    for (const auto& event : events) writer.write(event);
    writer.close();
    return sink->str();
}

std::vector<iox::IoxEvent> readEvents(const std::string& input) {
    iox::json::JsonEventReader reader;
    reader.feed(iox::ByteView(input));
    reader.finish();
    std::vector<iox::IoxEvent> events;
    while (true) {
        auto outcome = reader.next();
        if (outcome.progress == iox::ReaderProgress::End) break;
        IOX_CHECK_EQ(iox::ReaderProgress::Event, outcome.progress);
        IOX_CHECK(outcome.event.has_value());
        events.push_back(std::move(*outcome.event));
    }
    return events;
}

std::vector<iox::IoxEvent> representativeEvents() {
    iox::StartTransferEvent start;
    start.header.version = iox::XtfVersion::V24;
    start.header.sender = u8"Prüfstelle";
    start.header.comment = "comment";
    start.header.models.push_back(
        {"Model", "2026-01-01", "https://example.test/model",
         {"urn:model", "Model", "m"}});
    start.header.oidSpaces.push_back({"oids", "UUIDOID"});
    start.header.extensions.push_back(
        {{"urn:ext", "flag", "e"},
         {{{"urn:ext", "name", "e"}, "value"}}, "text", {}});

    iox::StartBasketEvent basket;
    basket.basket.topic = iox::IomName(
        "Model.Topic", {"urn:model", "Topic", "m"});
    basket.basket.basketId = "basket-1";
    basket.basket.kind = iox::BasketKind::Update;
    basket.basket.consistency = iox::Consistency::Incomplete;
    basket.basket.startState = "s0";
    basket.basket.endState = "s1";
    basket.basket.domains = {"Model.Domain"};
    basket.basket.topics = {"Model.Topic"};
    basket.basket.location = {"input.xtf", 18U, 2U, 3U};

    iox::ObjectEvent object;
    object.object = iox::IomObject(
        iox::IomName("Model.Topic.Class",
                     {"urn:model", "Class", "m"}),
        "object-1");
    object.object.setOperation(iox::ObjectOperation::Update);
    object.object.setConsistency(iox::Consistency::Adapted);
    object.object.setReference({"target-1", "basket-2", 9U});
    object.object.setSourceLocation({"input.xtf", 99U, 8U, 7U});
    object.object.appendPrimitive(iox::IomName("number"), "001.2300");
    object.object.appendPrimitive(iox::IomName("number"), "-0");
    iox::IomObject child(iox::IomName("Model.Topic.Struct"));
    child.setPrimitive(iox::IomName("text"), u8"Grüezi 🌍");
    object.object.setObject(iox::IomName("structure"), child);

    return {start, basket, object, iox::EndBasketEvent{},
            iox::EndTransferEvent{}};
}

} // namespace

IOX_TEST(json_schema_two_roundtrip_is_lossless) {
    const auto events = representativeEvents();
    const auto output = writeEvents(events);
    IOX_CHECK(output.find("\"schema\":\"iox-event/2\"") !=
              std::string::npos);
    IOX_CHECK(output.find("\"type\"") == std::string::npos);
    IOX_CHECK(output.find("001.2300") != std::string::npos);

    const auto parsed = readEvents(output);
    IOX_CHECK_EQ(events.size(), parsed.size());
    const auto& header = std::get<iox::StartTransferEvent>(parsed[0]).header;
    IOX_CHECK_EQ(iox::XtfVersion::V24, header.version);
    IOX_CHECK_EQ(std::string(u8"Prüfstelle"), header.sender);
    IOX_CHECK_EQ(static_cast<std::size_t>(1), header.models.size());
    IOX_CHECK_EQ(std::string("urn:model"),
                 header.models[0].xmlNamespace.namespaceUri);

    const auto& basket = std::get<iox::StartBasketEvent>(parsed[1]).basket;
    IOX_CHECK_EQ(std::string("basket-1"), basket.basketId);
    IOX_CHECK_EQ(iox::BasketKind::Update, basket.kind);
    IOX_CHECK_EQ(std::uint64_t{18}, basket.location.byteOffset);

    const auto& object = std::get<iox::ObjectEvent>(parsed[2]).object;
    IOX_CHECK(object.semanticallyEquals(
        std::get<iox::ObjectEvent>(events[2]).object));
    IOX_CHECK_EQ(std::string_view("001.2300"),
                 *object.primitive("number", 0));
}

IOX_TEST(json_writer_is_deterministic) {
    const auto events = representativeEvents();
    IOX_CHECK_EQ(writeEvents(events), writeEvents(events));
}

IOX_TEST(json_rejects_old_schema_and_unknown_fields) {
    const std::vector<std::string> invalid{
        "{\"type\":\"StartTransfer\"}\n",
        "{\"schema\":\"iox-event/1\",\"event\":\"endTransfer\"}\n",
        "{\"schema\":\"iox-event/2\",\"event\":\"endTransfer\",\"extra\":1}\n"};
    for (const auto& input : invalid) {
        iox::json::JsonEventReader reader;
        bool threw = false;
        try {
            reader.feed(iox::ByteView(input));
        } catch (const iox::IoxError& error) {
            threw = error.code() == iox::DiagnosticCode::JsonMalformed;
        }
        IOX_CHECK(threw);
    }
}

IOX_TEST(json_writer_rejects_invalid_event_order_terminally) {
    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::json::JsonEventWriter writer(sink);
    bool firstThrew = false;
    try {
        writer.write(iox::EndTransferEvent{});
    } catch (const iox::IoxError& error) {
        firstThrew = error.code() == iox::DiagnosticCode::InvalidEventOrder;
    }
    IOX_CHECK(firstThrew);

    bool secondThrew = false;
    try {
        writer.write(iox::StartTransferEvent{});
    } catch (const iox::IoxError&) {
        secondThrew = true;
    }
    IOX_CHECK(secondThrew);
}

#include "iox/test/TestMain.h"
