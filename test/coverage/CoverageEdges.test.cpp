#include "iox/Basket.h"
#include "iox/ByteView.h"
#include "iox/Events.h"
#include "iox/FormatRegistry.h"
#include "iox/IomObject.h"
#include "iox/IomValue.h"
#include "iox/Reader.h"
#include "iox/Writer.h"
#include "iox/abi/iox.h"
#include "iox/json/JsonEventReader.h"
#include "iox/json/JsonEventWriter.h"
#include "iox/test/Test.h"
#include "iox/xml/ExpatParser.h"
#include "iox/xml/XmlWriter.h"
#include "iox/xtf/XtfReader.h"
#include "iox/xtf/Xtf23Dialect.h"
#include "iox/xtf/Xtf24Dialect.h"
#include "iox/xtf/XtfWriter.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

std::vector<iox::IoxEvent> readJson(const std::string& input) {
    iox::json::JsonEventReader reader;
    reader.feed(iox::ByteView(input));
    reader.finish();
    std::vector<iox::IoxEvent> result;
    while (true) {
        auto outcome = reader.next();
        if (outcome.event) result.push_back(std::move(*outcome.event));
        if (outcome.status == iox::ReadOutcome::Status::End) break;
        if (outcome.status == iox::ReadOutcome::Status::NeedInput) break;
    }
    (void)reader.takeDiagnostics();
    return result;
}

void parseBadJson(const std::string& line) {
    iox::json::JsonEventReader reader;
    reader.feed(iox::ByteView(line + "\n"));
    reader.finish();
    (void)reader.next();
    (void)reader.takeDiagnostics();
}

class SequenceReader final : public iox::Reader {
public:
    explicit SequenceReader(std::vector<iox::ReadOutcome> outcomes,
                            bool finished = true)
        : outcomes_(std::move(outcomes)), finished_(finished) {}

