#include "iox/Events.h"
#include "iox/Writer.h"
#include "iox/xtf/XtfReader.h"
#include "iox/xtf/XtfWriter.h"
#include "iox/test/Test.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

iox::StartTransferEvent transfer() {
    iox::StartTransferEvent result;
    result.header.version = iox::XtfVersion::V23;
    result.header.sender = "association-test";
    return result;
}

iox::StartBasketEvent basket(std::string topic = "M.T.Data",
                             std::string id = "B1") {
    iox::StartBasketEvent result;
    result.basket.topic = iox::IomName(std::move(topic));
    result.basket.basketId = std::move(id);
    return result;
}

iox::ObjectEvent object(std::string tag, std::string oid,
                        iox::ObjectOperation operation =
                            iox::ObjectOperation::Insert) {
    iox::ObjectEvent result;
    result.object = iox::IomObject(iox::IomName(std::move(tag)),
                                   std::move(oid));
    result.object.setOperation(operation);
    return result;
}

iox::IomObject reference(std::string targetOid,
                         std::optional<std::string> targetBasket = std::nullopt,
                         std::optional<std::uint64_t> order = std::nullopt) {
    iox::IomObject result(iox::IomName("REFERENCE"));
    result.setReference(
        {std::move(targetOid), std::move(targetBasket), order});
    return result;
}

std::vector<iox::IoxEvent> roundtrip(
    const std::vector<iox::IoxEvent>& events) {
    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::xtf::XtfWriterOptions options;
    options.version = iox::XtfVersion::V23;
    options.pretty = false;
    iox::xtf::XtfWriter writer(sink, options);
    for (const auto& event : events) writer.write(event);
    writer.close();

    iox::xtf::XtfReader reader;
    reader.feed(iox::ByteView(sink->str()));
    reader.finish();
    std::vector<iox::IoxEvent> result;
    while (true) {
        auto outcome = reader.next();
        if (outcome.progress == iox::ReaderProgress::End) break;
        IOX_CHECK_EQ(iox::ReaderProgress::Event, outcome.progress);
        result.push_back(std::move(*outcome.event));
    }
    return result;
}

const iox::IomObject& parsedObject(const std::vector<iox::IoxEvent>& events,
                                   std::size_t index = 2) {
    return std::get<iox::ObjectEvent>(events[index]).object;
}

} // namespace

IOX_TEST(association_embedded_1to1_ref) {
    auto target = object("M.T.ClassB", "TID_B");
    target.object.setPrimitive(iox::IomName("Name"), "TargetB");
    auto source = object("M.T.ClassA", "TID_A");
    source.object.setObject(iox::IomName("RefToB"), reference("TID_B"));
    const auto parsed = roundtrip(
        {transfer(), basket(), target, source, iox::EndBasketEvent{},
         iox::EndTransferEvent{}});
    const auto ref = parsedObject(parsed, 3).object("RefToB");
    IOX_CHECK(ref.has_value());
    IOX_CHECK_EQ(std::string("TID_B"), *ref->reference().targetOid);
}

IOX_TEST(association_embedded_1toN_refs) {
    auto source = object("M.T.ClassA", "TID_A");
    source.object.appendObject(iox::IomName("RefsToB"),
                               reference("TID_B1"));
    source.object.appendObject(iox::IomName("RefsToB"),
                               reference("TID_B2"));
    const auto parsed = roundtrip(
        {transfer(), basket(), source, iox::EndBasketEvent{},
         iox::EndTransferEvent{}});
    const auto& result = parsedObject(parsed);
    IOX_CHECK_EQ(static_cast<std::size_t>(2), result.valueCount("RefsToB"));
    IOX_CHECK_EQ(std::string("TID_B2"),
        *result.object("RefsToB", 1)->reference().targetOid);
}

