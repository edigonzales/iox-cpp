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

    iox::xtf::XtfReaderOptions readerOptions;
    readerOptions.requireAtLeastOneModel = false;
    iox::xtf::XtfReader reader(readerOptions);
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

std::string geometryDocument(std::string geometry) {
    return "<?xml version=\"1.0\"?><TRANSFER "
           "xmlns=\"http://www.interlis.ch/INTERLIS2.3\">"
           "<HEADERSECTION SENDER=\"geometry\" VERSION=\"2.3\">"
           "<MODELS><MODEL NAME=\"M\" VERSION=\"1\" URI=\"urn:m\"/>"
           "</MODELS>"
           "</HEADERSECTION>"
           "<DATASECTION><M.T BID=\"B\"><M.T.C TID=\"O\"><geom>" +
           geometry + "</geom></M.T.C></M.T></DATASECTION></TRANSFER>";
}

std::vector<iox::IoxEvent> parseGeometry(
    const std::string& xml, iox::xtf::Strictness strictness) {
    iox::xtf::XtfReaderOptions options;
    options.strictness = strictness;
    iox::xtf::XtfReader reader(options);
    reader.feed(iox::ByteView(xml));
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

IOX_TEST(xtf23_normative_polyline_normalizes_segments_without_numbers) {
    const auto events = parseGeometry(
        geometryDocument(
            "<POLYLINE><COORD><C1>001.00</C1><C2>2</C2></COORD>"
            "<ARC><C1>3</C1><C2>4</C2><A1>5</A1><A2>6</A2>"
            "<R>07.0</R></ARC></POLYLINE>"),
        iox::xtf::Strictness::Strict);
    const auto line = std::get<iox::ObjectEvent>(events[2])
                          .object.object("geom");
    IOX_CHECK(line.has_value());
    IOX_CHECK_EQ(std::string("POLYLINE"), line->tag().interlisName());
    const auto sequence = line->object("sequence");
    IOX_CHECK(sequence.has_value());
    IOX_CHECK_EQ(static_cast<std::size_t>(2),
                 sequence->valueCount("segment"));
    IOX_CHECK_EQ(std::string_view("001.00"),
                 *sequence->object("segment", 0)->primitive("C1"));
    IOX_CHECK_EQ(std::string_view("07.0"),
                 *sequence->object("segment", 1)->primitive("R"));
}

IOX_TEST(xtf23_normative_line_attributes_and_clipping_are_preserved) {
    const auto events = parseGeometry(
        geometryDocument(
            "<POLYLINE><LINEATTR><M.LineAttr><width>001.20</width>"
            "</M.LineAttr></LINEATTR>"
            "<CLIPPED><COORD><C1>1</C1><C2>2</C2></COORD>"
            "<ARC><C1>3</C1><C2>4</C2><A1>2</A1><A2>3</A2>"
            "</ARC></CLIPPED>"
            "<CLIPPED><COORD><C1>5</C1><C2>6</C2></COORD>"
            "</CLIPPED></POLYLINE>"),
        iox::xtf::Strictness::Strict);
    const auto line = std::get<iox::ObjectEvent>(events[2])
                          .object.object("geom");
    IOX_CHECK(line.has_value());
    IOX_CHECK_EQ(iox::Consistency::Incomplete, line->consistency());
    IOX_CHECK_EQ(static_cast<std::size_t>(2), line->valueCount("sequence"));
    const auto lineAttr = line->object("lineattr");
    IOX_CHECK(lineAttr.has_value());
    IOX_CHECK_EQ(std::string("M.LineAttr"), lineAttr->tag().interlisName());
    IOX_CHECK_EQ(std::string_view("001.20"), *lineAttr->primitive("width"));
}

IOX_TEST(xtf23_normative_clipped_surface_groups_are_preserved) {
    const auto events = parseGeometry(
        geometryDocument(
            "<SURFACE><CLIPPED><BOUNDARY><POLYLINE>"
            "<COORD><C1>1</C1><C2>2</C2></COORD>"
            "</POLYLINE></BOUNDARY></CLIPPED>"
            "<CLIPPED><BOUNDARY><POLYLINE>"
            "<COORD><C1>3</C1><C2>4</C2></COORD>"
            "</POLYLINE></BOUNDARY></CLIPPED></SURFACE>"),
        iox::xtf::Strictness::Strict);
    const auto surface = std::get<iox::ObjectEvent>(events[2])
                             .object.object("geom");
    IOX_CHECK(surface.has_value());
    IOX_CHECK_EQ(iox::Consistency::Incomplete, surface->consistency());
    IOX_CHECK_EQ(static_cast<std::size_t>(2),
                 surface->valueCount("clipped"));
    IOX_CHECK(surface->object("clipped", 0)->object("boundary").has_value());
}

IOX_TEST(xtf23_invalid_geometry_is_diagnostic_or_fatal_by_mode) {
    const auto xml = geometryDocument("<POLYLINE/>");
    iox::xtf::XtfReader lenient;
    lenient.feed(iox::ByteView(xml));
    lenient.finish();
    while (lenient.next().progress == iox::ReaderProgress::Event) {}
    const auto diagnostics = lenient.takeDiagnostics();
    IOX_CHECK_EQ(static_cast<std::size_t>(1), diagnostics.size());
    IOX_CHECK_EQ(iox::DiagnosticCode::InvalidGeometry,
                 diagnostics.front().code);

    bool strictFailure = false;
    try {
        iox::xtf::XtfReaderOptions options;
        options.strictness = iox::xtf::Strictness::Strict;
        iox::xtf::XtfReader strict(options);
        strict.feed(iox::ByteView(xml));
    } catch (const iox::IoxError& error) {
        strictFailure = error.code() == iox::DiagnosticCode::InvalidGeometry;
    }
    IOX_CHECK(strictFailure);
}

#include "iox/test/TestMain.h"
