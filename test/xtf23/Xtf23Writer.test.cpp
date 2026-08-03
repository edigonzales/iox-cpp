#include "iox/Events.h"
#include "iox/Writer.h"
#include "iox/xtf/XtfReader.h"
#include "iox/xtf/XtfWriter.h"
#include "iox/test/Test.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr const char* xtf23Namespace =
    "http://www.interlis.ch/INTERLIS2.3";

iox::StartTransferEvent transferHeader() {
    iox::StartTransferEvent event;
    event.header.version = iox::XtfVersion::V23;
    event.header.sender = "golden";
    event.header.comment = "Grüezi";
    event.header.models.push_back({"M", "2026", "urn:m", {}});
    event.header.oidSpaces.push_back({"uuid", "INTERLIS.UUIDOID"});

    iox::ExtensionElement alias;
    alias.name = {xtf23Namespace, "ALIAS", {}};
    iox::ExtensionElement entries;
    entries.name = {xtf23Namespace, "ENTRIES", {}};
    entries.attributes.push_back({{{}, "FOR", {}}, "M"});
    iox::ExtensionElement tagEntry;
    tagEntry.name = {xtf23Namespace, "TAGENTRY", {}};
    tagEntry.attributes.push_back({{{}, "FROM", {}}, "old"});
    tagEntry.attributes.push_back({{{}, "TO", {}}, "new"});
    entries.children.push_back(std::move(tagEntry));
    alias.children.push_back(std::move(entries));
    event.header.extensions.push_back(std::move(alias));
    return event;
}

iox::StartBasketEvent basket() {
    iox::StartBasketEvent event;
    event.basket.topic = iox::IomName("M.T");
    event.basket.basketId = "B1";
    event.basket.kind = iox::BasketKind::Update;
    event.basket.startState = "s0";
    event.basket.endState = "s1";
    event.basket.consistency = iox::Consistency::Incomplete;
    event.basket.topics = {"M.T2", "M.T3"};
    return event;
}

iox::IomObject reference(std::string oid,
                         std::string bid = {},
                         std::uint64_t order = 0U) {
    iox::IomObject value(iox::IomName("REFERENCE"));
    iox::ReferenceInfo info;
    info.targetOid = std::move(oid);
    if (!bid.empty()) info.targetBasketId = std::move(bid);
    if (order != 0U) info.orderPosition = order;
    value.setReference(std::move(info));
    return value;
}

std::string write(const std::vector<iox::IoxEvent>& events,
                  iox::xtf::XtfWriterOptions options = {}) {
    options.version = iox::XtfVersion::V23;
    options.pretty = false;
    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::xtf::XtfWriter writer(sink, options);
    for (const auto& event : events) writer.write(event);
    writer.close();
    IOX_CHECK(writer.isClosed());
    IOX_CHECK(writer.takeDiagnostics().empty());
    return sink->str();
}

std::vector<iox::IoxEvent> read(const std::string& xml) {
    iox::xtf::XtfReader reader;
    reader.feed(iox::ByteView(xml));
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

iox::IomObject coordinate(std::string c1, std::string c2) {
    iox::IomObject result(iox::IomName("COORD"));
    result.setPrimitive(iox::IomName("C1"), std::move(c1));
    result.setPrimitive(iox::IomName("C2"), std::move(c2));
    return result;
}

iox::IomObject arc() {
    iox::IomObject result(iox::IomName("ARC"));
    result.setPrimitive(iox::IomName("C1"), "3");
    result.setPrimitive(iox::IomName("C2"), "4");
    result.setPrimitive(iox::IomName("A1"), "2.5");
    result.setPrimitive(iox::IomName("A2"), "3.5");
    result.setPrimitive(iox::IomName("R"), "07.0");
    return result;
}

class ZeroWriteSink final : public iox::OutputSink {
public:
    std::size_t write(const void*, std::size_t) override { return 0U; }
};

class ThrowOnCloseSink final : public iox::OutputSink {
public:
    std::size_t write(const void* data, std::size_t size) override {
        buffer_.append(static_cast<const char*>(data), size);
        return size;
    }
    void close() override { throw std::runtime_error("close marker"); }

private:
    std::string buffer_;
};

} // namespace

