#include "iox/xtf/XtfReader.h"
#include "iox/test/Test.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace {

std::string transferWithObjects(std::size_t count) {
    std::string result =
        "<?xml version=\"1.0\"?><TRANSFER "
        "xmlns=\"http://www.interlis.ch/INTERLIS2.3\">"
        "<HEADERSECTION SENDER=\"state\" VERSION=\"2.3\">"
        "<MODELS><MODEL NAME=\"M\"/></MODELS></HEADERSECTION>"
        "<DATASECTION><M.T BID=\"B1\">";
    for (std::size_t index = 0; index < count; ++index) {
        result += "<M.T.C TID=\"T" + std::to_string(index) +
                  "\"><value>" + std::to_string(index) +
                  "</value></M.T.C>";
    }
    result += "</M.T></DATASECTION></TRANSFER>";
    return result;
}

} // namespace

IOX_TEST(xtf23_reader_rejects_input_after_finish) {
    const std::string xml =
        "<?xml version=\"1.0\"?><TRANSFER xmlns=\"http://www.interlis.ch/INTERLIS2.3\">"
        "<HEADERSECTION SENDER=\"t\" VERSION=\"2.3\"/>"
        "<DATASECTION/></TRANSFER>";
    iox::xtf::XtfReaderOptions options;
    options.requireAtLeastOneModel = false;
    iox::xtf::XtfReader reader(options);
    reader.feed(iox::ByteView(xml));
    reader.finish();
    bool found = false;
    try {
        reader.feed(iox::ByteView(xml));
    } catch (const iox::IoxError& error) {
        found = error.code() == iox::DiagnosticCode::InvalidState;
    }
    IOX_CHECK(found);
}

IOX_TEST(xtf23_reader_rejects_double_finish) {
    const std::string xml =
        "<?xml version=\"1.0\"?><TRANSFER xmlns=\"http://www.interlis.ch/INTERLIS2.3\">"
        "<HEADERSECTION SENDER=\"t\" VERSION=\"2.3\"/>"
        "<DATASECTION/></TRANSFER>";
    iox::xtf::XtfReaderOptions options;
    options.requireAtLeastOneModel = false;
    iox::xtf::XtfReader reader(options);
    reader.feed(iox::ByteView(xml));
    reader.finish();
    bool found = false;
    try {
        reader.finish();
    } catch (const iox::IoxError& error) {
        found = error.code() == iox::DiagnosticCode::InvalidState;
    }
    IOX_CHECK(found);
}

IOX_TEST(xtf23_reader_queue_backpressure_resumes_buffered_input) {
    const auto xml = transferWithObjects(25);
    iox::xtf::XtfReaderOptions options;
    options.xmlLimits.maxQueuedEvents = 1;
    options.sourceName = "queued.xtf";
    iox::xtf::XtfReader reader(options);

    IOX_CHECK_EQ(iox::ReaderProgress::NeedInput, reader.next().progress);
    for (std::size_t offset = 0; offset < xml.size(); offset += 17U) {
        const auto count = std::min<std::size_t>(17U, xml.size() - offset);
        reader.feed(iox::ByteView(
            reinterpret_cast<const std::uint8_t*>(xml.data() + offset),
            count));
    }
    reader.finish();

    std::size_t objectCount = 0;
    std::vector<iox::EventKind> kinds;
    while (true) {
        auto outcome = reader.next();
        if (outcome.progress == iox::ReaderProgress::End) break;
        IOX_CHECK_EQ(iox::ReaderProgress::Event, outcome.progress);
        kinds.push_back(iox::eventKind(*outcome.event));
        if (const auto* object = std::get_if<iox::ObjectEvent>(&*outcome.event)) {
            IOX_CHECK_EQ(std::string("queued.xtf"),
                         object->object.sourceLocation().sourceName);
            ++objectCount;
        }
    }
    IOX_CHECK_EQ(static_cast<std::size_t>(29), kinds.size());
    IOX_CHECK_EQ(static_cast<std::size_t>(25), objectCount);
    IOX_CHECK_EQ(iox::EventKind::StartTransfer, kinds.front());
    IOX_CHECK_EQ(iox::EventKind::EndTransfer, kinds.back());
    IOX_CHECK(reader.isFinished());
    IOX_CHECK_EQ(iox::ReaderProgress::End, reader.next().progress);
}

IOX_TEST(xtf23_reader_rejects_zero_queue_limit) {
    bool rejected = false;
    try {
        iox::xtf::XtfReaderOptions options;
        options.xmlLimits.maxQueuedEvents = 0;
        iox::xtf::XtfReader reader(options);
    } catch (const iox::IoxError& error) {
        rejected = error.code() == iox::DiagnosticCode::InvalidArgument;
    }
    IOX_CHECK(rejected);
}

IOX_TEST(xtf23_reader_applies_every_xml_limit) {
    const auto xml = transferWithObjects(1);
    const auto rejectedBy = [&](iox::xtf::XmlLimits limits,
                                const std::string& input) {
        try {
            iox::xtf::XtfReaderOptions options;
            options.xmlLimits = limits;
            iox::xtf::XtfReader reader(options);
            reader.feed(iox::ByteView(input));
            reader.finish();
        } catch (const iox::IoxError& error) {
            return error.code() == iox::DiagnosticCode::XmlLimitExceeded;
        }
        return false;
    };

    iox::xtf::XmlLimits depth;
    depth.maxDepth = 3;
    IOX_CHECK(rejectedBy(depth, xml));

    iox::xtf::XmlLimits attributes;
    attributes.maxAttributesPerElement = 1;
    IOX_CHECK(rejectedBy(attributes, xml));

    iox::xtf::XmlLimits text;
    text.maxTextBytesPerNode = 1;
    IOX_CHECK(rejectedBy(text, transferWithObjects(11)));

    iox::xtf::XmlLimits total;
    total.maxTotalInputBytes = xml.size() - 1U;
    IOX_CHECK(rejectedBy(total, xml));
}

#include "iox/test/TestMain.h"
