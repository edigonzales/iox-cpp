/// Model-based (modellbasiert) XTF tests using IlicModelIndex,
/// IlicXtfReader, and IlicXtfWriter.

#include "iox/ilic/IlicModelIndex.h"
#include "iox/ilic/ModelDef.h"
#include "iox/xtf/XtfReaderOptions.h"
#include "iox/xtf/XtfWriter.h"
#include "iox/Events.h"
#include "iox/Writer.h"
#include "iox/test/Test.h"

#include <memory>
#include <string>
#include <vector>

using namespace iox::ilic;

// ============================================================================
// Helpers
// ============================================================================

static ModelDef makeTestModel() {
    return ModelDef::createTestModel();
}

static std::string writeXtf23(const std::vector<iox::IoxEvent>& events) {
    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::xtf::XtfWriterOptions opts;
    opts.version = iox::xtf::XtfVersion::Xtf23;
    opts.pretty = false;
    opts.sender = "Test";
    opts.software = "iox-test";
    iox::xtf::XtfWriter writer(sink, opts);
    for (const auto& e : events) writer.write(e);
    writer.close();
    return sink->str();
}

// ============================================================================
// Tests
// ============================================================================

IOX_TEST(model_based_index_find_topic) {
    auto model = makeTestModel();
    IlicModelIndex index(model);

    auto* topic = index.findTopic("TestModel.TopicA");
    IOX_CHECK(topic != nullptr);
    IOX_CHECK_EQ(std::string("TestModel.TopicA"), topic->name);

    auto* missing = index.findTopic("Nonexistent.Topic");
    IOX_CHECK(missing == nullptr);
}

IOX_TEST(model_based_index_find_class) {
    auto model = makeTestModel();
    IlicModelIndex index(model);

    auto* cls = index.findClass("TestModel.TopicA.ClassA");
    IOX_CHECK(cls != nullptr);
    IOX_CHECK_EQ(std::string("TestModel.TopicA.ClassA"), cls->name);

    auto* missing = index.findClass("TestModel.TopicA.NoSuchClass");
    IOX_CHECK(missing == nullptr);
}

IOX_TEST(model_based_index_find_property) {
    auto model = makeTestModel();
    IlicModelIndex index(model);

    auto* cls = index.findClass("TestModel.TopicA.ClassA");
    IOX_CHECK(cls != nullptr);

    auto* prop = index.findProperty(*cls, "Name");
    IOX_CHECK(prop != nullptr);
    IOX_CHECK_EQ(std::string("Name"), prop->name);
    IOX_CHECK_EQ(PropertyType::Text, prop->type);

    auto* missing = index.findProperty(*cls, "NoSuchProperty");
    IOX_CHECK(missing == nullptr);
}

IOX_TEST(model_based_transfer_properties) {
    auto model = makeTestModel();
    IlicModelIndex index(model);

    auto* cls = index.findClass("TestModel.TopicA.ClassA");
    IOX_CHECK(cls != nullptr);

    auto props = index.transferProperties(*cls);
    // Should have 4 properties in transfer order
    IOX_CHECK_EQ(static_cast<std::size_t>(4), props.size());
    IOX_CHECK_EQ(std::string("Name"), props[0]->name);
    IOX_CHECK_EQ(std::string("Count"), props[1]->name);
    IOX_CHECK_EQ(std::string("IsActive"), props[2]->name);
    IOX_CHECK_EQ(std::string("Value"), props[3]->name);
}

IOX_TEST(model_based_reader_known_class_ok) {
    auto model = makeTestModel();
    IlicXtfReaderOptions opts;
    opts.rejectUnknownClasses = true;

    IlicXtfReader reader(model, opts);

    // Build XTF with a known class
    std::vector<iox::IoxEvent> events;
    iox::StartTransferEvent st; st.version = 23; events.push_back(st);
    iox::StartBasketEvent sb;
    sb.basketType = iox::IomName("TestModel.TopicA"); sb.bid = "B1"; events.push_back(sb);
    iox::ObjectEvent obj;
    obj.operation = "insert"; obj.objectId = "T1";
    obj.object = iox::IomObject(iox::IomName("TestModel.TopicA.ClassA"));
    obj.object.setPrimitive("Name", iox::IomValue::text("test"));
    events.push_back(obj);
    iox::EndBasketEvent eb; eb.bid = "B1"; events.push_back(eb);
    iox::EndTransferEvent et; events.push_back(et);

    auto xml = writeXtf23(events);
    reader.feed(iox::ByteView(xml.data(), xml.size()));
    reader.finish();

    int count = 0;
    bool hasDiag = false;
    while (true) {
        auto outcome = reader.next();
        if (outcome.status == iox::ReadOutcome::Status::End) break;
        if (outcome.event) ++count;
        if (!outcome.diagnostics.empty()) hasDiag = true;
    }
    IOX_CHECK(count > 0);
    // No errors for known class
    auto diags = reader.takeDiagnostics();
    for (auto& d : diags) {
        if (d.severity == iox::Diagnostic::Severity::Error) hasDiag = true;
    }
    IOX_CHECK(!hasDiag);
}

