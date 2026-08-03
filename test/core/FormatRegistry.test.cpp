#include "iox/FormatRegistry.h"
#include "iox/test/Test.h"

namespace {

std::unique_ptr<iox::Reader> makeNullReader() { return nullptr; }
std::unique_ptr<iox::Writer> makeNullWriter(std::shared_ptr<iox::OutputSink>) { return nullptr; }

} // namespace

IOX_TEST(format_registry_add_and_find) {
    iox::FormatRegistry registry;

    iox::FormatEntry entry;
    entry.name = "test-format";
    entry.description = "A test format";
    entry.readerFactory = makeNullReader;
    entry.writerFactory = makeNullWriter;

    registry.addFormat(std::move(entry));

    auto* found = registry.findByName("test-format");
    IOX_CHECK(found != nullptr);
    IOX_CHECK_EQ(std::string("test-format"), found->name);

    auto* missing = registry.findByName("nonexistent");
    IOX_CHECK(missing == nullptr);
}

IOX_TEST(format_registry_names) {
    iox::FormatRegistry registry;

    iox::FormatEntry a;
    a.name = "a";
    registry.addFormat(std::move(a));

    iox::FormatEntry b;
    b.name = "b";
    registry.addFormat(std::move(b));

    auto names = registry.formatNames();
    IOX_CHECK_EQ(static_cast<std::size_t>(2), names.size());
    IOX_CHECK_EQ(std::string("a"), names[0]);
    IOX_CHECK_EQ(std::string("b"), names[1]);
}

IOX_TEST(format_registry_remove) {
    iox::FormatRegistry registry;

    iox::FormatEntry entry;
    entry.name = "removable";
    registry.addFormat(std::move(entry));

    IOX_CHECK(registry.removeFormat("removable"));
    IOX_CHECK(registry.findByName("removable") == nullptr);
    IOX_CHECK(!registry.removeFormat("nonexistent"));
}

IOX_TEST(format_registry_replace) {
    iox::FormatRegistry registry;

    iox::FormatEntry v1;
    v1.name = "fmt";
    v1.description = "first";
    registry.addFormat(std::move(v1));

    iox::FormatEntry v2;
    v2.name = "fmt";
    v2.description = "second";
    registry.addFormat(std::move(v2));

    auto* found = registry.findByName("fmt");
    IOX_CHECK(found != nullptr);
    IOX_CHECK_EQ(std::string("second"), found->description);
}

IOX_TEST(format_registry_sniffing) {
    iox::FormatRegistry registry;

    iox::FormatEntry jsonFmt;
    jsonFmt.name = "json-events";
    jsonFmt.readerFactory = makeNullReader;
    jsonFmt.sniffer = [](iox::ByteView chunk) -> std::string {
        if (!chunk.empty() && chunk.data()[0] ==
                                  static_cast<std::uint8_t>('{')) {
            return "json-events";
        }
        return "";
    };
    registry.addFormat(std::move(jsonFmt));

    const std::string event =
        "{\"schema\":\"iox-event/2\",\"event\":\"startTransfer\"}\n";
    auto reader = registry.createReaderBySniffing(iox::ByteView(event));
    // Reader factory returns nullptr in our test setup
    (void)reader;

    const std::string xml = "<xml>";
    auto reader2 = registry.createReaderBySniffing(iox::ByteView(xml));
    IOX_CHECK(reader2 == nullptr);
}

#include "iox/test/TestMain.h"
