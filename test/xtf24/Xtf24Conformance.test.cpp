#include "iox/Events.h"
#include "iox/Writer.h"
#include "iox/xtf/XtfReader.h"
#include "iox/xtf/XtfWriter.h"
#include "iox/test/Test.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

const std::string fullTransfer =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<ili:transfer xmlns:ili=\"http://www.interlis.ch/xtf/2.4/INTERLIS\" "
    "xmlns:geom=\"http://www.interlis.ch/geometry/1.0\" "
    "xmlns:m=\"http://www.interlis.ch/xtf/2.4/M\" "
    "xmlns:x=\"urn:extension\">"
    "<ili:headersection><ili:models><ili:model>M</ili:model></ili:models>"
    "<ili:sender>S</ili:sender><ili:comment>C</ili:comment>"
    "<x:header code=\"h\">opaque</x:header></ili:headersection>"
    "<ili:datasection><m:Topic ili:bid=\"B1\" ili:kind=\"UPDATE\" "
    "ili:startstate=\"s0\" ili:endstate=\"s1\" "
    "ili:domains=\"D1 D2\" ili:consistency=\"INCOMPLETE\" x:flag=\"yes\">"
    "<m:Class ili:tid=\"T1\" ili:operation=\"UPDATE\">"
    "<m:Lexical>001.2300</m:Lexical>"
    "<m:Reference ili:ref=\"R1\" ili:bid=\"B2\" ili:order_pos=\"2\"/>"
    "<m:Role ili:ref=\"P1\"><m:Association>"
    "<m:Value>embedded</m:Value></m:Association></m:Role>"
    "<m:Position><geom:coord><geom:c1>2600000.000</geom:c1>"
    "<geom:c2>1200000.000</geom:c2></geom:coord></m:Position>"
    "</m:Class></m:Topic></ili:datasection></ili:transfer>";

struct Parsed final {
    std::vector<iox::IoxEvent> events;
    std::vector<iox::Diagnostic> diagnostics;
};

Parsed parse(std::string_view input, iox::xtf::XtfReaderOptions options = {},
             const std::vector<std::size_t>& chunks = {}) {
    iox::xtf::XtfReader reader(std::move(options));
    std::size_t offset = 0;
    std::size_t chunkIndex = 0;
    while (offset < input.size()) {
        const auto requested = chunks.empty()
                                   ? input.size()
                                   : chunks[chunkIndex++ % chunks.size()];
        const auto count = std::min(requested, input.size() - offset);
        reader.feed(iox::ByteView(
            reinterpret_cast<const std::uint8_t*>(input.data() + offset),
            count));
        offset += count;
    }
    reader.finish();

    Parsed result;
    while (true) {
        auto outcome = reader.next();
        if (outcome.progress == iox::ReaderProgress::End) break;
        IOX_CHECK_EQ(iox::ReaderProgress::Event, outcome.progress);
        result.events.push_back(std::move(*outcome.event));
    }
    result.diagnostics = reader.takeDiagnostics();
    return result;
}

std::optional<iox::DiagnosticCode> rejectedBy(
    std::string_view input, iox::xtf::XtfReaderOptions options = {}) {
    try {
        (void)parse(input, std::move(options));
    } catch (const iox::IoxError& error) {
        return error.code();
    }
    return std::nullopt;
}

iox::StartTransferEvent transfer() {
    iox::StartTransferEvent result;
    result.header.version = iox::XtfVersion::V24;
    result.header.sender = "S";
    result.header.models.push_back(
        {"M", std::nullopt, std::nullopt, {"urn:m", "M", "m"}});
    return result;
}

std::string write(const std::vector<iox::IoxEvent>& events,
                  iox::xtf::XtfWriterOptions options = {}) {
    options.version = iox::XtfVersion::V24;
    options.pretty = false;
    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::xtf::XtfWriter writer(sink, std::move(options));
    for (const auto& event : events) writer.write(event);
    writer.close();
    return sink->str();
}

} // namespace

