#include "iox/json/JsonEventReader.h"
#include "iox/json/JsonEventWriter.h"
#include "iox/Events.h"
#include "iox/Writer.h"

#include "iox/test/Test.h"

#include <memory>
#include <string>
#include <vector>

// Helper: feed NDJSON in chunks of varying sizes and check we get
// the expected number of events.
static int countEventsChunked(const std::string& data, std::size_t chunkSize) {
    iox::json::JsonEventReader reader;

    for (std::size_t offset = 0; offset < data.size(); offset += chunkSize) {
        auto count = chunkSize;
        if (offset + count > data.size()) count = data.size() - offset;
        reader.feed(iox::ByteView(data.data() + offset, count));
    }
    reader.finish();

    int eventCount = 0;
    while (true) {
        auto outcome = reader.next();
        if (outcome.status == iox::ReadOutcome::Status::End) break;
        if (outcome.status == iox::ReadOutcome::Status::NeedInput) {
            // All data already fed and finished; shouldn't happen but break to avoid loop
            break;
        }
        if (outcome.event) ++eventCount;
    }
    return eventCount;
}

// Build test NDJSON data
static std::string buildTestData(int numObjects) {
    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::json::JsonEventWriter writer(sink);

    iox::StartTransferEvent st;
    st.version = 23;
    writer.write(st);

    iox::StartBasketEvent sb;
    sb.basketType = iox::IomName("M.T.B");
    sb.bid = "B1";
    writer.write(sb);

    for (int i = 0; i < numObjects; ++i) {
        iox::ObjectEvent obj;
        obj.operation = "insert";
        obj.objectId = "TID" + std::to_string(i);
        obj.object = iox::IomObject(iox::IomName("M.T.C"));
        obj.object.setPrimitive("idx", iox::IomValue::integer(i));
        writer.write(obj);
    }

    iox::EndBasketEvent eb;
    eb.bid = "B1";
    writer.write(eb);

    iox::EndTransferEvent et;
    writer.write(et);

    writer.close();
    return sink->str();
}

IOX_TEST(json_chunked_one_byte) {
    auto data = buildTestData(5);
    // Total events: StartTransfer + StartBasket + 5 Objects + EndBasket + EndTransfer = 9
    int count = countEventsChunked(data, 1);
    IOX_CHECK_EQ(9, count);
}

IOX_TEST(json_chunked_two_bytes) {
    auto data = buildTestData(3);
    // Total: 1 + 1 + 3 + 1 + 1 = 7
    int count = countEventsChunked(data, 2);
    IOX_CHECK_EQ(7, count);
}

IOX_TEST(json_chunked_whole) {
    auto data = buildTestData(10);
    // Total: 1 + 1 + 10 + 1 + 1 = 14
    int count = countEventsChunked(data, data.size());
    IOX_CHECK_EQ(14, count);
}

IOX_TEST(json_chunked_seven) {
    auto data = buildTestData(4);
    int count = countEventsChunked(data, 7);
    IOX_CHECK_EQ(8, count);
}

IOX_TEST(json_invalid_input_empty) {
    iox::json::JsonEventReader reader;
    reader.feed(iox::ByteView("", 0));
    reader.finish();

    auto outcome = reader.next();
    IOX_CHECK_EQ(iox::ReadOutcome::Status::End, outcome.status);
}

IOX_TEST(json_invalid_input_not_json) {
    iox::json::JsonEventReader reader;
    reader.feed(iox::ByteView("this is not json\n", 18));
    reader.finish();

    // Should produce diagnostics, not crash
    auto outcome = reader.next();
    // The invalid line may produce an error diagnostic; subsequent
    // lines may or may not be parsed.
    (void)outcome;
}

#include "iox/test/TestMain.h"
