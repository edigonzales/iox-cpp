/// Model-aware tests against the concrete ilic-fork metamodel API.

#include "iox/ilic/IlicModelIndex.h"
#include "iox/xtf/XtfReaderOptions.h"
#include "iox/xtf/XtfWriter.h"
#include "iox/Events.h"
#include "iox/Writer.h"
#include "iox/test/Test.h"

#include <memory>
#include <string>
#include <vector>

struct TestModel final {
    metamodel::Model model;
    metamodel::SubModel topic;
    metamodel::Class classA;
    metamodel::Class classB;
    metamodel::AttrOrParam name;
    metamodel::AttrOrParam count;
    metamodel::AttrOrParam refToA;
    metamodel::TextType text;
    metamodel::NumType number;
    metamodel::ReferenceType reference;

    TestModel() {
        model.Name = "TestModel";
        model.xmlns = "urn:example:model";
        topic.Name = "TopicA";
        topic.ElementInPackage = &model;
        model.Element.push_back(&topic);

        classA.Name = "ClassA";
        classA.ElementInPackage = &topic;
        classA.ClassAttribute.push_back(&name);
        classA.ClassAttribute.push_back(&count);
        topic.Element.push_back(&classA);

        classB.Name = "ClassB";
        classB.ElementInPackage = &topic;
        classB.ClassAttribute.push_back(&refToA);
        topic.Element.push_back(&classB);

        name.Name = "Name";
        name.AttrParent = &classA;
        name.Type = &text;
        count.Name = "Count";
        count.AttrParent = &classA;
        count.Type = &number;

        refToA.Name = "RefToA";
        refToA.AttrParent = &classB;
        refToA.Type = &reference;
        reference._baseclass = &classA;
    }
};

static std::string writeXtf23(const std::vector<iox::IoxEvent>& events) {
    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::xtf::XtfWriterOptions opts;
    opts.version = iox::xtf::XtfVersion::V23;
    opts.pretty = false;
    opts.sender = "Test";
    opts.software = "iox-test";
    iox::xtf::XtfWriter writer(sink, opts);
    for (const auto& event : events) writer.write(event);
    writer.close();
    return sink->str();
}

static std::vector<iox::IoxEvent> knownEvents(const std::string& className) {
    std::vector<iox::IoxEvent> events;
    iox::StartTransferEvent start;
    start.header.version = iox::XtfVersion::V23;
    start.header.sender = "Test";
    start.header.models.push_back(
        {"TestModel", "1", "urn:example:model", {}});
    events.push_back(start);
    iox::StartBasketEvent basket;
    basket.basket.topic = iox::IomName("TestModel.TopicA");
    basket.basket.basketId = "B1";
    events.push_back(basket);
    iox::ObjectEvent object;
    object.object = iox::IomObject(iox::IomName(className), "T1");
    object.object.setOperation(iox::ObjectOperation::Insert);
    events.push_back(object);
    events.push_back(iox::EndBasketEvent{});
    events.push_back(iox::EndTransferEvent{});
    return events;
}

IOX_TEST(model_based_index_find_topic) {
    TestModel fixture;
    iox::ilic::IlicModelIndex index(fixture.model);
    const auto* topic = index.findTopic("TestModel.TopicA");
    IOX_CHECK(topic != nullptr);
    IOX_CHECK_EQ(std::string("TopicA"), topic->Name);
    IOX_CHECK(index.findTopic("Nonexistent.Topic") == nullptr);
}

IOX_TEST(model_based_index_find_class_and_properties) {
    TestModel fixture;
    iox::ilic::IlicModelIndex index(fixture.model);
    const auto* klass = index.findClass("TestModel.TopicA.ClassA");
    IOX_CHECK(klass != nullptr);
    IOX_CHECK_EQ(std::string("ClassA"), klass->Name);

    const auto* property = index.findProperty(*klass, "Name");
    IOX_CHECK(property != nullptr);
    IOX_CHECK_EQ(std::string("Name"), property->Name);
    IOX_CHECK(dynamic_cast<const metamodel::TextType*>(property->Type) != nullptr);

    const auto properties = index.transferProperties(*klass);
    IOX_CHECK_EQ(static_cast<std::size_t>(2), properties.size());
    IOX_CHECK_EQ(std::string("Name"), properties[0]->Name);
    IOX_CHECK_EQ(std::string("Count"), properties[1]->Name);
}

