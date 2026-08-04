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

iox::ExtensionElement richExtension() {
    iox::ExtensionElement extension;
    extension.name = {"urn:extension", "extra", "x"};
    extension.attributes.push_back(
        {{"urn:extension", "flag", "x"}, "yes"});
    extension.text = "outer";
    iox::ExtensionElement child;
    child.name = {"urn:extension", "child", "x"};
    child.text = "inner";
    extension.children.push_back(std::move(child));
    return extension;
}

std::vector<iox::IoxEvent> representativeEvents() {
    iox::StartTransferEvent transfer;
    transfer.header.version = iox::XtfVersion::V24;
    transfer.header.sender = "S\"\\\n";
    transfer.header.comment = "C";
    transfer.header.models.push_back(
        {"M", "1", "urn:m", {"urn:model", "M", "m"}});
    transfer.header.oidSpaces.push_back({"oid", "UUIDOID"});
    transfer.header.extensions.push_back(richExtension());
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
    basket.basket.extensions.push_back(richExtension());
    iox::ObjectEvent object;
    object.object = richObject();
    return {transfer, basket, object, iox::EndBasketEvent{},
            iox::EndTransferEvent{}};
}

iox::StartTransferEvent xtfTransfer(iox::XtfVersion version) {
    iox::StartTransferEvent transfer;
    transfer.header.version = version;
    transfer.header.sender = "coverage";
    if (version == iox::XtfVersion::V23) {
        transfer.header.models.push_back({"M", "1", "urn:m", {}});
    } else {
        transfer.header.models.push_back(
            {"M", std::nullopt, std::nullopt, {"urn:m", "M", "m"}});
    }
    return transfer;
}

iox::StartBasketEvent xtfBasket(iox::XtfVersion version) {
    iox::StartBasketEvent basket;
    basket.basket.basketId = "B";
    basket.basket.topic = version == iox::XtfVersion::V23
        ? iox::IomName("M.T")
        : iox::IomName("M.T", {"urn:m", "T", "m"});
    return basket;
}

iox::ObjectEvent xtfObject(iox::XtfVersion version) {
    iox::ObjectEvent object;
    object.object = version == iox::XtfVersion::V23
        ? iox::IomObject(iox::IomName("M.T.C"), "O")
        : iox::IomObject(
              iox::IomName("M.T.C", {"urn:m", "C", "m"}), "O");
    return object;
}

bool malformedJson(std::string_view input) {
    iox::json::JsonEventReader reader;
    try {
        reader.feed(iox::ByteView(
            reinterpret_cast<const std::uint8_t*>(input.data()), input.size()));
        reader.finish();
        while (reader.next().progress == iox::ReaderProgress::Event) {}
    } catch (const iox::IoxError& error) {
        return error.code() == iox::DiagnosticCode::JsonMalformed ||
               error.code() == iox::DiagnosticCode::InvalidEventOrder;
    }
    return false;
}

class RejectingSink final : public iox::OutputSink {
public:
    explicit RejectingSink(bool oversized = false) : oversized_(oversized) {}
    std::size_t write(const void*, std::size_t size) override {
        return oversized_ ? size + 1U : 0U;
    }
private:
    bool oversized_;
};

class FlushFailingSink final : public iox::OutputSink {
public:
    std::size_t write(const void*, std::size_t size) override { return size; }
    void flush() override { throw std::runtime_error("flush"); }
};

