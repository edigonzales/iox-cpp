#include "iox/Basket.h"
#include "iox/ByteView.h"
#include "iox/Events.h"
#include "iox/FormatRegistry.h"
#include "iox/IomObject.h"
#include "iox/Reader.h"
#include "iox/Writer.h"
#include "iox/abi/iox.h"
#include "iox/json/JsonEventReader.h"
#include "iox/json/JsonEventWriter.h"
#include "iox/test/Test.h"
#include "xml/ExpatParser.h"
#include "xml/XmlWriter.h"
#include "iox/xtf/Xtf24Dialect.h"
#include "iox/xtf/XtfReader.h"
#include "iox/xtf/XtfWriter.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

class SequenceReader final : public iox::Reader {
public:
    explicit SequenceReader(std::vector<iox::ReadOutcome> outcomes)
        : outcomes_(std::move(outcomes)) {}

    iox::ReadOutcome next() override {
        if (position_ == outcomes_.size()) {
            return {iox::ReaderProgress::End, std::nullopt};
        }
        return std::move(outcomes_[position_++]);
    }
    void feed(iox::ByteView) override {}
    void finish() override { finished_ = true; }
    bool isFinished() const noexcept override { return finished_; }
    std::vector<iox::Diagnostic> takeDiagnostics() override { return {}; }

private:
    std::vector<iox::ReadOutcome> outcomes_;
    std::size_t position_ = 0;
    bool finished_ = true;
};

iox::ReadOutcome event(iox::IoxEvent value) {
    return {iox::ReaderProgress::Event, std::move(value)};
}

std::unique_ptr<iox::Reader> sequence(std::vector<iox::IoxEvent> values) {
    std::vector<iox::ReadOutcome> outcomes;
    for (auto& value : values) outcomes.push_back(event(std::move(value)));
    outcomes.push_back({iox::ReaderProgress::End, std::nullopt});
    return std::make_unique<SequenceReader>(std::move(outcomes));
}

iox::IomObject richObject() {
    iox::IomObject object(
        iox::IomName("M.T.C", {"urn:model", "C", "m"}), "T1");
    object.setOperation(iox::ObjectOperation::Update);
    object.setConsistency(iox::Consistency::Incomplete);
    object.setReference({"R1", "B1", 7U});
    object.setSourceLocation({"source.xtf", 9U, 2U, 4U});
    object.setPrimitive(iox::IomName("lexical"), "001.2500");
    object.setPrimitive(iox::IomName("controls"), "\b\f\r\t\x01");
    object.appendPrimitive(iox::IomName("repeated"), "one");
    object.appendPrimitive(iox::IomName("repeated"), "two");
    iox::IomObject child(iox::IomName("M.T.S"));
    child.setPrimitive(iox::IomName("value"), "child");
    object.setObject(iox::IomName("child"), child);
    iox::IomObject reference(iox::IomName("REFERENCE"));
    reference.setReference({"AR", "AB", 4U});
    object.setObject(iox::IomName("ref"), reference);
    return object;
}

std::vector<iox::IoxEvent> representativeEvents() {
    iox::StartTransferEvent transfer;
    transfer.header.version = iox::XtfVersion::V24;
    transfer.header.sender = "S\"\\\n";
    transfer.header.comment = "C";
    transfer.header.models.push_back(
        {"M", "1", "urn:m", {"urn:model", "M", "m"}});
    transfer.header.oidSpaces.push_back({"oid", "UUIDOID"});
    iox::StartBasketEvent basket;
    basket.basket.topic =
        iox::IomName("M.T", {"urn:model", "T", "m"});
    basket.basket.basketId = "B";
    basket.basket.kind = iox::BasketKind::Update;
    basket.basket.consistency = iox::Consistency::Adapted;
    basket.basket.startState = "a";
    basket.basket.endState = "b";
    basket.basket.domains = {"d1", "d2"};
    basket.basket.topics = {"M.T"};
    iox::ObjectEvent object;
    object.object = richObject();
    return {transfer, basket, object, iox::EndBasketEvent{},
            iox::EndTransferEvent{}};
}

std::string jsonEvents(const std::vector<iox::IoxEvent>& events) {
    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::json::JsonEventWriter writer(sink);
    for (const auto& value : events) writer.write(value);
    writer.close();
    return sink->str();
}

std::vector<std::string> lines(const std::string& input) {
    std::vector<std::string> result;
    std::size_t start = 0;
    while (start < input.size()) {
        const auto end = input.find('\n', start);
        if (end == std::string::npos) break;
        result.push_back(input.substr(start, end - start));
        start = end + 1U;
    }
    return result;
}

} // namespace