IOX_TEST(model_based_index_reference_type) {
    TestModel fixture;
    iox::ilic::IlicModelIndex index(fixture.model);
    const auto* klass = index.findClass("TestModel.TopicA.ClassB");
    IOX_CHECK(klass != nullptr);
    const auto* property = index.findProperty(*klass, "RefToA");
    IOX_CHECK(property != nullptr);
    const auto* reference = dynamic_cast<const metamodel::ReferenceType*>(property->Type);
    IOX_CHECK(reference != nullptr);
    IOX_CHECK(reference->_baseclass == index.findClass("TestModel.TopicA.ClassA"));
}

IOX_TEST(model_based_reader_known_class_ok) {
    TestModel fixture;
    iox::ilic::IlicXtfReaderOptions options;
    options.rejectUnknownClasses = true;
    iox::ilic::IlicXtfReader reader(fixture.model, options);

    auto events = knownEvents("TestModel.TopicA.ClassA");
    auto xml = writeXtf23(events);
    reader.feed(iox::ByteView(xml));
    reader.finish();

    int count = 0;
    while (true) {
        const auto outcome = reader.next();
        if (outcome.event) ++count;
        if (outcome.progress == iox::ReaderProgress::End) break;
    }
    IOX_CHECK_EQ(5, count);
    for (const auto& diagnostic : reader.takeDiagnostics()) {
        IOX_CHECK(diagnostic.severity != iox::DiagnosticSeverity::Error);
    }
}

IOX_TEST(model_based_reader_unknown_class_rejected) {
    TestModel fixture;
    iox::ilic::IlicXtfReaderOptions options;
    options.rejectUnknownClasses = true;
    iox::ilic::IlicXtfReader reader(fixture.model, options);
    auto xml = writeXtf23(knownEvents("TestModel.TopicA.UnknownClass"));
    reader.feed(iox::ByteView(xml));
    reader.finish();

    bool hasError = false;
    while (true) {
        const auto outcome = reader.next();
        if (outcome.progress == iox::ReaderProgress::End) break;
    }
    for (const auto& diagnostic : reader.takeDiagnostics()) {
        if (diagnostic.code == iox::DiagnosticCode::UnknownInterlisName &&
            diagnostic.message.find("Unknown class") != std::string::npos) {
            hasError = true;
        }
    }
    IOX_CHECK(hasError);
}

IOX_TEST(model_based_reader_unknown_property_rejected) {
    TestModel fixture;
    iox::ilic::IlicXtfReaderOptions options;
    options.rejectUnknownProperties = true;
    iox::ilic::IlicXtfReader reader(fixture.model, options);

    auto events = knownEvents("TestModel.TopicA.ClassA");
    auto& object = std::get<iox::ObjectEvent>(events[2]);
    object.object.setPrimitive(iox::IomName("UnknownProp"), std::to_string(42));
    auto xml = writeXtf23(events);
    reader.feed(iox::ByteView(xml));
    reader.finish();
    while (reader.next().progress != iox::ReaderProgress::End) {}

    bool hasError = false;
    for (const auto& diagnostic : reader.takeDiagnostics()) {
        if (diagnostic.code == iox::DiagnosticCode::UnknownInterlisName &&
            diagnostic.message.find("Unknown property") != std::string::npos) {
            hasError = true;
        }
    }
    IOX_CHECK(hasError);
}

IOX_TEST(model_based_writer_unknown_class_rejected) {
    TestModel fixture;
    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::ilic::IlicXtfWriterOptions options;
    options.xtf.version = iox::xtf::XtfVersion::V23;
    options.rejectUnknownClasses = true;
    iox::ilic::IlicXtfWriter writer(fixture.model, sink, options);
    iox::StartTransferEvent transfer;
    transfer.header.sender = "Test";
    writer.write(transfer);
    iox::StartBasketEvent startBasket;
    startBasket.basket.topic = iox::IomName("TestModel.TopicA");
    startBasket.basket.basketId = "B1";
    writer.write(startBasket);
    iox::ObjectEvent object;
    object.object = iox::IomObject(
        iox::IomName("TestModel.TopicA.NoSuchClass"), "T1");
    object.object.setOperation(iox::ObjectOperation::Insert);
    writer.write(object);

    bool hasError = false;
    for (const auto& diagnostic : writer.takeDiagnostics()) {
        if (diagnostic.code == iox::DiagnosticCode::UnknownInterlisName &&
            diagnostic.message.find("unknown class") != std::string::npos) {
            hasError = true;
        }
    }
    IOX_CHECK(hasError);
}

