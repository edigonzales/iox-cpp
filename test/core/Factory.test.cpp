#include "iox/Factory.h"
#include "iox/json/JsonEventReader.h"
#include "iox/xtf/XtfReader.h"
#include "iox/xtf/XtfWriter.h"
#include "iox/test/Test.h"

#include <memory>
#include <string>

IOX_TEST(factory_default_registry_has_builtins) {
    auto& registry = iox::defaultFormatRegistry();
    IOX_CHECK(registry.findByName("xtf") != nullptr);
    IOX_CHECK(registry.findByName("json-events") != nullptr);
}

IOX_TEST(factory_content_sniff_outranks_wrong_extension) {
    const std::string json = "{\"event\":\"endTransfer\"}\n";
    auto reader = iox::ReaderFactory::create(
        "input.xtf", iox::ByteView(json));
    IOX_CHECK(dynamic_cast<iox::json::JsonEventReader*>(reader.get()) != nullptr);
}

IOX_TEST(factory_xtf_extensions_select_xtf_reader) {
    const std::string prefix =
        "<?xml version=\"1.0\"?><ili:TRANSFER xmlns:ili=\"http://www.interlis.ch/INTERLIS2.3\">";
    auto xtf = iox::ReaderFactory::create("transfer.xtf", iox::ByteView(prefix));
    auto xml = iox::ReaderFactory::create("transfer.xml", iox::ByteView(prefix));
    IOX_CHECK(dynamic_cast<iox::xtf::XtfReader*>(xtf.get()) != nullptr);
    IOX_CHECK(dynamic_cast<iox::xtf::XtfReader*>(xml.get()) != nullptr);
}

IOX_TEST(factory_explicit_names_and_unknown_format) {
    auto reader = iox::ReaderFactory::createByName("xtf");
    IOX_CHECK(dynamic_cast<iox::xtf::XtfReader*>(reader.get()) != nullptr);
    IOX_CHECK(iox::ReaderFactory::createByName("does-not-exist") == nullptr);

    auto sink = std::make_shared<iox::StringOutputSink>();
    auto writer = iox::WriterFactory::create("xtf", sink);
    IOX_CHECK(dynamic_cast<iox::xtf::XtfWriter*>(writer.get()) != nullptr);
}

#include "iox/test/TestMain.h"