IOX_TEST(xtf23_writer_matches_independently_checked_metadata_object_golden) {
    auto start = transferHeader();
    auto startBasket = basket();

    iox::ObjectEvent object;
    object.object = iox::IomObject(iox::IomName("M.T.C"), "O1");
    object.object.setOperation(iox::ObjectOperation::Update);
    object.object.setConsistency(iox::Consistency::Adapted);
    object.object.setReference({std::nullopt, "B0", std::nullopt});
    object.object.setPrimitive(iox::IomName("number"), "001.2300");
    object.object.setObject(iox::IomName("role"),
                            reference("R1", "B2", 7U));

    iox::IomObject embedded(iox::IomName("M.T.Assoc"));
    embedded.setReference({"R2", std::nullopt, std::nullopt});
    embedded.setPrimitive(iox::IomName("weight"), "04.0");
    object.object.setObject(iox::IomName("embedded"), std::move(embedded));

    iox::IomObject oidValue(iox::IomName("OID"), "uuid:1");
    object.object.setObject(iox::IomName("oidAttr"), std::move(oidValue));

    iox::IomObject structure(iox::IomName("M.T.S"));
    structure.setPrimitive(iox::IomName("value"), "x");
    object.object.setObject(iox::IomName("item"), std::move(structure));

    iox::ObjectEvent deleted;
    deleted.object = iox::IomObject(iox::IomName("DELETE"), "D1");
    deleted.object.setOperation(iox::ObjectOperation::Delete);
    deleted.object.setObject(iox::IomName("role"), reference("R9"));

    const auto output = write(
        {start, startBasket, object, deleted, iox::EndBasketEvent{},
         iox::EndTransferEvent{}});
    const std::string expected =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<TRANSFER xmlns=\"http://www.interlis.ch/INTERLIS2.3\">"
        "<HEADERSECTION VERSION=\"2.3\" SENDER=\"golden\">"
        "<MODELS><MODEL NAME=\"M\" VERSION=\"2026\" URI=\"urn:m\"/>"
        "</MODELS><ALIAS><ENTRIES FOR=\"M\"><TAGENTRY FROM=\"old\" "
        "TO=\"new\"/></ENTRIES></ALIAS><OIDSPACES><OIDSPACE NAME=\"uuid\" "
        "OIDDOMAIN=\"INTERLIS.UUIDOID\"/></OIDSPACES><COMMENT>Grüezi</COMMENT>"
        "</HEADERSECTION><DATASECTION><M.T BID=\"B1\" KIND=\"UPDATE\" "
        "STARTSTATE=\"s0\" ENDSTATE=\"s1\" CONSISTENCY=\"INCOMPLETE\" "
        "TOPICS=\"M.T2,M.T3\"><M.T.C TID=\"O1\" BID=\"B0\" "
        "OPERATION=\"UPDATE\" CONSISTENCY=\"ADAPTED\">"
        "<number>001.2300</number><role REF=\"R1\" BID=\"B2\" "
        "ORDER_POS=\"7\"/><embedded REF=\"R2\"><M.T.Assoc>"
        "<weight>04.0</weight></M.T.Assoc></embedded>"
        "<oidAttr OID=\"uuid:1\"/><item><M.T.S><value>x</value>"
        "</M.T.S></item></M.T.C><DELETE TID=\"D1\"><role REF=\"R9\"/>"
        "</DELETE></M.T></DATASECTION></TRANSFER>";
    IOX_CHECK_EQ(expected, output);

    const auto events = read(output);
    IOX_CHECK_EQ(static_cast<std::size_t>(6), events.size());
    const auto& parsed = std::get<iox::ObjectEvent>(events[2]).object;
    IOX_CHECK_EQ(std::string_view("001.2300"), *parsed.primitive("number"));
    IOX_CHECK_EQ(std::string("R2"),
                 *parsed.object("embedded")->reference().targetOid);
    IOX_CHECK_EQ(std::string_view("04.0"),
                 *parsed.object("embedded")->primitive("weight"));
    IOX_CHECK_EQ(std::string("uuid:1"), *parsed.object("oidAttr")->oid());
}

