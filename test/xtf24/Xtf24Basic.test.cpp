#include "iox/xtf/XtfReader.h"
#include "iox/xtf/XtfWriter.h"
#include "iox/xtf/XtfVersion.h"
#include "iox/Events.h"
#include "iox/Writer.h"
#include "iox/test/Test.h"
#include <memory>
#include <string>
#include <vector>

static std::string writeXtf(const std::vector<iox::IoxEvent>& events,
                             iox::xtf::XtfVersion version) {
    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::xtf::XtfWriterOptions opts;
    opts.version = version; opts.pretty = false;
    opts.sender = "T"; opts.software = "t";
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
        auto o = reader.next();
        if (o.status == iox::ReadOutcome::Status::End) break;
        if (o.status == iox::ReadOutcome::Status::NeedInput) break;
        if (o.event) events.push_back(std::move(*o.event));
    }
    return events;
}

IOX_TEST(xtf24_minimal_header) {
    std::vector<iox::IoxEvent> events;
    iox::StartTransferEvent st; st.version = 24; events.push_back(st);
    iox::EndTransferEvent et; events.push_back(et);
    auto xml = writeXtf(events, iox::xtf::XtfVersion::Xtf24);
    auto parsed = readXtf(xml);
    IOX_CHECK(!parsed.empty());
    IOX_CHECK(std::holds_alternative<iox::StartTransferEvent>(parsed[0]));
}

IOX_TEST(xtf24_object_roundtrip) {
    std::vector<iox::IoxEvent> events;
    iox::StartTransferEvent st; st.version = 24; events.push_back(st);
    iox::StartBasketEvent sb;
    sb.basketType = iox::IomName("M.T.B"); sb.bid = "B1";
    events.push_back(sb);
    iox::ObjectEvent obj;
    obj.operation = "insert"; obj.objectId = "T1";
    obj.object = iox::IomObject(iox::IomName("M.T.C"));
    obj.object.setPrimitive("Name", iox::IomValue::text("val"));
    events.push_back(obj);
    iox::EndBasketEvent eb; eb.bid = "B1"; events.push_back(eb);
    iox::EndTransferEvent et; events.push_back(et);

    auto xml = writeXtf(events, iox::xtf::XtfVersion::Xtf24);
    auto parsed = readXtf(xml);
    IOX_CHECK_EQ(static_cast<std::size_t>(5), parsed.size());
}

IOX_TEST(xtf24_geometry_coord) {
    std::vector<iox::IoxEvent> events;
    iox::StartTransferEvent st; st.version = 24; events.push_back(st);
    iox::StartBasketEvent sb;
    sb.basketType = iox::IomName("M.T.B"); sb.bid = "B1"; events.push_back(sb);
    iox::ObjectEvent obj;
    obj.operation = "insert"; obj.objectId = "T1";
    obj.object = iox::IomObject(iox::IomName("M.T.G"));
    iox::IomObject coord(iox::IomName("COORD"));
    coord.setPrimitive("C1", iox::IomValue::text("1.0"));
    coord.setPrimitive("C2", iox::IomValue::text("2.0"));
    auto& attr = obj.object.setAttribute(iox::IomName("Pos"));
    attr.values.push_back(std::move(coord));
    events.push_back(obj);
    iox::EndBasketEvent eb; eb.bid = "B1"; events.push_back(eb);
    iox::EndTransferEvent et; events.push_back(et);

    auto xml = writeXtf(events, iox::xtf::XtfVersion::Xtf24);
    auto parsed = readXtf(xml);
    IOX_CHECK_EQ(static_cast<std::size_t>(5), parsed.size());
}

#include "iox/test/TestMain.h"
