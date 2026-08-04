#include "xml/XmlWriter.h"
#include "iox/test/Test.h"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace {

iox::XmlQualifiedName name(std::string local,
                           std::string uri = {},
                           std::string prefix = {}) {
    return {std::move(uri), std::move(local), std::move(prefix)};
}

std::string simpleDocument(bool pretty = false) {
    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::xml::XmlWriterOptions options;
    options.pretty = pretty;
    iox::xml::XmlWriter writer(sink, options);
    writer.startDocument();
    writer.startElement(name("root"));
    writer.startElement(name("child"));
    writer.text("hello");
    writer.endElement();
    writer.endElement();
    writer.endDocument();
    return sink->str();
}

class ShortWriteSink final : public iox::OutputSink {
public:
    std::size_t write(const void* data, std::size_t size) override {
        const auto count = std::min<std::size_t>(2, size);
        output.append(static_cast<const char*>(data), count);
        return count;
    }
    std::string output;
};

class ZeroWriteSink final : public iox::OutputSink {
public:
    std::size_t write(const void*, std::size_t) override { return 0; }
};

class ThrowingSink final : public iox::OutputSink {
public:
    std::size_t write(const void*, std::size_t) override {
        throw std::runtime_error("sink marker");
    }
};

class UnknownThrowingSink final : public iox::OutputSink {
public:
    std::size_t write(const void*, std::size_t) override { throw 7; }
};

class OversizedWriteSink final : public iox::OutputSink {
public:
    std::size_t write(const void*, std::size_t size) override {
        return size + 1U;
    }
};

class ThrowingFlushSink final : public iox::OutputSink {
public:
    explicit ThrowingFlushSink(bool unknown) : unknown_(unknown) {}
    std::size_t write(const void*, std::size_t size) override { return size; }
    void flush() override {
        if (unknown_) throw 9;
        throw std::runtime_error("flush marker");
    }
private:
    bool unknown_;
};

} // namespace

IOX_TEST(xml_writer_declaration_nesting_and_escaping) {
    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::xml::XmlWriterOptions options;
    options.pretty = false;
    iox::xml::XmlWriter writer(sink, options);
    writer.startDocument();
    writer.startElement(name("root"));
    writer.writeAttribute(name("value"), "<&\"\t\n\r");
    writer.text("]]> <& \" '");
    writer.endElement();
    writer.endDocument();

    IOX_CHECK_EQ(
        std::string("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                    "<root value=\"&lt;&amp;&quot;&#x9;&#xA;&#xD;\">"
                    "]]&gt; &lt;&amp; \" '</root>"),
        sink->str());
}

IOX_TEST(xml_writer_empty_elements_and_pretty_output_are_deterministic) {
    auto make = [] {
        auto sink = std::make_shared<iox::StringOutputSink>();
        iox::xml::XmlWriterOptions options;
        options.pretty = true;
        iox::xml::XmlWriter writer(sink, options);
        writer.startDocument();
        writer.startElement(name("root"));
        writer.startElement(name("empty"));
        writer.endElement();
        writer.endElement();
        writer.endDocument();
        return sink->str();
    };
    const auto first = make();
    IOX_CHECK_EQ(first, make());
    IOX_CHECK(first.find("\n  <empty/>") != std::string::npos);
    IOX_CHECK(first.find("\n</root>\n") != std::string::npos);
}

IOX_TEST(xml_writer_tracks_namespace_scopes_and_assigns_prefixes) {
    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::xml::XmlWriterOptions options;
    options.pretty = false;
    iox::xml::XmlWriter writer(sink, options);
    writer.startDocument();
    writer.startElement(name("root", "urn:a", "m"));
    writer.writeNamespace("g", "urn:global");
    writer.writeAttribute(name("id", "urn:global", "g"), "1");
    writer.startElement(name("child", "urn:a", "m"));
    writer.endElement();
    writer.startElement(name("other", "urn:b", "m"));
    writer.endElement();
    writer.endElement();
    writer.endDocument();

    const auto& output = sink->str();
    IOX_CHECK(output.find("<m:root xmlns:m=\"urn:a\" xmlns:g=\"urn:global\"") !=
              std::string::npos);
    IOX_CHECK(output.find("<m:child/>") != std::string::npos);
    IOX_CHECK(output.find("<ns0:other xmlns:ns0=\"urn:b\"/>") !=
              std::string::npos);
}

