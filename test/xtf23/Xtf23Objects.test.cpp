#include "iox/Events.h"
#include "iox/Writer.h"
#include "iox/xtf/XtfReader.h"
#include "iox/xtf/XtfWriter.h"
#include "iox/test/Test.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {

iox::StartTransferEvent transfer() {
    iox::StartTransferEvent result;
    result.header.version = iox::XtfVersion::V23;
    result.header.sender = "TestSender";
    return result;
}

iox::StartBasketEvent basket(std::string id,
                             iox::Consistency consistency =
                                 iox::Consistency::Unspecified) {
    iox::StartBasketEvent result;
    result.basket.topic = iox::IomName("TestModel.TopicA.DataBasket");
    result.basket.basketId = std::move(id);
    result.basket.consistency = consistency;
    result.basket.kind = iox::BasketKind::Full;
    return result;
}

iox::ObjectEvent object(std::string id, std::string value) {
    iox::ObjectEvent result;
    result.object = iox::IomObject(
        iox::IomName("TestModel.TopicA.TestClass"), std::move(id));
    result.object.setOperation(iox::ObjectOperation::Insert);
    result.object.setPrimitive(iox::IomName("Name"), std::move(value));
    return result;
}

std::string write(const std::vector<iox::IoxEvent>& events) {
    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::xtf::XtfWriterOptions options;
    options.version = iox::XtfVersion::V23;
    options.pretty = false;
    iox::xtf::XtfWriter writer(sink, options);
    for (const auto& event : events) writer.write(event);
    writer.close();
    return sink->str();
}