IOX_TEST(xtf23_writer_matches_normative_clipped_geometry_golden) {
    auto start = transferHeader();
    auto startBasket = basket();
    startBasket.basket.kind = iox::BasketKind::Full;
    startBasket.basket.startState.reset();
    startBasket.basket.endState.reset();
    startBasket.basket.consistency = iox::Consistency::Complete;
    startBasket.basket.topics.clear();

    iox::IomObject line(iox::IomName("POLYLINE"));
    line.setConsistency(iox::Consistency::Incomplete);
    iox::IomObject lineAttr(iox::IomName("M.LineAttr"));
    lineAttr.setPrimitive(iox::IomName("width"), "001.20");
    line.setObject(iox::IomName("lineattr"), std::move(lineAttr));
    iox::IomObject first(iox::IomName("SEGMENTS"));
    first.appendObject(iox::IomName("segment"), coordinate("1", "2"));
    iox::IomObject second(iox::IomName("SEGMENTS"));
    second.appendObject(iox::IomName("segment"), arc());
    line.appendObject(iox::IomName("sequence"), std::move(first));
    line.appendObject(iox::IomName("sequence"), std::move(second));

    iox::ObjectEvent object;
    object.object = iox::IomObject(iox::IomName("M.T.C"), "O1");
    object.object.setObject(iox::IomName("geom"), std::move(line));

    iox::IomObject ringSegments(iox::IomName("SEGMENTS"));
    ringSegments.appendObject(iox::IomName("segment"), coordinate("0", "0"));
    iox::IomObject ring(iox::IomName("POLYLINE"));
    ring.setObject(iox::IomName("sequence"), std::move(ringSegments));
    iox::IomObject boundary(iox::IomName("BOUNDARY"));
    boundary.setObject(iox::IomName("polyline"), std::move(ring));
    iox::IomObject clipped(iox::IomName("BOUNDARIES"));
    clipped.setObject(iox::IomName("boundary"), std::move(boundary));
    iox::IomObject surface(iox::IomName("SURFACE"));
    surface.setConsistency(iox::Consistency::Incomplete);
    surface.setObject(iox::IomName("clipped"), std::move(clipped));
    object.object.setObject(iox::IomName("surface"), std::move(surface));
    const auto output = write(
        {start, startBasket, object, iox::EndBasketEvent{},
         iox::EndTransferEvent{}});
    const std::string geometryGolden =
        "<geom><POLYLINE><LINEATTR><M.LineAttr><width>001.20</width>"
        "</M.LineAttr></LINEATTR><CLIPPED><COORD><C1>1</C1><C2>2</C2>"
        "</COORD></CLIPPED><CLIPPED><ARC><C1>3</C1><C2>4</C2>"
        "<A1>2.5</A1><A2>3.5</A2><R>07.0</R></ARC></CLIPPED>"
        "</POLYLINE></geom>";
    IOX_CHECK(output.find(geometryGolden) != std::string::npos);
    const std::string surfaceGolden =
        "<surface><SURFACE><CLIPPED><BOUNDARY><POLYLINE><COORD>"
        "<C1>0</C1><C2>0</C2></COORD></POLYLINE></BOUNDARY></CLIPPED>"
        "</SURFACE></surface>";
    IOX_CHECK(output.find(surfaceGolden) != std::string::npos);

    const auto events = read(output);
    const auto geometry =
        std::get<iox::ObjectEvent>(events[2]).object.object("geom");
    IOX_CHECK(geometry.has_value());
    IOX_CHECK_EQ(iox::Consistency::Incomplete, geometry->consistency());
    IOX_CHECK_EQ(static_cast<std::size_t>(2),
                 geometry->valueCount("sequence"));
    const auto parsedSurface =
        std::get<iox::ObjectEvent>(events[2]).object.object("surface");
    IOX_CHECK(parsedSurface.has_value());
    IOX_CHECK_EQ(iox::Consistency::Incomplete, parsedSurface->consistency());
    IOX_CHECK_EQ(static_cast<std::size_t>(1),
                 parsedSurface->valueCount("clipped"));
}