IOX_TEST(xtf24_reader_preserves_complete_metadata_and_values) {
    auto result = parse(fullTransfer, {}, {1U, 2U, 7U, 31U});
    IOX_CHECK_EQ(static_cast<std::size_t>(5), result.events.size());

    const auto& header =
        std::get<iox::StartTransferEvent>(result.events[0]).header;
    IOX_CHECK_EQ(iox::XtfVersion::V24, header.version);
    IOX_CHECK_EQ(std::string("S"), header.sender);
    IOX_CHECK_EQ(std::string("C"), *header.comment);
    IOX_CHECK_EQ(static_cast<std::size_t>(1), header.models.size());
    IOX_CHECK_EQ(std::string("M"), header.models[0].name);
    IOX_CHECK_EQ(std::string("http://www.interlis.ch/xtf/2.4/M"),
                 header.models[0].xmlNamespace.namespaceUri);
    IOX_CHECK_EQ(static_cast<std::size_t>(1), header.extensions.size());
    IOX_CHECK_EQ(std::string("opaque"), header.extensions[0].text);

    const auto& basket =
        std::get<iox::StartBasketEvent>(result.events[1]).basket;
    IOX_CHECK_EQ(std::string("M.Topic"), basket.topic.interlisName());
    IOX_CHECK_EQ(std::string("http://www.interlis.ch/xtf/2.4/M"),
                 basket.topic.xmlName().namespaceUri);
    IOX_CHECK_EQ(iox::BasketKind::Update, basket.kind);
    IOX_CHECK_EQ(iox::Consistency::Incomplete, basket.consistency);
    IOX_CHECK_EQ(std::string("s0"), *basket.startState);
    IOX_CHECK_EQ(std::string("s1"), *basket.endState);
    IOX_CHECK_EQ(static_cast<std::size_t>(2), basket.domains.size());
    IOX_CHECK_EQ(static_cast<std::size_t>(1), basket.extensions.size());
    IOX_CHECK(!basket.location.empty());

    const auto& object = std::get<iox::ObjectEvent>(result.events[2]).object;
    IOX_CHECK_EQ(std::string("M.Topic.Class"), object.tag().interlisName());
    IOX_CHECK_EQ(std::string("T1"), *object.oid());
    IOX_CHECK_EQ(iox::ObjectOperation::Update, object.operation());
    IOX_CHECK_EQ(std::string_view("001.2300"),
                 *object.primitive("Lexical"));
    IOX_CHECK(!object.sourceLocation().empty());

    const auto reference = object.object("Reference");
    IOX_CHECK(reference.has_value());
    IOX_CHECK_EQ(std::string("R1"), *reference->reference().targetOid);
    IOX_CHECK_EQ(std::string("B2"),
                 *reference->reference().targetBasketId);
    IOX_CHECK_EQ(std::uint64_t{2}, *reference->reference().orderPosition);

    const auto role = object.object("Role");
    IOX_CHECK(role.has_value());
    IOX_CHECK_EQ(std::string("P1"), *role->reference().targetOid);
    IOX_CHECK_EQ(std::string("Association"), role->tag().interlisName());
    IOX_CHECK_EQ(std::string_view("embedded"), *role->primitive("Value"));

    const auto position = object.object("Position");
    IOX_CHECK(position.has_value());
    IOX_CHECK_EQ(std::string("COORD"), position->tag().interlisName());
    IOX_CHECK_EQ(std::string_view("2600000.000"),
                 *position->primitive("C1"));
    IOX_CHECK(!result.diagnostics.empty());
}

IOX_TEST(xtf24_explicit_version_and_chunking_are_semantically_identical) {
    const auto automatic = parse(fullTransfer, {}, {fullTransfer.size()});
    iox::xtf::XtfReaderOptions explicitOptions;
    explicitOptions.expectedVersion = iox::XtfVersion::V24;
    explicitOptions.allowVersionAutoDetection = false;
    const auto explicitVersion =
        parse(fullTransfer, explicitOptions, {3U, 17U, 5U, 61U});

    IOX_CHECK_EQ(automatic.events.size(), explicitVersion.events.size());
    const auto& automaticObject =
        std::get<iox::ObjectEvent>(automatic.events[2]).object;
    const auto& explicitObject =
        std::get<iox::ObjectEvent>(explicitVersion.events[2]).object;
    IOX_CHECK(automaticObject.semanticallyEquals(explicitObject));
}

