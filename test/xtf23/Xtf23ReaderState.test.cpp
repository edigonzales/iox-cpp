#include "iox/xtf/XtfReader.h"
#include "iox/test/Test.h"

#include <string>

IOX_TEST(xtf23_reader_rejects_input_after_finish) {
    const std::string xml =
        "<?xml version=\"1.0\"?><TRANSFER xmlns=\"http://www.interlis.ch/INTERLIS2.3\">"
        "<HEADERSECTION SENDER=\"t\" VERSION=\"2.3\"/></TRANSFER>";
    iox::xtf::XtfReader reader;
    reader.feed(iox::ByteView(xml));
    reader.finish();
    reader.feed(iox::ByteView(xml));
    auto diagnostics = reader.takeDiagnostics();
    bool found = false;
    for (const auto& diagnostic : diagnostics) {
        if (diagnostic.code == iox::ErrorCode::InvalidState) found = true;
    }
    IOX_CHECK(found);
}

IOX_TEST(xtf23_reader_rejects_double_finish) {
    const std::string xml =
        "<?xml version=\"1.0\"?><TRANSFER xmlns=\"http://www.interlis.ch/INTERLIS2.3\">"
        "<HEADERSECTION SENDER=\"t\" VERSION=\"2.3\"/></TRANSFER>";
    iox::xtf::XtfReader reader;
    reader.feed(iox::ByteView(xml));
    reader.finish();
    reader.finish();
    auto diagnostics = reader.takeDiagnostics();
    bool found = false;
    for (const auto& diagnostic : diagnostics) {
        if (diagnostic.code == iox::ErrorCode::InvalidState) found = true;
    }
    IOX_CHECK(found);
}

#include "iox/test/TestMain.h"