IOX_TEST(xml_writer_rejects_duplicate_attributes_and_invalid_content) {
    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::xml::XmlWriterOptions options;
    options.pretty = false;
    iox::xml::XmlWriter writer(sink, options);
    writer.startDocument();
    writer.startElement(name("root"));
    writer.writeAttribute(name("a", "urn:x", "x"), "1");
    bool duplicate = false;
    try {
        writer.writeAttribute(name("a", "urn:x", "other"), "2");
    } catch (const iox::IoxError& error) {
        duplicate = error.code() == iox::DiagnosticCode::UnexpectedAttribute;
    }
    IOX_CHECK(duplicate);

    auto invalidSink = std::make_shared<iox::StringOutputSink>();
    iox::xml::XmlWriter invalid(invalidSink, options);
    invalid.startDocument();
    invalid.startElement(name("root"));
    bool character = false;
    try {
        invalid.text(std::string("bad\x01", 4));
    } catch (const iox::IoxError& error) {
        character = error.code() == iox::DiagnosticCode::InvalidArgument;
    }
    IOX_CHECK(character);

    auto nameSink = std::make_shared<iox::StringOutputSink>();
    iox::xml::XmlWriter invalidName(nameSink, options);
    invalidName.startDocument();
    bool rejectedName = false;
    try {
        invalidName.startElement(name("1bad"));
    } catch (const iox::IoxError& error) {
        rejectedName = error.code() == iox::DiagnosticCode::InvalidArgument;
    }
    IOX_CHECK(rejectedName);

    auto reservedSink = std::make_shared<iox::StringOutputSink>();
    iox::xml::XmlWriter reserved(reservedSink, options);
    reserved.startDocument();
    reserved.startElement(name("root"));
    bool rejectedXmlnsName = false;
    try {
        reserved.writeAttribute(
            name("forbidden", "http://www.w3.org/2000/xmlns/", "xmlns"),
            "value");
    } catch (const iox::IoxError& error) {
        rejectedXmlnsName =
            error.code() == iox::DiagnosticCode::InvalidArgument;
    }
    IOX_CHECK(rejectedXmlnsName);
}

IOX_TEST(xml_writer_completes_short_writes_and_reports_sink_failures) {
    auto shortSink = std::make_shared<ShortWriteSink>();
    iox::xml::XmlWriterOptions options;
    options.pretty = false;
    iox::xml::XmlWriter writer(shortSink, options);
    writer.startDocument();
    writer.startElement(name("root"));
    writer.endElement();
    writer.endDocument();
    IOX_CHECK_EQ(std::string("<?xml version=\"1.0\" encoding=\"UTF-8\"?><root/>"),
                 shortSink->output);

    for (const auto& failing : {
             std::shared_ptr<iox::OutputSink>(std::make_shared<ZeroWriteSink>()),
             std::shared_ptr<iox::OutputSink>(std::make_shared<ThrowingSink>()),
             std::shared_ptr<iox::OutputSink>(std::make_shared<UnknownThrowingSink>()),
             std::shared_ptr<iox::OutputSink>(std::make_shared<OversizedWriteSink>())}) {
        bool ioError = false;
        try {
            iox::xml::XmlWriter failed(failing, options);
            failed.startDocument();
        } catch (const iox::IoxError& error) {
            ioError = error.code() == iox::DiagnosticCode::IoError;
        }
        IOX_CHECK(ioError);
    }
}

