#include "iox/ByteView.h"
#include "xml/ExpatParser.h"

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,
                                       std::size_t size) {
    try {
        iox::xml::XmlLimits limits;
        limits.maxDepth = 64U;
        limits.maxAttributesPerElement = 128U;
        limits.maxTextBytesPerNode = 1024U * 1024U;
        limits.maxTotalInputBytes = 2U * 1024U * 1024U;
        iox::xml::ExpatParser parser(limits, "fuzz.xml");
        parser.setStartHandler([](const auto&) {});
        parser.setEndHandler([](const auto&) {});
        parser.setTextHandler([](std::string_view, const auto&) {});
        parser.feed(iox::ByteView(data, size));
        parser.finish();
    } catch (const iox::IoxError&) {
        // Invalid XML and configured resource limits are expected outcomes.
    }
    return 0;
}

#include "FuzzMain.h"
