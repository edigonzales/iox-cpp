#include "iox/xtf/XtfReader.h"
#include "iox/xtf/XtfWriter.h"
#include "iox/xtf/XtfVersion.h"
#include "iox/Events.h"
#include "iox/Writer.h"
#include "iox/test/Test.h"

#include <memory>
#include <string>
#include <vector>

// Helper: write events with XtfWriter and return output string
static std::string writeXtf(const std::vector<iox::IoxEvent>& events,
                             iox::xtf::XtfVersion version) {
    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::xtf::XtfWriterOptions opts;
    opts.version = version;
    opts.pretty = false;
    opts.sender = "Test";
    opts.software = "iox-test";

    iox::xtf::XtfWriter writer(sink, opts);
    for (const auto& e : events) {
        writer.write(e);
    }
    writer.close();
    return sink->str();
}

// Helper: read XTF and return events
static std::vector<iox::IoxEvent> readXtf(const std::string& data) {
    iox::xtf::XtfReader reader;
    reader.feed(iox::ByteView(data.data(), data.size()));
    reader.finish();

    std::vector<iox::IoxEvent> events;
    while (true) {
        auto outcome = reader.next();
        if (outcome.status == iox::ReadOutcome::Status::End) break;
        if (outcome.status == iox::ReadOutcome::Status::NeedInput) break;
        if (outcome.event) events.push_back(std::move(*outcome.event));
    }
    return events;
}

IOX_TEST(xtf23_detect_version) {
    // Write a minimal XTF 2.3 header
    std::vector<iox::IoxEvent> events;
    iox::StartTransferEvent st;
    st.sender = "S";
    st.version = 23;
    events.push_back(st);
    iox::EndTransferEvent et;
    events.push_back(et);

    auto xml = writeXtf(events, iox::xtf::XtfVersion::Xtf23);

    // Read back and check version detection
    iox::xtf::XtfReader reader;
    reader.feed(iox::ByteView(xml.data(), xml.size()));
    reader.finish();

    IOX_CHECK_EQ(iox::xtf::XtfVersion::Xtf23, reader.detectedVersion());
}

IOX_TEST(xtf23_read_header) {
    std::vector<iox::IoxEvent> events;
    iox::StartTransferEvent st;
    st.sender = "MySender";
    st.comment = "Test Comment";
    st.version = 23;
    events.push_back(st);
    iox::EndTransferEvent et;
    events.push_back(et);

    auto xml = writeXtf(events, iox::xtf::XtfVersion::Xtf23);
    auto parsed = readXtf(xml);

    // Should have at least a StartTransferEvent
    bool found = false;
    for (const auto& e : parsed) {
        if (auto* s = std::get_if<iox::StartTransferEvent>(&e)) {
            found = true;
            break;
        }
    }
    IOX_CHECK(found);
}

IOX_TEST(xtf24_detect_version) {
    std::vector<iox::IoxEvent> events;
    iox::StartTransferEvent st;
    st.sender = "S";
    st.version = 24;
    events.push_back(st);
    iox::EndTransferEvent et;
    events.push_back(et);

    auto xml = writeXtf(events, iox::xtf::XtfVersion::Xtf24);

    iox::xtf::XtfReader reader;
    reader.feed(iox::ByteView(xml.data(), xml.size()));
    reader.finish();

    IOX_CHECK_EQ(iox::xtf::XtfVersion::Xtf24, reader.detectedVersion());
}

IOX_TEST(xtf_roundtrip_minimal_23) {
    std::vector<iox::IoxEvent> events;
    iox::StartTransferEvent st;
    st.sender = "TestSender";
    st.version = 23;
    events.push_back(st);
    iox::EndTransferEvent et;
    events.push_back(et);

    auto xml = writeXtf(events, iox::xtf::XtfVersion::Xtf23);
    auto parsed = readXtf(xml);

    // Should have events
    IOX_CHECK(!parsed.empty());

    // First event should be StartTransfer
    IOX_CHECK(std::holds_alternative<iox::StartTransferEvent>(parsed[0]));
}

IOX_TEST(xtf_roundtrip_minimal_24) {
    std::vector<iox::IoxEvent> events;
    iox::StartTransferEvent st;
    st.sender = "TestSender";
    st.version = 24;
    events.push_back(st);
    iox::EndTransferEvent et;
    events.push_back(et);

    auto xml = writeXtf(events, iox::xtf::XtfVersion::Xtf24);
    auto parsed = readXtf(xml);

    IOX_CHECK(!parsed.empty());
    IOX_CHECK(std::holds_alternative<iox::StartTransferEvent>(parsed[0]));
}

IOX_TEST(xtf22_rejected) {
    // Feed something that looks like XTF but isn't 2.3 or 2.4
    iox::xtf::XtfReader reader;
    reader.feed(iox::ByteView("<UNKNOWN>", 9));
    reader.finish();

    // Should produce fatal diagnostic
    auto diags = reader.takeDiagnostics();
    bool hasFatal = false;
    for (const auto& d : diags) {
        if (d.severity == iox::Diagnostic::Severity::Fatal) {
            hasFatal = true;
        }
    }
    IOX_CHECK(hasFatal);
}

IOX_TEST(xtf_writer_state_machine) {
    // Test that the writer rejects events in wrong order
    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::xtf::XtfWriterOptions opts;
    opts.version = iox::xtf::XtfVersion::Xtf23;

    iox::xtf::XtfWriter writer(sink, opts);

    // Try to write EndTransfer first — should produce error
    iox::EndTransferEvent et;
    writer.write(et);

    auto diags = writer.takeDiagnostics();
    IOX_CHECK(!diags.empty());
}

IOX_TEST(xtf_writer_deterministic) {
    auto makeOutput = [](iox::xtf::XtfVersion v) -> std::string {
        auto sink = std::make_shared<iox::StringOutputSink>();
        iox::xtf::XtfWriterOptions opts;
        opts.version = v;
        opts.pretty = false;
        opts.sender = "S";
        opts.software = "T";
        iox::xtf::XtfWriter writer(sink, opts);

        iox::StartTransferEvent st;
        st.sender = "S";
        st.version = (v == iox::xtf::XtfVersion::Xtf24) ? 24 : 23;
        writer.write(st);
        iox::EndTransferEvent et;
        writer.write(et);
        writer.close();
        return sink->str();
    };

    auto out1 = makeOutput(iox::xtf::XtfVersion::Xtf23);
    auto out2 = makeOutput(iox::xtf::XtfVersion::Xtf23);
    IOX_CHECK_EQ(out1, out2);
}

#include "iox/test/TestMain.h"
