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
    // Some minimal writer-state tests deliberately omit model declarations.
    options.requireAtLeastOneModel = false;
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

std::string transfer23(std::string headerChildren = {},
                       std::string dataChildren = {},
                       std::string headerAttributes =
                           " SENDER=\"sender\" VERSION=\"2.3\"") {
    return "<?xml version=\"1.0\"?>"
           "<TRANSFER xmlns=\"http://www.interlis.ch/INTERLIS2.3\">"
           "<HEADERSECTION" + headerAttributes + ">" + headerChildren +
           "</HEADERSECTION><DATASECTION>" + dataChildren +
           "</DATASECTION></TRANSFER>";
}

std::vector<iox::IoxEvent> drain(iox::xtf::XtfReader& reader) {
    std::vector<iox::IoxEvent> result;
    while (true) {
        auto outcome = reader.next();
        if (outcome.progress == iox::ReaderProgress::End) break;
        IOX_CHECK_EQ(iox::ReaderProgress::Event, outcome.progress);
        result.push_back(std::move(*outcome.event));
    }
    return result;
}

} // namespace

IOX_TEST(xtf23_detect_version) {
    const auto xml = writeXtf(
        {start(iox::XtfVersion::V23), iox::EndTransferEvent{}},
        iox::XtfVersion::V23);
    iox::xtf::XtfReaderOptions options;
    options.requireAtLeastOneModel = false;
    iox::xtf::XtfReader reader(options);
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

IOX_TEST(xtf23_header_preserves_models_oidspaces_alias_and_comment) {
    const auto xml = transfer23(
        "<MODELS><MODEL NAME=\"M1\" VERSION=\"2026\" URI=\"urn:m1\"/>"
        "<MODEL NAME=\"M2\"/></MODELS>"
        "<ALIAS><ENTRIES FOR=\"M1\"><TAGENTRY FROM=\"old\" TO=\"new\"/>"
        "</ENTRIES></ALIAS>"
        "<OIDSPACES><OIDSPACE NAME=\"uuid\" OIDDOMAIN=\"INTERLIS.UUIDOID\"/>"
        "</OIDSPACES><COMMENT>Grüezi 世界</COMMENT>");
    iox::xtf::XtfReader reader;
    reader.feed(iox::ByteView(xml));
    reader.finish();
    const auto events = drain(reader);
    const auto& header = std::get<iox::StartTransferEvent>(events.front()).header;
    IOX_CHECK_EQ(std::string("sender"), header.sender);
    IOX_CHECK_EQ(static_cast<std::size_t>(2), header.models.size());
    IOX_CHECK_EQ(std::string("M1"), header.models[0].name);
    IOX_CHECK_EQ(std::string("2026"), *header.models[0].version);
    IOX_CHECK_EQ(std::string("urn:m1"), *header.models[0].uri);
    IOX_CHECK(!header.models[1].version.has_value());
    IOX_CHECK_EQ(static_cast<std::size_t>(1), header.oidSpaces.size());
    IOX_CHECK_EQ(std::string("uuid"), header.oidSpaces[0].name);
    IOX_CHECK_EQ(std::string("INTERLIS.UUIDOID"),
                 header.oidSpaces[0].domain);
    IOX_CHECK_EQ(std::string("Grüezi 世界"), *header.comment);
    IOX_CHECK_EQ(static_cast<std::size_t>(1), header.extensions.size());
    IOX_CHECK_EQ(std::string("ALIAS"), header.extensions[0].name.localName);
    IOX_CHECK_EQ(std::string("ENTRIES"),
                 header.extensions[0].children[0].name.localName);
    IOX_CHECK(reader.takeDiagnostics().empty());
}

IOX_TEST(xtf23_reader_options_have_observable_effects) {
    const auto noModels = transfer23();
    bool modelRequired = false;
    try {
        iox::xtf::XtfReader reader;
        reader.feed(iox::ByteView(noModels));
    } catch (const iox::IoxError& error) {
        modelRequired = error.code() == iox::DiagnosticCode::MissingModelEntry;
    }
    IOX_CHECK(modelRequired);

    iox::xtf::XtfReaderOptions relaxed;
    relaxed.requireAtLeastOneModel = false;
    iox::xtf::XtfReader accepted(relaxed);
    accepted.feed(iox::ByteView(noModels));
    accepted.finish();
    IOX_CHECK_EQ(static_cast<std::size_t>(2), drain(accepted).size());

    bool requiresExpected = false;
    try {
        iox::xtf::XtfReaderOptions invalid;
        invalid.allowVersionAutoDetection = false;
        iox::xtf::XtfReader reader(invalid);
    } catch (const iox::IoxError& error) {
        requiresExpected = error.code() == iox::DiagnosticCode::InvalidArgument;
    }
    IOX_CHECK(requiresExpected);

    bool mismatch = false;
    try {
        iox::xtf::XtfReaderOptions expected;
        expected.expectedVersion = iox::XtfVersion::V24;
        expected.allowVersionAutoDetection = false;
        iox::xtf::XtfReader reader(expected);
        reader.feed(iox::ByteView(noModels));
    } catch (const iox::IoxError& error) {
        mismatch = error.code() == iox::DiagnosticCode::UnsupportedXtfVersion;
    }
    IOX_CHECK(mismatch);

    iox::xtf::XtfReaderOptions pinned;
    pinned.expectedVersion = iox::XtfVersion::V23;
    pinned.allowVersionAutoDetection = false;
    pinned.requireAtLeastOneModel = false;
    iox::xtf::XtfReader pinnedReader(pinned);
    pinnedReader.feed(iox::ByteView(noModels));
    pinnedReader.finish();
    IOX_CHECK_EQ(static_cast<std::size_t>(2), drain(pinnedReader).size());

    bool strictModelMetadata = false;
    try {
        iox::xtf::XtfReaderOptions strict;
        strict.strictness = iox::xtf::Strictness::Strict;
        iox::xtf::XtfReader strictReader(strict);
        const auto missingUri = transfer23(
            "<MODELS><MODEL NAME=\"M\"/></MODELS>");
        strictReader.feed(iox::ByteView(missingUri));
    } catch (const iox::IoxError& error) {
        strictModelMetadata =
            error.code() == iox::DiagnosticCode::MissingModelEntry;
    }
    IOX_CHECK(strictModelMetadata);

    const auto legacyChildren = transfer23(
        "<SENDER>legacy</SENDER><VERSION>2.3</VERSION>", {}, "");
    iox::xtf::XtfReaderOptions legacyOptions;
    legacyOptions.requireAtLeastOneModel = false;
    iox::xtf::XtfReader legacyReader(legacyOptions);
    legacyReader.feed(iox::ByteView(legacyChildren));
    legacyReader.finish();
    const auto legacyEvents = drain(legacyReader);
    IOX_CHECK_EQ(std::string("legacy"),
                 std::get<iox::StartTransferEvent>(legacyEvents.front())
                     .header.sender);
    IOX_CHECK_EQ(static_cast<std::size_t>(2),
                 legacyReader.takeDiagnostics().size());

    bool strictLegacyRejected = false;
    try {
        iox::xtf::XtfReaderOptions strict;
        strict.strictness = iox::xtf::Strictness::Strict;
        strict.requireAtLeastOneModel = false;
        iox::xtf::XtfReader strictReader(strict);
        strictReader.feed(iox::ByteView(legacyChildren));
    } catch (const iox::IoxError& error) {
        strictLegacyRejected =
            error.code() == iox::DiagnosticCode::UnexpectedElement;
    }
    IOX_CHECK(strictLegacyRejected);
}

IOX_TEST(xtf23_extension_policy_is_lenient_transparent_and_strict) {
    const auto xml = transfer23(
        "<MODELS><MODEL NAME=\"M\" VERSION=\"1\" URI=\"urn:m\"/>"
        "</MODELS>"
        "<VENDOR flag=\"yes\"><VALUE>42</VALUE></VENDOR>");
    iox::xtf::XtfReader preserved;
    preserved.feed(iox::ByteView(xml));
    preserved.finish();
    const auto events = drain(preserved);
    const auto& header = std::get<iox::StartTransferEvent>(events.front()).header;
    IOX_CHECK_EQ(static_cast<std::size_t>(1), header.extensions.size());
    IOX_CHECK_EQ(std::string("VENDOR"), header.extensions[0].name.localName);
    const auto preservedDiagnostics = preserved.takeDiagnostics();
    IOX_CHECK_EQ(static_cast<std::size_t>(1), preservedDiagnostics.size());
    IOX_CHECK_EQ(iox::DiagnosticCode::UnknownExtensionPreserved,
                 preservedDiagnostics[0].code);

    iox::xtf::XtfReaderOptions droppedOptions;
    droppedOptions.preserveUnknownExtensions = false;
    iox::xtf::XtfReader dropped(droppedOptions);
    dropped.feed(iox::ByteView(xml));
    dropped.finish();
    const auto droppedEvents = drain(dropped);
    IOX_CHECK(std::get<iox::StartTransferEvent>(droppedEvents.front())
                  .header.extensions.empty());
    IOX_CHECK_EQ(iox::DiagnosticSeverity::Error,
                 dropped.takeDiagnostics().front().severity);

    bool strictFailure = false;
    try {
        iox::xtf::XtfReaderOptions strictOptions;
        strictOptions.strictness = iox::xtf::Strictness::Strict;
        strictOptions.preserveUnknownExtensions = false;
        iox::xtf::XtfReader strict(strictOptions);
        strict.feed(iox::ByteView(xml));
    } catch (const iox::IoxError& error) {
        strictFailure = error.code() == iox::DiagnosticCode::UnexpectedElement;
    }
    IOX_CHECK(strictFailure);
}

#include "iox/test/TestMain.h"
