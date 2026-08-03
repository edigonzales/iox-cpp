#include "iox/Events.h"
#include "iox/Writer.h"
#include "iox/json/JsonEventReader.h"
#include "iox/json/JsonEventWriter.h"
#include "iox/test/Test.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>

namespace {

std::string buildEvents(int objectCount) {
    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::json::JsonEventWriter writer(sink);
    iox::StartTransferEvent start;
    start.header.sender = "test";
    writer.write(start);
    iox::StartBasketEvent basket;
    basket.basket.topic = iox::IomName("M.T");
    basket.basket.basketId = "B1";
    writer.write(basket);
    for (int i = 0; i < objectCount; ++i) {
        iox::ObjectEvent object;
        object.object = iox::IomObject(iox::IomName("M.T.C"),
                                       "T" + std::to_string(i));
        object.object.setPrimitive(iox::IomName("idx"),
                                   std::to_string(i));
        writer.write(object);
    }
    writer.write(iox::EndBasketEvent{});
    writer.write(iox::EndTransferEvent{});
    writer.close();
    return sink->str();
}

int countChunked(const std::string& input, std::size_t chunkSize) {
    iox::json::JsonEventReader reader;
    for (std::size_t offset = 0; offset < input.size(); offset += chunkSize) {
        const auto count = std::min(chunkSize, input.size() - offset);
        reader.feed(iox::ByteView(
            reinterpret_cast<const std::uint8_t*>(input.data() + offset),
            count));
    }
    reader.finish();
    int count = 0;
    while (true) {
        const auto outcome = reader.next();
        if (outcome.progress == iox::ReaderProgress::End) break;
        IOX_CHECK_EQ(iox::ReaderProgress::Event, outcome.progress);
        ++count;
    }
    return count;
}

} // namespace

IOX_TEST(json_accepts_all_chunk_boundaries) {
    const auto input = buildEvents(5);
    IOX_CHECK_EQ(9, countChunked(input, 1));
    IOX_CHECK_EQ(9, countChunked(input, 2));
    IOX_CHECK_EQ(9, countChunked(input, 7));
    IOX_CHECK_EQ(9, countChunked(input, input.size()));
}

IOX_TEST(json_empty_finished_stream_is_rejected) {
    iox::json::JsonEventReader reader;
    reader.feed(iox::ByteView{});
    bool threw = false;
    try {
        reader.finish();
    } catch (const iox::IoxError& error) {
        threw = error.code() == iox::DiagnosticCode::InvalidEventOrder;
    }
    IOX_CHECK(threw);
}

IOX_TEST(json_line_limit_is_enforced) {
    iox::json::JsonEventReader reader({8U, "events.ndjson"});
    const std::string input = "{\"schema\":\"iox-event/2\"}\n";
    bool threw = false;
    try {
        reader.feed(iox::ByteView(input));
    } catch (const iox::IoxError& error) {
        threw = error.code() == iox::DiagnosticCode::JsonMalformed;
    }
    IOX_CHECK(threw);
}

IOX_TEST(json_finish_rejects_truncated_line) {
    iox::json::JsonEventReader reader;
    const std::string input = "{\"schema\":";
    reader.feed(iox::ByteView(input));
    bool threw = false;
    try {
        reader.finish();
    } catch (const iox::IoxError& error) {
        threw = error.code() == iox::DiagnosticCode::JsonMalformed;
    }
    IOX_CHECK(threw);
}

#include "iox/test/TestMain.h"
