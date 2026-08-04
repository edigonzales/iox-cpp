#include "iox/ByteView.h"
#include "iox/Events.h"
#include "iox/xtf/XtfReader.h"
#include "iox/xtf/XtfWriter.h"
#include "iox/test/Test.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>

#if defined(__APPLE__) || defined(__linux__)
#include <sys/resource.h>
#endif

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

class CountingSink final : public iox::OutputSink {
public:
    std::size_t write(const void*, std::size_t size) override {
        bytes_ += size;
        return size;
    }
    std::size_t bytes() const noexcept { return bytes_; }

private:
    std::size_t bytes_ = 0;
};

std::size_t readObjects(std::string_view xml, std::size_t queueLimit) {
    iox::xtf::XtfReaderOptions options;
    options.xmlLimits.maxQueuedEvents = queueLimit;
    iox::xtf::XtfReader reader(options);
    std::size_t objects = 0;
    constexpr std::size_t chunkSize = 4096;
    const auto consume = [&](const iox::IoxEvent& event) {
        if (const auto* object = std::get_if<iox::ObjectEvent>(&event)) {
            IOX_CHECK(object->object.oid().has_value());
            IOX_CHECK_EQ("T" + std::to_string(objects),
                         *object->object.oid());
            ++objects;
        }
    };
    const auto drain = [&] {
        while (true) {
            const auto outcome = reader.next();
            if (outcome.progress != iox::ReaderProgress::Event) break;
            consume(*outcome.event);
        }
    };
    for (std::size_t offset = 0; offset < xml.size(); offset += chunkSize) {
        const auto size = std::min(chunkSize, xml.size() - offset);
        reader.feed(iox::ByteView(
            reinterpret_cast<const std::uint8_t*>(xml.data() + offset),
            size));
        drain();
    }
    reader.finish();
    while (true) {
        const auto outcome = reader.next();
        if (outcome.progress == iox::ReaderProgress::End) break;
        IOX_CHECK_EQ(iox::ReaderProgress::Event, outcome.progress);
        consume(*outcome.event);
    }
    return objects;
}

std::size_t writeObjects(std::size_t objectCount) {
    auto sink = std::make_shared<CountingSink>();
    iox::xtf::XtfWriterOptions options;
    options.pretty = false;
    iox::xtf::XtfWriter writer(sink, options);

    iox::StartTransferEvent transfer;
    transfer.header.version = iox::XtfVersion::V23;
    transfer.header.sender = "stream-test";
    transfer.header.models.push_back(
        {"M", std::string("1"), std::string("urn:m"), {}});
    writer.write(transfer);

    iox::StartBasketEvent basket;
    basket.basket.topic = iox::IomName("M.Topic");
    basket.basket.basketId = "B1";
    writer.write(basket);
    for (std::size_t index = 0; index < objectCount; ++index) {
        iox::ObjectEvent object;
        object.object = iox::IomObject(
            iox::IomName("M.Topic.Class"), "T" + std::to_string(index));
        object.object.setPrimitive(
            iox::IomName("value"), std::to_string(index));
        writer.write(object);
    }
    writer.write(iox::EndBasketEvent{});
    writer.write(iox::EndTransferEvent{});
    writer.close();
    return sink->bytes();
}

std::size_t peakResidentBytes() {
#if defined(__APPLE__) || defined(__linux__)
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0) return 0U;
#if defined(__APPLE__)
    return static_cast<std::size_t>(usage.ru_maxrss);
#else
    return static_cast<std::size_t>(usage.ru_maxrss) * 1024U;
#endif
#else
    return 0U;
#endif
}

constexpr bool addressSanitizerEnabled() {
#if defined(__has_feature)
#if __has_feature(address_sanitizer)
    return true;
#endif
#endif
#if defined(__SANITIZE_ADDRESS__)
    return true;
#else
    return false;
#endif
}

} // namespace

IOX_TEST(large_xtf23_transfer_is_incremental_and_ordered) {
    constexpr std::size_t objectCount = 100000;
    const auto xml = makeLargeTransfer(objectCount);
    IOX_CHECK_EQ(objectCount, readObjects(xml, 2U));
}

IOX_TEST(large_xtf23_writer_streams_one_hundred_thousand_objects) {
    constexpr std::size_t objectCount = 100000;
    const auto bytes = writeObjects(objectCount);
    IOX_CHECK(bytes > objectCount * 40U);
}

IOX_TEST(repeated_streaming_has_stable_memory_use) {
    constexpr std::size_t objectCount = 5000;
    const auto xml = makeLargeTransfer(objectCount);
    IOX_CHECK_EQ(objectCount, readObjects(xml, 1U));
    const auto expectedBytes = writeObjects(objectCount);
    const auto warmedResident = peakResidentBytes();
    for (std::size_t repetition = 0; repetition < 4U; ++repetition) {
        IOX_CHECK_EQ(objectCount, readObjects(xml, 1U));
        IOX_CHECK_EQ(expectedBytes, writeObjects(objectCount));
    }
    const auto finalResident = peakResidentBytes();
    if (!addressSanitizerEnabled() && warmedResident != 0U &&
        finalResident >= warmedResident) {
        constexpr std::size_t allowedGrowth = 128U * 1024U * 1024U;
        IOX_CHECK(finalResident - warmedResident < allowedGrowth);
    }
}

#include "iox/test/TestMain.h"
