#include "iox/Events.h"
#include "iox/Writer.h"
#include "iox/xtf/XtfReader.h"
#include "iox/xtf/XtfWriter.h"
#include "iox/test/Test.h"

#include <memory>
#include <string>
#include <vector>

namespace {

std::string writeXtf(const std::vector<iox::IoxEvent>& events,
                     iox::XtfVersion version) {
    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::xtf::XtfWriterOptions options;
    options.version = version;
    options.pretty = false;
    iox::xtf::XtfWriter writer(sink, options);
    for (const auto& event : events) writer.write(event);
    writer.close();
    return sink->str();
}

std::vector<iox::IoxEvent> readXtf(const std::string& input,
                                   iox::xtf::XtfReaderOptions options = {}) {
    iox::xtf::XtfReader reader(std::move(options));
    reader.feed(iox::ByteView(input));
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

iox::StartTransferEvent start(iox::XtfVersion version,
                              std::string sender = "S") {
    iox::StartTransferEvent event;
    event.header.version = version;
    event.header.sender = std::move(sender);
    return event;
}

} // namespace

IOX_TEST(xtf23_detect_version) {
    const auto xml = writeXtf(
        {start(iox::XtfVersion::V23), iox::EndTransferEvent{}},
        iox::XtfVersion::V23);
    iox::xtf::XtfReader reader;
    reader.feed(iox::ByteView(xml));
    reader.finish();
    IOX_CHECK(reader.detectedVersion().has_value());
    IOX_CHECK_EQ(iox::XtfVersion::V23, *reader.detectedVersion());
}

IOX_TEST(xtf23_read_header) {
    auto transfer = start(iox::XtfVersion::V23, "MySender");
    transfer.header.comment = "Test Comment";
    const auto parsed = readXtf(writeXtf(
        {transfer, iox::EndTransferEvent{}}, iox::XtfVersion::V23));
    const auto& header = std::get<iox::StartTransferEvent>(parsed.front()).header;
    IOX_CHECK_EQ(std::string("MySender"), header.sender);
    IOX_CHECK_EQ(std::string("Test Comment"), *header.comment);
}

IOX_TEST(xtf24_detect_version) {
    const auto xml = writeXtf(
        {start(iox::XtfVersion::V24), iox::EndTransferEvent{}},
        iox::XtfVersion::V24);
    iox::xtf::XtfReader reader;
    reader.feed(iox::ByteView(xml));
    reader.finish();
    IOX_CHECK_EQ(iox::XtfVersion::V24, *reader.detectedVersion());
}

IOX_TEST(xtf_roundtrip_minimal_23) {
    const auto parsed = readXtf(writeXtf(
        {start(iox::XtfVersion::V23), iox::EndTransferEvent{}},
        iox::XtfVersion::V23));
    IOX_CHECK_EQ(static_cast<std::size_t>(2), parsed.size());
    IOX_CHECK(std::holds_alternative<iox::StartTransferEvent>(parsed[0]));
    IOX_CHECK(std::holds_alternative<iox::EndTransferEvent>(parsed[1]));
}

IOX_TEST(xtf_roundtrip_minimal_24) {
    const auto parsed = readXtf(writeXtf(
        {start(iox::XtfVersion::V24), iox::EndTransferEvent{}},
        iox::XtfVersion::V24));
    IOX_CHECK_EQ(static_cast<std::size_t>(2), parsed.size());
}

IOX_TEST(xtf22_rejected) {
    // XTF 2.2 is deliberately outside the 0.2 contract.
    const std::string xml =
        "<?xml version=\"1.0\"?><TRANSFER xmlns=\"http://www.interlis.ch/INTERLIS2.2\"><HEADERSECTION SENDER=\"S\" VERSION=\"2.2\"/><DATASECTION/></TRANSFER>";
    iox::xtf::XtfReader reader;
    bool rejected = false;
    try {
        reader.feed(iox::ByteView(xml));
        reader.finish();
    } catch (const iox::IoxError& error) {
        rejected = error.code() == iox::DiagnosticCode::UnsupportedXtfVersion ||
                   error.code() == iox::DiagnosticCode::InvalidXtfNamespace ||
                   error.code() == iox::DiagnosticCode::XmlMalformed;
    }
    if (!rejected) {
        for (const auto& diagnostic : reader.takeDiagnostics()) {
            if (diagnostic.severity == iox::DiagnosticSeverity::Fatal) {
                rejected = true;
            }
        }
    }
    IOX_CHECK(rejected);
}

IOX_TEST(xtf_writer_state_machine) {
    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::xtf::XtfWriter writer(sink, {});
    bool threw = false;
    try {
        writer.write(iox::EndTransferEvent{});
    } catch (const iox::IoxError& error) {
        threw = error.code() == iox::DiagnosticCode::InvalidEventOrder;
    }
    IOX_CHECK(threw);

    bool terminal = false;
    try {
        writer.write(start(iox::XtfVersion::V24));
    } catch (const iox::IoxError&) {
        terminal = true;
    }
    IOX_CHECK(terminal);
}

IOX_TEST(xtf_writer_deterministic) {
    const std::vector<iox::IoxEvent> events{
        start(iox::XtfVersion::V23), iox::EndTransferEvent{}};
    IOX_CHECK_EQ(writeXtf(events, iox::XtfVersion::V23),
                 writeXtf(events, iox::XtfVersion::V23));
}

#include "iox/test/TestMain.h"