IOX_TEST(association_order_pos) {
    auto source = object("M.T.ClassA", "TID_A");
    source.object.setObject(iox::IomName("OrderedRefs"),
                            reference("TID_B1", std::nullopt, 1U));
    const auto parsed = roundtrip(
        {transfer(), basket(), source, iox::EndBasketEvent{},
         iox::EndTransferEvent{}});
    const auto ref = parsedObject(parsed).object("OrderedRefs");
    IOX_CHECK(ref.has_value());
    IOX_CHECK_EQ(std::uint64_t{1}, *ref->reference().orderPosition);
}

IOX_TEST(association_with_attributes) {
    auto association = object("M.T.AssocClass", "TID_ASSOC");
    association.object.setPrimitive(iox::IomName("AssocAttr"), "link-data");
    association.object.setObject(iox::IomName("RoleA"), reference("TID_A"));
    association.object.setObject(iox::IomName("RoleB"), reference("TID_B"));
    const auto parsed = roundtrip(
        {transfer(), basket(), association, iox::EndBasketEvent{},
         iox::EndTransferEvent{}});
    const auto& result = parsedObject(parsed);
    IOX_CHECK_EQ(std::string("TID_ASSOC"), *result.oid());
    IOX_CHECK_EQ(std::string_view("link-data"),
                 *result.primitive("AssocAttr"));
    IOX_CHECK(result.object("RoleA")->isReference());
    IOX_CHECK(result.object("RoleB")->isReference());
}

IOX_TEST(association_delete_with_ref) {
    auto deleted = object("M.T.ClassA", "TID_DEL",
                          iox::ObjectOperation::Delete);
    deleted.object.setObject(iox::IomName("RefToB"), reference("TID_B"));
    const auto parsed = roundtrip(
        {transfer(), basket(), deleted, iox::EndBasketEvent{},
         iox::EndTransferEvent{}});
    IOX_CHECK_EQ(iox::ObjectOperation::Delete,
                 parsedObject(parsed).operation());
}

IOX_TEST(association_standalone) {
    auto first = object("M.T.ClassA", "A1");
    first.object.setPrimitive(iox::IomName("Name"), "A");
    auto link = object("M.T.ABLink", "L1");
    link.object.setObject(iox::IomName("RoleA"), reference("A1"));
    link.object.setObject(iox::IomName("RoleB"), reference("B1"));
    const auto parsed = roundtrip(
        {transfer(), basket("M.T.Data", "B1"), first,
         iox::EndBasketEvent{}, basket("M.T.Links", "B2"), link,
         iox::EndBasketEvent{}, iox::EndTransferEvent{}});
    IOX_CHECK_EQ(static_cast<std::size_t>(8), parsed.size());
    IOX_CHECK_EQ(std::string("L1"), *parsedObject(parsed, 5).oid());
}

IOX_TEST(association_object_with_bid) {
    auto value = object("M.T.ClassX", "TID_1");
    value.object.setReference({std::nullopt, "B_MAIN", std::nullopt});
    const auto parsed = roundtrip(
        {transfer(), basket("M.T.Data", "B_MAIN"), value,
         iox::EndBasketEvent{}, iox::EndTransferEvent{}});
    IOX_CHECK_EQ(std::string("B_MAIN"),
                 *parsedObject(parsed).reference().targetBasketId);
}

IOX_TEST(association_same_target_class) {
    auto parent = object("M.T.Parent", "P1");
    parent.object.setObject(iox::IomName("Child1"), reference("C1"));
    parent.object.setObject(iox::IomName("Child2"), reference("C2"));
    const auto parsed = roundtrip(
        {transfer(), basket(), parent, iox::EndBasketEvent{},
         iox::EndTransferEvent{}});
    IOX_CHECK_EQ(std::string("C1"),
        *parsedObject(parsed).object("Child1")->reference().targetOid);
    IOX_CHECK_EQ(std::string("C2"),
        *parsedObject(parsed).object("Child2")->reference().targetOid);
}

#include "iox/test/TestMain.h"
