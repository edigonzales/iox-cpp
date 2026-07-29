#include "iox/xtf/XtfReader.h"
#include "iox/xtf/XtfWriter.h"
#include "iox/xtf/XtfVersion.h"
#include "iox/Events.h"
#include "iox/Writer.h"
#include "iox/test/Test.h"

#include <memory>
#include <string>
#include <vector>

// ============================================================================
// Helpers
// ============================================================================

static std::string writeXtf(const std::vector<iox::IoxEvent>& events,
                             iox::xtf::XtfVersion version) {
    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::xtf::XtfWriterOptions opts;
    opts.version = version;
    opts.pretty = false;
    opts.sender = "TestSender";
    opts.software = "iox-test";

    iox::xtf::XtfWriter writer(sink, opts);
    for (const auto& e : events) writer.write(e);
    writer.close();
    return sink->str();
}

static std::vector<iox::IoxEvent> readXtf(const std::string& data) {
    iox::xtf::XtfReader reader;
    reader.feed(iox::ByteView(data.data(), data.size()));
    reader.finish();

    std::vector<iox::IoxEvent> events;
    while (true) {
        auto outcome = reader.next();
        if (outcome.status == iox::ReadOutcome::Status::End) break;
        if (outcome.status == iox::ReadOutcome::Status::NeedInput) break;
        if (outcome.event) events.push_back(std::move(*outcome.event));
    }
    return events;
}

// ============================================================================
// Tests
// ============================================================================

IOX_TEST(xtf23_simple_object_roundtrip) {
    // Build a complete transfer with one object
    std::vector<iox::IoxEvent> events;

    iox::StartTransferEvent st;
    st.sender = "TestSender";
    st.version = 23;
    events.push_back(st);

    iox::StartBasketEvent sb;
    sb.basketType = iox::IomName("TestModel.TopicA.DataBasket");
    sb.bid = "BID001";
    sb.consistency = "complete";
    sb.operation = "insert";
    events.push_back(sb);

    iox::ObjectEvent obj;
    obj.operation = "insert";
    obj.objectId = "TID001";
    obj.object = iox::IomObject(iox::IomName("TestModel.TopicA.MyClass"));
    obj.object.setPrimitive("Name", iox::IomValue::text("test-value"));
    obj.object.setPrimitive("Count", iox::IomValue::integer(42));
    events.push_back(obj);

    iox::EndBasketEvent eb;
    eb.bid = "BID001";
    events.push_back(eb);

    iox::EndTransferEvent et;
    events.push_back(et);

    // Write then read
    auto xml = writeXtf(events, iox::xtf::XtfVersion::Xtf23);
    auto parsed = readXtf(xml);

    // Should have events: StartTransfer, StartBasket, Object, EndBasket, EndTransfer
    IOX_CHECK_EQ(static_cast<std::size_t>(5), parsed.size());
    IOX_CHECK(std::holds_alternative<iox::StartTransferEvent>(parsed[0]));
    IOX_CHECK(std::holds_alternative<iox::StartBasketEvent>(parsed[1]));
    IOX_CHECK(std::holds_alternative<iox::ObjectEvent>(parsed[2]));
    IOX_CHECK(std::holds_alternative<iox::EndBasketEvent>(parsed[3]));
    IOX_CHECK(std::holds_alternative<iox::EndTransferEvent>(parsed[4]));

    // Check the object event
    auto& objEvent = std::get<iox::ObjectEvent>(parsed[2]);
    IOX_CHECK_EQ(std::string("TID001"), objEvent.objectId);
    IOX_CHECK_EQ(std::string("insert"), objEvent.operation);
}

IOX_TEST(xtf23_multiple_objects) {
    std::vector<iox::IoxEvent> events;
    iox::StartTransferEvent st;
    st.version = 23;
    events.push_back(st);

    iox::StartBasketEvent sb;
    sb.basketType = iox::IomName("M.T.B");
    sb.bid = "B1";
    events.push_back(sb);

    for (int i = 0; i < 3; ++i) {
        iox::ObjectEvent obj;
        obj.operation = "insert";
        obj.objectId = "TID" + std::to_string(i);
        obj.object = iox::IomObject(iox::IomName("M.T.C"));
        obj.object.setPrimitive("idx", iox::IomValue::integer(i));
        events.push_back(obj);
    }

    iox::EndBasketEvent eb;
    eb.bid = "B1";
    events.push_back(eb);

    iox::EndTransferEvent et;
    events.push_back(et);

    auto xml = writeXtf(events, iox::xtf::XtfVersion::Xtf23);
    auto parsed = readXtf(xml);

    // 1 + 1 + 3 + 1 + 1 = 7 events
    IOX_CHECK_EQ(static_cast<std::size_t>(7), parsed.size());

    int objCount = 0;
    for (const auto& e : parsed) {
        if (std::holds_alternative<iox::ObjectEvent>(e)) ++objCount;
    }
    IOX_CHECK_EQ(3, objCount);
}