IOX_TEST(xml_writer_covers_unicode_names_utf8_and_namespace_edges) {
    const std::vector<std::string> validNames{
        "_", "A", "z", "\xC3\x80", "\xC3\x98", "\xC3\xB8",
        "\xCD\xB0", "\xE2\x80\x8C", "\xE2\x81\xB0",
        "\xE2\xB0\x80", "\xE3\x80\x81", "\xEF\xA4\x80",
        "\xEF\xB7\xB0", "\xF0\x90\x80\x80", "n0-._\xC2\xB7\xCC\x80\xE2\x80\xBF"};
    for (const auto& local : validNames) {
        auto sink = std::make_shared<iox::StringOutputSink>();
        iox::xml::XmlWriter writer(sink, {});
        writer.startDocument();
        writer.startElement(name(local));
        writer.text("\xC2\xA0\xE2\x82\xAC\xF0\x9F\x98\x80");
        writer.endElement();
        writer.endDocument();
    }

    const std::vector<std::string> invalidUtf8{
        std::string("\x80", 1), std::string("\xE2\x82", 2),
        std::string("\xE2x\x82", 3), std::string("\xE0\x80\x80", 3),
        std::string("\xF0\x80\x80\x80", 4),
        std::string("\xED\xA0\x80", 3),
        std::string("\xF4\x90\x80\x80", 4)};
    for (const auto& value : invalidUtf8) {
        bool rejected = false;
        try {
            auto sink = std::make_shared<iox::StringOutputSink>();
            iox::xml::XmlWriter writer(sink, {});
            writer.startDocument();
            writer.startElement(name("r"));
            writer.text(value);
        } catch (const iox::IoxError& error) {
            rejected = error.code() == iox::DiagnosticCode::InvalidArgument;
        }
        IOX_CHECK(rejected);
    }

    const std::vector<iox::XmlQualifiedName> invalidNames{
        name(""), name("a:b"), name("a", {}, "p"),
        name("a", "http://www.w3.org/2000/xmlns/", "x")};
    for (const auto& invalid : invalidNames) {
        bool rejected = false;
        try {
            auto sink = std::make_shared<iox::StringOutputSink>();
            iox::xml::XmlWriter writer(sink, {});
            writer.startDocument();
            writer.startElement(invalid);
        } catch (const iox::IoxError& error) {
            rejected = error.code() == iox::DiagnosticCode::InvalidArgument;
        }
        IOX_CHECK(rejected);
    }

    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::xml::XmlWriter namespaces(sink, {});
    namespaces.startDocument();
    namespaces.startElement(name("root", "urn:default"));
    namespaces.writeNamespace("again", "urn:default");
    namespaces.writeNamespace("again", "urn:default");
    namespaces.writeAttribute(name("lang", "http://www.w3.org/XML/1998/namespace"), "de");
    namespaces.startElement(name("child", "urn:default"));
    namespaces.writeNamespace("again", "urn:default");
    namespaces.writeAttribute(name("value", "urn:default"), "x");
    namespaces.endElement();
    namespaces.endElement();
    namespaces.endDocument();

    const std::vector<std::pair<std::string, std::string>> invalidDeclarations{
        {"xmlns", "urn:x"}, {"xml", "urn:x"}, {"p", ""}};
    for (const auto& declaration : invalidDeclarations) {
        bool rejected = false;
        try {
            auto declarationSink = std::make_shared<iox::StringOutputSink>();
            iox::xml::XmlWriter writer(declarationSink, {});
            writer.startDocument();
            writer.startElement(name("r"));
            writer.writeNamespace(declaration.first, declaration.second);
        } catch (const iox::IoxError& error) {
            rejected = error.code() == iox::DiagnosticCode::InvalidArgument;
        }
        IOX_CHECK(rejected);
    }
}

