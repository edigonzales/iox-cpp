#include "iox/ByteView.h"
#include "iox/Events.h"
#include "iox/xtf/XtfReader.h"
#include "iox/test/Test.h"

#include <algorithm>
#include <cstdint>
#include <string>

namespace {

std::string makeLargeTransfer(std::size_t objectCount) {
    std::string xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<ili:TRANSFER xmlns:ili=\"http://www.interlis.ch/INTERLIS2.3\">"
        "<ili:HEADERSECTION SENDER=\"stream-test\" VERSION=\"2.3\">"
        "<ili:MODELS><ili:MODEL NAME=\"M\"/></ili:MODELS>"
        "</ili:HEADERSECTION><ili:DATASECTION>"
        "<Topic BID=\"B1\">";
    for (std::size_t i = 0; i < objectCount; ++i) {
        xml += "<Class TID=\"T" + std::to_string(i) + "\"><value>";
        xml += std::to_string(i);
        xml += "</value></Class>";
    }
    xml += "</Topic></ili:DATASECTION></ili:TRANSFER>";
    return xml;
}

} // namespace

IOX_TEST(large_xtf23_transfer_is_incremental_and_ordered) {
    constexpr std::size_t objectCount = 10000;
    const auto xml = makeLargeTransfer(objectCount);

    iox::xtf::XtfReader reader;
    constexpr std::size_t chunkSize = 4096;
    for (std::size_t offset = 0; offset < xml.size(); offset += chunkSize) {
        const auto size = std::min(chunkSize, xml.size() - offset);
        reader.feed(iox::ByteView(
            reinterpret_cast<const std::uint8_t*>(xml.data() + offset),
            size));
    }
    reader.finish();

    std::size_t objects = 0;
    std::size_t lastObject = 0;
    while (true) {
        const auto outcome = reader.next();
        if (outcome.progress == iox::ReaderProgress::End) break;
        IOX_CHECK(outcome.progress != iox::ReaderProgress::NeedInput);
        if (const auto* object = std::get_if<iox::ObjectEvent>(&*outcome.event)) {
            IOX_CHECK(object->object.oid().has_value());
            IOX_CHECK_EQ("T" + std::to_string(objects),
                         *object->object.oid());
            lastObject = objects;
            ++objects;
        }
    }

    IOX_CHECK_EQ(objectCount, objects);
    IOX_CHECK_EQ(objectCount - 1, lastObject);
}

#include "iox/test/TestMain.h"