IOX_TEST(xtf23_object_with_structure_attribute) {
    std::vector<iox::IoxEvent> events;

    iox::StartTransferEvent st;
    st.version = 23;
    events.push_back(st);

    iox::StartBasketEvent sb;
    sb.basketType = iox::IomName("M.T.B");
    sb.bid = "B1";
    events.push_back(sb);

    iox::ObjectEvent obj;
    obj.operation = "insert";
    obj.objectId = "TID1";
    obj.object = iox::IomObject(iox::IomName("M.T.Main"));
    // Add a structure attribute
    iox::IomObject addr(iox::IomName("M.T.Address"));
    addr.setPrimitive("Street", iox::IomValue::text("Main St"));
    addr.setPrimitive("Number", iox::IomValue::integer(10));
    obj.object.setStructure("Address", addr);
    obj.object.setPrimitive("Name", iox::IomValue::text("Test"));
    events.push_back(obj);

    iox::EndBasketEvent eb;
    eb.bid = "B1";
    events.push_back(eb);
    iox::EndTransferEvent et;
    events.push_back(et);

    auto xml = writeXtf(events, iox::xtf::XtfVersion::Xtf23);
    auto parsed = readXtf(xml);

    // Should have at least the Object event
    bool foundObj = false;
    for (const auto& e : parsed) {
        if (auto* o = std::get_if<iox::ObjectEvent>(&e)) {
            foundObj = true;
            IOX_CHECK_EQ(std::string("TID1"), o->objectId);
            break;
        }
    }
    IOX_CHECK(foundObj);
}

IOX_TEST(xtf23_basket_with_consistency) {
    std::vector<iox::IoxEvent> events;
    iox::StartTransferEvent st;
    st.version = 23;
    events.push_back(st);

    iox::StartBasketEvent sb;
    sb.basketType = iox::IomName("M.T.B");
    sb.bid = "B_INCOMPLETE";
    sb.consistency = "incomplete";
    sb.operation = "update";
    events.push_back(sb);

    iox::EndBasketEvent eb;
    eb.bid = "B_INCOMPLETE";
    events.push_back(eb);
    iox::EndTransferEvent et;
    events.push_back(et);

    auto xml = writeXtf(events, iox::xtf::XtfVersion::Xtf23);
    auto parsed = readXtf(xml);

    IOX_CHECK_EQ(static_cast<std::size_t>(4), parsed.size());
    auto& basketEvent = std::get<iox::StartBasketEvent>(parsed[1]);
    IOX_CHECK_EQ(std::string("B_INCOMPLETE"), basketEvent.bid);
}

IOX_TEST(xtf23_multiple_baskets) {
    std::vector<iox::IoxEvent> events;
    iox::StartTransferEvent st;
    st.version = 23;
    events.push_back(st);

    // Basket 1
    iox::StartBasketEvent sb1;
    sb1.basketType = iox::IomName("M.T.B");
    sb1.bid = "B1";
    events.push_back(sb1);
    iox::EndBasketEvent eb1;
    eb1.bid = "B1";
    events.push_back(eb1);

    // Basket 2
    iox::StartBasketEvent sb2;
    sb2.basketType = iox::IomName("M.T.B");
    sb2.bid = "B2";
    events.push_back(sb2);
    iox::EndBasketEvent eb2;
    eb2.bid = "B2";
    events.push_back(eb2);

    iox::EndTransferEvent et;
    events.push_back(et);

    auto xml = writeXtf(events, iox::xtf::XtfVersion::Xtf23);
    auto parsed = readXtf(xml);

    // ST + SB1 + EB1 + SB2 + EB2 + ET = 6
    IOX_CHECK_EQ(static_cast<std::size_t>(6), parsed.size());
}

IOX_TEST(xtf23_chunked_read) {
    // Write a simple document, then read in 3-byte chunks
    std::vector<iox::IoxEvent> events;
    iox::StartTransferEvent st;
    st.version = 23;
    events.push_back(st);

    iox::StartBasketEvent sb;
    sb.basketType = iox::IomName("M.T.B");
    sb.bid = "B1";
    events.push_back(sb);

    iox::ObjectEvent obj;
    obj.operation = "insert";
    obj.objectId = "TID1";
    obj.object = iox::IomObject(iox::IomName("M.T.C"));
    obj.object.setPrimitive("val", iox::IomValue::text("chunk-test"));
    events.push_back(obj);

    iox::EndBasketEvent eb;
    eb.bid = "B1";
    events.push_back(eb);

    iox::EndTransferEvent et;
    events.push_back(et);

    auto xml = writeXtf(events, iox::xtf::XtfVersion::Xtf23);

    // Read in 3-byte chunks
    iox::xtf::XtfReader reader;
    for (std::size_t off = 0; off < xml.size(); off += 3) {
        auto n = std::min(std::size_t(3), xml.size() - off);
        reader.feed(iox::ByteView(xml.data() + off, n));
    }
    reader.finish();

    int count = 0;
    while (true) {
        auto outcome = reader.next();
        if (outcome.status == iox::ReadOutcome::Status::End) break;
        if (outcome.status == iox::ReadOutcome::Status::NeedInput) break;
        if (outcome.event) ++count;
    }
    IOX_CHECK_EQ(5, count);
}

#include "iox/test/TestMain.h"