class UnknownCloseSink final : public iox::OutputSink {
public:
    std::size_t write(const void*, std::size_t size) override { return size; }
    void close() override { throw 17; }
};

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

    iox::json::JsonReaderOptions zeroOptions;
    zeroOptions.maxLineBytes = 0;
    bool zeroLimit = false;
    try {
        iox::json::JsonEventReader zero(zeroOptions);
    } catch (const iox::IoxError& error) {
        zeroLimit = error.code() == iox::DiagnosticCode::InvalidArgument;
    }
    IOX_CHECK(zeroLimit);

    const std::vector<std::string> malformedFields{
        "{\"schema\":1,\"event\":\"endTransfer\"}\n",
        "{\"schema\":\"iox-event/2\",\"event\":1}\n",
        "{\"schema\":\"iox-event/2\",\"event\":\"startTransfer\",\"header\":[]}\n",
        "{\"schema\":\"iox-event/2\",\"event\":\"startTransfer\",\"header\":{\"version\":\"9\",\"sender\":\"s\",\"models\":[],\"oidSpaces\":[],\"extensions\":[]}}\n",
        "{\"schema\":\"iox-event/2\",\"event\":\"startTransfer\",\"header\":{\"version\":\"2.3\",\"sender\":\"s\",\"comment\":3,\"models\":[],\"oidSpaces\":[],\"extensions\":[]}}\n",
        "{\"schema\":\"iox-event/2\",\"event\":\"startTransfer\",\"header\":{\"version\":\"2.3\",\"sender\":\"s\",\"models\":{},\"oidSpaces\":[],\"extensions\":[]}}\n",
        "{\"schema\":\"iox-event/2\",\"event\":\"startTransfer\",\"header\":{\"version\":\"2.3\",\"sender\":\"s\",\"models\":[],\"oidSpaces\":[],\"extensions\":{}}}\n",
        "{\"schema\":\"iox-event/2\",\"event\":\"startTransfer\",\"header\":{\"version\":\"2.3\",\"sender\":\"s\",\"models\":[],\"oidSpaces\":[],\"extensions\":[{\"name\":{},\"attributes\":[],\"text\":\"\",\"children\":[]}]}}\n",
        "{\"schema\":\"iox-event/2\",\"event\":\"startBasket\",\"basket\":{}}\n",
        "{\"schema\":\"iox-event/2\",\"event\":\"object\",\"object\":{}}\n"};
    for (const auto& malformed : malformedFields) {
        IOX_CHECK(malformedJson(malformed));
    }

    IOX_CHECK(malformedJson("\n"));
    iox::json::JsonReaderOptions tinyOptions;
    tinyOptions.maxLineBytes = 2;
    iox::json::JsonEventReader tiny(tinyOptions);
    bool lineLimit = false;
    try { tiny.feed(iox::ByteView(std::string("abc"))); }
    catch (const iox::IoxError& error) {
        lineLimit = error.code() == iox::DiagnosticCode::JsonMalformed;
    }
    IOX_CHECK(lineLimit);
    bool failedFeed = false;
    try { tiny.feed(iox::ByteView(std::string("x"))); }
    catch (const iox::IoxError& error) {
        failedFeed = error.code() == iox::DiagnosticCode::InvalidState;
    }
    IOX_CHECK(failedFeed);

    iox::json::JsonEventReader finishedReader;
    IOX_CHECK(!finishedReader.isFinished());
    finishedReader.feed(iox::ByteView(valid));
    finishedReader.finish();
    bool finishedFeed = false;
    try { finishedReader.feed(iox::ByteView(std::string("x"))); }
    catch (const iox::IoxError& error) {
        finishedFeed = error.code() == iox::DiagnosticCode::InvalidState;
    }
    IOX_CHECK(finishedFeed);
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

    bool nullSink = false;
    try {
        iox::json::JsonEventWriter missing(nullptr);
    } catch (const iox::IoxError& error) {
        nullSink = error.code() == iox::DiagnosticCode::InvalidArgument;
    }
    IOX_CHECK(nullSink);

    for (const auto oversized : {false, true}) {
        iox::json::JsonEventWriter failing(
            std::make_shared<RejectingSink>(oversized));
        bool failed = false;
        try {
            failing.write(iox::StartTransferEvent{});
        } catch (const iox::IoxError& error) {
            failed = error.code() == iox::DiagnosticCode::IoError;
        }
        IOX_CHECK(failed);
        bool terminalFlush = false;
        try {
            failing.flush();
        } catch (const iox::IoxError& error) {
            terminalFlush = error.code() == iox::DiagnosticCode::WriterStateError;
        }
        IOX_CHECK(terminalFlush);
    }

    auto incompleteSink = std::make_shared<iox::StringOutputSink>();
    iox::json::JsonEventWriter incomplete(incompleteSink);
    incomplete.write(iox::StartTransferEvent{});
    bool incompleteClose = false;
    try {
        incomplete.close();
    } catch (const iox::IoxError& error) {
        incompleteClose = error.code() == iox::DiagnosticCode::InvalidEventOrder;
    }
    IOX_CHECK(incompleteClose);

    std::vector<iox::IoxEvent> enumEvents;
    iox::StartTransferEvent enumTransfer;
    enumTransfer.header.version = iox::XtfVersion::V23;
    enumEvents.push_back(enumTransfer);
    const std::vector<iox::BasketKind> kinds{
        iox::BasketKind::Full, iox::BasketKind::Initial,
        iox::BasketKind::Unspecified};
    const std::vector<iox::ObjectOperation> operations{
        iox::ObjectOperation::Insert, iox::ObjectOperation::Delete,
        iox::ObjectOperation::None};
    const std::vector<iox::Consistency> consistencies{
        iox::Consistency::Complete, iox::Consistency::Inconsistent,
        iox::Consistency::Unspecified};
    for (std::size_t index = 0; index < kinds.size(); ++index) {
        iox::StartBasketEvent enumBasket;
        enumBasket.basket.kind = kinds[index];
        enumBasket.basket.consistency = consistencies[index];
        enumEvents.push_back(enumBasket);
        iox::ObjectEvent enumObject;
        enumObject.object.setOperation(operations[index]);
        enumObject.object.setConsistency(consistencies[index]);
        enumEvents.push_back(enumObject);
        enumEvents.push_back(iox::EndBasketEvent{});
    }
    enumEvents.push_back(iox::EndTransferEvent{});
    const auto enumJson = jsonEvents(enumEvents);
    iox::json::JsonEventReader enumReader;
    enumReader.feed(iox::ByteView(enumJson));
    enumReader.finish();
    while (enumReader.next().progress == iox::ReaderProgress::Event) {}
    IOX_CHECK(enumReader.isFinished());
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