IOX_TEST(xtf24_strictness_extension_policy_and_root_are_observable) {
    auto nonCanonical = fullTransfer;
    const auto kind = nonCanonical.find("ili:kind=\"UPDATE\"");
    IOX_CHECK(kind != std::string::npos);
    nonCanonical.replace(kind, std::string("ili:kind=\"UPDATE\"").size(),
                         "ili:kind=\"update\"");

    const auto lenient = parse(nonCanonical);
    IOX_CHECK(!lenient.diagnostics.empty());
    iox::xtf::XtfReaderOptions strict;
    strict.strictness = iox::xtf::Strictness::Strict;
    IOX_CHECK_EQ(iox::DiagnosticCode::UnexpectedAttribute,
                 *rejectedBy(nonCanonical, strict));

    iox::xtf::XtfReaderOptions dropped;
    dropped.preserveUnknownExtensions = false;
    const auto withoutExtensions = parse(fullTransfer, dropped);
    const auto& header =
        std::get<iox::StartTransferEvent>(withoutExtensions.events[0]).header;
    IOX_CHECK(header.extensions.empty());
    IOX_CHECK(!withoutExtensions.diagnostics.empty());

    dropped.strictness = iox::xtf::Strictness::Strict;
    IOX_CHECK_EQ(iox::DiagnosticCode::UnexpectedElement,
                 *rejectedBy(fullTransfer, dropped));

    auto wrongRoot = fullTransfer;
    const auto rootOffset = wrongRoot.find("ili:transfer");
    wrongRoot.replace(rootOffset, std::string("ili:transfer").size(),
                      "ili:TRANSFER");
    IOX_CHECK_EQ(iox::DiagnosticCode::InvalidXtfNamespace,
                 *rejectedBy(wrongRoot));

    const std::string xtf22 =
        "<TRANSFER xmlns=\"http://www.interlis.ch/INTERLIS2.2\"/>";
    IOX_CHECK_EQ(iox::DiagnosticCode::UnsupportedXtfVersion,
                 *rejectedBy(xtf22));
}

IOX_TEST(xtf24_writer_matches_checked_minimal_golden) {
    const auto output = write({transfer(), iox::EndTransferEvent{}});
    const std::string expected =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<ili:transfer xmlns:ili=\"http://www.interlis.ch/xtf/2.4/INTERLIS\" "
        "xmlns:geom=\"http://www.interlis.ch/geometry/1.0\" "
        "xmlns:m=\"urn:m\"><ili:headersection><ili:models>"
        "<ili:model>M</ili:model></ili:models><ili:sender>S</ili:sender>"
        "</ili:headersection><ili:datasection/></ili:transfer>";
    IOX_CHECK_EQ(expected, output);
    IOX_CHECK_EQ(static_cast<std::size_t>(2), parse(output).events.size());
}

IOX_TEST(xtf24_writer_never_invents_a_model_namespace) {
    auto start = transfer();
    start.header.models[0].xmlNamespace = {};
    iox::StartBasketEvent basket;
    basket.basket.topic = iox::IomName("M.Topic");
    basket.basket.basketId = "B1";

    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::xtf::XtfWriterOptions options;
    options.version = iox::XtfVersion::V24;
    options.pretty = false;
    iox::xtf::XtfWriter writer(sink, options);
    writer.write(start);
    bool rejected = false;
    try {
        writer.write(basket);
    } catch (const iox::IoxError& error) {
        rejected = error.code() == iox::DiagnosticCode::UnknownInterlisName;
    }
    IOX_CHECK(rejected);
    IOX_CHECK(sink->str().find("http://www.interlis.ch/xtf/2.4/M") ==
              std::string::npos);
}

#include "iox/test/TestMain.h"
