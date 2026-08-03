/// Conformance tests using iox-ili test fixtures.
/// Fixtures sourced from https://github.com/claeis/iox-ili (MIT License)
/// under src/test/data/Xtf23Reader/

#include "iox/xtf/XtfReader.h"
#include "iox/xtf/XtfWriter.h"
#include "iox/xtf/XtfVersion.h"
#include "iox/Events.h"
#include "iox/Writer.h"
#include "iox/test/Test.h"

#include <memory>
#include <cctype>
#include <cstdint>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <functional>
#include <cstring>
#include <optional>

// ============================================================================
// Helpers
// ============================================================================

static std::string readFixture(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return "";
    auto size = file.tellg();
    file.seekg(0);
    std::string data(static_cast<std::size_t>(size), '\0');
    file.read(&data[0], size);
    return data;
}

static std::vector<iox::IoxEvent> parseXtf(const std::string& data,
                                           bool requireModels = true) {
    iox::xtf::XtfReaderOptions options;
    options.requireAtLeastOneModel = requireModels;
    iox::xtf::XtfReader reader(options);
    reader.feed(iox::ByteView(data));
    reader.finish();

    std::vector<iox::IoxEvent> events;
    while (true) {
        auto outcome = reader.next();
        if (outcome.progress == iox::ReaderProgress::End) break;
        if (outcome.progress == iox::ReaderProgress::NeedInput) break;
        if (outcome.event) events.push_back(std::move(*outcome.event));
    }
    return events;
}

static std::string transfer23(std::string_view dataContent,
                              std::string_view sender = "T") {
    return "<?xml version=\"1.0\"?>"
           "<ili:TRANSFER xmlns:ili=\"http://www.interlis.ch/INTERLIS2.3\">"
           "<ili:HEADERSECTION SENDER=\"" + std::string(sender) +
           "\" VERSION=\"2.3\"><ili:MODELS>"
           "<ili:MODEL NAME=\"M\"/></ili:MODELS></ili:HEADERSECTION>"
           "<ili:DATASECTION>" + std::string(dataContent) +
           "</ili:DATASECTION></ili:TRANSFER>";
}

static std::optional<iox::DiagnosticCode> parseFailure(
    const std::string& data) {
    try {
        iox::xtf::XtfReader reader;
        reader.feed(iox::ByteView(data));
        reader.finish();
    } catch (const iox::IoxError& error) {
        return error.code();
    }
    return std::nullopt;
}

static int countEventType(const std::vector<iox::IoxEvent>& events,
                           const char* typeName) {
    std::string expected(typeName);
    if (!expected.empty()) {
        expected[0] = static_cast<char>(
            std::tolower(static_cast<unsigned char>(expected[0])));
    }
    int count = 0;
    for (const auto& e : events) {
        if (iox::eventKindName(iox::eventKind(e)) == expected) ++count;
    }
    return count;
}

#define FIXTURE_DIR "test/fixtures"

// ============================================================================
// Header Tests
// ============================================================================

IOX_TEST(conformance_xtf23_valid_header) {
    auto data = readFixture(FIXTURE_DIR "/xtf23/headerSection/ValidHeaderSection.xtf");
    IOX_CHECK(!data.empty());

    auto events = parseXtf(data);
    IOX_CHECK(!events.empty());

    // Should have StartTransfer + EndTransfer
    IOX_CHECK(std::holds_alternative<iox::StartTransferEvent>(events[0]));
}

IOX_TEST(conformance_xtf23_comments_in_file) {
    auto data = readFixture(FIXTURE_DIR "/xtf23/headerSection/CommentsInFile.xtf");
    IOX_CHECK(!data.empty());

    auto events = parseXtf(data);
    // Comments should not prevent parsing
    IOX_CHECK(!events.empty());
    IOX_CHECK(std::holds_alternative<iox::StartTransferEvent>(events[0]));
}

