#include "iox/xml/ExpatParser.h"
#include "iox/test/Test.h"

#include <string>

static bool parse(const std::string& input, iox::xml::ExpatLimits limits = {}) {
    iox::xml::ExpatParser parser({}, limits);
    for (char c : input) {
        if (!parser.feed(iox::ByteView(&c, 1))) return false;
    }
    return parser.finish();
}

IOX_TEST(expat_rejects_dtd) {
    IOX_CHECK(!parse("<!DOCTYPE root [<!ENTITY x 'bad'>]><root>&x;</root>"));
}

IOX_TEST(expat_rejects_external_entity) {
    IOX_CHECK(!parse("<!DOCTYPE root SYSTEM 'file:///tmp/secret'><root/>"));
}

IOX_TEST(expat_enforces_depth_limit) {
    iox::xml::ExpatLimits limits;
    limits.maxElementDepth = 2;
    IOX_CHECK(!parse("<a><b><c/></b></a>", limits));
}

IOX_TEST(expat_handles_split_utf8_and_entities) {
    IOX_CHECK(parse("<root>Gr\xC3\xBC\xC3\x9F" "e &amp; emoji "
                    "\xF0\x9F\x98\x80</root>"));
}

#include "iox/test/TestMain.h"
