#include "xml/ExpatParser.h"
#include "iox/test/Test.h"

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::optional<iox::IoxError> parseFailure(
    const std::string& input,
    iox::xml::XmlLimits limits = {},
    std::size_t chunkSize = 1,
    std::string sourceName = "security.xtf") {
    try {
        iox::xml::ExpatParser parser(limits, std::move(sourceName));
        for (std::size_t offset = 0; offset < input.size(); offset += chunkSize) {
            const auto size = std::min(chunkSize, input.size() - offset);
            parser.feed(iox::ByteView(
                reinterpret_cast<const std::uint8_t*>(input.data() + offset),
                size));
        }
        parser.finish();
    } catch (const iox::IoxError& error) {
        return error;
    }
    return std::nullopt;
}

} // namespace

IOX_TEST(expat_rejects_dtd_and_external_entities) {
    const auto internal = parseFailure(
        "<!DOCTYPE root [<!ENTITY x 'bad'>]><root>&x;</root>");
    IOX_CHECK(internal.has_value());
    IOX_CHECK(internal->code() == iox::DiagnosticCode::XmlDtdForbidden);
    IOX_CHECK_EQ(std::string("security.xtf"), internal->location().sourceName);

    const auto external = parseFailure(
        "<!DOCTYPE root SYSTEM 'file:///tmp/secret'><root/>");
    IOX_CHECK(external.has_value());
    IOX_CHECK(external->code() == iox::DiagnosticCode::XmlDtdForbidden);

    const auto parameter = parseFailure(
        "<!DOCTYPE root [<!ENTITY % x SYSTEM 'https://example.test/x'>%x;]><root/>");
    IOX_CHECK(parameter.has_value());
    IOX_CHECK(parameter->code() == iox::DiagnosticCode::XmlDtdForbidden);
}

IOX_TEST(expat_enforces_all_configured_limits) {
    iox::xml::XmlLimits limits;
    limits.maxDepth = 2;
    auto failure = parseFailure("<a><b><c/></b></a>", limits);
    IOX_CHECK(failure.has_value());
    IOX_CHECK(failure->code() == iox::DiagnosticCode::XmlLimitExceeded);

    limits = {};
    limits.maxAttributesPerElement = 1;
    failure = parseFailure("<a x='1' y='2'/>", limits);
    IOX_CHECK(failure.has_value());
    IOX_CHECK(failure->code() == iox::DiagnosticCode::XmlLimitExceeded);

    limits = {};
    limits.maxTextBytesPerNode = 4;
    failure = parseFailure("<a>12345</a>", limits);
    IOX_CHECK(failure.has_value());
    IOX_CHECK(failure->code() == iox::DiagnosticCode::XmlLimitExceeded);

    limits = {};
    limits.maxTotalInputBytes = 6;
    failure = parseFailure("<root/>", limits);
    IOX_CHECK(failure.has_value());
    IOX_CHECK(failure->code() == iox::DiagnosticCode::XmlLimitExceeded);
}

IOX_TEST(expat_rejects_disabled_safety_limits) {
    iox::xml::XmlLimits limits;
    limits.maxDepth = 0;
    bool rejected = false;
    try {
        iox::xml::ExpatParser parser(limits);
    } catch (const iox::IoxError& error) {
        rejected = error.code() == iox::DiagnosticCode::InvalidArgument;
    }
    IOX_CHECK(rejected);
}

IOX_TEST(expat_handles_utf8_entities_namespaces_and_locations_across_splits) {
    const std::string input =
        "<p:root xmlns:p='urn:test'>\n  <p:child a='1'>Gr\xC3\xBC\xC3\x9F"
        "e &amp; emoji \xF0\x9F\x98\x80</p:child></p:root>";
    std::vector<iox::xml::XmlStartElement> starts;
    std::string text;
    iox::xml::ExpatParser parser({}, "unicode.xtf");
    parser.setStartHandler([&](const auto& element) { starts.push_back(element); });
    parser.setTextHandler([&](std::string_view value, const auto&) {
        text.append(value);
    });
    for (char byte : input) {
        parser.feed(iox::ByteView(
            reinterpret_cast<const std::uint8_t*>(&byte), 1));
    }
    parser.finish();

    IOX_CHECK(parser.finished());
    IOX_CHECK_EQ(std::size_t(2), starts.size());
    IOX_CHECK_EQ(std::string("urn:test"), starts[1].name.namespaceUri);
    IOX_CHECK_EQ(std::string("child"), starts[1].name.localName);
    IOX_CHECK_EQ(std::string("p"), starts[1].name.prefixHint);
    IOX_CHECK_EQ(std::uint32_t(2), starts[1].location.line);
    IOX_CHECK_EQ(std::uint32_t(3), starts[1].location.column);
    IOX_CHECK_EQ(std::string("unicode.xtf"), starts[1].location.sourceName);
    IOX_CHECK(text.find("Gr\xC3\xBC\xC3\x9F" "e & emoji") != std::string::npos);
}

IOX_TEST(expat_rejects_invalid_utf8_encoding_and_truncation) {
    auto failure = parseFailure(std::string("<a>\xC0\xAF</a>", 9));
    IOX_CHECK(failure.has_value());
    IOX_CHECK(failure->code() == iox::DiagnosticCode::XmlMalformed);

    failure = parseFailure("<?xml version='1.0' encoding='ISO-8859-1'?><a/>");
    IOX_CHECK(failure.has_value());
    IOX_CHECK(failure->code() == iox::DiagnosticCode::XmlMalformed);

    failure = parseFailure("<root><child>");
    IOX_CHECK(failure.has_value());
    IOX_CHECK(failure->code() == iox::DiagnosticCode::XmlMalformed);
}

IOX_TEST(expat_callback_exceptions_are_rethrown_outside_the_c_frame) {
    iox::xml::ExpatParser parser;
    parser.setStartHandler([](const auto&) {
        throw std::runtime_error("callback marker");
    });
    bool wrapped = false;
    try {
        parser.feed(iox::ByteView(std::string("<root/>")));
    } catch (const iox::IoxError& error) {
        wrapped = error.code() == iox::DiagnosticCode::InternalError &&
                  std::string(error.what()).find("callback marker") !=
                      std::string::npos;
    }
    IOX_CHECK(wrapped);

    bool terminal = false;
    try {
        parser.feed(iox::ByteView(std::string("<again/>")));
    } catch (const iox::IoxError& error) {
        terminal = error.code() == iox::DiagnosticCode::InvalidState;
    }
    IOX_CHECK(terminal);
}

IOX_TEST(expat_finish_and_feed_are_one_shot) {
    iox::xml::ExpatParser parser;
    parser.feed(iox::ByteView(std::string("<root/>")));
    parser.finish();

    bool doubleFinish = false;
    try {
        parser.finish();
    } catch (const iox::IoxError& error) {
        doubleFinish = error.code() == iox::DiagnosticCode::InvalidState;
    }
    IOX_CHECK(doubleFinish);

    bool feedAfterFinish = false;
    try {
        parser.feed(iox::ByteView(std::string("x")));
    } catch (const iox::IoxError& error) {
        feedAfterFinish = error.code() == iox::DiagnosticCode::InvalidState;
    }
    IOX_CHECK(feedAfterFinish);
}

#include "iox/test/TestMain.h"
