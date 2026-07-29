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

// Helper to build a COORD IomObject
static iox::IomObject makeCoord(double c1, double c2, double c3) {
    iox::IomObject coord(iox::IomName("COORD"));
    coord.setPrimitive("C1", iox::IomValue::text(std::to_string(c1)));
    coord.setPrimitive("C2", iox::IomValue::text(std::to_string(c2)));
    coord.setPrimitive("C3", iox::IomValue::text(std::to_string(c3)));
    return coord;
}

// Helper to build a POLYLINE IomObject
static iox::IomObject makePolyline(const std::vector<std::pair<double,double>>& points) {
    iox::IomObject polyline(iox::IomName("POLYLINE"));
    iox::IomObject segments(iox::IomName("SEGMENTS"));
    for (const auto& p : points) {
        auto& segAttr = segments.setAttribute(iox::IomName("segment"));
        segAttr.values.push_back(makeCoord(p.first, p.second, 0.0));
    }
    auto& seqAttr = polyline.setAttribute(iox::IomName("sequence"));
    seqAttr.values.push_back(std::move(segments));
    return polyline;
}

// ============================================================================
// Tests
// ============================================================================

IOX_TEST(xtf23_geometry_coord) {
    std::vector<iox::IoxEvent> events;
    iox::StartTransferEvent st; st.version = 23; events.push_back(st);
    iox::StartBasketEvent sb;
    sb.basketType = iox::IomName("M.T.B");
    sb.bid = "B1";
    events.push_back(sb);

    iox::ObjectEvent obj;
    obj.operation = "insert";
    obj.objectId = "TID1";
    obj.object = iox::IomObject(iox::IomName("M.T.WithCoord"));
    auto& attr = obj.object.setAttribute(iox::IomName("Location"));
    attr.values.push_back(makeCoord(2600000.0, 1200000.0, 500.0));
    events.push_back(obj);

    iox::EndBasketEvent eb; eb.bid = "B1"; events.push_back(eb);
    iox::EndTransferEvent et; events.push_back(et);

    auto xml = writeXtf(events, iox::xtf::XtfVersion::Xtf23);
    auto parsed = readXtf(xml);

    // Should have Object event with COORD
    IOX_CHECK_EQ(static_cast<std::size_t>(5), parsed.size());
    auto& objEvent = std::get<iox::ObjectEvent>(parsed[2]);
    auto* locAttr = objEvent.object.findAttribute("Location");
    IOX_CHECK(locAttr != nullptr);
    IOX_CHECK_EQ(static_cast<std::size_t>(1), locAttr->values.size());
    IOX_CHECK(std::holds_alternative<iox::IomObject>(locAttr->values[0]));
}

IOX_TEST(xtf23_geometry_polyline) {
    std::vector<iox::IoxEvent> events;
    iox::StartTransferEvent st; st.version = 23; events.push_back(st);
    iox::StartBasketEvent sb;
    sb.basketType = iox::IomName("M.T.B");
    sb.bid = "B1";
    events.push_back(sb);

    iox::ObjectEvent obj;
    obj.operation = "insert";
    obj.objectId = "TID1";
    obj.object = iox::IomObject(iox::IomName("M.T.WithLine"));
    auto polyline = makePolyline({{2600000,1200000},{2600100,1200100},{2600200,1200000}});
    auto& attr = obj.object.setAttribute(iox::IomName("Line"));
    attr.values.push_back(std::move(polyline));
    events.push_back(obj);

    iox::EndBasketEvent eb; eb.bid = "B1"; events.push_back(eb);
    iox::EndTransferEvent et; events.push_back(et);

    auto xml = writeXtf(events, iox::xtf::XtfVersion::Xtf23);
    auto parsed = readXtf(xml);

    IOX_CHECK_EQ(static_cast<std::size_t>(5), parsed.size());
    auto& objEvent = std::get<iox::ObjectEvent>(parsed[2]);
    auto* lineAttr = objEvent.object.findAttribute("Line");
    IOX_CHECK(lineAttr != nullptr);
    IOX_CHECK(!lineAttr->values.empty());
    IOX_CHECK(std::holds_alternative<iox::IomObject>(lineAttr->values[0]));

    // Verify the nested structure
    auto& poly = std::get<iox::IomObject>(lineAttr->values[0]);
    IOX_CHECK_EQ(std::string("POLYLINE"), poly.tag().iliName());
}

