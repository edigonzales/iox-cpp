#include "iox/xtf/XtfReader.h"
#include "iox/xtf/XtfWriter.h"
#include "iox/xtf/XtfVersion.h"
#include "iox/Events.h"
#include "iox/Writer.h"
#include "iox/test/Test.h"
#include <memory>
#include <string>
#include <vector>
#include <cmath>
#include <fstream>

// ============================================================================
// Helpers
// ============================================================================

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

static std::vector<iox::IoxEvent> readXtfChunked(const std::string& data,
                                                 std::size_t chunkSize) {
    iox::xtf::XtfReader reader;
    for (std::size_t offset = 0; offset < data.size(); offset += chunkSize) {
        const auto size = std::min(chunkSize, data.size() - offset);
        reader.feed(iox::ByteView(data.data() + offset, size));
    }
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

static std::string readFixture(const char* path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return {};
    const auto size = file.tellg();
    file.seekg(0);
    std::string data(static_cast<std::size_t>(size), '\0');
    file.read(data.data(), size);
    return data;
}

static std::size_t countTag(const iox::IomObject& object, std::string_view tag) {
    std::size_t count = object.tag().iliName() == tag ? 1u : 0u;
    for (std::size_t i = 0; i < object.attributeCount(); ++i) {
        const auto& attribute = object.attributeAt(i);
        for (const auto& value : attribute.values) {
            if (const auto* nested = std::get_if<iox::IomObject>(&value)) {
                count += countTag(*nested, tag);
            }
        }
    }
    return count;
}

static iox::IomObject makeCoord(double c1, double c2, double c3) {
    iox::IomObject c(iox::IomName("COORD"));
    c.setPrimitive("C1", iox::IomValue::text(std::to_string(c1)));
    c.setPrimitive("C2", iox::IomValue::text(std::to_string(c2)));
    c.setPrimitive("C3", iox::IomValue::text(std::to_string(c3)));
    return c;
}

static iox::IomObject makeArc(double c1,double c2,double c3,
                               double a1,double a2,double a3,double r) {
    iox::IomObject arc(iox::IomName("ARC"));
    arc.setPrimitive("C1", iox::IomValue::text(std::to_string(c1)));
    arc.setPrimitive("C2", iox::IomValue::text(std::to_string(c2)));
    arc.setPrimitive("C3", iox::IomValue::text(std::to_string(c3)));
    arc.setPrimitive("A1", iox::IomValue::text(std::to_string(a1)));
    arc.setPrimitive("A2", iox::IomValue::text(std::to_string(a2)));
    arc.setPrimitive("A3", iox::IomValue::text(std::to_string(a3)));
    arc.setPrimitive("R",  iox::IomValue::text(std::to_string(r)));
    return arc;
}

static iox::IomObject makePolyline(std::vector<iox::IomObject> segments) {
    iox::IomObject pl(iox::IomName("POLYLINE"));
    iox::IomObject segs(iox::IomName("SEGMENTS"));
    for (auto& s : segments) {
        auto& attr = segs.setAttribute(iox::IomName("segment"));
        attr.values.push_back(std::move(s));
    }
    auto& seq = pl.setAttribute(iox::IomName("sequence"));
    seq.values.push_back(std::move(segs));
    return pl;
}

static iox::IomObject makeSurface(iox::IomObject boundary) {
    iox::IomObject sf(iox::IomName("SURFACE"));
    auto& attr = sf.setAttribute(iox::IomName("boundary"));
    attr.values.push_back(std::move(boundary));
    return sf;
}

static iox::IomObject makeArea(iox::IomObject exterior, iox::IomObject interior) {
    iox::IomObject ar(iox::IomName("AREA"));
    auto& ext = ar.setAttribute(iox::IomName("exterior"));
    ext.values.push_back(std::move(exterior));
    auto& in = ar.setAttribute(iox::IomName("interior"));
    in.values.push_back(std::move(interior));
    return ar;
}

// Wrap in Transfer+Basket envelope
static std::string roundtripGeometry(iox::IomObject geom, const char* className) {
    std::vector<iox::IoxEvent> events;
    iox::StartTransferEvent st; st.version = 24; events.push_back(st);
    iox::StartBasketEvent sb;
    sb.basketType = iox::IomName("M.T.B"); sb.bid = "B1"; events.push_back(sb);
    iox::ObjectEvent obj;
    obj.operation = "insert"; obj.objectId = "T1";
    obj.object = iox::IomObject(iox::IomName(className));
    auto& attr = obj.object.setAttribute(iox::IomName("Geom"));
    attr.values.push_back(std::move(geom));
    events.push_back(obj);
    iox::EndBasketEvent eb; eb.bid = "B1"; events.push_back(eb);
    iox::EndTransferEvent et; events.push_back(et);

    auto xml = writeXtf(events, iox::xtf::XtfVersion::Xtf24);
    auto parsed = readXtf(xml);

    IOX_CHECK_EQ(static_cast<std::size_t>(5), parsed.size());
    auto& objEvent = std::get<iox::ObjectEvent>(parsed[2]);
    auto* geomAttr = objEvent.object.findAttribute("Geom");
    IOX_CHECK(geomAttr != nullptr);
    IOX_CHECK_EQ(static_cast<std::size_t>(1), geomAttr->values.size());
    IOX_CHECK(std::holds_alternative<iox::IomObject>(geomAttr->values[0]));

    return xml;
}

// ============================================================================
// Tests
// ============================================================================

IOX_TEST(xtf24_geom_coord) {
    auto coord = makeCoord(2600000, 1200000, 500);
    roundtripGeometry(std::move(coord), "M.T.C");
}

IOX_TEST(xtf24_geom_arc) {
    auto arc = makeArc(2600000,1200000,500, 2600100,1200100,500, 150);
    roundtripGeometry(std::move(arc), "M.T.A");
}

IOX_TEST(xtf24_geom_polyline) {
    std::vector<iox::IomObject> segments;
    segments.push_back(makeCoord(0,0,0));
    segments.push_back(makeCoord(100,0,0));
    segments.push_back(makeCoord(100,100,0));
    auto pl = makePolyline(std::move(segments));
    roundtripGeometry(std::move(pl), "M.T.P");
}

IOX_TEST(xtf24_geom_polyline_with_arc) {
    std::vector<iox::IomObject> segments;
    segments.push_back(makeCoord(0,0,0));
    segments.push_back(makeArc(10,10,0, 20,20,0, 15));
    segments.push_back(makeCoord(30,30,0));
    auto pl = makePolyline(std::move(segments));
    roundtripGeometry(std::move(pl), "M.T.PA");
}

IOX_TEST(xtf24_geom_surface) {
    std::vector<iox::IomObject> segs;
    segs.push_back(makeCoord(0,0,0));
    segs.push_back(makeCoord(100,0,0));
    segs.push_back(makeCoord(100,100,0));
    segs.push_back(makeCoord(0,100,0));
    segs.push_back(makeCoord(0,0,0));
    auto boundary = makePolyline(std::move(segs));
    auto surface = makeSurface(std::move(boundary));
    roundtripGeometry(std::move(surface), "M.T.S");
}

IOX_TEST(xtf24_geom_area) {
    // Exterior ring
    std::vector<iox::IomObject> extSegs;
    extSegs.push_back(makeCoord(0,0,0));
    extSegs.push_back(makeCoord(200,0,0));
    extSegs.push_back(makeCoord(200,200,0));
    extSegs.push_back(makeCoord(0,200,0));
    extSegs.push_back(makeCoord(0,0,0));
    auto exterior = makePolyline(std::move(extSegs));

    // Interior ring (hole)
    std::vector<iox::IomObject> intSegs;
    intSegs.push_back(makeCoord(50,50,0));
    intSegs.push_back(makeCoord(150,50,0));
    intSegs.push_back(makeCoord(150,150,0));
    intSegs.push_back(makeCoord(50,150,0));
    intSegs.push_back(makeCoord(50,50,0));
    auto interior = makePolyline(std::move(intSegs));

    auto area = makeArea(std::move(exterior), std::move(interior));
    roundtripGeometry(std::move(area), "M.T.A");
}

IOX_TEST(xtf24_geom_multicoord) {
    iox::IomObject multi(iox::IomName("MULTICOORD"));
    auto& a = multi.setAttribute(iox::IomName("coord"));
    a.values.push_back(makeCoord(0,0,0));
    a.values.push_back(makeCoord(10,10,0));
    a.values.push_back(makeCoord(20,20,0));
    roundtripGeometry(std::move(multi), "M.T.MC");
}

IOX_TEST(xtf24_geom_multipolyline) {
    std::vector<iox::IomObject> segs1;
    segs1.push_back(makeCoord(0,0,0));
    segs1.push_back(makeCoord(100,0,0));
    auto pl1 = makePolyline(std::move(segs1));

    std::vector<iox::IomObject> segs2;
    segs2.push_back(makeCoord(200,200,0));
    segs2.push_back(makeCoord(300,200,0));
    auto pl2 = makePolyline(std::move(segs2));

    iox::IomObject multi(iox::IomName("MULTIPOLYLINE"));
    auto& a = multi.setAttribute(iox::IomName("polyline"));
    a.values.push_back(std::move(pl1));
    a.values.push_back(std::move(pl2));
    roundtripGeometry(std::move(multi), "M.T.MPL");
}

IOX_TEST(xtf24_geom_multisurface) {
    std::vector<iox::IomObject> segs1;
    segs1.push_back(makeCoord(0,0,0));
    segs1.push_back(makeCoord(50,0,0));
    segs1.push_back(makeCoord(50,50,0));
    segs1.push_back(makeCoord(0,50,0));
    segs1.push_back(makeCoord(0,0,0));
    auto sf1 = makeSurface(makePolyline(std::move(segs1)));

    std::vector<iox::IomObject> segs2;
    segs2.push_back(makeCoord(100,100,0));
    segs2.push_back(makeCoord(150,100,0));
    segs2.push_back(makeCoord(150,150,0));
    segs2.push_back(makeCoord(100,150,0));
    segs2.push_back(makeCoord(100,100,0));
    auto sf2 = makeSurface(makePolyline(std::move(segs2)));

    iox::IomObject multi(iox::IomName("MULTISURFACE"));
    auto& a = multi.setAttribute(iox::IomName("surface"));
    a.values.push_back(std::move(sf1));
    a.values.push_back(std::move(sf2));
    roundtripGeometry(std::move(multi), "M.T.MS");
}

IOX_TEST(xtf24_geom_multiarea) {
    // Area 1
    std::vector<iox::IomObject> e1;
    e1.push_back(makeCoord(0,0,0)); e1.push_back(makeCoord(100,0,0));
    e1.push_back(makeCoord(100,100,0)); e1.push_back(makeCoord(0,100,0));
    e1.push_back(makeCoord(0,0,0));
    auto ar1 = makeArea(makePolyline(std::move(e1)), makePolyline({}));

    // Area 2
    std::vector<iox::IomObject> e2;
    e2.push_back(makeCoord(200,200,0)); e2.push_back(makeCoord(300,200,0));
    e2.push_back(makeCoord(300,300,0)); e2.push_back(makeCoord(200,300,0));
    e2.push_back(makeCoord(200,200,0));
    auto ar2 = makeArea(makePolyline(std::move(e2)), makePolyline({}));

    iox::IomObject multi(iox::IomName("MULTIAREA"));
    auto& a = multi.setAttribute(iox::IomName("area"));
    a.values.push_back(std::move(ar1));
    a.values.push_back(std::move(ar2));
    roundtripGeometry(std::move(multi), "M.T.MA");
}

IOX_TEST(xtf24_geom_incomplete_polyline) {
    // Two sequences = clipped polyline
    std::vector<iox::IomObject> segs1;
    segs1.push_back(makeCoord(0,0,0));
    segs1.push_back(makeCoord(10,10,0));
    iox::IomObject s1(iox::IomName("SEGMENTS"));
    for (auto& s : segs1) {
        auto& a = s1.setAttribute(iox::IomName("segment"));
        a.values.push_back(std::move(s));
    }

    std::vector<iox::IomObject> segs2;
    segs2.push_back(makeCoord(20,20,0));
    segs2.push_back(makeCoord(30,30,0));
    iox::IomObject s2(iox::IomName("SEGMENTS"));
    for (auto& s : segs2) {
        auto& a = s2.setAttribute(iox::IomName("segment"));
        a.values.push_back(std::move(s));
    }

    iox::IomObject pl(iox::IomName("POLYLINE"));
    auto& seq = pl.setAttribute(iox::IomName("sequence"));
    seq.values.push_back(std::move(s1));
    seq.values.push_back(std::move(s2));
    roundtripGeometry(std::move(pl), "M.T.INC");
}

IOX_TEST(xtf24_geometry_fixture_chunk_matrix_preserves_multigeometry) {
    const auto data = readFixture("test/fixtures/xtf24/dataSection/Area.xml");
    IOX_CHECK(!data.empty());

    for (const auto chunkSize : {std::size_t(1), std::size_t(7), std::size_t(64)}) {
        const auto events = readXtfChunked(data, chunkSize);
        IOX_CHECK_EQ(static_cast<std::size_t>(5), events.size());
        const auto* objectEvent = std::get_if<iox::ObjectEvent>(&events[2]);
        IOX_CHECK(objectEvent != nullptr);
        const auto* form = objectEvent->object.findAttribute("formR");
        IOX_CHECK(form != nullptr);
        IOX_CHECK_EQ(static_cast<std::size_t>(1), form->values.size());
        const auto* surface = std::get_if<iox::IomObject>(&form->values[0]);
        IOX_CHECK(surface != nullptr);
        IOX_CHECK_EQ(static_cast<std::size_t>(1), countTag(*surface, "surface"));
        IOX_CHECK_EQ(static_cast<std::size_t>(8), countTag(*surface, "polyline"));
        IOX_CHECK_EQ(static_cast<std::size_t>(2), countTag(*surface, "arc"));
        IOX_CHECK(surface->findAttribute("exterior") != nullptr);
        IOX_CHECK(surface->findAttribute("interior") != nullptr);
    }
}

IOX_TEST(xtf24_geometry_fixture_preserves_custom_line_form) {
    const auto data = readFixture("test/fixtures/xtf24/dataSection/UnsupportedGeometry.xml");
    IOX_CHECK(!data.empty());
    const auto events = readXtfChunked(data, 1);
    IOX_CHECK_EQ(static_cast<std::size_t>(5), events.size());
    const auto* objectEvent = std::get_if<iox::ObjectEvent>(&events[2]);
    IOX_CHECK(objectEvent != nullptr);
    const auto* attribute = objectEvent->object.findAttribute("attrS");
    IOX_CHECK(attribute != nullptr);
    IOX_CHECK_EQ(static_cast<std::size_t>(1), attribute->values.size());
    const auto* custom = std::get_if<iox::IomObject>(&attribute->values[0]);
    IOX_CHECK(custom != nullptr);
    IOX_CHECK_EQ(std::string("orientablecurve"), custom->tag().iliName());
    IOX_CHECK_EQ(static_cast<std::size_t>(2), countTag(*custom, "coord"));
}

IOX_TEST(xtf24_writer_emits_canonical_geometry_and_basket_envelope) {
    std::vector<iox::IoxEvent> events;
    iox::StartTransferEvent start;
    start.version = 24;
    events.push_back(start);
    iox::StartBasketEvent basket;
    basket.basketType = iox::IomName("M.Topic");
    basket.bid = "B1";
    events.push_back(basket);
    iox::ObjectEvent object;
    object.objectId = "T1";
    object.object = iox::IomObject(iox::IomName("M.Class"));
    iox::IomObject multi(iox::IomName("MULTICOORD"));
    auto& coords = multi.setAttribute(iox::IomName("coord"));
    coords.values.push_back(makeCoord(1, 2, 3));
    coords.values.push_back(makeCoord(4, 5, 6));
    object.object.setStructure("Position", std::move(multi));
    events.push_back(object);
    events.push_back(iox::EndBasketEvent{"B1"});
    events.push_back(iox::EndTransferEvent{});

    const auto xml = writeXtf(events, iox::xtf::XtfVersion::Xtf24);
    IOX_CHECK(xml.find("ili:datasection") != std::string::npos);
    IOX_CHECK(xml.find("ili:bid=\"B1\"") != std::string::npos);
    IOX_CHECK(xml.find("geom:multicoord") != std::string::npos);
    IOX_CHECK(xml.find("geom:coord") != std::string::npos);

    const auto parsed = readXtf(xml);
    IOX_CHECK_EQ(static_cast<std::size_t>(5), parsed.size());
    const auto* parsedBasket = std::get_if<iox::StartBasketEvent>(&parsed[1]);
    IOX_CHECK(parsedBasket != nullptr);
    IOX_CHECK_EQ(std::string("M.Topic"), parsedBasket->basketType.iliName());
    const auto* parsedObject = std::get_if<iox::ObjectEvent>(&parsed[2]);
    IOX_CHECK(parsedObject != nullptr);
    const auto* position = parsedObject->object.findAttribute("Position");
    IOX_CHECK(position != nullptr);
    const auto* parsedMulti = std::get_if<iox::IomObject>(&position->values[0]);
    IOX_CHECK(parsedMulti != nullptr);
    IOX_CHECK_EQ(static_cast<std::size_t>(2), parsedMulti->findAttribute("coord")->values.size());
}

#include "iox/test/TestMain.h"
