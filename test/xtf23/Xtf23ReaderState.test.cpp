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
        "<HEADERSECTION SENDER=\"t\" VERSION=\"2.3\"/></TRANSFER>";
    iox::xtf::XtfReader reader;
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

#include "iox/test/TestMain.h"