IOX_TEST(xtf23_writer_state_errors_and_sink_failures_are_terminal) {
    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::xtf::XtfWriter invalidOrder(sink, {});
    bool orderFailure = false;
    try {
        invalidOrder.write(iox::EndTransferEvent{});
    } catch (const iox::IoxError& error) {
        orderFailure = error.code() == iox::DiagnosticCode::WriterStateError;
    }
    IOX_CHECK(orderFailure);
    bool terminal = false;
    try {
        invalidOrder.write(transferHeader());
    } catch (const iox::IoxError& error) {
        terminal = error.code() == iox::DiagnosticCode::WriterStateError;
    }
    IOX_CHECK(terminal);

    auto missingIdSink = std::make_shared<iox::StringOutputSink>();
    iox::xtf::XtfWriter missingId(missingIdSink, {});
    missingId.write(transferHeader());
    missingId.write(basket());
    iox::ObjectEvent anonymous;
    anonymous.object = iox::IomObject(iox::IomName("M.T.C"));
    try {
        missingId.write(anonymous);
        IOX_CHECK(false);
    } catch (const iox::IoxError& error) {
        IOX_CHECK_EQ(iox::DiagnosticCode::MissingObjectId, error.code());
    }
    try {
        missingId.write(iox::EndBasketEvent{});
        IOX_CHECK(false);
    } catch (const iox::IoxError& error) {
        IOX_CHECK_EQ(iox::DiagnosticCode::WriterStateError, error.code());
    }

    auto incompleteSink = std::make_shared<iox::StringOutputSink>();
    iox::xtf::XtfWriter incomplete(incompleteSink, {});
    incomplete.write(transferHeader());
    bool closeRejected = false;
    try {
        incomplete.close();
    } catch (const iox::IoxError& error) {
        closeRejected = error.code() == iox::DiagnosticCode::WriterStateError;
    }
    IOX_CHECK(closeRejected);
    IOX_CHECK(incompleteSink->str().find("</TRANSFER>") == std::string::npos);

    auto zero = std::make_shared<ZeroWriteSink>();
    iox::xtf::XtfWriter failedSink(zero, {});
    bool ioFailure = false;
    try {
        failedSink.write(transferHeader());
    } catch (const iox::IoxError& error) {
        ioFailure = error.code() == iox::DiagnosticCode::IoError;
    }
    IOX_CHECK(ioFailure);
    terminal = false;
    try {
        failedSink.close();
    } catch (const iox::IoxError& error) {
        terminal = error.code() == iox::DiagnosticCode::WriterStateError;
    }
    IOX_CHECK(terminal);

    auto closeSink = std::make_shared<ThrowOnCloseSink>();
    iox::xtf::XtfWriter closeFailure(closeSink, {});
    closeFailure.write(transferHeader());
    closeFailure.write(iox::EndTransferEvent{});
    try {
        closeFailure.close();
        IOX_CHECK(false);
    } catch (const iox::IoxError& error) {
        IOX_CHECK_EQ(iox::DiagnosticCode::IoError, error.code());
    }

    auto completeSink = std::make_shared<iox::StringOutputSink>();
    iox::xtf::XtfWriter complete(completeSink, {});
    complete.write(transferHeader());
    complete.write(iox::EndTransferEvent{});
    complete.close();
    complete.close();
    IOX_CHECK(complete.isClosed());
    IOX_CHECK(completeSink->str().find("<DATASECTION/>") != std::string::npos);
}

IOX_TEST(xtf23_writer_extension_policy_and_strict_validation_are_observable) {
    auto start = transferHeader();
    iox::ExtensionElement vendor;
    vendor.name = {"urn:vendor", "extra", "v"};
    vendor.text = "42";
    start.header.extensions.push_back(vendor);

    iox::xtf::XtfWriterOptions preservedOptions;
    preservedOptions.version = iox::XtfVersion::V23;
    preservedOptions.pretty = false;
    auto preservedSink = std::make_shared<iox::StringOutputSink>();
    iox::xtf::XtfWriter preserved(preservedSink, preservedOptions);
    preserved.write(start);
    preserved.write(iox::EndTransferEvent{});
    preserved.close();
    IOX_CHECK(preservedSink->str().find("urn:vendor") != std::string::npos);
    const auto preservedDiagnostics = preserved.takeDiagnostics();
    IOX_CHECK_EQ(static_cast<std::size_t>(1), preservedDiagnostics.size());
    IOX_CHECK_EQ(iox::DiagnosticCode::UnknownExtensionPreserved,
                 preservedDiagnostics.front().code);

    iox::xtf::XtfWriterOptions droppedOptions;
    droppedOptions.version = iox::XtfVersion::V23;
    droppedOptions.pretty = false;
    droppedOptions.preserveUnknownExtensions = false;
    auto droppedSink = std::make_shared<iox::StringOutputSink>();
    iox::xtf::XtfWriter dropped(droppedSink, droppedOptions);
    dropped.write(start);
    dropped.write(iox::EndTransferEvent{});
    dropped.close();
    IOX_CHECK(droppedSink->str().find("urn:vendor") == std::string::npos);
    const auto diagnostics = dropped.takeDiagnostics();
    IOX_CHECK_EQ(static_cast<std::size_t>(1), diagnostics.size());
    IOX_CHECK_EQ(iox::DiagnosticCode::UnexpectedElement,
                 diagnostics.front().code);

    iox::xtf::XtfWriterOptions strictOptions = droppedOptions;
    strictOptions.strictness = iox::xtf::Strictness::Strict;
    auto strictSink = std::make_shared<iox::StringOutputSink>();
    iox::xtf::XtfWriter strict(strictSink, strictOptions);
    bool strictRejected = false;
    try {
        strict.write(start);
    } catch (const iox::IoxError& error) {
        strictRejected = error.code() == iox::DiagnosticCode::UnexpectedElement;
    }
    IOX_CHECK(strictRejected);
}

#include "iox/test/TestMain.h"