IOX_TEST(model_based_writer_uses_model_attribute_order) {
    TestModel fixture;
    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::ilic::IlicXtfWriterOptions options;
    options.xtf.version = iox::xtf::XtfVersion::V23;
    iox::ilic::IlicXtfWriter writer(fixture.model, sink, options);
    iox::StartTransferEvent transfer;
    transfer.header.sender = "Test";
    writer.write(transfer);
    iox::StartBasketEvent basket;
    basket.basket.topic = iox::IomName("TestModel.TopicA");
    basket.basket.basketId = "B1";
    writer.write(basket);
    iox::ObjectEvent object;
    object.object = iox::IomObject(
        iox::IomName("TestModel.TopicA.ClassA"), "T1");
    object.object.setOperation(iox::ObjectOperation::Insert);
    object.object.setPrimitive(iox::IomName("Count"), std::to_string(2));
    object.object.setPrimitive(iox::IomName("Name"), "first");
    writer.write(object);
    writer.write(iox::EndBasketEvent{});
    writer.write(iox::EndTransferEvent{});
    writer.close();

    const auto xml = sink->str();
    IOX_CHECK(xml.find("<Name>first</Name>") < xml.find("<Count>2</Count>"));
}

IOX_TEST(model_based_xtf24_namespace_mapping) {
    TestModel fixture;
    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::ilic::IlicXtfWriterOptions writerOptions;
    writerOptions.xtf.version = iox::xtf::XtfVersion::V24;
    writerOptions.xtf.pretty = false;
    iox::ilic::IlicXtfWriter writer(fixture.model, sink, writerOptions);

    iox::StartTransferEvent transfer;
    transfer.header.version = iox::XtfVersion::V24;
    transfer.header.sender = "Test";
    transfer.header.models.push_back(
        {"TestModel", std::nullopt, std::nullopt,
         {"urn:example:model", "TestModel", "model"}});
    writer.write(transfer);
    iox::StartBasketEvent basket;
    basket.basket.topic = iox::IomName(
        "TestModel.TopicA",
        iox::XmlQualifiedName("urn:example:model", "TopicA", "model"));
    basket.basket.basketId = "B1";
    writer.write(basket);
    iox::ObjectEvent object;
    object.object = iox::IomObject(
        iox::IomName("TestModel.TopicA.ClassA",
                     iox::XmlQualifiedName("urn:example:model", "ClassA", "model")),
        "T1");
    object.object.setPrimitive(
        iox::IomName("Name",
                     iox::XmlQualifiedName("urn:example:model", "Name", "model")),
        "first");
    writer.write(object);
    writer.write(iox::EndBasketEvent{});
    writer.write(iox::EndTransferEvent{});
    writer.close();

    const auto xml = sink->str();
    IOX_CHECK(xml.find("xmlns:model=\"urn:example:model\"") != std::string::npos);
    IOX_CHECK(xml.find("<model:ClassA") != std::string::npos);
    IOX_CHECK(xml.find("<model:Name") != std::string::npos);
    IOX_CHECK(xml.find(">first</model:Name>") != std::string::npos);

    iox::ilic::IlicXtfReaderOptions readerOptions;
    readerOptions.xtf.expectedVersion = iox::xtf::XtfVersion::V24;
    readerOptions.rejectUnknownClasses = true;
    readerOptions.rejectUnknownProperties = true;
    iox::ilic::IlicXtfReader reader(fixture.model, readerOptions);
    reader.feed(iox::ByteView(xml));
    reader.finish();

    int count = 0;
    while (true) {
        const auto outcome = reader.next();
        if (outcome.event) ++count;
        if (outcome.progress == iox::ReaderProgress::End) break;
    }
    IOX_CHECK_EQ(5, count);
    for (const auto& diagnostic : reader.takeDiagnostics()) {
        IOX_CHECK(diagnostic.severity != iox::DiagnosticSeverity::Error);
    }
}

#include "iox/test/TestMain.h"
