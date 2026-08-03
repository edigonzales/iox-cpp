#include "iox/xtf/XtfReader.h"
#include "iox/Events.h"
#include "iox/test/Test.h"

#include <string>

IOX_TEST(xtf24_reader_preserves_expanded_qnames) {
    const std::string xml =
        "<?xml version=\"1.0\"?>"
        "<ili:transfer xmlns:ili=\"http://www.interlis.ch/xtf/2.4/INTERLIS\" "
        "xmlns:m=\"urn:example:model\">"
        "<ili:headersection><ili:models><ili:model>M</ili:model>"
        "</ili:models></ili:headersection>"
        "<ili:datasection><m:Basket ili:bid=\"b\"><m:Class ili:tid=\"t\">"
        "<m:Name>\xC3\x84</m:Name></m:Class></m:Basket></ili:datasection>"
        "</ili:transfer>";
    iox::xtf::XtfReader reader;
    reader.feed(iox::ByteView(xml));
    reader.finish();
    iox::IomObject object;
    iox::TransferHeader header;
    while (true) {
        auto outcome = reader.next();
        if (outcome.progress == iox::ReaderProgress::End) break;
        if (outcome.event) {
            if (const auto* event =
                    std::get_if<iox::StartTransferEvent>(&*outcome.event)) {
                header = event->header;
            }
            if (const auto* event = std::get_if<iox::ObjectEvent>(&*outcome.event)) {
                object = event->object;
                break;
            }
        }
    }
    IOX_CHECK(object.tag().hasXmlName());
    IOX_CHECK_EQ(static_cast<std::size_t>(1), header.models.size());
    IOX_CHECK_EQ(std::string("urn:example:model"),
                 header.models[0].xmlNamespace.namespaceUri);
    IOX_CHECK_EQ(std::string("urn:example:model"),
                 object.tag().xmlName().namespaceUri);
    IOX_CHECK_EQ(std::string("Class"), object.tag().xmlName().localName);
    IOX_CHECK(object.hasAttribute("Name"));
    IOX_CHECK(object.attributeName(0).hasXmlName());
}

#include "iox/test/TestMain.h"
