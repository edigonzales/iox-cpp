#include "iox/ByteView.h"
#include "iox/json/JsonEventReader.h"

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,
                                       std::size_t size) {
    try {
        iox::json::JsonReaderOptions options;
        options.maxLineBytes = 2U * 1024U * 1024U;
        options.sourceName = "fuzz.ndjson";
        iox::json::JsonEventReader reader(options);
        reader.feed(iox::ByteView(data, size));
        reader.finish();
        while (reader.next().progress == iox::ReaderProgress::Event) {
        }
    } catch (const iox::IoxError&) {
        // Invalid event JSON is an expected parser outcome.
    }
    return 0;
}

#include "FuzzMain.h"