std::vector<iox::IoxEvent> read(const std::string& input,
                                std::size_t chunkSize = 0) {
    iox::xtf::XtfReaderOptions options;
    options.requireAtLeastOneModel = false;
    iox::xtf::XtfReader reader(options);
    if (chunkSize == 0) {
        reader.feed(iox::ByteView(input));
    } else {
        for (std::size_t offset = 0; offset < input.size(); offset += chunkSize) {
            const auto count = std::min(chunkSize, input.size() - offset);
            reader.feed(iox::ByteView(
                reinterpret_cast<const std::uint8_t*>(input.data() + offset),
                count));
        }
    }
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

std::vector<iox::IoxEvent> oneObjectEvents(iox::ObjectEvent value) {
    return {transfer(), basket("BID001"), std::move(value),
            iox::EndBasketEvent{}, iox::EndTransferEvent{}};
}

std::string document23(std::string data) {
    return "<?xml version=\"1.0\"?><TRANSFER "
           "xmlns=\"http://www.interlis.ch/INTERLIS2.3\">"
           "<HEADERSECTION SENDER=\"objects\" VERSION=\"2.3\">"
           "<MODELS><MODEL NAME=\"M\" VERSION=\"1\" URI=\"urn:m\"/>"
           "</MODELS>"
           "</HEADERSECTION>"
           "<DATASECTION>" + data + "</DATASECTION></TRANSFER>";
}

std::vector<iox::IoxEvent> readWithPattern(
    const std::string& input, const std::vector<std::size_t>& pattern) {
    iox::xtf::XtfReader reader;
    std::size_t offset = 0;
    std::size_t index = 0;
    while (offset < input.size()) {
        const auto requested = pattern[index++ % pattern.size()];
        const auto count = std::min(requested, input.size() - offset);
        reader.feed(iox::ByteView(
            reinterpret_cast<const std::uint8_t*>(input.data() + offset),
            count));
        offset += count;
    }
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

} // namespace

IOX_TEST(xtf23_simple_object_roundtrip) {
    auto value = object("TID001", "test-value");
    value.object.setPrimitive(iox::IomName("Count"), "00042");
    const auto parsed = read(write(oneObjectEvents(value)));
    IOX_CHECK_EQ(static_cast<std::size_t>(5), parsed.size());
    const auto& result = std::get<iox::ObjectEvent>(parsed[2]).object;
    IOX_CHECK_EQ(std::string("TID001"), *result.oid());
    IOX_CHECK_EQ(iox::ObjectOperation::Insert, result.operation());
    IOX_CHECK_EQ(std::string_view("test-value"), *result.primitive("Name"));
    IOX_CHECK_EQ(std::string_view("00042"), *result.primitive("Count"));
}

IOX_TEST(xtf23_multiple_objects) {
    std::vector<iox::IoxEvent> events{transfer(), basket("B1")};
    for (int i = 0; i < 10; ++i) {
        events.push_back(object("TID" + std::to_string(i),
                                std::to_string(i)));
    }
    events.push_back(iox::EndBasketEvent{});
    events.push_back(iox::EndTransferEvent{});
    const auto parsed = read(write(events));
    IOX_CHECK_EQ(static_cast<std::size_t>(14), parsed.size());
    for (int i = 0; i < 10; ++i) {
        const auto& value = std::get<iox::ObjectEvent>(parsed[2U + i]).object;
        IOX_CHECK_EQ("TID" + std::to_string(i), *value.oid());
    }
}

IOX_TEST(xtf23_object_with_structure_attribute) {
    auto value = object("TID1", "Test");
    iox::IomObject address(iox::IomName("TestModel.TopicA.Address"));
    address.setPrimitive(iox::IomName("Street"), "Main St");
    address.setPrimitive(iox::IomName("Number"), "10");
    value.object.setObject(iox::IomName("Address"), address);

    const auto parsed = read(write(oneObjectEvents(value)));
    const auto result =
        std::get<iox::ObjectEvent>(parsed[2]).object.object("Address");
    IOX_CHECK(result.has_value());
    IOX_CHECK_EQ(std::string_view("Main St"), *result->primitive("Street"));
    IOX_CHECK_EQ(std::string_view("10"), *result->primitive("Number"));
}

IOX_TEST(xtf23_basket_with_consistency) {
    const auto parsed = read(write(
        {transfer(), basket("B_INCOMPLETE", iox::Consistency::Incomplete),
         iox::EndBasketEvent{}, iox::EndTransferEvent{}}));
    const auto& metadata = std::get<iox::StartBasketEvent>(parsed[1]).basket;
    IOX_CHECK_EQ(std::string("B_INCOMPLETE"), metadata.basketId);
    IOX_CHECK_EQ(iox::Consistency::Incomplete, metadata.consistency);
}

IOX_TEST(xtf23_multiple_baskets) {
    const auto parsed = read(write(
        {transfer(), basket("B1"), iox::EndBasketEvent{}, basket("B2"),
         iox::EndBasketEvent{}, iox::EndTransferEvent{}}));
    IOX_CHECK_EQ(static_cast<std::size_t>(6), parsed.size());
    IOX_CHECK_EQ(std::string("B1"),
                 std::get<iox::StartBasketEvent>(parsed[1]).basket.basketId);
    IOX_CHECK_EQ(std::string("B2"),
                 std::get<iox::StartBasketEvent>(parsed[3]).basket.basketId);
}

IOX_TEST(xtf23_chunked_read) {
    const auto xml = write(oneObjectEvents(object("TID1", "chunk-test")));
    const auto whole = read(xml);
    const auto oneByte = read(xml, 1);
    const auto sevenBytes = read(xml, 7);
    IOX_CHECK_EQ(whole.size(), oneByte.size());
    IOX_CHECK_EQ(whole.size(), sevenBytes.size());
    IOX_CHECK_EQ(std::string_view("chunk-test"),
        *std::get<iox::ObjectEvent>(oneByte[2]).object.primitive("Name"));
}

IOX_TEST(xtf23_reads_complete_basket_object_reference_and_delete_metadata) {
    const auto xml = document23(
        "<M.T BID=\"B1\" KIND=\"UPDATE\" CONSISTENCY=\"ADAPTED\" "
        "STARTSTATE=\"s0\" ENDSTATE=\"s1\" DOMAINS=\"D1, D2\" "
        "TOPICS=\"T1,T2\">"
        "<M.T.C TID=\"O1\" BID=\"B0\" OPERATION=\"UPDATE\" "
        "CONSISTENCY=\"INCOMPLETE\">"
        "<number>001.2300</number><text>Grüezi 世界</text>"
        "<role REF=\"O2\" BID=\"B2\" ORDER_POS=\"7\"/>"
        "<items><M.T.Struct><value>a</value></M.T.Struct>"
        "<M.T.Struct><value>b</value></M.T.Struct></items>"
        "</M.T.C><DELETE TID=\"O9\"/></M.T>");
    const auto events = readWithPattern(xml, {xml.size()});
    IOX_CHECK_EQ(static_cast<std::size_t>(6), events.size());
    const auto& basketMetadata =
        std::get<iox::StartBasketEvent>(events[1]).basket;
    IOX_CHECK_EQ(iox::BasketKind::Update, basketMetadata.kind);
    IOX_CHECK_EQ(iox::Consistency::Adapted, basketMetadata.consistency);
    IOX_CHECK_EQ(std::string("s0"), *basketMetadata.startState);
    IOX_CHECK_EQ(std::string("s1"), *basketMetadata.endState);
    IOX_CHECK_EQ(std::vector<std::string>({"D1", "D2"}),
                 basketMetadata.domains);
    IOX_CHECK_EQ(std::vector<std::string>({"T1", "T2"}),
                 basketMetadata.topics);
    IOX_CHECK(basketMetadata.location.line > 0U);

    const auto& parsed = std::get<iox::ObjectEvent>(events[2]).object;
    IOX_CHECK_EQ(iox::ObjectOperation::Update, parsed.operation());
    IOX_CHECK_EQ(iox::Consistency::Incomplete, parsed.consistency());
    IOX_CHECK_EQ(std::string("B0"), *parsed.reference().targetBasketId);
    IOX_CHECK_EQ(std::string_view("001.2300"), *parsed.primitive("number"));
    IOX_CHECK_EQ(std::string_view("Grüezi 世界"), *parsed.primitive("text"));
    const auto role = parsed.object("role");
    IOX_CHECK(role.has_value());
    IOX_CHECK_EQ(std::string("O2"), *role->reference().targetOid);
    IOX_CHECK_EQ(std::string("B2"), *role->reference().targetBasketId);
    IOX_CHECK_EQ(std::uint64_t{7}, *role->reference().orderPosition);
    IOX_CHECK_EQ(static_cast<std::size_t>(2), parsed.valueCount("items"));
    IOX_CHECK_EQ(std::string_view("b"),
                 *parsed.object("items", 1)->primitive("value"));

    const auto& deleted = std::get<iox::ObjectEvent>(events[3]).object;
    IOX_CHECK_EQ(std::string("DELETE"), deleted.tag().interlisName());
    IOX_CHECK_EQ(iox::ObjectOperation::Delete, deleted.operation());
    IOX_CHECK_EQ(std::string("O9"), *deleted.oid());
}

IOX_TEST(xtf23_strict_and_lenient_rules_are_distinct) {
    const auto lowerCase = document23(
        "<M.T BID=\"B\"><M.T.C TID=\"O\" OPERATION=\"update\">"
        "<ref REF=\"R\" ORDER_POS=\"not-a-number\"/>"
        "</M.T.C></M.T>");
    iox::xtf::XtfReader lenient;
    lenient.feed(iox::ByteView(lowerCase));
    lenient.finish();
    while (lenient.next().progress == iox::ReaderProgress::Event) {}
    const auto diagnostics = lenient.takeDiagnostics();
    IOX_CHECK(std::any_of(diagnostics.begin(), diagnostics.end(),
        [](const auto& diagnostic) {
            return diagnostic.code == iox::DiagnosticCode::InvalidReference;
        }));
    IOX_CHECK(std::any_of(diagnostics.begin(), diagnostics.end(),
        [](const auto& diagnostic) {
            return diagnostic.code == iox::DiagnosticCode::UnexpectedAttribute;
        }));

    bool strictRejected = false;
    try {
        iox::xtf::XtfReaderOptions options;
        options.strictness = iox::xtf::Strictness::Strict;
        iox::xtf::XtfReader strict(options);
        strict.feed(iox::ByteView(lowerCase));
    } catch (const iox::IoxError& error) {
        strictRejected =
            error.code() == iox::DiagnosticCode::UnexpectedAttribute;
    }
    IOX_CHECK(strictRejected);

    const auto missingIds = document23(
        "<M.T BID=\"B\"><M.T.C><value>x</value></M.T.C></M.T>");
    iox::xtf::XtfReader tolerantMissing;
    tolerantMissing.feed(iox::ByteView(missingIds));
    tolerantMissing.finish();
    while (tolerantMissing.next().progress == iox::ReaderProgress::Event) {}
    const auto missingDiagnostics = tolerantMissing.takeDiagnostics();
    IOX_CHECK(std::any_of(missingDiagnostics.begin(),
                          missingDiagnostics.end(),
                          [](const auto& diagnostic) {
                              return diagnostic.code ==
                                  iox::DiagnosticCode::MissingObjectId;
                          }));

    bool missingBasket = false;
    try {
        const auto invalid = document23("<M.T/>");
        iox::xtf::XtfReader reader;
        reader.feed(iox::ByteView(invalid));
    } catch (const iox::IoxError& error) {
        missingBasket = error.code() == iox::DiagnosticCode::MissingBasketId;
    }
    IOX_CHECK(missingBasket);
}

IOX_TEST(xtf23_order_position_must_be_positive) {
    const auto xml = document23(
        "<M.T BID=\"B\"><M.T.C TID=\"O\">"
        "<role REF=\"R\" ORDER_POS=\"0\"/>"
        "</M.T.C></M.T>");
    iox::xtf::XtfReader reader;
    reader.feed(iox::ByteView(xml));
    reader.finish();
    while (reader.next().progress == iox::ReaderProgress::Event) {}
    const auto diagnostics = reader.takeDiagnostics();
    IOX_CHECK(std::any_of(diagnostics.begin(), diagnostics.end(),
        [](const auto& diagnostic) {
            return diagnostic.code == iox::DiagnosticCode::InvalidReference;
        }));
}

IOX_TEST(xtf23_chunk_matrix_preserves_unicode_entities_and_lexical_values) {
    const auto xml = document23(
        "<M.T BID=\"B\"><M.T.C TID=\"O\">"
        "<text>😀 &amp; é</text><number>0001.2300</number>"
        "</M.T.C></M.T>");
    const auto whole = readWithPattern(xml, {xml.size()});
    for (const auto& pattern : std::vector<std::vector<std::size_t>>{
             {1}, {2}, {3}, {7}, {64}, {5, 1, 13, 2, 8, 3}}) {
        const auto parsed = readWithPattern(xml, pattern);
        IOX_CHECK_EQ(whole.size(), parsed.size());
        const auto& object = std::get<iox::ObjectEvent>(parsed[2]).object;
        IOX_CHECK_EQ(std::string_view("😀 & é"),
                     *object.primitive("text"));
        IOX_CHECK_EQ(std::string_view("0001.2300"),
                     *object.primitive("number"));
    }
}

#include "iox/test/TestMain.h"