IOX_TEST(conformance_xtf23_empty_transfer) {
    // Empty TRANSFER (no header section)
    std::string xtf =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<ili:TRANSFER xmlns:ili=\"http://www.interlis.ch/INTERLIS2.3\">"
        "</ili:TRANSFER>";
    const auto code = parseFailure(xtf);
    IOX_CHECK(code.has_value());
    IOX_CHECK_EQ(iox::DiagnosticCode::InvalidEventOrder, *code);
}

IOX_TEST(conformance_xtf23_malformed_xml) {
    const std::string malformed = "<ili:TRANSFER><ili:HEADERSECTION>";
    const auto code = parseFailure(malformed);
    IOX_CHECK(code.has_value());
    IOX_CHECK(*code == iox::DiagnosticCode::XmlMalformed);
}

IOX_TEST(conformance_xtf23_wrong_root) {
    // Wrong root element
    std::string xml = "<?xml version=\"1.0\"?><NOT_TRANSFER xmlns:ili=\"http://www.interlis.ch/INTERLIS2.3\"><ili:HEADERSECTION/></NOT_TRANSFER>";
    const auto code = parseFailure(xml);
    IOX_CHECK(code.has_value());
    IOX_CHECK(*code == iox::DiagnosticCode::InvalidXtfNamespace);
}

IOX_TEST(conformance_xtf23_wrong_namespace) {
    // Wrong INTERLIS namespace prefix — but our reader detects version from namespace
    // This should either parse as unknown or produce diagnostic
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<xxx:TRANSFER xmlns:xxx=\"http://www.interlis.ch/INTERLIS2.3\">"
        "<xxx:HEADERSECTION SENDER=\"T\" VERSION=\"2.3\">"
        "<xxx:MODELS><xxx:MODEL NAME=\"M\"/></xxx:MODELS>"
        "</xxx:HEADERSECTION><xxx:DATASECTION/>"
        "</xxx:TRANSFER>";
    auto events = parseXtf(xml);
    // Should still parse (namespace URI matters, not prefix)
    IOX_CHECK(!events.empty());
}

IOX_TEST(conformance_xtf23_no_header_section) {
    // File without HEADERSECTION
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<ili:TRANSFER xmlns:ili=\"http://www.interlis.ch/INTERLIS2.3\">"
        "</ili:TRANSFER>";
    const auto code = parseFailure(xml);
    IOX_CHECK(code.has_value());
    IOX_CHECK_EQ(iox::DiagnosticCode::InvalidEventOrder, *code);
}

// ============================================================================
// Data Type Tests
// ============================================================================

IOX_TEST(conformance_xtf23_text_types) {
    auto data = readFixture(FIXTURE_DIR "/xtf23/dataSection/TextTypes.xtf");
    IOX_CHECK(!data.empty());

    auto events = parseXtf(data);
    // Should have at least one ObjectEvent with text attributes
    int objCount = countEventType(events, "Object");
    IOX_CHECK(objCount > 0);
}

IOX_TEST(conformance_xtf23_boolean_type) {
    auto data = readFixture(FIXTURE_DIR "/xtf23/dataSection/BooleanType.xtf");
    IOX_CHECK(!data.empty());

    auto events = parseXtf(data);
    int objCount = countEventType(events, "Object");
    IOX_CHECK(objCount > 0);
}

IOX_TEST(conformance_xtf23_numeric_types) {
    auto data = readFixture(FIXTURE_DIR "/xtf23/dataSection/NumericTypes.xtf");
    IOX_CHECK(!data.empty());

    auto events = parseXtf(data);
    int objCount = countEventType(events, "Object");
    IOX_CHECK(objCount > 0);
}

IOX_TEST(conformance_xtf23_enumeration_types) {
    auto data = readFixture(FIXTURE_DIR "/xtf23/dataSection/EnumerationTypes.xtf");
    IOX_CHECK(!data.empty());

    auto events = parseXtf(data);
    int objCount = countEventType(events, "Object");
    IOX_CHECK(objCount > 0);
}

// ============================================================================
// Structure & Reference Tests
// ============================================================================

IOX_TEST(conformance_xtf23_structures) {
    auto data = readFixture(FIXTURE_DIR "/xtf23/dataSection/Structures.xtf");
    IOX_CHECK(!data.empty());

    auto events = parseXtf(data);
    int objCount = countEventType(events, "Object");
    IOX_CHECK(objCount > 0);
}