    iox::ReadOutcome next() override {
        if (position_ == outcomes_.size()) {
            return {iox::ReadOutcome::Status::End, std::nullopt, {}};
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
    bool finished_;
};

iox::ReadOutcome event(iox::IoxEvent value) {
    iox::ReadOutcome outcome;
    outcome.status = iox::ReadOutcome::Status::Event;
    outcome.event = std::move(value);
    return outcome;
}

std::unique_ptr<iox::Reader> sequence(std::vector<iox::IoxEvent> values) {
    std::vector<iox::ReadOutcome> outcomes;
    for (auto& value : values) outcomes.push_back(event(std::move(value)));
    outcomes.push_back({iox::ReadOutcome::Status::End, std::nullopt, {}});
    return std::make_unique<SequenceReader>(std::move(outcomes));
}

iox::IomObject richObject() {
    iox::IomObject object(iox::IomName("M.T.C"));
    object.setRef("R1");
    object.setBid("B1");
    object.setOrderPos(7);
    object.setPrimitive("null", iox::IomValue::null());
    object.setPrimitive("text", iox::IomValue::text("quote\" slash\\ newline\n"));
    object.setPrimitive("integer", iox::IomValue::integer(-17));
    object.setPrimitive("decimal", iox::IomValue::decimal(1.25));
    object.setPrimitive("boolean", iox::IomValue::boolean(true));
    object.setPrimitive("controls", iox::IomValue::text("\b\f\r\t\x01"));
    auto& repeated = object.setAttribute(iox::IomName("repeated"));
    repeated.values.emplace_back(iox::IomValue::integer(1));
    repeated.values.emplace_back(iox::IomValue::boolean(false));
    iox::IomObject child(iox::IomName("M.T.S"));
    child.setPrimitive("value", iox::IomValue::text("child"));
    object.setStructure("child", child);
    auto& attr = object.setAttribute(iox::IomName("refs"));
    attr.ref = "AR";
    attr.bid = "AB";
    attr.orderPos = 4;
    attr.values.emplace_back(child);
    return object;
}

} // namespace

IOX_TEST(coverage_iom_object_edge_paths) {
    const std::string byteBacking = "abc";
    iox::ByteView bytes(byteBacking);
    IOX_CHECK_EQ(static_cast<std::size_t>(2), bytes.subspan(1, 99).size());
    IOX_CHECK(bytes.subspan(99, 1).empty());
    IOX_CHECK_EQ("abc", bytes.str());
    IOX_CHECK(iox::ByteView("abc", 3) == bytes);
    IOX_CHECK(iox::ByteView("ab", 2) != bytes);

    const auto unknownVersion = iox::xtf::toString(iox::xtf::XtfVersion::Unknown);
    IOX_CHECK(std::string(unknownVersion) == "unknown");
    iox::XmlQualifiedName xmlOne("u", "n", "p");
    iox::XmlQualifiedName xmlTwo("u", "n", "q");
    iox::XmlQualifiedName xmlOther("v", "n");
    IOX_CHECK(xmlOne == xmlTwo);
    IOX_CHECK(xmlOne != xmlOther);
    iox::IomName nameOne("M.C");
    iox::IomName nameTwo("M.C");
    iox::IomName nameThree("M.D");
    IOX_CHECK(nameOne == nameTwo);
    IOX_CHECK(nameOne != nameThree);
    nameOne.setXmlName(xmlOne);
    IOX_CHECK(nameOne.hasXmlName());
    IOX_CHECK(nameOne != nameTwo);
    IOX_CHECK(iox::IomName::fromExpandedXmlName(xmlOne).hasXmlName());

    auto object = richObject();
    (void)object.attributes();
    IOX_CHECK(object.findAttribute("missing") == nullptr);
    IOX_CHECK(!object.getPrimitive("missing"));
    IOX_CHECK(!object.getPrimitive("repeated"));
    IOX_CHECK(!object.getPrimitive("child"));
    IOX_CHECK(object.getStructure("child").tag().iliName() == "M.T.S");
    IOX_CHECK(object.getStructure("text").tag().iliName().empty());
    IOX_CHECK(object.getStructure("missing").tag().iliName().empty());

    auto copy = object.deepCopy();
    IOX_CHECK(copy == object);
    copy.setTag(iox::IomName("M.T.Other"));
    IOX_CHECK(copy != object);

    auto sameTag = object.deepCopy();
    sameTag.setRef("different");
    IOX_CHECK(sameTag != object);
    sameTag = object.deepCopy();
    sameTag.setBid("different");
    IOX_CHECK(sameTag != object);
    sameTag = object.deepCopy();
    sameTag.setOrderPos(8);
    IOX_CHECK(sameTag != object);
    sameTag = object.deepCopy();
    sameTag.removeAttribute("integer");
    IOX_CHECK(sameTag != object);
    sameTag = object.deepCopy();
    sameTag.setPrimitive("integer", iox::IomValue::integer(18));
    IOX_CHECK(sameTag != object);
    IOX_CHECK(!object.removeAttribute("absent"));
    IOX_CHECK(object.attributeAt(0).name.iliName() == "null");

    try {
        (void)object.attributeAt(999);
        IOX_CHECK(false);
    } catch (const std::out_of_range&) {
        IOX_CHECK(true);
    }
}

IOX_TEST(coverage_json_all_values_and_error_paths) {
    const std::string input =
        "{\"event\":\"startTransfer\",\"sender\":\"S\",\"comment\":\"C\","
        "\"iliVersion\":\"2.4\",\"software\":\"W\",\"date\":\"D\",\"version\":\"24\"}\n"
        "{\"event\":\"startBasket\",\"basketType\":\"M.T.B\",\"bid\":\"B\","
        "\"consistency\":\"complete\",\"operation\":\"insert\",\"oidDomain\":3,"
        "\"startState\":\"a\",\"endState\":\"b\",\"kind\":\"snapshot\","
        "\"domains\":[\"d1\",4]}\n"
        "{\"event\":\"object\",\"operation\":\"update\",\"objectId\":\"T\","
        "\"consistency\":\"complete\",\"refBid\":\"B\",\"refOrderPos\":\"2\","
        "\"object\":{\"tag\":\"M.T.C\",\"ref\":\"R\",\"bid\":\"B\","
        "\"orderPos\":\"9\",\"attrs\":["
        "{\"name\":\"null\",\"value\":null},"
        "{\"name\":\"bool\",\"value\":true},"
        "{\"name\":\"int\",\"value\":-2},"
        "{\"name\":\"decimal\",\"value\":1.2e2},"
        "{\"name\":\"text\",\"value\":\"\\\"\\\\\\/\\b\\f\\n\\r\\t\\u0041\\u07ff\\u1234\"},"
        "{\"name\":\"many\",\"values\":[1,{\"tag\":\"M.T.S\",\"attrs\":[]}]},"
        "{\"name\":\"wrong\",\"ref\":4,\"bid\":true,\"orderPos\":false}]} }\n"
        "{\"type\":\"EndBasket\",\"bid\":\"B\"}\n"
        "{\"type\":\"EndTransfer\"}\n";
    auto events = readJson(input);
    IOX_CHECK_EQ(static_cast<std::size_t>(5), events.size());
    IOX_CHECK(std::holds_alternative<iox::ObjectEvent>(events[2]));
    const auto& object = std::get<iox::ObjectEvent>(events[2]).object;
    IOX_CHECK(object.ref().has_value());
    IOX_CHECK(object.findAttribute("many") != nullptr);

    const auto numericAndNull = readJson(
        "{\"event\":\"object\",\"object\":{\"tag\":\"M.C\",\"attrs\":["
        "{\"name\":\"expPlus\",\"value\":1e+2},"
        "{\"name\":\"expMinus\",\"value\":1e-2},"
        "{\"name\":\"array\",\"value\":[]}]},\"refBid\":null}\t\r\n");
    IOX_CHECK_EQ(static_cast<std::size_t>(1), numericAndNull.size());

    for (const auto& bad : {"{", "[]", "{\"x\"}", "{\"x\":}",
                            "{\"x\":[}", "{\"event\":\"unknown\"}",
                            "{\"event\":\"startTransfer\",\"sender\":\"\\u\"}",
                            "{\"event\":\"startTransfer\",\"sender\":\"\\q\"}",
                            "truX"}) {
        parseBadJson(bad);
    }

    iox::json::JsonEventReader state;
    state.feed(iox::ByteView(std::string("{\"event\":\"endTransfer\"}\n")));
    state.finish();
    state.finish();
    state.feed(iox::ByteView("x", 1));
    auto outcome = state.next();
    IOX_CHECK(outcome.event.has_value());
    IOX_CHECK(state.isFinished() == false);
    (void)state.next();
    IOX_CHECK(state.isFinished());
    (void)state.takeDiagnostics();
}

IOX_TEST(coverage_json_writer_all_fields) {
    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::json::JsonEventWriter writer(sink);
    iox::StartTransferEvent transfer;
    transfer.sender = "S\"\\\n";
    transfer.comment = "C";
    transfer.iliVersion = "2.4";
    transfer.software = "W";
    transfer.date = "D";
    transfer.version = 24;
    writer.write(transfer);

    iox::StartBasketEvent basket;
    basket.basketType = iox::IomName("M.T.B");
    basket.bid = "B";
    basket.consistency = "complete";
    basket.operation = "insert";
    basket.oidDomain = 5;
    basket.startState = "a";
    basket.endState = "b";
    basket.kind = "snapshot";
    basket.domains = {"d1", "d2"};
    writer.write(basket);

    iox::ObjectEvent object;
    object.operation = "insert";
    object.objectId = "T";
    object.consistency = "complete";
    object.refBid = "B";
    object.refOrderPos = "1";
    object.object = richObject();
    writer.write(object);
    writer.write(iox::EndBasketEvent{"B"});
    writer.write(iox::EndTransferEvent{});
    writer.flush();
    writer.close();
    writer.write(iox::EndTransferEvent{});
    IOX_CHECK(writer.isClosed());
    IOX_CHECK(!sink->str().empty());
    IOX_CHECK(!writer.takeDiagnostics().empty());
}

IOX_TEST(coverage_xml_callbacks_limits_and_writer) {
    std::string starts;
    std::string ends;
    std::string text;
    std::string comments;
    std::string instructions;
    iox::xml::ExpatCallbacks callbacks;
    callbacks.onStartElement = [&](std::string_view name,
                                   const std::vector<std::pair<std::string_view, std::string_view>>& attrs) {
        starts += std::string(name) + ":" + std::to_string(attrs.size()) + ";";
    };
    callbacks.onEndElement = [&](std::string_view name) { ends += std::string(name) + ";"; };
    callbacks.onCharacterData = [&](std::string_view value) { text += value; };
    callbacks.onComment = [&](std::string_view value) { comments += value; };
    callbacks.onProcessingInstruction = [&](std::string_view target, std::string_view value) {
        instructions += std::string(target);
        instructions += std::string(value);
    };
    callbacks.onXmlDeclaration = [&](std::string_view, std::string_view, bool) { instructions += "xml"; };
    iox::xml::ExpatParser parser(std::move(callbacks));
    IOX_CHECK(parser.feed(iox::ByteView(std::string(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?><r a=\"v\"><c>text</c><!-- x --></r>"))));
    IOX_CHECK(parser.finish());
    IOX_CHECK(!starts.empty() && !ends.empty() && text == "text");
    IOX_CHECK(comments == " x ");
    IOX_CHECK(!instructions.empty());
    IOX_CHECK(!parser.feed(iox::ByteView(std::string("x"))));
    IOX_CHECK(parser.finish());
    IOX_CHECK(parser.byteOffset() > 0 && parser.line() > 0 && parser.column() >= 0);

    iox::xml::ExpatParser noCallbacks({});
    IOX_CHECK(noCallbacks.feed(iox::ByteView(std::string(
        "<?xml version=\"1.0\"?><r>text<!--comment--><?pi value?></r>"))));
    IOX_CHECK(noCallbacks.finish());

    auto malformed = iox::xml::ExpatParser({});
    IOX_CHECK(malformed.feed(iox::ByteView(std::string("<r>"))));
    IOX_CHECK(!malformed.finish());
    (void)malformed.takeDiagnostics();
    auto dtd = iox::xml::ExpatParser({});
    IOX_CHECK(!dtd.feed(iox::ByteView(std::string("<!DOCTYPE r><r/>"))));
    (void)dtd.takeDiagnostics();

    iox::xml::ExpatLimits limit;
    limit.maxAttributeCount = 0;
    auto attrLimited = iox::xml::ExpatParser({}, limit);
    IOX_CHECK(!attrLimited.feed(iox::ByteView(std::string("<r a=\"1\"/>"))));
    limit = {};
    limit.maxAttributeValueLength = 0;
    auto valueLimited = iox::xml::ExpatParser({}, limit);
    IOX_CHECK(!valueLimited.feed(iox::ByteView(std::string("<r a=\"1\"/>"))));
    limit = {};
    limit.maxElementNameLength = 1;
    auto nameLimited = iox::xml::ExpatParser({}, limit);
    IOX_CHECK(!nameLimited.feed(iox::ByteView(std::string("<root/>"))));
    limit = {};
    limit.maxElementDepth = 1;
    auto depthLimited = iox::xml::ExpatParser({}, limit);
    IOX_CHECK(!depthLimited.feed(iox::ByteView(std::string("<r><c/></r>"))));

    const auto throwingParser = [](iox::xml::ExpatCallbacks callbacks,
                                   std::string input) {
        auto parser = iox::xml::ExpatParser(std::move(callbacks));
        (void)parser.feed(iox::ByteView(input));
        (void)parser.takeDiagnostics();
    };
    {
        iox::xml::ExpatCallbacks cb;
        cb.onStartElement = [](std::string_view, const auto&) { throw std::runtime_error("start"); };
        throwingParser(std::move(cb), "<r/>");
    }
    {
        iox::xml::ExpatCallbacks cb;
        cb.onEndElement = [](std::string_view) { throw std::runtime_error("end"); };
        throwingParser(std::move(cb), "<r/>");
    }
    {
        iox::xml::ExpatCallbacks cb;
        cb.onCharacterData = [](std::string_view) { throw std::runtime_error("text"); };
        throwingParser(std::move(cb), "<r>x</r>");
    }
    {
        iox::xml::ExpatCallbacks cb;
        cb.onProcessingInstruction = [](std::string_view, std::string_view) { throw std::runtime_error("pi"); };
        throwingParser(std::move(cb), "<?pi x?><r/>");
    }
    {
        iox::xml::ExpatCallbacks cb;
        cb.onComment = [](std::string_view) { throw std::runtime_error("comment"); };
        throwingParser(std::move(cb), "<!--x--><r/>");
    }
    {
        iox::xml::ExpatCallbacks cb;
        cb.onXmlDeclaration = [](std::string_view, std::string_view, bool) { throw std::runtime_error("decl"); };
        throwingParser(std::move(cb), "<?xml version=\"1.0\"?><r/>");
    }

    std::string xml;
    iox::xml::XmlWriter writer([&](const void* data, std::size_t size) {
        xml.append(static_cast<const char*>(data), size);
    }, true, 1);
    writer.writeDeclaration();
    writer.writeStartElement("root", {{"a", "<&>\"'"}}, false);
    writer.writeComment("bad--comment");
    writer.writeStartElement("empty", {}, true);
    writer.writeStartElement("child", {}, false);
    writer.writeText("<&>\"'");
    writer.writeEndElement("child");
    writer.writeEndElement("root");
    writer.flush();
    IOX_CHECK(writer.hasWritten());
    IOX_CHECK_EQ(0, writer.depth());
    IOX_CHECK(xml.find("&amp;") != std::string::npos);
}

IOX_TEST(coverage_basket_and_registry_state_paths) {
    iox::StartTransferEvent transfer;
    iox::StartBasketEvent basket;
    basket.basketType = iox::IomName("M.T.B");
    basket.bid = "B";
    iox::ObjectEvent object;
    object.object = iox::IomObject(iox::IomName("M.T.C"));
    object.objectId = "T";

    iox::BasketReader valid(sequence({transfer, basket, object,
                                      iox::EndBasketEvent{"B"},
                                      iox::EndTransferEvent{}}));
    IOX_CHECK(valid.header().has_value());
    IOX_CHECK(valid.readBasket().has_value());
    IOX_CHECK(!valid.readBasket().has_value());
    (void)valid.takeDiagnostics();

    iox::BasketReader missing(sequence({iox::EndTransferEvent{}}));
    IOX_CHECK(!missing.header().has_value());
    (void)missing.readBasket();
    (void)missing.takeDiagnostics();

    iox::BasketReader early(sequence({basket, transfer}));
    IOX_CHECK(!early.header().has_value());
    (void)early.takeDiagnostics();

    iox::BasketReader unexpected(sequence({transfer, object}));
    IOX_CHECK(unexpected.header().has_value());
    IOX_CHECK(!unexpected.readBasket().has_value());
    (void)unexpected.takeDiagnostics();

    iox::BasketReader mismatch(sequence({transfer, basket, iox::EndBasketEvent{"other"}}));
    IOX_CHECK(!mismatch.readBasket().has_value());
    (void)mismatch.takeDiagnostics();

    iox::BasketReader limited(sequence({transfer, basket, object,
                                        iox::EndBasketEvent{"B"}}), 0);
    IOX_CHECK(limited.readBasket().has_value());
    iox::BasketReader limitedOne(sequence({transfer, basket, object, object,
                                           iox::EndBasketEvent{"B"}}), 1);
    IOX_CHECK(!limitedOne.readBasket().has_value());
    (void)limitedOne.takeDiagnostics();

    iox::ReadOutcome needsInputOutcome;
    needsInputOutcome.status = iox::ReadOutcome::Status::NeedInput;
    needsInputOutcome.diagnostics.push_back(
        {iox::Diagnostic::Severity::Warning, iox::ErrorCode::InternalError, "warning"});
    iox::BasketReader needsInput(std::make_unique<SequenceReader>(
        std::vector<iox::ReadOutcome>{std::move(needsInputOutcome)}, false));
    IOX_CHECK(!needsInput.header().has_value());
    (void)needsInput.takeDiagnostics();
    iox::BasketReader noReader(nullptr);
    IOX_CHECK(!noReader.header().has_value());
    (void)noReader.takeDiagnostics();
    iox::BasketReader endedEarly(sequence({transfer, basket}));
    IOX_CHECK(!endedEarly.readBasket().has_value());
    (void)endedEarly.takeDiagnostics();
    iox::BasketReader unexpectedInside(sequence({transfer, basket, iox::EndTransferEvent{}}));
    IOX_CHECK(!unexpectedInside.readBasket().has_value());
    (void)unexpectedInside.takeDiagnostics();
    iox::BasketReader emptyEndBid(sequence({transfer, basket, iox::EndBasketEvent{""}}));
    IOX_CHECK(emptyEndBid.readBasket().has_value());
    iox::StartBasketEvent noBasketBid = basket;
    noBasketBid.bid.clear();
    iox::BasketReader emptyBasketBid(sequence({transfer, noBasketBid,
                                               iox::EndBasketEvent{"B"}}));
    IOX_CHECK(emptyBasketBid.readBasket().has_value());

    iox::FormatRegistry registry;
    iox::FormatEntry noRead;
    noRead.name = "none";
    noRead.extensions = {"NONE"};
    noRead.canRead = false;
    noRead.canWrite = false;
    registry.addFormat(noRead);
    IOX_CHECK(!registry.createReader("none"));
    IOX_CHECK(!registry.createWriter("none", std::make_shared<iox::StringOutputSink>()));
    noRead.canRead = true;
    noRead.readerFactory = [] { return std::make_unique<iox::json::JsonEventReader>(); };
    noRead.scoreSniffer = [](iox::ByteView) { return 101; };
    registry.addFormat(noRead);
    IOX_CHECK(registry.createReaderBySniffing(iox::ByteView("x", 1)) != nullptr);
    noRead.scoreSniffer = [](iox::ByteView) { return -1; };
    registry.addFormat(noRead);
    IOX_CHECK(registry.createReaderBySniffing({}, "none") != nullptr);

    iox::FormatEntry sniffed;
    sniffed.name = "sniffed";
    sniffed.extensions = {"SNF"};
    sniffed.canRead = true;
    sniffed.readerFactory = [] { return std::make_unique<iox::json::JsonEventReader>(); };
    sniffed.sniffer = [](iox::ByteView input) {
        return input.size() == 1 ? std::string("sniffed") : std::string();
    };
    registry.addFormat(sniffed);
    IOX_CHECK(registry.createReaderBySniffing(iox::ByteView("x", 1)) != nullptr);
    IOX_CHECK(registry.createReaderBySniffing({}, "snf") != nullptr);
    iox::FormatEntry noFactory = sniffed;
    noFactory.name = "no-factory";
    noFactory.readerFactory = {};
    noFactory.sniffer = {};
    noFactory.extensions = {"NF"};
    registry.addFormat(noFactory);
    IOX_CHECK(!registry.createReaderBySniffing({}, "nf"));
    iox::FormatEntry noWriter;
    noWriter.name = "no-writer";
    noWriter.canWrite = true;
    registry.addFormat(noWriter);
    IOX_CHECK(!registry.createWriter("no-writer", std::make_shared<iox::StringOutputSink>()));
    IOX_CHECK(registry.removeFormat("none"));
    IOX_CHECK(!registry.removeFormat("none"));
}

IOX_TEST(coverage_xtf_and_abi_state_paths) {
    const std::string minimal =
        "<?xml version=\"1.0\"?><ili:TRANSFER xmlns:ili=\"http://www.interlis.ch/INTERLIS2.3\">"
        "<ili:HEADERSECTION><ili:SENDER>S</ili:SENDER><ili:COMMENT>C</ili:COMMENT>"
        "<ili:VERSION>2.3</ili:VERSION><ili:SOFTWARE>W</ili:SOFTWARE><ili:DATE>D</ili:DATE>"
        "</ili:HEADERSECTION></ili:TRANSFER>";
    iox::xtf::XtfReaderOptions options;
    options.expectedVersion = iox::xtf::XtfVersion::Xtf24;
    iox::xtf::XtfReader wrong(options);
    wrong.feed(iox::ByteView(minimal));
    wrong.finish();
    (void)wrong.next();
    (void)wrong.takeDiagnostics();
    wrong.feed(iox::ByteView("x", 1));
    wrong.finish();

    iox::xtf::XtfReader needInput;
    IOX_CHECK_EQ(iox::ReadOutcome::Status::NeedInput, needInput.next().status);
    needInput.feed(iox::ByteView(std::string("<root/>")));
    (void)needInput.next();
    (void)needInput.takeDiagnostics();
    iox::xtf::XtfReader headerOnly;
    headerOnly.feed(iox::ByteView(minimal));
    headerOnly.finish();
    IOX_CHECK(headerOnly.detectedVersion() == iox::xtf::XtfVersion::Xtf23);
    (void)headerOnly.next();
    headerOnly.finish();
    (void)headerOnly.takeDiagnostics();

    const std::string namespaced24 =
        "<?xml version=\"1.0\"?><ili:TRANSFER xmlns:ili=\"http://www.interlis.ch/xtf/2.4/INTERLIS\""
        " xmlns:m=\"http://model\"><ili:HEADERSECTION/><ili:datasection>"
        "<ili:basket ili:bid=\"B\"><m:Class ili:tid=\"T\"><m:value>v</m:value>"
        "</m:Class></ili:basket></ili:datasection></ili:TRANSFER>";
    iox::xtf::XtfReader namespacedReader;
    namespacedReader.feed(iox::ByteView(namespaced24));
    namespacedReader.finish();
    while (namespacedReader.next().status != iox::ReadOutcome::Status::End) {}
    (void)namespacedReader.takeDiagnostics();
    for (const auto& namespaceUri : {std::string("http://www.interlis.ch/INTERLIS2.4"),
                                     std::string("http://www.interlis.ch/INTERLIS/2.4")}) {
        iox::xtf::XtfReader versionReader;
        const auto xml = "<TRANSFER xmlns=\"" + namespaceUri + "\"><HEADERSECTION/>"
                         "</TRANSFER>";
        versionReader.feed(iox::ByteView(xml));
        versionReader.finish();
        while (versionReader.next().status != iox::ReadOutcome::Status::End) {}
    }
    iox::xtf::XtfReader objectHeaderReader;
    objectHeaderReader.feed(iox::ByteView(std::string(
        "<TRANSFER><HEADERSECTION/><Class TID=\"T\"><value>x</value></Class></TRANSFER>")));
    objectHeaderReader.finish();
    while (objectHeaderReader.next().status != iox::ReadOutcome::Status::End) {}

    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::xtf::XtfWriterOptions writerOptions;
    writerOptions.version = iox::xtf::XtfVersion::Xtf24;
    writerOptions.pretty = true;
    writerOptions.sender = "S";
    writerOptions.comment = "C";
    writerOptions.software = "W";
    iox::xtf::XtfWriter writer(sink, writerOptions);
    writer.write(iox::EndTransferEvent{});
    iox::StartTransferEvent transfer;
    transfer.version = 24;
    transfer.sender = "S";
    writer.write(transfer);
    iox::StartBasketEvent basket;
    basket.basketType = iox::IomName("M.T.B");
    basket.bid = "B";
    basket.oidDomain = 1;
    basket.startState = "a";
    basket.endState = "b";
    basket.kind = "snapshot";
    writer.write(basket);
    iox::ObjectEvent object;
    object.objectId = "T";
    object.object = richObject();
    object.refBid = "B";
    object.refOrderPos = "1";
    writer.write(object);
    writer.write(iox::EndBasketEvent{"B"});
    writer.write(iox::EndTransferEvent{});
    writer.flush();
    writer.close();
    writer.write(transfer);
    (void)writer.takeDiagnostics();

    const std::string iliNs = "http://www.interlis.ch/xtf/2.4/INTERLIS";
    const std::string geometryNs = "http://www.interlis.ch/geometry/1.0";
    const auto qname = [](std::string ili, std::string uri, std::string local,
                          std::string prefix = {}) {
        return iox::IomName(std::move(ili),
                            iox::XmlQualifiedName(std::move(uri), std::move(local),
                                                  std::move(prefix)));
    };
    iox::IomObject qualifiedObject(qname("M.Class", iliNs, "Class"));
    auto& emptyQualified = qualifiedObject.setAttribute(qname("empty", "", "empty"));
    (void)emptyQualified;
    auto& geomAttribute = qualifiedObject.setAttribute(qname("coord", geometryNs, "coord"));
    geomAttribute.values.emplace_back(iox::IomValue::decimal(1.0));
    auto& modelOne = qualifiedObject.setAttribute(qname("one", "http://model-1", "one"));
    modelOne.values.emplace_back(iox::IomValue::text("one"));
    auto& modelTwo = qualifiedObject.setAttribute(qname("two", "http://model-2", "two", "ili"));
    modelTwo.values.emplace_back(iox::IomValue::text("two"));
    auto& modelThree = qualifiedObject.setAttribute(qname("three", "http://model-3", "three", "geom"));
    modelThree.values.emplace_back(iox::IomValue::text("three"));
    auto& modelFour = qualifiedObject.setAttribute(qname("four", "http://model-4", "four", "m0"));
    modelFour.values.emplace_back(iox::IomValue::text("four"));
    auto& sameModel = qualifiedObject.setAttribute(qname("same", "http://model-1", "same"));
    sameModel.values.emplace_back(iox::IomValue::text("same"));
    auto& defaultStructure = qualifiedObject.setAttribute(iox::IomName("defaultStructure"));
    defaultStructure.values.emplace_back(iox::IomObject{});
    qualifiedObject.setAttribute(iox::IomName("emptyValue"));

    iox::IomObject polyline(iox::IomName("polyline"));
    auto& sequenceAttribute = polyline.setAttribute(iox::IomName("sequence"));
    iox::IomObject sequenceObject(iox::IomName("SEQUENCE"));
    auto& segmentAttribute = sequenceObject.setAttribute(iox::IomName("segment"));
    segmentAttribute.values.emplace_back(iox::IomObject(iox::IomName("coord")));
    sequenceAttribute.values.emplace_back(std::move(sequenceObject));
    auto& geometryValue = qualifiedObject.setAttribute(iox::IomName("shape"));
    geometryValue.values.emplace_back(std::move(polyline));

    iox::IomObject multi(iox::IomName("multicoord"));
    auto& multiValues = multi.setAttribute(iox::IomName("coord"));
    multiValues.values.emplace_back(iox::IomObject(iox::IomName("coord")));
    multiValues.values.emplace_back(iox::IomValue::text("skipped"));
    auto& multiAttribute = qualifiedObject.setAttribute(iox::IomName("multi"));
    multiAttribute.values.emplace_back(std::move(multi));

    auto qualifiedSink = std::make_shared<iox::StringOutputSink>();
    iox::xtf::XtfWriter qualifiedWriter(qualifiedSink,
        iox::xtf::XtfWriterOptions{iox::xtf::XtfVersion::Xtf24, false, false,
                                   "sender", "comment", "software"});
    iox::StartTransferEvent qualifiedTransfer;
    qualifiedTransfer.version = 24;
    qualifiedWriter.write(qualifiedTransfer);
    iox::StartBasketEvent qualifiedBasket;
    qualifiedBasket.basketType = qname("M.Topic", "http://basket-model", "Topic");
    qualifiedBasket.bid = "B";
    qualifiedBasket.consistency = "incomplete";
    qualifiedBasket.operation = "update";
    qualifiedWriter.write(qualifiedBasket);
    iox::ObjectEvent qualifiedEvent;
    qualifiedEvent.objectId = "T";
    qualifiedEvent.operation = "update";
    qualifiedEvent.object = std::move(qualifiedObject);
    qualifiedWriter.write(qualifiedEvent);
    qualifiedWriter.write(iox::EndBasketEvent{"B"});
    qualifiedWriter.write(iox::EndTransferEvent{});
    qualifiedWriter.close();
    IOX_CHECK(qualifiedSink->str().find("xmlns:m") != std::string::npos);

    for (const auto& invalid : {iox::IoxEvent{iox::StartTransferEvent{}},
                                iox::IoxEvent{iox::StartBasketEvent{}},
                                iox::IoxEvent{iox::ObjectEvent{}},
                                iox::IoxEvent{iox::EndBasketEvent{}},
                                iox::IoxEvent{iox::EndTransferEvent{}}}) {
        auto invalidSink = std::make_shared<iox::StringOutputSink>();
        iox::xtf::XtfWriter invalidWriter(invalidSink, {});
        invalidWriter.write(invalid);
        (void)invalidWriter.takeDiagnostics();
    }

    IOX_CHECK(iox_reader_create(nullptr, nullptr) == nullptr);
    IOX_CHECK(iox_reader_create("unknown", nullptr) == nullptr);
    auto reader = iox_reader_create("xtf24", "{\"strict\":true,\"sourceName\":\"s\",\"expectedVersion\":\"24\"}");
    IOX_CHECK(reader != nullptr);
    auto reader23 = iox_reader_create("xtf23", nullptr);
    auto reader24 = iox_reader_create("xtf24", nullptr);
    auto explicit23 = iox_reader_create("xtf", "{\"expectedVersion\":\"2.3\"}");
    IOX_CHECK(reader23 != nullptr && reader24 != nullptr && explicit23 != nullptr);
    iox_reader_destroy(reader23);
    iox_reader_destroy(reader24);
    iox_reader_destroy(explicit23);
    auto malformedOptionsA = iox_reader_create("xtf", "{\"sourceName\"}");
    auto malformedOptionsB = iox_reader_create("xtf", "{\"sourceName\":}");
    auto malformedOptionsC = iox_reader_create("xtf", "{\"strict\":maybe}");
    IOX_CHECK(malformedOptionsA != nullptr && malformedOptionsB != nullptr && malformedOptionsC != nullptr);
    iox_reader_destroy(malformedOptionsA);
    iox_reader_destroy(malformedOptionsB);
    iox_reader_destroy(malformedOptionsC);
    iox_result_t* result = nullptr;
    IOX_CHECK(iox_reader_feed(reader, nullptr, 1) == IOX_STATUS_INVALID_ARGUMENT);
    IOX_CHECK(iox_reader_feed(reader, nullptr, 0) == IOX_STATUS_OK);
    IOX_CHECK(iox_reader_next(reader, nullptr) == IOX_STATUS_INVALID_ARGUMENT);
    (void)iox_reader_finish(reader);
    (void)iox_reader_next(reader, &result);
    if (result) iox_result_destroy(result);
    iox_reader_destroy(reader);

    auto malformedReader = iox_reader_create("xtf", "{\"preserveUnknownExtensions\":false}");
    IOX_CHECK(malformedReader != nullptr);
    IOX_CHECK(iox_reader_feed(malformedReader,
                              reinterpret_cast<const std::uint8_t*>("<r></x>"), 7) == IOX_STATUS_ERROR);
    IOX_CHECK(iox_reader_next(malformedReader, &result) == IOX_STATUS_ERROR);
    IOX_CHECK(result != nullptr);
    IOX_CHECK(std::string(iox_result_json(result)).find("location") != std::string::npos);
    iox_result_destroy(result);
    iox_reader_destroy(malformedReader);

    const char escapedOptions[] = "{\"sourceName\":\"q\\\"s\\\\b\b\f\n\r\t\"}";
    auto escapedReader = iox_reader_create("xtf", escapedOptions);
    IOX_CHECK(escapedReader != nullptr);
    IOX_CHECK(iox_reader_feed(escapedReader,
                              reinterpret_cast<const std::uint8_t*>("<r></x>"), 7) == IOX_STATUS_ERROR);
    IOX_CHECK(iox_reader_next(escapedReader, &result) == IOX_STATUS_ERROR);
    IOX_CHECK(result != nullptr);
    IOX_CHECK(std::string(iox_result_json(result)).find("error") != std::string::npos);
    iox_result_destroy(result);
    iox_reader_destroy(escapedReader);

    const std::string allJson =
        "{\"event\":\"startTransfer\",\"sender\":\"special\\n\"}\n"
        "{\"event\":\"startBasket\",\"basketType\":\"M.B\",\"bid\":\"B\"}\n"
        "{\"event\":\"object\",\"objectId\":\"T\",\"object\":{\"tag\":\"M.C\"}}\n"
        "{\"event\":\"endBasket\",\"bid\":\"B\"}\n"
        "{\"event\":\"endTransfer\"}\n";
    auto allReader = iox_reader_create("json-events", nullptr);
    IOX_CHECK(allReader != nullptr);
    IOX_CHECK(iox_reader_feed(allReader,
                              reinterpret_cast<const std::uint8_t*>(allJson.data()),
                              allJson.size()) == IOX_STATUS_OK);
    IOX_CHECK(iox_reader_finish(allReader) == IOX_STATUS_OK);
    int eventCount = 0;
    while (true) {
        const auto status = iox_reader_next(allReader, &result);
        if (result) iox_result_destroy(result);
        if (status == IOX_STATUS_END) break;
        IOX_CHECK_EQ(IOX_STATUS_EVENT, status);
        ++eventCount;
    }
    IOX_CHECK_EQ(5, eventCount);
    iox_reader_destroy(allReader);
    IOX_CHECK(iox_reader_next(nullptr, &result) == IOX_STATUS_INVALID_ARGUMENT);
    IOX_CHECK(iox_reader_finish(nullptr) == IOX_STATUS_INVALID_ARGUMENT);

    IOX_CHECK(iox_writer_create(nullptr, nullptr) == nullptr);
    IOX_CHECK(iox_writer_create("unknown", nullptr) == nullptr);
    auto jsonWriter = iox_writer_create("json-events", "{\"pretty\":true}");
    IOX_CHECK(jsonWriter != nullptr);
    IOX_CHECK(iox_writer_write_event_json(jsonWriter, nullptr, 0, &result) == IOX_STATUS_INVALID_ARGUMENT);
    IOX_CHECK(result != nullptr);
    iox_result_destroy(result);
    const char* end = "{\"event\":\"endTransfer\"}\n";
    IOX_CHECK(iox_writer_write_event_json(jsonWriter, end, std::strlen(end), &result) == IOX_STATUS_OK);
    iox_result_destroy(result);

    auto escapedWriter = iox_writer_create("json-events", nullptr);
    const char escapedEvent[] = "{\"event\":\"x\\\"\\\\\\b\\f\\n\\r\\t\\u0001\"}";
    IOX_CHECK(escapedWriter != nullptr);
    IOX_CHECK(iox_writer_write_event_json(escapedWriter, escapedEvent,
                                          std::strlen(escapedEvent), &result) == IOX_STATUS_ERROR);
    IOX_CHECK(result != nullptr);
    IOX_CHECK(std::string(iox_result_json(result)).find("\\\"") != std::string::npos);
    iox_result_destroy(result);
    iox_writer_destroy(escapedWriter);

    auto emptyWriter = iox_writer_create("json-events", nullptr);
    IOX_CHECK(emptyWriter != nullptr);
    IOX_CHECK(iox_writer_take_output(emptyWriter, &result) == IOX_STATUS_OK);
    IOX_CHECK(iox_result_bytes(result) == nullptr);
    IOX_CHECK_EQ(static_cast<std::size_t>(0), iox_result_size(result));
    IOX_CHECK_EQ(IOX_STATUS_OK, iox_result_status(result));
    iox_result_destroy(result);
    iox_writer_destroy(emptyWriter);

    IOX_CHECK(iox_writer_finish(jsonWriter, &result) == IOX_STATUS_OK);
    iox_result_destroy(result);
    IOX_CHECK(iox_writer_finish(jsonWriter, &result) == IOX_STATUS_INVALID_STATE);
    iox_result_destroy(result);
    IOX_CHECK(iox_writer_take_output(jsonWriter, &result) == IOX_STATUS_INVALID_STATE);
    iox_result_destroy(result);
    IOX_CHECK(iox_writer_write_event_json(jsonWriter, end, std::strlen(end), &result) == IOX_STATUS_INVALID_STATE);
    iox_result_destroy(result);
    IOX_CHECK(iox_writer_write_event_json(jsonWriter, end, std::strlen(end), nullptr) == IOX_STATUS_INVALID_ARGUMENT);
    IOX_CHECK(iox_writer_take_output(jsonWriter, nullptr) == IOX_STATUS_INVALID_ARGUMENT);
    IOX_CHECK(iox_writer_finish(jsonWriter, nullptr) == IOX_STATUS_INVALID_ARGUMENT);
    iox_writer_destroy(jsonWriter);
    IOX_CHECK(iox_result_json(nullptr) == nullptr);
    IOX_CHECK(iox_result_bytes(nullptr) == nullptr);
    IOX_CHECK(iox_result_size(nullptr) == 0);
    IOX_CHECK(iox_result_status(nullptr) == IOX_STATUS_INVALID_ARGUMENT);
    iox_reader_destroy(nullptr);
    iox_writer_destroy(nullptr);
    iox_result_destroy(nullptr);
}

IOX_TEST(coverage_direct_xtf_dialect_paths) {
    const auto expanded = [](std::string_view uri, std::string_view local) {
        return std::string(uri) + "\xFF" + std::string(local);
    };
    std::vector<iox::IoxEvent> events23;
    iox::xtf::Xtf23Dialect dialect23({
        [&](iox::IoxEvent value) { events23.push_back(std::move(value)); },
        [](iox::Diagnostic) {}});
    const std::vector<std::pair<std::string_view, std::string_view>> basketAttrs = {
        {"BID", "B"}, {"CONSISTENCY", "incomplete"}, {"OPERATION", "update"},
        {"OID_DOMAIN", "bad"}};
    dialect23.onStartElement("ili:BASKET", basketAttrs);
    dialect23.onStartElement("Class", {{"TID", "T"}, {"OPERATION", "delete"}, {"BID", "B"}});
    dialect23.onStartElement("Name", {{"REF", "R"}, {"BID", "B"}, {"ORDER_POS", "bad"}});
    dialect23.onCharacterData("one");
    dialect23.onEndElement("Name");
    dialect23.onStartElement("Name", {});
    dialect23.onCharacterData("two");
    dialect23.onEndElement("Name");
    dialect23.onStartElement("Address", {});
    dialect23.onStartElement("Street", {});
    dialect23.onCharacterData("Main");
    dialect23.onEndElement("Street");
    dialect23.onEndElement("Address");
    dialect23.onEndElement("Class");
    dialect23.onEndElement("BASKET");
    dialect23.onStartElement("unknown", {});
    dialect23.onEndElement("unknown");
    dialect23.reset();
    IOX_CHECK(!dialect23.isFatal());
    IOX_CHECK_EQ(static_cast<std::size_t>(3), events23.size());

    iox::xtf::Xtf23Dialect dialect23Names({
        [](iox::IoxEvent) {}, [](iox::Diagnostic) {}});
    dialect23Names.onStartElement("BASKET", {{"BID", "B"}});
    dialect23Names.onEndElement("BASKET");
    dialect23Names.onStartElement("ili:BASKET", {{"BID", "B"}});
    dialect23Names.onEndElement("ili:BASKET");
    dialect23Names.onStartElement("ili:Class", {{"TID", "T"}});
    dialect23Names.onEndElement("ili:Class");
    dialect23Names.onEndElement("none");
    dialect23Names.onStartElement("unknown", {});
    dialect23Names.onEndElement("unknown");
    dialect23Names.onCharacterData("ignored");

    std::vector<iox::IoxEvent> events24;
    iox::xtf::Xtf24Dialect dialect24({
        [&](iox::IoxEvent value) { events24.push_back(std::move(value)); },
        [](iox::Diagnostic) {}});
    const auto iliBasket = expanded("http://www.interlis.ch/xtf/2.4/INTERLIS", "basket");
    const auto modelClass = expanded("http://model", "Class");
    const auto geomCoord = expanded("http://www.interlis.ch/geometry/1.0", "coord");
    const auto geomArc = expanded("http://www.interlis.ch/geometry/1.0", "arc");
    dialect24.onStartElement(iliBasket,
                             {{"bid", "B"}, {"oid_domain", "1"}});
    dialect24.onStartElement(modelClass, {{"tid", "T"}});

    const auto addText = [&](std::string_view name) {
        dialect24.onStartElement(name, {});
        dialect24.onCharacterData("1");
        dialect24.onEndElement(name);
    };
    const auto addCoord = [&](std::string_view name) {
        dialect24.onStartElement(name, {});
        addText("c1");
        addText("c2");
        dialect24.onEndElement(name);
    };
    dialect24.onStartElement("plain", {});
    addText("value");
    dialect24.onEndElement("plain");
    dialect24.onStartElement("polyline", {});
    addCoord(geomCoord);
    dialect24.onStartElement(geomArc, {});
    addText("c1");
    dialect24.onEndElement(geomArc);
    dialect24.onEndElement("polyline");
    dialect24.onStartElement("multicoord", {});
    addCoord("coord");
    dialect24.onEndElement("multicoord");
    dialect24.onStartElement("multipolyline", {});
    dialect24.onStartElement("polyline", {});
    addCoord("coord");
    dialect24.onEndElement("polyline");
    dialect24.onEndElement("multipolyline");
    dialect24.onStartElement("multisurface", {});
    dialect24.onStartElement("surface", {});
    dialect24.onEndElement("surface");
    dialect24.onEndElement("multisurface");
    dialect24.onStartElement("multiarea", {});
    dialect24.onStartElement("area", {});
    dialect24.onEndElement("area");
    dialect24.onEndElement("multiarea");
    dialect24.onStartElement("orientablecurve", {});
    addCoord(geomCoord);
    dialect24.onEndElement("orientablecurve");
    dialect24.onEndElement(modelClass);
    dialect24.onEndElement(iliBasket);
    dialect24.reset();
    IOX_CHECK(!dialect24.isFatal());
    IOX_CHECK_EQ(static_cast<std::size_t>(3), events24.size());

    const auto exerciseClosed = [](std::string name,
                                   std::vector<std::pair<std::string_view, std::string_view>> attrs) {
        iox::xtf::Xtf24Dialect local({[](iox::IoxEvent) {}, [](iox::Diagnostic) {}});
        local.onStartElement(name, attrs);
        local.onEndElement(name);
        local.onEndElement(name);
    };
    exerciseClosed(expanded("http://www.interlis.ch/xtf/2.4/INTERLIS", "BASKET"), {{"BID", "B"}});
    exerciseClosed(expanded("http://www.interlis.ch/INTERLIS2.4", "BASKET"), {{"BID", "B"}});
    exerciseClosed("basket", {{"BID", "B"}});
    exerciseClosed("ili:BASKET", {{"BID", "B"}});

    iox::xtf::Xtf24Dialect attributePaths({[](iox::IoxEvent) {}, [](iox::Diagnostic) {}});
    attributePaths.onStartElement("basket", {{"BID", "B"}});
    attributePaths.onStartElement("Class", {{"TID", "T"}, {"BID", "B"}, {"OPERATION", "update"}});
    attributePaths.onStartElement("a", {{"REF", "R"}, {"BID", "B"}, {"ORDER_POS", "4"}});
    attributePaths.onCharacterData("value");
    attributePaths.onEndElement("a");
    attributePaths.onStartElement("a", {});
    attributePaths.onCharacterData("repeated");
    attributePaths.onEndElement("a");
    attributePaths.onStartElement("badOrder", {{"ORDER_POS", "not-a-number"}});
    attributePaths.onEndElement("badOrder");
    attributePaths.onEndElement("Class");
    attributePaths.onEndElement("basket");

    iox::xtf::Xtf24Dialect geometryNamespaces({[](iox::IoxEvent) {}, [](iox::Diagnostic) {}});
    geometryNamespaces.onStartElement("basket", {{"BID", "B"}});
    geometryNamespaces.onStartElement("Class", {{"TID", "T"}});
    geometryNamespaces.onStartElement("shape", {});
    geometryNamespaces.onStartElement("polyline", {});
    geometryNamespaces.onStartElement(expanded("http://custom", "member"), {});
    geometryNamespaces.onEndElement("member");
    geometryNamespaces.onStartElement(expanded("http://www.interlis.ch/geometry/1.0", "custom"), {});
    geometryNamespaces.onEndElement("custom");
    geometryNamespaces.onEndElement("polyline");
    geometryNamespaces.onEndElement("shape");
    geometryNamespaces.onStartElement("line", {});
    geometryNamespaces.onStartElement("orientablecurve", {});
    geometryNamespaces.onStartElement(expanded("http://custom", "member"), {});
    geometryNamespaces.onEndElement("member");
    geometryNamespaces.onStartElement(expanded("http://www.interlis.ch/geometry/1.0", "arc"), {});
    geometryNamespaces.onEndElement("arc");
    geometryNamespaces.onEndElement("orientablecurve");
    geometryNamespaces.onEndElement("line");
    geometryNamespaces.onEndElement("Class");
    geometryNamespaces.onEndElement("basket");

    iox::xtf::Xtf24Dialect unknown({[](iox::IoxEvent) {}, [](iox::Diagnostic) {}});
    unknown.onEndElement("empty");
    unknown.onStartElement("unknown", {});
    unknown.onEndElement("unknown");
    unknown.onCharacterData("ignored");
    unknown.reset();
    unknown.onCharacterData("ignored");
}

#include "iox/test/TestMain.h"