IOX_TEST(model_based_reader_unknown_class_rejected) {
    auto model = makeTestModel();
    IlicXtfReaderOptions opts;
    opts.rejectUnknownClasses = true;

    IlicXtfReader reader(model, opts);

    // Build XTF with an unknown class
    std::vector<iox::IoxEvent> events;
    iox::StartTransferEvent st; st.version = 23; events.push_back(st);
    iox::StartBasketEvent sb;
    sb.basketType = iox::IomName("TestModel.TopicA"); sb.bid = "B1"; events.push_back(sb);
    iox::ObjectEvent obj;
    obj.operation = "insert"; obj.objectId = "T1";
    obj.object = iox::IomObject(iox::IomName("TestModel.TopicA.UnknownClass"));
    events.push_back(obj);
    iox::EndBasketEvent eb; eb.bid = "B1"; events.push_back(eb);
    iox::EndTransferEvent et; events.push_back(et);

    auto xml = writeXtf23(events);
    reader.feed(iox::ByteView(xml.data(), xml.size()));
    reader.finish();

    // Should produce diagnostic for unknown class
    bool hasError = false;
    while (true) {
        auto outcome = reader.next();
        // Check inline diagnostics too
        for (auto& d : outcome.diagnostics) {
            if (d.severity == iox::Diagnostic::Severity::Error &&
                d.code == "ilic.unknown_class") {
                hasError = true;
            }
        }
        if (outcome.status == iox::ReadOutcome::Status::End) break;
    }
    auto diags = reader.takeDiagnostics();
    for (auto& d : diags) {
        if (d.severity == iox::Diagnostic::Severity::Error &&
            d.code == "ilic.unknown_class") {
            hasError = true;
        }
    }
    IOX_CHECK(hasError);
}

IOX_TEST(model_based_reader_unknown_property_rejected) {
    auto model = makeTestModel();
    IlicXtfReaderOptions opts;
    opts.rejectUnknownProperties = true;

    IlicXtfReader reader(model, opts);

    // Build XTF with known class but unknown property
    std::vector<iox::IoxEvent> events;
    iox::StartTransferEvent st; st.version = 23; events.push_back(st);
    iox::StartBasketEvent sb;
    sb.basketType = iox::IomName("TestModel.TopicA"); sb.bid = "B1"; events.push_back(sb);
    iox::ObjectEvent obj;
    obj.operation = "insert"; obj.objectId = "T1";
    obj.object = iox::IomObject(iox::IomName("TestModel.TopicA.ClassA"));
    obj.object.setPrimitive("Name", iox::IomValue::text("ok"));
    obj.object.setPrimitive("UnknownProp", iox::IomValue::integer(42)); // unknown!
    events.push_back(obj);
    iox::EndBasketEvent eb; eb.bid = "B1"; events.push_back(eb);
    iox::EndTransferEvent et; events.push_back(et);

    auto xml = writeXtf23(events);
    reader.feed(iox::ByteView(xml.data(), xml.size()));
    reader.finish();

    // Drain events
    while (true) {
        auto outcome = reader.next();
        if (outcome.status == iox::ReadOutcome::Status::End) break;
    }

    auto diags = reader.takeDiagnostics();
    bool hasError = false;
    for (auto& d : diags) {
        if (d.severity == iox::Diagnostic::Severity::Error &&
            d.code == "ilic.unknown_property") {
            hasError = true;
        }
    }
    IOX_CHECK(hasError);
}

IOX_TEST(model_based_writer_unknown_class_rejected) {
    auto model = makeTestModel();
    auto sink = std::make_shared<iox::StringOutputSink>();
    IlicXtfWriterOptions opts;
    opts.rejectUnknownClasses = true;

    IlicXtfWriter writer(model, sink, opts);

    iox::StartTransferEvent st; st.version = 23;
    writer.write(st);
    iox::StartBasketEvent sb;
    sb.basketType = iox::IomName("TestModel.TopicA"); sb.bid = "B1";
    writer.write(sb);

    // Write object with unknown class
    iox::ObjectEvent obj;
    obj.operation = "insert"; obj.objectId = "T1";
    obj.object = iox::IomObject(iox::IomName("TestModel.TopicA.NoSuchClass"));
    writer.write(obj);

    auto diags = writer.takeDiagnostics();
    bool hasError = false;
    for (auto& d : diags) {
        if (d.code == "ilic.unknown_class") hasError = true;
    }
    IOX_CHECK(hasError);
}

IOX_TEST(model_based_reader_reference_type) {
    auto model = makeTestModel();
    IlicModelIndex index(model);

    auto* clsB = index.findClass("TestModel.TopicA.ClassB");
    IOX_CHECK(clsB != nullptr);

    auto* refProp = index.findProperty(*clsB, "RefToA");
    IOX_CHECK(refProp != nullptr);
    IOX_CHECK_EQ(PropertyType::Reference, refProp->type);
    IOX_CHECK_EQ(std::string("TestModel.TopicA.ClassA"), refProp->targetClass);
}

#include "iox/test/TestMain.h"
