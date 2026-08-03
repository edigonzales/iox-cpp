#include "iox/Events.h"
#include "iox/Writer.h"
#include "iox/xtf/XtfReader.h"
#include "iox/xtf/XtfWriter.h"
#include "iox/test/Test.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

iox::IomObject coordinate(double c1, double c2, double c3 = 0.0) {
    iox::IomObject result(iox::IomName("COORD"));
    result.setPrimitive(iox::IomName("C1"), std::to_string(c1));
    result.setPrimitive(iox::IomName("C2"), std::to_string(c2));
    result.setPrimitive(iox::IomName("C3"), std::to_string(c3));
    return result;
}

iox::IomObject polyline(
    const std::vector<std::pair<double, double>>& points) {
    iox::IomObject segments(iox::IomName("SEGMENTS"));
    for (const auto& point : points) {
        segments.appendObject(iox::IomName("segment"),
                              coordinate(point.first, point.second));
    }
    iox::IomObject result(iox::IomName("POLYLINE"));
    result.setObject(iox::IomName("sequence"), segments);
    return result;
}

std::vector<iox::IoxEvent> roundtrip(std::string attribute,
                                     iox::IomObject geometry) {
    iox::StartTransferEvent transfer;
    transfer.header.version = iox::XtfVersion::V23;
    transfer.header.sender = "geometry-test";
    iox::StartBasketEvent basket;
    basket.basket.topic = iox::IomName("M.T.B");
    basket.basket.basketId = "B1";
    iox::ObjectEvent object;
    object.object = iox::IomObject(iox::IomName("M.T.Geometry"), "TID1");
    object.object.setObject(iox::IomName(std::move(attribute)),
                            std::move(geometry));

    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::xtf::XtfWriterOptions options;
    options.version = iox::XtfVersion::V23;
    options.pretty = false;
    iox::xtf::XtfWriter writer(sink, options);
    for (const iox::IoxEvent& event :
         std::vector<iox::IoxEvent>{transfer, basket, object,
                                    iox::EndBasketEvent{},
                                    iox::EndTransferEvent{}}) {
        writer.write(event);
    }
    writer.close();

    iox::xtf::XtfReader reader;
    reader.feed(iox::ByteView(sink->str()));
    reader.finish();
    std::vector<iox::IoxEvent> events;
    while (true) {
        auto outcome = reader.next();
        if (outcome.progress == iox::ReaderProgress::End) break;
        IOX_CHECK_EQ(iox::ReaderProgress::Event, outcome.progress);
        events.push_back(std::move(*outcome.event));
    }
    return events;
}

} // namespace

IOX_TEST(xtf23_geometry_coord) {
    const auto events = roundtrip(
        "Location", coordinate(2600000.0, 1200000.0, 500.0));
    const auto geometry =
        std::get<iox::ObjectEvent>(events[2]).object.object("Location");
    IOX_CHECK(geometry.has_value());
    IOX_CHECK_EQ(std::string("COORD"), geometry->tag().interlisName());
    IOX_CHECK_EQ(std::string_view("2600000.000000"),
                 *geometry->primitive("C1"));
}

IOX_TEST(xtf23_geometry_polyline) {
    const auto events = roundtrip(
        "Line", polyline({{2600000, 1200000}, {2600100, 1200100},
                           {2600200, 1200000}}));
    const auto geometry =
        std::get<iox::ObjectEvent>(events[2]).object.object("Line");
    IOX_CHECK(geometry.has_value());
    IOX_CHECK_EQ(std::string("POLYLINE"), geometry->tag().interlisName());
    const auto sequence = geometry->object("sequence");
    IOX_CHECK(sequence.has_value());
    IOX_CHECK_EQ(static_cast<std::size_t>(3),
                 sequence->valueCount("segment"));
}

IOX_TEST(xtf23_geometry_surface) {
    iox::IomObject boundary(iox::IomName("BOUNDARY"));
    boundary.setObject(iox::IomName("polyline"),
                       polyline({{0, 0}, {100, 0}, {100, 100},
                                 {0, 100}, {0, 0}}));
    iox::IomObject surface(iox::IomName("SURFACE"));
    surface.setObject(iox::IomName("boundary"), boundary);
    const auto events = roundtrip("Area", surface);
    const auto geometry =
        std::get<iox::ObjectEvent>(events[2]).object.object("Area");
    IOX_CHECK(geometry.has_value());
    IOX_CHECK_EQ(std::string("SURFACE"), geometry->tag().interlisName());
    IOX_CHECK(geometry->object("boundary").has_value());
}

IOX_TEST(xtf23_geometry_incomplete_polyline) {
    iox::IomObject first(iox::IomName("SEGMENTS"));
    first.appendObject(iox::IomName("segment"), coordinate(0, 0));
    first.appendObject(iox::IomName("segment"), coordinate(10, 10));
    iox::IomObject second(iox::IomName("SEGMENTS"));
    second.appendObject(iox::IomName("segment"), coordinate(20, 20));
    second.appendObject(iox::IomName("segment"), coordinate(30, 30));
    iox::IomObject line(iox::IomName("POLYLINE"));
    line.appendObject(iox::IomName("sequence"), first);
    line.appendObject(iox::IomName("sequence"), second);

    const auto events = roundtrip("Geom", line);
    const auto geometry =
        std::get<iox::ObjectEvent>(events[2]).object.object("Geom");
    IOX_CHECK(geometry.has_value());
    IOX_CHECK_EQ(static_cast<std::size_t>(2),
                 geometry->valueCount("sequence"));
}

#include "iox/test/TestMain.h"