IOX_TEST(coverage_iom_object_edge_paths) {
    const std::string backing = "abc";
    const iox::ByteView bytes(backing);
    IOX_CHECK_EQ(static_cast<std::size_t>(2), bytes.subview(1, 99).size());
    bool badSubview = false;
    try {
        (void)bytes.subview(99, 1);
    } catch (const iox::IoxError& error) {
        badSubview = error.code() == iox::DiagnosticCode::InvalidArgument;
    }
    IOX_CHECK(badSubview);

    const iox::XmlQualifiedName first("u", "n", "p");
    const iox::XmlQualifiedName same("u", "n", "q");
    const iox::XmlQualifiedName other("v", "n");
    IOX_CHECK(first == same);
    IOX_CHECK(first != other);
    IOX_CHECK_EQ(std::string("{u}n"), first.expanded());
    IOX_CHECK(iox::IomName::fromExpandedXmlName(first).hasXmlName());

    const auto object = richObject();
    IOX_CHECK(!object.primitive("missing"));
    IOX_CHECK(!object.primitive("child"));
    IOX_CHECK(object.object("child").has_value());
    IOX_CHECK_EQ(std::string_view("001.2500"),
                 *object.primitive("lexical"));

    auto copy = object.deepCopy();
    IOX_CHECK(copy.semanticallyEquals(object));
    copy.setTag(iox::IomName("M.T.Other"));
    IOX_CHECK(!copy.semanticallyEquals(object));
    copy = object.deepCopy();
    copy.setReference({"different", "B1", 7U});
    IOX_CHECK(!copy.semanticallyEquals(object));
    copy = object.deepCopy();
    copy.eraseAttribute("lexical");
    IOX_CHECK(!copy.semanticallyEquals(object));

    bool badAttribute = false;
    try {
        (void)object.attributeName(999);
    } catch (const iox::IoxError&) {
        badAttribute = true;
    }
    IOX_CHECK(badAttribute);
}

IOX_TEST(coverage_json_all_fields_and_error_paths) {
    const auto values = representativeEvents();
    const auto input = jsonEvents(values);
    iox::json::JsonEventReader reader;
    for (std::size_t offset = 0; offset < input.size(); ++offset) {
        reader.feed(iox::ByteView(
            reinterpret_cast<const std::uint8_t*>(input.data() + offset), 1U));
    }
    reader.finish();
    std::size_t count = 0;
    while (reader.next().progress == iox::ReaderProgress::Event) ++count;
    IOX_CHECK_EQ(values.size(), count);

    const std::vector<std::string> invalid{
        "{", "[]\n", "{\"schema\":\"iox-event/2\"}\n",
        "{\"schema\":\"iox-event/2\",\"event\":\"unknown\"}\n",
        "{\"schema\":\"iox-event/2\",\"schema\":\"iox-event/2\",\"event\":\"endTransfer\"}\n"};
    for (const auto& value : invalid) {
        iox::json::JsonEventReader bad;
        bool threw = false;
        try {
            bad.feed(iox::ByteView(value));
            bad.finish();
            (void)bad.next();
        } catch (const iox::IoxError&) {
            threw = true;
        }
        IOX_CHECK(threw);
    }

    iox::json::JsonEventReader state;
    const auto valid = jsonEvents(representativeEvents());
    state.feed(iox::ByteView(valid));
    state.finish();
    bool doubleFinish = false;
    try {
        state.finish();
    } catch (const iox::IoxError& error) {
        doubleFinish = error.code() == iox::DiagnosticCode::InvalidState;
    }
    IOX_CHECK(doubleFinish);
}

IOX_TEST(coverage_json_writer_state_paths) {
    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::json::JsonEventWriter writer(sink);
    for (const auto& value : representativeEvents()) writer.write(value);
    writer.flush();
    writer.close();
    IOX_CHECK(writer.isClosed());
    IOX_CHECK(sink->str().find("001.2500") != std::string::npos);

    bool closedWrite = false;
    try {
        writer.write(iox::EndTransferEvent{});
    } catch (const iox::IoxError& error) {
        closedWrite = error.code() == iox::DiagnosticCode::WriterStateError;
    }
    IOX_CHECK(closedWrite);

    auto invalidSink = std::make_shared<iox::StringOutputSink>();
    iox::json::JsonEventWriter invalid(invalidSink);
    bool badOrder = false;
    try {
        invalid.write(iox::StartBasketEvent{});
    } catch (const iox::IoxError& error) {
        badOrder = error.code() == iox::DiagnosticCode::InvalidEventOrder;
    }
    IOX_CHECK(badOrder);
}