IOX_TEST(coverage_xtf_writer_validation_matrix) {
    auto headerRejects = [](iox::XtfVersion version,
                            iox::StartTransferEvent transfer,
                            iox::DiagnosticCode code) {
        auto sink = std::make_shared<iox::StringOutputSink>();
        iox::xtf::XtfWriterOptions options;
        options.version = version;
        options.strictness = iox::xtf::Strictness::Strict;
        options.pretty = false;
        iox::xtf::XtfWriter writer(sink, options);
        try { writer.write(transfer); }
        catch (const iox::IoxError& error) { return error.code() == code; }
        return false;
    };
    auto basketRejects = [](iox::XtfVersion version,
                            iox::StartBasketEvent basket,
                            iox::DiagnosticCode code) {
        auto sink = std::make_shared<iox::StringOutputSink>();
        iox::xtf::XtfWriterOptions options;
        options.version = version;
        options.strictness = iox::xtf::Strictness::Strict;
        options.pretty = false;
        iox::xtf::XtfWriter writer(sink, options);
        writer.write(xtfTransfer(version));
        try { writer.write(basket); }
        catch (const iox::IoxError& error) { return error.code() == code; }
        return false;
    };
    auto objectRejects = [](iox::XtfVersion version,
                            iox::ObjectEvent object,
                            iox::DiagnosticCode code) {
        auto sink = std::make_shared<iox::StringOutputSink>();
        iox::xtf::XtfWriterOptions options;
        options.version = version;
        options.strictness = iox::xtf::Strictness::Strict;
        options.pretty = false;
        iox::xtf::XtfWriter writer(sink, options);
        writer.write(xtfTransfer(version));
        writer.write(xtfBasket(version));
        try { writer.write(object); }
        catch (const iox::IoxError& error) { return error.code() == code; }
        return false;
    };

    auto header = xtfTransfer(iox::XtfVersion::V23);
    header.header.version = iox::XtfVersion::V24;
    IOX_CHECK(headerRejects(iox::XtfVersion::V23, header,
                            iox::DiagnosticCode::ModelMismatch));
    header = xtfTransfer(iox::XtfVersion::V23);
    header.header.models.front().name.clear();
    IOX_CHECK(headerRejects(iox::XtfVersion::V23, header,
                            iox::DiagnosticCode::MissingModelEntry));
    header = xtfTransfer(iox::XtfVersion::V23);
    header.header.oidSpaces.push_back({"", ""});
    IOX_CHECK(headerRejects(iox::XtfVersion::V23, header,
                            iox::DiagnosticCode::MissingRequiredHeader));

    header = xtfTransfer(iox::XtfVersion::V24);
    header.header.version = iox::XtfVersion::V23;
    IOX_CHECK(headerRejects(iox::XtfVersion::V24, header,
                            iox::DiagnosticCode::ModelMismatch));
    header = xtfTransfer(iox::XtfVersion::V24);
    header.header.models.clear();
    IOX_CHECK(headerRejects(iox::XtfVersion::V24, header,
                            iox::DiagnosticCode::MissingModelEntry));
    header = xtfTransfer(iox::XtfVersion::V24);
    header.header.models.front().name.clear();
    IOX_CHECK(headerRejects(iox::XtfVersion::V24, header,
                            iox::DiagnosticCode::MissingModelEntry));
    header = xtfTransfer(iox::XtfVersion::V24);
    header.header.models.front().version = "1";
    IOX_CHECK(headerRejects(iox::XtfVersion::V24, header,
                            iox::DiagnosticCode::UnexpectedAttribute));
    header = xtfTransfer(iox::XtfVersion::V24);
    header.header.models.front().xmlNamespace.localName = "Other";
    IOX_CHECK(headerRejects(iox::XtfVersion::V24, header,
                            iox::DiagnosticCode::ModelMismatch));
    header = xtfTransfer(iox::XtfVersion::V24);
    header.header.models.push_back(
        {"M", std::nullopt, std::nullopt, {"urn:other", "M", "o"}});
    IOX_CHECK(headerRejects(iox::XtfVersion::V24, header,
                            iox::DiagnosticCode::ModelMismatch));
    header = xtfTransfer(iox::XtfVersion::V24);
    header.header.oidSpaces.push_back({"x", "y"});
    IOX_CHECK(headerRejects(iox::XtfVersion::V24, header,
                            iox::DiagnosticCode::UnexpectedElement));

    for (const auto version : {iox::XtfVersion::V23, iox::XtfVersion::V24}) {
        auto basket = xtfBasket(version);
        basket.basket.basketId.clear();
        IOX_CHECK(basketRejects(version, basket,
                                iox::DiagnosticCode::MissingBasketId));
    }
    auto basket24 = xtfBasket(iox::XtfVersion::V24);
    basket24.basket.topic = iox::IomName("Topic");
    IOX_CHECK(basketRejects(iox::XtfVersion::V24, basket24,
                            iox::DiagnosticCode::UnknownInterlisName));
    basket24 = xtfBasket(iox::XtfVersion::V24);
    basket24.basket.kind = iox::BasketKind::Update;
    IOX_CHECK(basketRejects(iox::XtfVersion::V24, basket24,
                            iox::DiagnosticCode::MissingRequiredHeader));
    basket24 = xtfBasket(iox::XtfVersion::V24);
    basket24.basket.consistency = iox::Consistency::Inconsistent;
    IOX_CHECK(basketRejects(iox::XtfVersion::V24, basket24,
                            iox::DiagnosticCode::UnexpectedAttribute));
    basket24 = xtfBasket(iox::XtfVersion::V24);
    basket24.basket.topics.push_back("M.Other");
    IOX_CHECK(basketRejects(iox::XtfVersion::V24, basket24,
                            iox::DiagnosticCode::UnexpectedAttribute));

    for (const auto version : {iox::XtfVersion::V23, iox::XtfVersion::V24}) {
        auto object = xtfObject(version);
        object.object.setOid("");
        IOX_CHECK(objectRejects(version, object,
                                iox::DiagnosticCode::MissingObjectId));

        object = xtfObject(version);
        iox::IomObject reference(iox::IomName("REFERENCE"));
        reference.setReference({std::nullopt, std::nullopt, 1U});
        const auto roleName = version == iox::XtfVersion::V23
            ? iox::IomName("role")
            : iox::IomName("role", {"urn:m", "role", "m"});
        object.object.setObject(roleName, reference);
        IOX_CHECK(objectRejects(version, object,
                                iox::DiagnosticCode::InvalidReference));

        object = xtfObject(version);
        reference.setReference({"R", "", std::nullopt});
        object.object.setObject(roleName, reference);
        IOX_CHECK(objectRejects(version, object,
                                iox::DiagnosticCode::InvalidReference));

        object = xtfObject(version);
        reference.setReference({"R", std::nullopt, 0U});
        object.object.setObject(roleName, reference);
        IOX_CHECK(objectRejects(version, object,
                                iox::DiagnosticCode::InvalidReference));
    }

    auto delete23 = xtfObject(iox::XtfVersion::V23);
    delete23.object.setTag(iox::IomName("DELETE"));
    delete23.object.setOperation(iox::ObjectOperation::Delete);
    delete23.object.setReference({std::nullopt, "B", std::nullopt});
    IOX_CHECK(objectRejects(iox::XtfVersion::V23, delete23,
                            iox::DiagnosticCode::InvalidReference));

    auto badName24 = xtfObject(iox::XtfVersion::V24);
    badName24.object.setTag(iox::IomName("C"));
    IOX_CHECK(objectRejects(iox::XtfVersion::V24, badName24,
                            iox::DiagnosticCode::UnknownInterlisName));

    auto geometryRejects = [&](iox::XtfVersion version,
                               iox::IomObject geometry) {
        auto object = xtfObject(version);
        const auto attribute = version == iox::XtfVersion::V23
            ? iox::IomName("geom")
            : iox::IomName("geom", {"urn:m", "geom", "m"});
        object.object.setObject(attribute, std::move(geometry));
        return objectRejects(version, object,
                             iox::DiagnosticCode::InvalidGeometry);
    };

    for (const auto version : {iox::XtfVersion::V23, iox::XtfVersion::V24}) {
        iox::IomObject coord(iox::IomName("COORD"));
        IOX_CHECK(geometryRejects(version, coord));
        coord.setObject(iox::IomName("C1"), iox::IomObject(iox::IomName("x")));
        IOX_CHECK(geometryRejects(version, coord));

        iox::IomObject sequence(iox::IomName("SEGMENTS"));
        iox::IomObject line(iox::IomName("POLYLINE"));
        IOX_CHECK(geometryRejects(version, line));
        line.setPrimitive(iox::IomName("sequence"), "bad");
        IOX_CHECK(geometryRejects(version, line));
        line.eraseAttribute("sequence");
        line.setObject(iox::IomName("sequence"), sequence);
        IOX_CHECK(geometryRejects(version, line));
        sequence.setPrimitive(iox::IomName("segment"), "bad");
        line.setObject(iox::IomName("sequence"), sequence);
        IOX_CHECK(geometryRejects(version, line));

        iox::IomObject boundary(iox::IomName("BOUNDARY"));
        iox::IomObject surface(iox::IomName("SURFACE"));
        if (version == iox::XtfVersion::V23) {
            surface.setObject(iox::IomName("boundary"), boundary);
            IOX_CHECK(geometryRejects(version, surface));
        } else {
            IOX_CHECK(geometryRejects(version, surface));
            iox::IomObject multiCoord(iox::IomName("MULTICOORD"));
            IOX_CHECK(geometryRejects(version, multiCoord));
            multiCoord.setPrimitive(iox::IomName("coord"), "bad");
            IOX_CHECK(geometryRejects(version, multiCoord));
            iox::IomObject multiLine(iox::IomName("MULTIPOLYLINE"));
            IOX_CHECK(geometryRejects(version, multiLine));
            multiLine.setPrimitive(iox::IomName("polyline"), "bad");
            IOX_CHECK(geometryRejects(version, multiLine));
            iox::IomObject multiSurface(iox::IomName("MULTISURFACE"));
            IOX_CHECK(geometryRejects(version, multiSurface));
            multiSurface.setPrimitive(iox::IomName("surface"), "bad");
            IOX_CHECK(geometryRejects(version, multiSurface));
        }
    }

    bool nullOutput = false;
    try { iox::xtf::XtfWriter nullWriter(nullptr, {}); }
    catch (const iox::IoxError& error) {
        nullOutput = error.code() == iox::DiagnosticCode::InvalidArgument;
    }
    IOX_CHECK(nullOutput);
    auto stateSink = std::make_shared<iox::StringOutputSink>();
    iox::xtf::XtfWriter stateWriter(stateSink, {});
    bool badFlush = false;
    try { stateWriter.flush(); }
    catch (const iox::IoxError& error) {
        badFlush = error.code() == iox::DiagnosticCode::WriterStateError;
    }
    IOX_CHECK(badFlush);
    stateWriter.write(xtfTransfer(iox::XtfVersion::V23));
    stateWriter.flush();
    stateWriter.write(iox::EndTransferEvent{});
    stateWriter.close();
    badFlush = false;
    try { stateWriter.flush(); }
    catch (const iox::IoxError& error) {
        badFlush = error.code() == iox::DiagnosticCode::WriterStateError;
    }
    IOX_CHECK(badFlush);

    auto flushSink = std::make_shared<FlushFailingSink>();
    iox::xtf::XtfWriter flushWriter(flushSink, {});
    flushWriter.write(xtfTransfer(iox::XtfVersion::V23));
    bool flushIo = false;
    try { flushWriter.flush(); }
    catch (const iox::IoxError& error) {
        flushIo = error.code() == iox::DiagnosticCode::IoError;
    }
    IOX_CHECK(flushIo);

    auto closeSink = std::make_shared<UnknownCloseSink>();
    iox::xtf::XtfWriter closeWriter(closeSink, {});
    closeWriter.write(xtfTransfer(iox::XtfVersion::V23));
    closeWriter.write(iox::EndTransferEvent{});
    bool closeIo = false;
    try { closeWriter.close(); }
    catch (const iox::IoxError& error) {
        closeIo = error.code() == iox::DiagnosticCode::IoError;
    }
    IOX_CHECK(closeIo);

    auto coordinate = [] {
        iox::IomObject value(iox::IomName("COORD"));
        value.setPrimitive(iox::IomName("C1"), "1");
        value.setPrimitive(iox::IomName("C2"), "2");
        return value;
    };
    auto segments = [&] {
        iox::IomObject value(iox::IomName("SEGMENTS"));
        value.appendObject(iox::IomName("segment"), coordinate());
        return value;
    };
    auto line = [&] {
        iox::IomObject value(iox::IomName("POLYLINE"));
        value.setObject(iox::IomName("sequence"), segments());
        return value;
    };

    iox::IomObject repeatedLine(iox::IomName("POLYLINE"));
    repeatedLine.appendObject(iox::IomName("lineattr"),
                              iox::IomObject(iox::IomName("A")));
    repeatedLine.appendObject(iox::IomName("lineattr"),
                              iox::IomObject(iox::IomName("A")));
    repeatedLine.setObject(iox::IomName("sequence"), segments());
    IOX_CHECK(geometryRejects(iox::XtfVersion::V23, repeatedLine));
    iox::IomObject primitiveLine(iox::IomName("POLYLINE"));
    primitiveLine.setPrimitive(iox::IomName("lineattr"), "bad");
    primitiveLine.setObject(iox::IomName("sequence"), segments());
    IOX_CHECK(geometryRejects(iox::XtfVersion::V23, primitiveLine));
    iox::IomObject multipartLine(iox::IomName("POLYLINE"));
    multipartLine.appendObject(iox::IomName("sequence"), segments());
    multipartLine.appendObject(iox::IomName("sequence"), segments());
    IOX_CHECK(geometryRejects(iox::XtfVersion::V23, multipartLine));
    auto unknownSegments = segments();
    unknownSegments.setPrimitive(iox::IomName("unknown"), "x");
    iox::IomObject unknownLine(iox::IomName("POLYLINE"));
    unknownLine.setObject(iox::IomName("sequence"), unknownSegments);
    IOX_CHECK(geometryRejects(iox::XtfVersion::V23, unknownLine));
    iox::IomObject badBoundary(iox::IomName("BOUNDARY"));
    badBoundary.setPrimitive(iox::IomName("polyline"), "bad");
    iox::IomObject badSurface(iox::IomName("SURFACE"));
    badSurface.setObject(iox::IomName("boundary"), badBoundary);
    IOX_CHECK(geometryRejects(iox::XtfVersion::V23, badSurface));
    iox::IomObject emptyMultiSurface(iox::IomName("MULTISURFACE"));
    IOX_CHECK(geometryRejects(iox::XtfVersion::V23, emptyMultiSurface));
    iox::IomObject manyMultiSurface(iox::IomName("MULTISURFACE"));
    manyMultiSurface.appendObject(iox::IomName("surface"),
                                  iox::IomObject(iox::IomName("SURFACE")));
    manyMultiSurface.appendObject(iox::IomName("surface"),
                                  iox::IomObject(iox::IomName("SURFACE")));
    IOX_CHECK(geometryRejects(iox::XtfVersion::V23, manyMultiSurface));

    auto object23 = xtfObject(iox::XtfVersion::V23);
    object23.object.setReference({"R", std::nullopt, std::nullopt});
    IOX_CHECK(objectRejects(iox::XtfVersion::V23, object23,
                            iox::DiagnosticCode::InvalidReference));
    object23 = xtfObject(iox::XtfVersion::V23);
    iox::IomObject oidMarker(iox::IomName("OID"), "");
    object23.object.setObject(iox::IomName("oid"), oidMarker);
    IOX_CHECK(objectRejects(iox::XtfVersion::V23, object23,
                            iox::DiagnosticCode::UnexpectedAttribute));

    iox::IomObject c3Only(iox::IomName("COORD"));
    c3Only.setPrimitive(iox::IomName("C1"), "1");
    c3Only.setPrimitive(iox::IomName("C3"), "3");
    IOX_CHECK(geometryRejects(iox::XtfVersion::V24, c3Only));
    auto lineAttr24 = line();
    lineAttr24.setObject(iox::IomName("lineattr"),
                         iox::IomObject(iox::IomName("A")));
    IOX_CHECK(geometryRejects(iox::XtfVersion::V24, lineAttr24));

    iox::IomObject mixedSurface(iox::IomName("SURFACE"));
    mixedSurface.setObject(iox::IomName("exterior"), line());
    iox::IomObject boundaryForm(iox::IomName("BOUNDARY"));
    boundaryForm.setObject(iox::IomName("polyline"), line());
    mixedSurface.setObject(iox::IomName("boundary"), boundaryForm);
    IOX_CHECK(geometryRejects(iox::XtfVersion::V24, mixedSurface));
    iox::IomObject repeatedExterior(iox::IomName("SURFACE"));
    repeatedExterior.appendObject(iox::IomName("exterior"), line());
    repeatedExterior.appendObject(iox::IomName("exterior"), line());
    IOX_CHECK(geometryRejects(iox::XtfVersion::V24, repeatedExterior));
    iox::IomObject primitiveExterior(iox::IomName("SURFACE"));
    primitiveExterior.setPrimitive(iox::IomName("exterior"), "bad");
    IOX_CHECK(geometryRejects(iox::XtfVersion::V24, primitiveExterior));
    iox::IomObject primitiveInterior(iox::IomName("SURFACE"));
    primitiveInterior.setObject(iox::IomName("exterior"), line());
    primitiveInterior.setPrimitive(iox::IomName("interior"), "bad");
    IOX_CHECK(geometryRejects(iox::XtfVersion::V24, primitiveInterior));
    iox::IomObject primitiveBoundary(iox::IomName("SURFACE"));
    primitiveBoundary.setPrimitive(iox::IomName("boundary"), "bad");
    IOX_CHECK(geometryRejects(iox::XtfVersion::V24, primitiveBoundary));

    auto object24 = xtfObject(iox::XtfVersion::V24);
    object24.object.setReference({"R", std::nullopt, std::nullopt});
    IOX_CHECK(objectRejects(iox::XtfVersion::V24, object24,
                            iox::DiagnosticCode::InvalidReference));
    object24 = xtfObject(iox::XtfVersion::V24);
    object24.object.setConsistency(iox::Consistency::Adapted);
    IOX_CHECK(objectRejects(iox::XtfVersion::V24, object24,
                            iox::DiagnosticCode::UnexpectedAttribute));
    object24 = xtfObject(iox::XtfVersion::V24);
    object24.object.setTag(iox::IomName(
        "DELETE", {"http://www.interlis.ch/xtf/2.4/INTERLIS", "delete", "ili"}));
    object24.object.setOperation(iox::ObjectOperation::Update);
    IOX_CHECK(objectRejects(iox::XtfVersion::V24, object24,
                            iox::DiagnosticCode::InvalidEventOrder));
    object24 = xtfObject(iox::XtfVersion::V24);
    object24.object.setObject(
        iox::IomName("value", {"urn:m", "value", "m"}),
        iox::IomObject(iox::IomName("Unknown")));
    IOX_CHECK(objectRejects(iox::XtfVersion::V24, object24,
                            iox::DiagnosticCode::UnknownInterlisName));
    object24 = xtfObject(iox::XtfVersion::V24);
    object24.object.setObject(
        iox::IomName("value", {"urn:m", "value", "m"}),
        iox::IomObject(iox::IomName(
            "Bad", {"http://www.interlis.ch/xtf/2.4/INTERLIS", "bad", "ili"})));
    IOX_CHECK(objectRejects(iox::XtfVersion::V24, object24,
                            iox::DiagnosticCode::InvalidXtfNamespace));
}