IOX_TEST(conformance_xtf23_references) {
    auto data = readFixture(FIXTURE_DIR "/xtf23/dataSection/References.xtf");
    IOX_CHECK(!data.empty());

    auto events = parseXtf(data);
    int objCount = countEventType(events, "Object");
    IOX_CHECK(objCount > 0);
}

// ============================================================================
// Geometry Tests (from iox-ili fixtures)
// ============================================================================

IOX_TEST(conformance_xtf23_coord) {
    auto data = readFixture(FIXTURE_DIR "/xtf23/dataSection/Coord.xtf");
    IOX_CHECK(!data.empty());

    auto events = parseXtf(data);
    int objCount = countEventType(events, "Object");
    IOX_CHECK(objCount > 0);

    // The object should have COORD geometry
    bool hasCoord = false;
    for (auto& e : events) {
        if (auto* obj = std::get_if<iox::ObjectEvent>(&e)) {
            for (std::size_t i = 0; i < obj->object.attributeCount(); ++i) {
                const auto& name = obj->object.attributeName(i);
                for (std::size_t valueIndex = 0;
                     valueIndex < obj->object.valueCount(name.interlisName());
                     ++valueIndex) {
                    const auto& value =
                        obj->object.value(name.interlisName(), valueIndex);
                    if (value.isObject() &&
                        value.object().tag().interlisName() == "COORD") {
                        hasCoord = true;
                    }
                }
            }
        }
    }
    IOX_CHECK(hasCoord);
}

IOX_TEST(conformance_xtf23_surface_preserves_arc_segments) {
    auto data = readFixture(FIXTURE_DIR "/xtf23/dataSection/Surface.xtf");
    IOX_CHECK(!data.empty());
    auto events = parseXtf(data);
    bool hasArc = false;
    bool hasSurface = false;
    std::function<void(const iox::IomObject&)> inspect =
        [&](const iox::IomObject& value) {
            if (value.tag().interlisName() == "SURFACE") hasSurface = true;
            if (value.tag().interlisName() == "ARC") hasArc = true;
            for (std::size_t i = 0; i < value.attributeCount(); ++i) {
                const auto& name = value.attributeName(i);
                if (name.interlisName() == "ARC") hasArc = true;
                for (std::size_t valueIndex = 0;
                     valueIndex < value.valueCount(name.interlisName());
                     ++valueIndex) {
                    const auto& nestedValue =
                        value.value(name.interlisName(), valueIndex);
                    if (nestedValue.isObject()) inspect(nestedValue.object());
                }
            }
        };
    for (const auto& event : events) {
        if (const auto* object = std::get_if<iox::ObjectEvent>(&event)) {
            inspect(object->object);
        }
    }
    IOX_CHECK(hasSurface);
    IOX_CHECK(hasArc);
}

IOX_TEST(conformance_xtf23_polyline) {
    auto data = readFixture(FIXTURE_DIR "/xtf23/dataSection/PolylineWithStraights.xtf");
    IOX_CHECK(!data.empty());

    auto events = parseXtf(data);
    int objCount = countEventType(events, "Object");
    IOX_CHECK(objCount > 0);
}

// ============================================================================
// Basket Tests
// ============================================================================

IOX_TEST(conformance_xtf23_empty_basket) {
    // Note: iox-ili uses default-namespace XTF 2.3 format.
    // Our reader uses the ili:-prefixed canonical format.
    // Test with our format.
    const auto xml = transfer23("<M.T BID=\"B1\"/>");
    auto events = parseXtf(xml);
    int startBaskets = countEventType(events, "StartBasket");
    int endBaskets = countEventType(events, "EndBasket");
    IOX_CHECK(startBaskets > 0);
    IOX_CHECK_EQ(startBaskets, endBaskets);
}