IOX_TEST(xtf23_geometry_surface) {
    // Build a SURFACE with one exterior POLYLINE
    iox::IomObject surface(iox::IomName("SURFACE"));
    iox::IomObject boundary(iox::IomName("BOUNDARY"));
    auto polyline = makePolyline({{0,0},{100,0},{100,100},{0,100},{0,0}});
    auto& bAttr = boundary.setAttribute(iox::IomName("polyline"));
    bAttr.values.push_back(std::move(polyline));
    auto& sAttr = surface.setAttribute(iox::IomName("boundary"));
    sAttr.values.push_back(std::move(boundary));

    std::vector<iox::IoxEvent> events;
    iox::StartTransferEvent st; st.version = 23; events.push_back(st);
    iox::StartBasketEvent sb;
    sb.basketType = iox::IomName("M.T.B");
    sb.bid = "B1";
    events.push_back(sb);

    iox::ObjectEvent obj;
    obj.operation = "insert";
    obj.objectId = "TID1";
    obj.object = iox::IomObject(iox::IomName("M.T.SurfaceClass"));
    auto& attr = obj.object.setAttribute(iox::IomName("Area"));
    attr.values.push_back(std::move(surface));
    events.push_back(obj);

    iox::EndBasketEvent eb; eb.bid = "B1"; events.push_back(eb);
    iox::EndTransferEvent et; events.push_back(et);

    auto xml = writeXtf(events, iox::xtf::XtfVersion::Xtf23);
    auto parsed = readXtf(xml);

    IOX_CHECK_EQ(static_cast<std::size_t>(5), parsed.size());
    auto& objEvent = std::get<iox::ObjectEvent>(parsed[2]);
    auto* areaAttr = objEvent.object.findAttribute("Area");
    IOX_CHECK(areaAttr != nullptr);
    IOX_CHECK(std::holds_alternative<iox::IomObject>(areaAttr->values[0]));
}

IOX_TEST(xtf23_geometry_incomplete_polyline) {
    // POLYLINE with multiple sequences (clipping)
    iox::IomObject polyline(iox::IomName("POLYLINE"));
    iox::IomObject segments1(iox::IomName("SEGMENTS"));
    auto& s1Attr = segments1.setAttribute(iox::IomName("segment"));
    s1Attr.values.push_back(makeCoord(0, 0, 0));
    s1Attr.values.push_back(makeCoord(10, 10, 0));

    iox::IomObject segments2(iox::IomName("SEGMENTS"));
    auto& s2Attr = segments2.setAttribute(iox::IomName("segment"));
    s2Attr.values.push_back(makeCoord(20, 20, 0));
    s2Attr.values.push_back(makeCoord(30, 30, 0));

    auto& seqAttr = polyline.setAttribute(iox::IomName("sequence"));
    seqAttr.values.push_back(std::move(segments1));
    seqAttr.values.push_back(std::move(segments2));

    std::vector<iox::IoxEvent> events;
    iox::StartTransferEvent st; st.version = 23; events.push_back(st);
    iox::StartBasketEvent sb;
    sb.basketType = iox::IomName("M.T.B");
    sb.bid = "B1";
    events.push_back(sb);

    iox::ObjectEvent obj;
    obj.operation = "insert";
    obj.objectId = "TID1";
    obj.object = iox::IomObject(iox::IomName("M.T.Clipped"));
    auto& attr = obj.object.setAttribute(iox::IomName("Geom"));
    attr.values.push_back(std::move(polyline));
    events.push_back(obj);

    iox::EndBasketEvent eb; eb.bid = "B1"; events.push_back(eb);
    iox::EndTransferEvent et; events.push_back(et);

    auto xml = writeXtf(events, iox::xtf::XtfVersion::Xtf23);
    auto parsed = readXtf(xml);

    IOX_CHECK_EQ(static_cast<std::size_t>(5), parsed.size());
    auto& objEvent = std::get<iox::ObjectEvent>(parsed[2]);
    auto* geomAttr = objEvent.object.findAttribute("Geom");
    IOX_CHECK(geomAttr != nullptr);
    auto& poly = std::get<iox::IomObject>(geomAttr->values[0]);
    // POLYLINE has one "sequence" attribute with 2 values (2 SEGMENTS)
    auto* seq = poly.findAttribute("sequence");
    IOX_CHECK(seq != nullptr);
    IOX_CHECK_EQ(static_cast<std::size_t>(2), seq->values.size());
}

#include "iox/test/TestMain.h"