IOX_TEST(coverage_xml_callbacks_limits_and_writer) {
    std::string starts;
    std::string ends;
    std::string text;
    iox::xml::ExpatParser parser;
    parser.setStartHandler([&](const auto& element) {
        starts += element.name.localName + ':' +
                  std::to_string(element.attributes.size());
    });
    parser.setEndHandler([&](const auto& element) {
        ends += element.name.localName;
    });
    parser.setTextHandler([&](std::string_view value, const auto&) {
        text += value;
    });
    const std::string xml =
        "<?xml version=\"1.0\"?><r a=\"v\"><c>text</c><!-- x --></r>";
    parser.feed(iox::ByteView(xml));
    parser.finish();
    IOX_CHECK(!starts.empty() && !ends.empty());
    IOX_CHECK_EQ(std::string("text"), text);

    iox::xml::XmlLimits limits;
    limits.maxAttributesPerElement = 1;
    iox::xml::ExpatParser limited(limits);
    const std::string withAttributes = "<r a=\"1\" b=\"2\"/>";
    bool limitedFailed = false;
    try {
        limited.feed(iox::ByteView(withAttributes));
    } catch (const iox::IoxError& error) {
        limitedFailed = error.code() == iox::DiagnosticCode::XmlLimitExceeded;
    }
    IOX_CHECK(limitedFailed);

    iox::xml::ExpatParser callbackParser;
    callbackParser.setStartHandler([](const auto&) {
        throw std::runtime_error("callback");
    });
    const std::string root = "<r/>";
    bool callbackFailed = false;
    try {
        callbackParser.feed(iox::ByteView(root));
    } catch (const iox::IoxError& error) {
        callbackFailed = error.code() == iox::DiagnosticCode::InternalError;
    }
    IOX_CHECK(callbackFailed);

    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::xml::XmlWriter writer(sink);
    writer.startDocument();
    writer.startElement({{}, "root", {}});
    writer.writeAttribute({{}, "a", {}}, "<&>\"'");
    writer.startElement({{}, "child", {}});
    writer.text("<&>\"'");
    writer.endElement();
    writer.endElement();
    writer.endDocument();
    writer.flush();
    IOX_CHECK(sink->str().find("&amp;") != std::string::npos);
}

IOX_TEST(coverage_basket_and_registry_state_paths) {
    iox::StartTransferEvent transfer;
    transfer.header.sender = "S";
    iox::StartBasketEvent basket;
    basket.basket.topic = iox::IomName("M.T");
    basket.basket.basketId = "B";
    iox::ObjectEvent object;
    object.object = iox::IomObject(iox::IomName("M.T.C"), "T");

    iox::BasketReader valid(sequence({transfer, basket, object,
                                      iox::EndBasketEvent{},
                                      iox::EndTransferEvent{}}));
    IOX_CHECK(valid.header().has_value());
    IOX_CHECK(valid.readBasket().has_value());
    IOX_CHECK(!valid.readBasket().has_value());

    iox::BasketReader missing(sequence({iox::EndTransferEvent{}}));
    IOX_CHECK(!missing.header().has_value());
    IOX_CHECK(!missing.takeDiagnostics().empty());

    iox::BasketReader earlyEnd(sequence({transfer, basket}));
    IOX_CHECK(!earlyEnd.readBasket().has_value());
    iox::BasketReader limited(sequence({transfer, basket, object, object,
                                        iox::EndBasketEvent{}}), 1);
    IOX_CHECK(!limited.readBasket().has_value());

    iox::FormatRegistry registry;
    iox::FormatEntry entry;
    entry.name = "json";
    entry.extensions = {".ndjson"};
    entry.readerFactory = [] {
        return std::make_unique<iox::json::JsonEventReader>();
    };
    entry.writerFactory = [](std::shared_ptr<iox::OutputSink> sink) {
        return std::make_unique<iox::json::JsonEventWriter>(std::move(sink));
    };
    entry.scoreSniffer = [](iox::ByteView prefix) {
        return !prefix.empty() && prefix.data()[0] == '{' ? 100 : 0;
    };
    registry.addFormat(entry);
    const std::string prefix = "{";
    IOX_CHECK(registry.createReaderBySniffing(iox::ByteView(prefix)) != nullptr);
    IOX_CHECK(registry.createWriter(
        "json", std::make_shared<iox::StringOutputSink>()) != nullptr);
    IOX_CHECK(registry.removeFormat("json"));
    IOX_CHECK(!registry.removeFormat("json"));
}