IOX_TEST(coverage_xtf23_lenient_reader_edge_document) {
    const std::string xml =
        "<TRANSFER xmlns=\"http://www.interlis.ch/INTERLIS2.3\">"
        "<HEADERSECTION VERSION=\"2.3\" SENDER=\"s\" X=\"1\">"
        "<MODELS X=\"1\">noise"
        "<MODEL NAME=\"M\" VERSION=\"1\" URI=\"urn:m\" X=\"1\"><N/></MODEL>"
        "<MODEL/><VENDOR/></MODELS>"
        "<MODELS><MODEL NAME=\"M2\"/></MODELS>"
        "<ALIAS><BAD/><ENTRIES><TAGENTRY FROM=\"a\"/>"
        "<VALENTRY TAG=\"t\"/><DELENTRY/><OTHER/></ENTRIES></ALIAS>"
        "<OIDSPACES><BAD/><OIDSPACE NAME=\"\" OIDDOMAIN=\"\"/>"
        "<OIDSPACE NAME=\"o\" OIDDOMAIN=\"d\" X=\"1\"/></OIDSPACES>"
        "<OIDSPACES/><COMMENT><X/></COMMENT><UNKNOWN/></HEADERSECTION>"
        "<DATASECTION><M.T BID=\"B\" KIND=\"odd\" CONSISTENCY=\"odd\" "
        "STARTSTATE=\"a\" ENDSTATE=\"b\" DOMAINS=\"d1,d2\" "
        "TOPICS=\"t1 t2\" X=\"1\">"
        "<M.T.C TID=\"O\" BID=\"OB\" OPERATION=\"odd\" "
        "CONSISTENCY=\"odd\" X=\"1\">"
        "<oid OID=\"\" X=\"1\">text<child/></oid>"
        "<ref REF=\"\" BID=\"\" ORDER_POS=\"0\">text<a/><b/></ref>"
        "<mixed>text<a/></mixed>"
        "<coord><COORD X=\"1\"><C3>3</C3><C3>4</C3><OTHER>z</OTHER></COORD></coord>"
        "<line><POLYLINE><LINEATTR/><COORD><C1>1</C1></COORD>"
        "<CLIPPED/><LINEATTR/><OTHER/></POLYLINE></line>"
        "<surface><SURFACE><BOUNDARY/><CLIPPED><OTHER/></CLIPPED>"
        "<BOUNDARY><POLYLINE/></BOUNDARY><OTHER/></SURFACE></surface>"
        "</M.T.C><DELETE TID=\"D\" OPERATION=\"UPDATE\"/></M.T>"
        "</DATASECTION></TRANSFER>";
    iox::xtf::XtfReaderOptions options;
    options.requireAtLeastOneModel = false;
    iox::xtf::XtfReader reader(options);
    reader.feed(iox::ByteView(xml));
    reader.finish();
    std::size_t events = 0;
    while (reader.next().progress == iox::ReaderProgress::Event) ++events;
    IOX_CHECK(events >= 5U);
    IOX_CHECK(reader.takeDiagnostics().size() >= 10U);

    const std::vector<std::string> strictFailures{
        "<TRANSFER xmlns=\"http://www.interlis.ch/INTERLIS2.3\"><HEADERSECTION VERSION=\"2.3\" SENDER=\"s\"><MODELS><MODEL NAME=\"M\" VERSION=\"1\" URI=\"u\"/></MODELS></HEADERSECTION><DATASECTION><M.T BID=\"B\" KIND=\"full\"/></DATASECTION></TRANSFER>",
        "<TRANSFER xmlns=\"http://www.interlis.ch/INTERLIS2.3\"><HEADERSECTION VERSION=\"2.3\" SENDER=\"s\"><MODELS><MODEL NAME=\"M\" VERSION=\"1\" URI=\"u\"/></MODELS></HEADERSECTION><DATASECTION><M.T BID=\"B\" CONSISTENCY=\"complete\"/></DATASECTION></TRANSFER>",
        "<TRANSFER xmlns=\"http://www.interlis.ch/INTERLIS2.3\"><HEADERSECTION VERSION=\"2.3\" SENDER=\"s\"><MODELS><MODEL NAME=\"M\" VERSION=\"1\" URI=\"u\"/></MODELS></HEADERSECTION><DATASECTION><M.T BID=\"B\"><M.T.C TID=\"O\" OPERATION=\"insert\"/></M.T></DATASECTION></TRANSFER>"};
    for (const auto& input : strictFailures) {
        bool rejected = false;
        try {
            iox::xtf::XtfReaderOptions strictOptions;
            strictOptions.strictness = iox::xtf::Strictness::Strict;
            iox::xtf::XtfReader strict(strictOptions);
            strict.feed(iox::ByteView(input));
            strict.finish();
        } catch (const iox::IoxError& error) {
            rejected = error.code() == iox::DiagnosticCode::UnexpectedAttribute;
        }
        IOX_CHECK(rejected);
    }
}

