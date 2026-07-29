#include "iox/xml/XmlWriter.h"
#include "iox/test/Test.h"

#include <string>
#include <vector>
#include <utility>

IOX_TEST(xml_writer_declaration) {
    std::string output;
    auto writer = [&](const void* data, std::size_t size) {
        output.append(static_cast<const char*>(data), size);
    };

    iox::xml::XmlWriter xml(writer, false, 0);
    xml.writeDeclaration();

    IOX_CHECK_EQ(std::string("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"), output);
}

IOX_TEST(xml_writer_simple_element) {
    std::string output;
    auto writer = [&](const void* data, std::size_t size) {
        output.append(static_cast<const char*>(data), size);
    };

    iox::xml::XmlWriter xml(writer, false, 0);
    xml.writeStartElement("root");
    xml.writeEndElement("root");

    IOX_CHECK_EQ(std::string("<root/>"), output);
}

IOX_TEST(xml_writer_nested_elements) {
    std::string output;
    auto writer = [&](const void* data, std::size_t size) {
        output.append(static_cast<const char*>(data), size);
    };

    iox::xml::XmlWriter xml(writer, false, 0);
    xml.writeStartElement("a");
    xml.writeStartElement("b");
    xml.writeText("hello");
    xml.writeEndElement("b");
    xml.writeEndElement("a");

    IOX_CHECK_EQ(std::string("<a><b>hello</b></a>"), output);
}

IOX_TEST(xml_writer_escaping) {
    std::string output;
    auto writer = [&](const void* data, std::size_t size) {
        output.append(static_cast<const char*>(data), size);
    };

    iox::xml::XmlWriter xml(writer, false, 0);
    xml.writeStartElement("x");
    xml.writeText("<>&\"'");
    xml.writeEndElement("x");

    IOX_CHECK_EQ(std::string("<x>&lt;&gt;&amp;&quot;&apos;</x>"), output);
}

IOX_TEST(xml_writer_attributes) {
    std::string output;
    auto writer = [&](const void* data, std::size_t size) {
        output.append(static_cast<const char*>(data), size);
    };

    iox::xml::XmlWriter xml(writer, false, 0);
    std::vector<std::pair<std::string, std::string>> attrs = {
        {"id", "123"},
        {"name", "test"}
    };
    xml.writeStartElement("elem", attrs);
    xml.writeEndElement("elem");

    IOX_CHECK_EQ(std::string("<elem id=\"123\" name=\"test\"/>"), output);
}

IOX_TEST(xml_writer_self_closing) {
    std::string output;
    auto writer = [&](const void* data, std::size_t size) {
        output.append(static_cast<const char*>(data), size);
    };

    iox::xml::XmlWriter xml(writer, false, 0);
    xml.writeStartElement("br", {}, true);

    IOX_CHECK_EQ(std::string("<br/>"), output);
}

IOX_TEST(xml_writer_comment) {
    std::string output;
    auto writer = [&](const void* data, std::size_t size) {
        output.append(static_cast<const char*>(data), size);
    };

    iox::xml::XmlWriter xml(writer, false, 0);
    xml.writeComment("test comment");
    xml.writeStartElement("x", {}, true);

    IOX_CHECK(output.find("<!-- test comment -->") != std::string::npos);
    IOX_CHECK(output.find("<x/>") != std::string::npos);
}

IOX_TEST(xml_writer_pretty_print) {
    std::string output;
    auto writer = [&](const void* data, std::size_t size) {
        output.append(static_cast<const char*>(data), size);
    };

    iox::xml::XmlWriter xml(writer, true, 2);
    xml.writeStartElement("root");
    xml.writeStartElement("child", {}, true);
    xml.writeEndElement("root");

    // Should contain newlines and indentation
    IOX_CHECK(output.find('\n') != std::string::npos);
    IOX_CHECK(output.find("<root>") != std::string::npos);
}

IOX_TEST(xml_writer_deterministic) {
    auto makeOutput = []() -> std::string {
        std::string out;
        auto writer = [&](const void* data, std::size_t size) {
            out.append(static_cast<const char*>(data), size);
        };
        iox::xml::XmlWriter xml(writer, false, 0);
        xml.writeDeclaration();
        xml.writeStartElement("a");
        xml.writeStartElement("b");
        xml.writeText("c");
        xml.writeEndElement("b");
        xml.writeEndElement("a");
        return out;
    };

    auto out1 = makeOutput();
    auto out2 = makeOutput();
    IOX_CHECK_EQ(out1, out2);
}

#include "iox/test/TestMain.h"