IOX_TEST(coverage_xtf_and_abi_state_paths) {
    const std::string minimal =
        "<?xml version=\"1.0\"?><ili:TRANSFER xmlns:ili=\"http://www.interlis.ch/INTERLIS2.3\"><ili:HEADERSECTION><ili:SENDER>S</ili:SENDER></ili:HEADERSECTION></ili:TRANSFER>";
    iox::xtf::XtfReaderOptions options;
    options.expectedVersion = iox::XtfVersion::V24;
    iox::xtf::XtfReader wrong(options);
    bool mismatch = false;
    try {
        wrong.feed(iox::ByteView(minimal));
        wrong.finish();
    } catch (const iox::IoxError& error) {
        mismatch = error.code() ==
                   iox::DiagnosticCode::UnsupportedXtfVersion;
    }
    IOX_CHECK(mismatch);

    iox::xtf::XtfReader needInput;
    IOX_CHECK_EQ(iox::ReaderProgress::NeedInput, needInput.next().progress);

    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::xtf::XtfWriterOptions writerOptions;
    writerOptions.version = iox::XtfVersion::V24;
    writerOptions.pretty = false;
    iox::xtf::XtfWriter writer(sink, writerOptions);
    bool invalidOrder = false;
    try {
        writer.write(iox::EndTransferEvent{});
    } catch (const iox::IoxError&) {
        invalidOrder = true;
    }
    IOX_CHECK(invalidOrder);

    IOX_CHECK(iox_reader_create(nullptr, nullptr) == nullptr);
    IOX_CHECK(iox_reader_create("unknown", nullptr) == nullptr);
    auto* reader = iox_reader_create("json-events", nullptr);
    IOX_CHECK(reader != nullptr);
    const auto ndjson = jsonEvents(representativeEvents());
    IOX_CHECK_EQ(IOX_STATUS_OK,
        iox_reader_feed(reader,
                        reinterpret_cast<const std::uint8_t*>(ndjson.data()),
                        ndjson.size()));
    IOX_CHECK_EQ(IOX_STATUS_OK, iox_reader_finish(reader));
    iox_result_t* result = nullptr;
    int eventCount = 0;
    while (true) {
        const auto status = iox_reader_next(reader, &result);
        IOX_CHECK(result != nullptr);
        const std::string resultJson = iox_result_json(result);
        IOX_CHECK(resultJson.find("\"schema\":\"iox-result/2\"") !=
                  std::string::npos);
        iox_result_destroy(result);
        result = nullptr;
        if (status == IOX_STATUS_END) break;
        IOX_CHECK_EQ(IOX_STATUS_EVENT, status);
        ++eventCount;
    }
    IOX_CHECK_EQ(5, eventCount);
    iox_reader_destroy(reader);

    auto* abiWriter = iox_writer_create("json-events", nullptr);
    IOX_CHECK(abiWriter != nullptr);
    for (const auto& line : lines(ndjson)) {
        IOX_CHECK_EQ(IOX_STATUS_OK,
            iox_writer_write_event_json(abiWriter, line.data(), line.size(),
                                        &result));
        iox_result_destroy(result);
        result = nullptr;
    }
    IOX_CHECK_EQ(IOX_STATUS_OK,
                 iox_writer_take_output(abiWriter, &result));
    IOX_CHECK(iox_result_size(result) > 0U);
    iox_result_destroy(result);
    IOX_CHECK_EQ(IOX_STATUS_OK, iox_writer_finish(abiWriter, &result));
    iox_result_destroy(result);
    IOX_CHECK_EQ(IOX_STATUS_INVALID_STATE,
                 iox_writer_finish(abiWriter, &result));
    iox_result_destroy(result);
    iox_writer_destroy(abiWriter);

    IOX_CHECK_EQ(IOX_STATUS_INVALID_ARGUMENT,
                 iox_reader_next(nullptr, &result));
    IOX_CHECK_EQ(IOX_STATUS_INVALID_ARGUMENT,
                 iox_writer_finish(nullptr, &result));
    IOX_CHECK(iox_result_json(nullptr) == nullptr);
}

IOX_TEST(coverage_direct_xtf_dialect_paths) {
    const auto expanded = [](std::string_view uri, std::string_view local) {
        return std::string(uri) + "\xFF" + std::string(local);
    };
    std::vector<iox::IoxEvent> events24;
    iox::xtf::Xtf24Dialect dialect24({
        [&](iox::IoxEvent value) { events24.push_back(std::move(value)); },
        [](iox::Diagnostic) {}});
    const auto basket = expanded(
        "http://www.interlis.ch/xtf/2.4/INTERLIS", "basket");
    const auto klass = expanded("urn:model", "Class");
    const auto coord = expanded(
        "http://www.interlis.ch/geometry/1.0", "coord");
    dialect24.onStartElement(basket, {{"bid", "B"}});
    dialect24.onStartElement(klass, {{"tid", "T"}});
    dialect24.onStartElement("position", {});
    dialect24.onStartElement(coord, {});
    dialect24.onStartElement("c1", {});
    dialect24.onCharacterData("1");
    dialect24.onEndElement("c1");
    dialect24.onEndElement(coord);
    dialect24.onEndElement("position");
    dialect24.onEndElement(klass);
    dialect24.onEndElement(basket);
    IOX_CHECK_EQ(static_cast<std::size_t>(3), events24.size());
    IOX_CHECK(!dialect24.isFatal());
}

#include "iox/test/TestMain.h"