IOX_TEST(coverage_xtf24_lenient_reader_edge_document) {
    const std::string xml =
        "<ili:transfer xmlns:ili=\"http://www.interlis.ch/xtf/2.4/INTERLIS\" "
        "xmlns:geom=\"http://www.interlis.ch/geometry/1.0\" "
        "xmlns:m=\"urn:m\" xmlns:u=\"urn:unknown\">"
        "<ili:headersection X=\"1\"><ili:comment X=\"1\"><u:x/></ili:comment>"
        "<ili:models X=\"1\">noise<ili:model X=\"1\">M<u:x/></ili:model>"
        "<ili:model> </ili:model><u:model/></ili:models>"
        "<ili:sender X=\"1\"><u:x/></ili:sender><ili:sender>two</ili:sender>"
        "<ili:comment>two</ili:comment><u:extra/></ili:headersection>"
        "<ili:datasection><m:T ili:bid=\"B\" ili:kind=\"odd\" "
        "ili:consistency=\"odd\" ili:startstate=\"a\" ili:endstate=\"b\" "
        "ili:domains=\"d1 d2\" u:x=\"1\">"
        "<m:C ili:tid=\"\" ili:bid=\"\" ili:operation=\"odd\" "
        "ili:consistency=\"odd\" u:x=\"1\">"
        "<m:ref ili:ref=\"\" ili:bid=\"\" ili:order_pos=\"0\" u:x=\"1\">"
        "text<m:a/><m:b/></m:ref><m:mixed>text<m:a/></m:mixed>"
        "<m:coord><geom:coord X=\"1\"><geom:c3>3</geom:c3>"
        "<geom:c3 X=\"1\">4</geom:c3><u:x/></geom:coord></m:coord>"
        "<m:line><geom:polyline X=\"1\"><ili:x/><u:x/></geom:polyline></m:line>"
        "<m:surface><geom:surface><geom:interior/><geom:exterior/>"
        "<geom:exterior/><u:x/></geom:surface></m:surface>"
        "<m:multi><geom:multicoord><u:x/></geom:multicoord></m:multi>"
        "</m:C><ili:delete ili:tid=\"D\" ili:operation=\"UPDATE\" "
        "ili:consistency=\"INCOMPLETE\"/></m:T></ili:datasection>"
        "</ili:transfer>";
    iox::xtf::XtfReaderOptions options;
    options.requireAtLeastOneModel = false;
    iox::xtf::XtfReader reader(options);
    reader.feed(iox::ByteView(xml));
    reader.finish();
    std::size_t events = 0;
    while (reader.next().progress == iox::ReaderProgress::Event) ++events;
    IOX_CHECK(events >= 5U);
    IOX_CHECK(reader.takeDiagnostics().size() >= 10U);

    const std::vector<std::string> strictBodies{
        "<m:T ili:bid=\"B\" ili:kind=\"full\"/>",
        "<m:T ili:bid=\"B\" ili:consistency=\"complete\"/>",
        "<m:T ili:bid=\"B\"><m:C ili:tid=\"O\" ili:operation=\"insert\"/></m:T>"};
    for (const auto& body : strictBodies) {
        const auto input =
            std::string("<ili:transfer xmlns:ili=\"http://www.interlis.ch/xtf/2.4/INTERLIS\" xmlns:m=\"urn:m\"><ili:headersection><ili:models><ili:model>M</ili:model></ili:models></ili:headersection><ili:datasection>") +
            body + "</ili:datasection></ili:transfer>";
        bool rejected = false;
        try {
            iox::xtf::XtfReaderOptions strictOptions;
            strictOptions.strictness = iox::xtf::Strictness::Strict;
            iox::xtf::XtfReader strict(strictOptions);
            strict.feed(iox::ByteView(input));
            strict.finish();
        } catch (const iox::IoxError& error) {
            rejected = error.code() == iox::DiagnosticCode::UnexpectedAttribute;
        }
        IOX_CHECK(rejected);
    }
}