IOX_TEST(conformance_xtf23_multiple_baskets) {
    // Construct a transfer with multiple baskets
    const auto xml = transfer23(
        "<M.T BID=\"B1\"/><M.T BID=\"B2\"/>");
    auto events = parseXtf(xml);

    int startBaskets = countEventType(events, "StartBasket");
    IOX_CHECK_EQ(2, startBaskets);
}

// ============================================================================
// Invalid Input Tests
// ============================================================================

IOX_TEST(conformance_xtf23_dtd_rejected) {
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<!DOCTYPE transfer [<!ENTITY foo \"bar\">]>"
        "<ili:TRANSFER xmlns:ili=\"http://www.interlis.ch/INTERLIS2.3\">"
        "</ili:TRANSFER>";
    const auto code = parseFailure(xml);
    IOX_CHECK(code.has_value());
    IOX_CHECK(*code == iox::DiagnosticCode::XmlDtdForbidden);
}

IOX_TEST(conformance_xtf23_wrong_spelled_end_transfer) {
    // Wrong spelled end transfer: </ili:TRASNFER>
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<ili:TRANSFER xmlns:ili=\"http://www.interlis.ch/INTERLIS2.3\">"
        "<ili:HEADERSECTION SENDER=\"T\" VERSION=\"2.3\">"
        "<ili:MODELS><ili:MODEL NAME=\"M\"/></ili:MODELS>"
        "</ili:HEADERSECTION><ili:DATASECTION/>"
        "</ili:TRASNFER>";
    const auto code = parseFailure(xml);
    IOX_CHECK(code.has_value());
    IOX_CHECK(*code == iox::DiagnosticCode::XmlMalformed);
}

IOX_TEST(conformance_xtf23_wrong_case_transfer) {
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<ili:transfer xmlns:ili=\"http://www.interlis.ch/INTERLIS2.3\">"
        "<ili:HEADERSECTION><ili:SENDER>T</ili:SENDER><ili:SOFTWARE>X</ili:SOFTWARE></ili:HEADERSECTION>"
        "</ili:transfer>";
    const auto code = parseFailure(xml);
    IOX_CHECK(code.has_value());
    IOX_CHECK_EQ(iox::DiagnosticCode::InvalidXtfNamespace, *code);
}

IOX_TEST(conformance_xtf23_truncated) {
    // Truncated XML
    const std::string truncated =
        "<?xml version=\"1.0\"?><ili:TRANSFER";
    const auto code = parseFailure(truncated);
    IOX_CHECK(code.has_value());
    IOX_CHECK(*code == iox::DiagnosticCode::XmlMalformed);
}

IOX_TEST(conformance_xtf23_text_between_elements) {
    // Unexpected text between elements
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<ili:TRANSFER xmlns:ili=\"http://www.interlis.ch/INTERLIS2.3\">"
        "unexpected text"
        "<ili:HEADERSECTION><ili:SENDER>T</ili:SENDER><ili:SOFTWARE>X</ili:SOFTWARE></ili:HEADERSECTION>"
        "</ili:TRANSFER>";
    const auto code = parseFailure(xml);
    IOX_CHECK(code.has_value());
    IOX_CHECK_EQ(iox::DiagnosticCode::UnexpectedElement, *code);
}

IOX_TEST(conformance_xtf23_nested_transfer) {
    // Nested TRANSFER element (invalid)
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<ili:TRANSFER xmlns:ili=\"http://www.interlis.ch/INTERLIS2.3\">"
        "<ili:TRANSFER>"
        "</ili:TRANSFER>"
        "</ili:TRANSFER>";
    const auto code = parseFailure(xml);
    IOX_CHECK(code.has_value());
    IOX_CHECK_EQ(iox::DiagnosticCode::InvalidEventOrder, *code);
}

// ============================================================================
// Basket Metadata Tests
// ============================================================================

IOX_TEST(conformance_xtf23_basket_consistency) {
    const auto xml = transfer23(
        "<M.T BID=\"B1\" CONSISTENCY=\"incomplete\"/>");
    auto events = parseXtf(xml);

    for (auto& e : events) {
        if (auto* sb = std::get_if<iox::StartBasketEvent>(&e)) {
            IOX_CHECK_EQ(iox::Consistency::Incomplete,
                         sb->basket.consistency);
        }
    }
}

IOX_TEST(conformance_xtf23_basket_operation) {
    const auto xml = transfer23(
        "<M.T BID=\"B1\" KIND=\"UPDATE\">"
        "<M.T.C TID=\"T1\" OPERATION=\"DELETE\"><Name>val</Name>"
        "</M.T.C></M.T>");
    auto events = parseXtf(xml);

    for (auto& e : events) {
        if (auto* sb = std::get_if<iox::StartBasketEvent>(&e)) {
            IOX_CHECK_EQ(iox::BasketKind::Update, sb->basket.kind);
        }
        if (auto* obj = std::get_if<iox::ObjectEvent>(&e)) {
            // OPERATION on object
            (void)obj;
        }
    }
}

// ============================================================================
// Unicode Tests
// ============================================================================

IOX_TEST(conformance_xtf23_unicode_umlauts) {
    const auto xml = transfer23(
        "<M.T BID=\"B1\"><M.T.C TID=\"1\">"
        "<Name>Straße école</Name></M.T.C></M.T>",
        "Prüfstelle");
    auto events = parseXtf(xml);
    IOX_CHECK(!events.empty());
    IOX_CHECK(std::holds_alternative<iox::StartTransferEvent>(events[0]));
    auto& st = std::get<iox::StartTransferEvent>(events[0]);
    IOX_CHECK_EQ(std::string("Prüfstelle"), st.header.sender);
}

// ============================================================================
// Semantic Roundtrip Tests
// ============================================================================

IOX_TEST(conformance_xtf23_roundtrip_header) {
    auto data = readFixture(FIXTURE_DIR "/xtf23/headerSection/ValidHeaderSection.xtf");
    IOX_CHECK(!data.empty());

    auto events = parseXtf(data);
    IOX_CHECK(!events.empty());

    // Write back and re-parse
    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::xtf::XtfWriterOptions opts;
    opts.version = iox::xtf::XtfVersion::V23;
    opts.pretty = false;
    iox::xtf::XtfWriter writer(sink, opts);
    for (const auto& e : events) writer.write(e);
    writer.close();

    auto rtEvents = parseXtf(sink->str(), false);
    IOX_CHECK(!rtEvents.empty());
    // Event counts should match
    IOX_CHECK_EQ(static_cast<int>(events.size()),
                 static_cast<int>(rtEvents.size()));
}

IOX_TEST(conformance_xtf23_roundtrip_data) {
    auto data = readFixture(FIXTURE_DIR "/xtf23/dataSection/TextTypes.xtf");
    IOX_CHECK(!data.empty());

    auto events = parseXtf(data);
    IOX_CHECK(!events.empty());

    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::xtf::XtfWriterOptions opts;
    opts.version = iox::xtf::XtfVersion::V23;
    opts.pretty = false;
    iox::xtf::XtfWriter writer(sink, opts);
    for (const auto& e : events) writer.write(e);
    writer.close();

    auto rtEvents = parseXtf(sink->str(), false);
    IOX_CHECK(!rtEvents.empty());
}

// ============================================================================
// 1-Byte Chunk Tests
// ============================================================================

IOX_TEST(conformance_xtf23_chunked_valid_header) {
    auto data = readFixture(FIXTURE_DIR "/xtf23/headerSection/ValidHeaderSection.xtf");
    IOX_CHECK(!data.empty());

    // Feed 1 byte at a time
    iox::xtf::XtfReader reader;
    for (std::size_t i = 0; i < data.size(); ++i) {
        reader.feed(iox::ByteView(
            reinterpret_cast<const std::uint8_t*>(data.data() + i), 1U));
    }
    reader.finish();

    int count = 0;
    while (true) {
        auto outcome = reader.next();
        if (outcome.progress == iox::ReaderProgress::End) break;
        if (outcome.progress == iox::ReaderProgress::NeedInput) break;
        if (outcome.event) ++count;
    }
    IOX_CHECK(count > 0);
}

#include "iox/test/TestMain.h"
