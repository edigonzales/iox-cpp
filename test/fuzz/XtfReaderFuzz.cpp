#include "iox/ByteView.h"
#include "iox/xtf/XtfReader.h"

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,
                                       std::size_t size) {
    try {
        iox::xtf::XtfReader reader;
        reader.feed(iox::ByteView(data, size));
        reader.finish();
        while (true) {
            const auto outcome = reader.next();
            if (outcome.progress == iox::ReaderProgress::End ||
                outcome.progress == iox::ReaderProgress::NeedInput) {
                break;
            }
        }
        (void)reader.takeDiagnostics();
    } catch (const iox::IoxError&) {
        // Malformed transfer input is an expected parser outcome.
    }
    return 0;
}

#include "FuzzMain.h"