IOX_TEST(coverage_xtf_writer_extensions_and_enum_fallbacks) {
    auto writeTransfer = [](iox::XtfVersion version,
                            iox::StartTransferEvent transfer,
                            iox::StartBasketEvent basket,
                            std::optional<iox::ObjectEvent> object,
                            iox::xtf::XtfWriterOptions options) {
        options.version = version;
        options.pretty = false;
        auto sink = std::make_shared<iox::StringOutputSink>();
        iox::xtf::XtfWriter writer(sink, options);
        writer.write(transfer);
        writer.write(basket);
        if (object) writer.write(*object);
        writer.write(iox::EndBasketEvent{});
        writer.write(iox::EndTransferEvent{});
        writer.close();
        return std::make_pair(sink->str(), writer.takeDiagnostics());
    };

    auto transfer23 = xtfTransfer(iox::XtfVersion::V23);
    transfer23.header.sender.clear();
    iox::ExtensionElement header23;
    header23.name = {{}, "VENDOR", {}};
    header23.text = "text";
    iox::ExtensionElement child23;
    child23.name = {"urn:vendor", "child", "v"};
    child23.attributes.push_back({{{}, "flag", {}}, "yes"});
    header23.children.push_back(child23);
    transfer23.header.extensions.push_back(header23);
    auto basket23 = xtfBasket(iox::XtfVersion::V23);
    basket23.basket.kind = static_cast<iox::BasketKind>(99);
    basket23.basket.consistency = static_cast<iox::Consistency>(99);
    iox::ExtensionElement basketAttributes23;
    basketAttributes23.name = {{}, "M.T", {}};
    basketAttributes23.attributes.push_back({{{}, "X", {}}, "1"});
    basketAttributes23.text = "not-representable";
    basket23.basket.extensions.push_back(basketAttributes23);
    iox::ExtensionElement basketChild23;
    basketChild23.name = {"urn:vendor", "basketChild", "v"};
    basket23.basket.extensions.push_back(basketChild23);
    auto object23 = xtfObject(iox::XtfVersion::V23);
    object23.object.setOperation(static_cast<iox::ObjectOperation>(99));
    object23.object.setConsistency(static_cast<iox::Consistency>(99));
    auto result23 = writeTransfer(iox::XtfVersion::V23, transfer23, basket23,
                                  object23, {});
    IOX_CHECK(result23.first.find("VENDOR") != std::string::npos);
    IOX_CHECK(result23.first.find("X=\"1\"") != std::string::npos);
    IOX_CHECK(!result23.second.empty());

    iox::xtf::XtfWriterOptions senderOptions;
    senderOptions.sender = "fallback";
    auto transferWithOption = xtfTransfer(iox::XtfVersion::V23);
    transferWithOption.header.sender.clear();
    auto plainBasket = xtfBasket(iox::XtfVersion::V23);
    const auto senderResult = writeTransfer(
        iox::XtfVersion::V23, transferWithOption, plainBasket, std::nullopt,
        senderOptions);
    IOX_CHECK(senderResult.first.find("SENDER=\"fallback\"") !=
              std::string::npos);

    auto transfer24 = xtfTransfer(iox::XtfVersion::V24);
    iox::ExtensionElement header24;
    header24.name = {"urn:vendor", "extra", "v"};
    header24.attributes.push_back(
        {{"urn:vendor", "flag", "v"}, "yes"});
    header24.text = "outer";
    iox::ExtensionElement child24;
    child24.name = {"urn:vendor", "child", "v"};
    child24.text = "inner";
    header24.children.push_back(child24);
    transfer24.header.extensions.push_back(header24);
    auto basket24 = xtfBasket(iox::XtfVersion::V24);
    basket24.basket.kind = iox::BasketKind::Initial;
    basket24.basket.startState = "a";
    basket24.basket.endState = "b";
    iox::ExtensionElement basketAttributes24;
    basketAttributes24.name = basket24.basket.topic.xmlName();
    basketAttributes24.attributes.push_back(
        {{"urn:vendor", "flag", "v"}, "yes"});
    basketAttributes24.text = "not-representable";
    basket24.basket.extensions.push_back(basketAttributes24);
    iox::ExtensionElement basketChild24;
    basketChild24.name = {"urn:vendor", "basketChild", "v"};
    basket24.basket.extensions.push_back(basketChild24);
    auto object24 = xtfObject(iox::XtfVersion::V24);
    object24.object.setOperation(iox::ObjectOperation::Update);
    object24.object.setReference({std::nullopt, "B2", std::nullopt});
    iox::xtf::XtfWriterOptions lenient24;
    lenient24.strictness = iox::xtf::Strictness::Lenient;
    auto result24 = writeTransfer(iox::XtfVersion::V24, transfer24, basket24,
                                  object24, lenient24);
    IOX_CHECK(result24.first.find("v:extra") != std::string::npos);
    IOX_CHECK(result24.first.find("ili:operation=\"UPDATE\"") !=
              std::string::npos);
    IOX_CHECK(result24.first.find("ili:bid=\"B2\"") != std::string::npos);
    IOX_CHECK(!result24.second.empty());

    auto delete24 = xtfObject(iox::XtfVersion::V24);
    delete24.object.setTag(iox::IomName(
        "anything",
        {"http://www.interlis.ch/xtf/2.4/INTERLIS", "delete", "ili"}));
    delete24.object.setOperation(iox::ObjectOperation::Delete);
    const auto deleteResult = writeTransfer(
        iox::XtfVersion::V24, xtfTransfer(iox::XtfVersion::V24),
        xtfBasket(iox::XtfVersion::V24), delete24, lenient24);
    IOX_CHECK(deleteResult.first.find("ili:delete") != std::string::npos);

    auto conflictingPrefixes = xtfTransfer(iox::XtfVersion::V24);
    conflictingPrefixes.header.models.push_back(
        {"N", std::nullopt, std::nullopt, {"urn:n", "N", "m"}});
    const auto prefixResult = writeTransfer(
        iox::XtfVersion::V24, conflictingPrefixes,
        xtfBasket(iox::XtfVersion::V24), std::nullopt, lenient24);
    IOX_CHECK(prefixResult.first.find("urn:n") != std::string::npos);

    auto rejectsHeaderExtension = [](iox::XtfVersion version,
                                     iox::ExtensionElement extension) {
        auto transfer = xtfTransfer(version);
        transfer.header.extensions.push_back(std::move(extension));
        auto sink = std::make_shared<iox::StringOutputSink>();
        iox::xtf::XtfWriterOptions options;
        options.version = version;
        iox::xtf::XtfWriter writer(sink, options);
        try { writer.write(transfer); }
        catch (const iox::IoxError& error) {
            return error.code() == iox::DiagnosticCode::UnknownInterlisName;
        }
        return false;
    };
    iox::ExtensionElement emptyElement;
    emptyElement.name = {"urn:vendor", "", "v"};
    IOX_CHECK(rejectsHeaderExtension(iox::XtfVersion::V23, emptyElement));
    IOX_CHECK(rejectsHeaderExtension(iox::XtfVersion::V24, emptyElement));
    iox::ExtensionElement emptyAttribute;
    emptyAttribute.name = {"urn:vendor", "extra", "v"};
    emptyAttribute.attributes.push_back(
        {{"urn:vendor", "", "v"}, "x"});
    IOX_CHECK(rejectsHeaderExtension(iox::XtfVersion::V23, emptyAttribute));
    IOX_CHECK(rejectsHeaderExtension(iox::XtfVersion::V24, emptyAttribute));

    auto invalidNamespaceObject = xtfObject(iox::XtfVersion::V24);
    invalidNamespaceObject.object.setTag(
        iox::IomName("M.T.C",
                     {"http://www.interlis.ch/geometry/1.0", "C", "geom"}));
    auto invalidSink = std::make_shared<iox::StringOutputSink>();
    iox::xtf::XtfWriterOptions invalidOptions;
    invalidOptions.version = iox::XtfVersion::V24;
    iox::xtf::XtfWriter invalidWriter(invalidSink, invalidOptions);
    invalidWriter.write(xtfTransfer(iox::XtfVersion::V24));
    invalidWriter.write(xtfBasket(iox::XtfVersion::V24));
    bool namespaceRejected = false;
    try { invalidWriter.write(invalidNamespaceObject); }
    catch (const iox::IoxError& error) {
        namespaceRejected =
            error.code() == iox::DiagnosticCode::InvalidXtfNamespace;
    }
    IOX_CHECK(namespaceRejected);
}

#include "iox/test/TestMain.h"
