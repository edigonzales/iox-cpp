#include "iox/Events.h"
#include "iox/Writer.h"
#include "iox/xtf/XtfWriter.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,
                                       std::size_t size) {
    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::xtf::XtfWriterOptions options;
    options.version = size != 0U && (data[0] & 1U) != 0U
        ? iox::XtfVersion::V24 : iox::XtfVersion::V23;
    options.pretty = size != 0U && (data[0] & 2U) != 0U;
    try {
        iox::xtf::XtfWriter writer(sink, options);
        iox::StartTransferEvent transfer;
        transfer.header.version = options.version;
        transfer.header.sender = "fuzz";
        if (options.version == iox::XtfVersion::V24) {
            transfer.header.models.push_back(
                {"M", {}, {}, {"urn:fuzz:model", "M", "m"}});
        } else {
            transfer.header.models.push_back({"M", {}, {}, {}});
        }
        writer.write(transfer);

        iox::StartBasketEvent basket;
        basket.basket.basketId = "b1";
        basket.basket.topic = options.version == iox::XtfVersion::V24
            ? iox::IomName("M.T", {"urn:fuzz:model", "T", "m"})
            : iox::IomName("M.T");
        writer.write(basket);

        const auto limit = std::min<std::size_t>(size, 64U);
        for (std::size_t index = 1; index < limit; ++index) {
            const auto className = options.version == iox::XtfVersion::V24
                ? iox::IomName("M.T.C", {"urn:fuzz:model", "C", "m"})
                : iox::IomName("M.T.C");
            iox::ObjectEvent event;
            event.object = iox::IomObject(
                className, "o" + std::to_string(index));
            event.object.appendPrimitive(
                iox::IomName("value"), std::to_string(data[index]));
            writer.write(event);
        }
        writer.write(iox::EndBasketEvent{});
        writer.write(iox::EndTransferEvent{});
        writer.close();
    } catch (const iox::IoxError&) {
        // Invalid generated event combinations are expected writer outcomes.
    }
    return 0;
}

#include "FuzzMain.h"
