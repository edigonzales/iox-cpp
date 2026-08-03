#include "iox/Events.h"
#include "iox/Writer.h"
#include "iox/xtf/XtfReader.h"
#include "iox/xtf/XtfWriter.h"
#include "iox/test/Test.h"

#include <memory>
#include <string>
#include <vector>

namespace {

std::vector<iox::IoxEvent> roundtrip(std::vector<iox::IoxEvent> events) {
    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::xtf::XtfWriterOptions options;
    options.version = iox::XtfVersion::V24;
    options.pretty = false;
    iox::xtf::XtfWriter writer(sink, options);
    for (const auto& event : events) writer.write(event);
    writer.close();

    iox::xtf::XtfReader reader;
    reader.feed(iox::ByteView(sink->str()));
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

iox::StartTransferEvent start() {
    iox::StartTransferEvent result;
    result.header.version = iox::XtfVersion::V24;
    result.header.sender = "test";
    return result;
}

iox::StartBasketEvent basket() {
    iox::StartBasketEvent result;
    result.basket.topic = iox::IomName(
        "M.T", {"urn:m", "T", "m"});
    result.basket.basketId = "B1";
    return result;
}

} // namespace

IOX_TEST(xtf24_minimal_header) {
    const auto events = roundtrip({start(), iox::EndTransferEvent{}});
    IOX_CHECK_EQ(static_cast<std::size_t>(2), events.size());
    IOX_CHECK_EQ(iox::XtfVersion::V24,
                 std::get<iox::StartTransferEvent>(events[0]).header.version);
}

IOX_TEST(xtf24_object_roundtrip) {
    iox::ObjectEvent object;
    object.object = iox::IomObject(
        iox::IomName("M.T.C", {"urn:m", "C", "m"}), "T1");
    object.object.setOperation(iox::ObjectOperation::Insert);
    object.object.setPrimitive(iox::IomName("Name"), "val");
    const auto events = roundtrip(
        {start(), basket(), object, iox::EndBasketEvent{},
         iox::EndTransferEvent{}});
    const auto& parsed = std::get<iox::ObjectEvent>(events[2]).object;
    IOX_CHECK_EQ(std::string("T1"), *parsed.oid());
    IOX_CHECK_EQ(std::string_view("val"), *parsed.primitive("Name"));
}

IOX_TEST(xtf24_geometry_coord) {
    iox::IomObject coordinate(iox::IomName("COORD"));
    coordinate.setPrimitive(iox::IomName("C1"), "1.0");
    coordinate.setPrimitive(iox::IomName("C2"), "2.0");
    iox::ObjectEvent object;
    object.object = iox::IomObject(
        iox::IomName("M.T.C", {"urn:m", "C", "m"}), "T1");
    object.object.setObject(iox::IomName("Pos"), coordinate);
    const auto events = roundtrip(
        {start(), basket(), object, iox::EndBasketEvent{},
         iox::EndTransferEvent{}});
    const auto parsed = std::get<iox::ObjectEvent>(events[2]).object.object("Pos");
    IOX_CHECK(parsed.has_value());
    IOX_CHECK_EQ(std::string_view("1.0"), *parsed->primitive("c1"));
    IOX_CHECK_EQ(std::string_view("2.0"), *parsed->primitive("c2"));
}

#include "iox/test/TestMain.h"
