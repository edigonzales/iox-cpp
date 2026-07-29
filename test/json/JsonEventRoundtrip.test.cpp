#include "iox/json/JsonEventReader.h"
#include "iox/json/JsonEventWriter.h"
#include "iox/Events.h"
#include "iox/Writer.h"

#include "iox/test/Test.h"

#include <memory>
#include <sstream>
#include <string>

// Helper: write events to a string via JsonEventWriter, then read them back
static std::string writeEventsToString(const std::vector<iox::IoxEvent>& events) {
    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::json::JsonEventWriter writer(sink);

    for (const auto& e : events) {
        writer.write(e);
    }
    writer.close();

    return sink->str();
}

static std::vector<iox::IoxEvent> readEventsFromString(const std::string& s) {
    iox::json::JsonEventReader reader;
    reader.feed(iox::ByteView(s.data(), s.size()));
    reader.finish();

    std::vector<iox::IoxEvent> events;
    while (true) {
        auto outcome = reader.next();
        if (outcome.status == iox::ReadOutcome::Status::End) break;
        if (outcome.status == iox::ReadOutcome::Status::NeedInput) break;
        if (outcome.event) {
            events.push_back(std::move(*outcome.event));
        }
    }
    return events;
}

IOX_TEST(json_roundtrip_start_transfer) {
    iox::StartTransferEvent st;
    st.sender = "TestSender";
    st.comment = "Test comment";
    st.iliVersion = "2.3";
    st.software = "iox-test";
    st.date = "2025-01-01";
    st.version = 23;

    auto output = writeEventsToString({st});
    // Output should be non-empty NDJSON
    IOX_CHECK(!output.empty());

    auto events = readEventsFromString(output);
    IOX_CHECK_EQ(static_cast<std::size_t>(1), events.size());
    IOX_CHECK(std::holds_alternative<iox::StartTransferEvent>(events[0]));

    auto& parsed = std::get<iox::StartTransferEvent>(events[0]);
    IOX_CHECK_EQ(std::string("StartTransfer"), iox::eventTypeName(events[0]));
}

IOX_TEST(json_roundtrip_full_stream) {
    // Create a complete event stream
    std::vector<iox::IoxEvent> events;

    iox::StartTransferEvent st;
    st.sender = "S";
    st.version = 23;
    events.push_back(st);

    iox::StartBasketEvent sb;
    sb.basketType = iox::IomName("Model.Topic.Basket");
    sb.bid = "BID001";
    sb.consistency = "complete";
    sb.operation = "insert";
    events.push_back(sb);

    iox::ObjectEvent obj;
    obj.operation = "insert";
    obj.objectId = "TID001";
    obj.object = iox::IomObject(iox::IomName("Model.Topic.TestClass"));
    obj.object.setPrimitive("Name", iox::IomValue::text("test-value"));
    obj.object.setPrimitive("Count", iox::IomValue::integer(42));
    events.push_back(obj);

    iox::EndBasketEvent eb;
    eb.bid = "BID001";
    events.push_back(eb);

    iox::EndTransferEvent et;
    events.push_back(et);

    auto output = writeEventsToString(events);
    IOX_CHECK(!output.empty());

    auto parsed = readEventsFromString(output);
    IOX_CHECK_EQ(static_cast<std::size_t>(5), parsed.size());
    IOX_CHECK(std::holds_alternative<iox::StartTransferEvent>(parsed[0]));
    IOX_CHECK(std::holds_alternative<iox::StartBasketEvent>(parsed[1]));
    IOX_CHECK(std::holds_alternative<iox::ObjectEvent>(parsed[2]));
    IOX_CHECK(std::holds_alternative<iox::EndBasketEvent>(parsed[3]));
    IOX_CHECK(std::holds_alternative<iox::EndTransferEvent>(parsed[4]));
}

IOX_TEST(json_roundtrip_event_types_match) {
    // Verify that event type names survive roundtrip
    std::vector<iox::IoxEvent> events;
    events.push_back(iox::StartTransferEvent{});
    events.push_back(iox::EndTransferEvent{});

    auto output = writeEventsToString(events);
    auto parsed = readEventsFromString(output);

    IOX_CHECK_EQ(static_cast<std::size_t>(2), parsed.size());
    IOX_CHECK_EQ(std::string("StartTransfer"), iox::eventTypeName(parsed[0]));
    IOX_CHECK_EQ(std::string("EndTransfer"), iox::eventTypeName(parsed[1]));
}

IOX_TEST(json_writer_deterministic) {
    iox::StartTransferEvent st;
    st.sender = "S";
    st.version = 23;

    auto out1 = writeEventsToString({st});
    auto out2 = writeEventsToString({st});

    IOX_CHECK_EQ(out1, out2);
}

IOX_TEST(json_writer_empty_basket) {
    std::vector<iox::IoxEvent> events;
    iox::StartTransferEvent st;
    st.version = 23;
    events.push_back(st);

    iox::StartBasketEvent sb;
    sb.basketType = iox::IomName("M.T.B");
    sb.bid = "B1";
    events.push_back(sb);

    iox::EndBasketEvent eb;
    eb.bid = "B1";
    events.push_back(eb);

    iox::EndTransferEvent et;
    events.push_back(et);

    auto output = writeEventsToString(events);
    auto parsed = readEventsFromString(output);

    IOX_CHECK_EQ(static_cast<std::size_t>(4), parsed.size());
}

#include "iox/test/TestMain.h"