IOX_TEST(xml_writer_covers_remaining_state_and_flush_edges) {
    bool nullSink = false;
    try {
        iox::xml::XmlWriter writer(nullptr, {});
    } catch (const iox::IoxError& error) {
        nullSink = error.code() == iox::DiagnosticCode::InvalidArgument;
    }
    IOX_CHECK(nullSink);

    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::xml::XmlWriter writer(sink, {});
    for (const auto action : {0, 1, 2, 3, 4}) {
        bool failed = false;
        try {
            if (action == 0) writer.startElement(name("r"));
            if (action == 1) writer.writeNamespace("p", "urn:p");
            if (action == 2) writer.writeAttribute(name("a"), "v");
            if (action == 3) writer.text("v");
            if (action == 4) writer.endElement();
        } catch (const iox::IoxError& error) {
            failed = error.code() == iox::DiagnosticCode::WriterStateError;
        }
        IOX_CHECK(failed);
        break; // the first state failure is terminal; other paths use fresh writers below
    }

    for (const auto action : {1, 2, 3, 4}) {
        auto stateSink = std::make_shared<iox::StringOutputSink>();
        iox::xml::XmlWriter stateWriter(stateSink, {});
        stateWriter.startDocument();
        bool failed = false;
        try {
            if (action == 1) stateWriter.writeNamespace("p", "urn:p");
            if (action == 2) stateWriter.writeAttribute(name("a"), "v");
            if (action == 3) stateWriter.text("v");
            if (action == 4) stateWriter.endElement();
        } catch (const iox::IoxError& error) {
            failed = error.code() == iox::DiagnosticCode::WriterStateError;
        }
        IOX_CHECK(failed);
    }

    auto onceSink = std::make_shared<iox::StringOutputSink>();
    iox::xml::XmlWriter once(onceSink, {});
    once.startDocument();
    bool twice = false;
    try { once.startDocument(); }
    catch (const iox::IoxError& error) {
        twice = error.code() == iox::DiagnosticCode::WriterStateError;
    }
    IOX_CHECK(twice);

    for (const auto unknown : {false, true}) {
        auto flushSink = std::make_shared<ThrowingFlushSink>(unknown);
        iox::xml::XmlWriter flushing(flushSink, {});
        flushing.startDocument();
        flushing.startElement(name("r"));
        bool failed = false;
        try { flushing.flush(); }
        catch (const iox::IoxError& error) {
            failed = error.code() == iox::DiagnosticCode::IoError;
        }
        IOX_CHECK(failed);
    }
}

IOX_TEST(xml_writer_validates_state_and_never_invents_end_elements) {
    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::xml::XmlWriterOptions options;
    options.pretty = false;
    iox::xml::XmlWriter writer(sink, options);
    writer.startDocument();
    writer.startElement(name("root"));
    bool openElement = false;
    try {
        writer.endDocument();
    } catch (const iox::IoxError& error) {
        openElement = error.code() == iox::DiagnosticCode::WriterStateError;
    }
    IOX_CHECK(openElement);
    IOX_CHECK(sink->str().find("</root>") == std::string::npos);

    auto emptySink = std::make_shared<iox::StringOutputSink>();
    iox::xml::XmlWriter empty(emptySink, options);
    empty.startDocument();
    bool missingRoot = false;
    try {
        empty.endDocument();
    } catch (const iox::IoxError& error) {
        missingRoot = error.code() == iox::DiagnosticCode::WriterStateError;
    }
    IOX_CHECK(missingRoot);

    auto rootsSink = std::make_shared<iox::StringOutputSink>();
    iox::xml::XmlWriter roots(rootsSink, options);
    roots.startDocument();
    roots.startElement(name("one"));
    roots.endElement();
    bool secondRoot = false;
    try {
        roots.startElement(name("two"));
    } catch (const iox::IoxError& error) {
        secondRoot = error.code() == iox::DiagnosticCode::WriterStateError;
    }
    IOX_CHECK(secondRoot);
}

#include "iox/test/TestMain.h"
