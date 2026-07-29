/// Association conformance tests (model-free, ili:-prefixed format).
/// Covers the same patterns as iox-ili Xtf23Reader/associations/
/// but using our canonical XTF 2.3 encoding.

#include "iox/xtf/XtfReader.h"
#include "iox/xtf/XtfWriter.h"
#include "iox/xtf/XtfVersion.h"
#include "iox/Events.h"
#include "iox/Writer.h"
#include "iox/test/Test.h"

#include <memory>
#include <string>
#include <vector>

// ============================================================================
// Helpers
// ============================================================================

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

// ============================================================================
// Embedded 1:1 Association (REF on attribute)
// ============================================================================

IOX_TEST(association_embedded_1to1_ref) {
    // ClassA has a reference to ClassB via REF attribute
    std::vector<iox::IoxEvent> events;
    iox::StartTransferEvent st; st.version = 23; events.push_back(st);
    iox::StartBasketEvent sb;
    sb.basketType = iox::IomName("TestModel.TopicA.Data");
    sb.bid = "B1"; events.push_back(sb);

    // Object B (the target)
    iox::ObjectEvent objB;
    objB.operation = "insert";
    objB.objectId = "TID_B";
    objB.object = iox::IomObject(iox::IomName("TestModel.TopicA.ClassB"));
    objB.object.setPrimitive("Name", iox::IomValue::text("TargetB"));
    events.push_back(objB);

    // Object A (references B)
    iox::ObjectEvent objA;
    objA.operation = "insert";
    objA.objectId = "TID_A";
    objA.object = iox::IomObject(iox::IomName("TestModel.TopicA.ClassA"));
    objA.object.setPrimitive("NameA", iox::IomValue::text("SourceA"));
    // Reference to B via REF
    auto& refAttr = objA.object.setAttribute(iox::IomName("RefToB"));
    refAttr.ref = "TID_B";
    refAttr.values.push_back(iox::IomValue::text("TargetB")); // display value
    events.push_back(objA);

    iox::EndBasketEvent eb; eb.bid = "B1"; events.push_back(eb);
    iox::EndTransferEvent et; events.push_back(et);

    auto xml = writeXtf23(events);
    auto parsed = readXtf(xml);

    // Should have 6 events: ST + SB + 2xObject + EB + ET
    IOX_CHECK_EQ(static_cast<std::size_t>(6), parsed.size());

    // Find object A and verify its REF
    bool foundRef = false;
    for (auto& e : parsed) {
        if (auto* obj = std::get_if<iox::ObjectEvent>(&e)) {
            for (std::size_t i = 0; i < obj->object.attributeCount(); ++i) {
                const auto& a = obj->object.attributeAt(i);
                if (a.ref && *a.ref == "TID_B") {
                    foundRef = true;
                }
            }
        }
    }
    IOX_CHECK(foundRef);
}

// ============================================================================
// Embedded 1:N Association (multiple REFs)
// ============================================================================

IOX_TEST(association_embedded_1toN_refs) {
    std::vector<iox::IoxEvent> events;
    iox::StartTransferEvent st; st.version = 23; events.push_back(st);
    iox::StartBasketEvent sb;
    sb.basketType = iox::IomName("M.T.Data"); sb.bid = "B1"; events.push_back(sb);

    // Object A references multiple B's
    iox::ObjectEvent objA;
    objA.operation = "insert";
    objA.objectId = "TID_A";
    objA.object = iox::IomObject(iox::IomName("M.T.ClassA"));
    auto& multiRef = objA.object.setAttribute(iox::IomName("RefsToB"));
    multiRef.ref = "TID_B1";
    multiRef.values.push_back(iox::IomValue::text("B1"));
    // Add second value with different REF
    multiRef.values.push_back(iox::IomValue::text("B2"));
    events.push_back(objA);

    iox::EndBasketEvent eb; eb.bid = "B1"; events.push_back(eb);
    iox::EndTransferEvent et; events.push_back(et);

    auto xml = writeXtf23(events);
    auto parsed = readXtf(xml);

    // Find the multi-valued REF attribute
    bool foundMulti = false;
    for (auto& e : parsed) {
        if (auto* obj = std::get_if<iox::ObjectEvent>(&e)) {
            for (std::size_t i = 0; i < obj->object.attributeCount(); ++i) {
                const auto& a = obj->object.attributeAt(i);
                if (a.values.size() >= 2) foundMulti = true;
            }
        }
    }
    IOX_CHECK(foundMulti);
}

// ============================================================================
// ORDER_POS on reference
// ============================================================================

IOX_TEST(association_order_pos) {
    std::vector<iox::IoxEvent> events;
    iox::StartTransferEvent st; st.version = 23; events.push_back(st);
    iox::StartBasketEvent sb;
    sb.basketType = iox::IomName("M.T.Data"); sb.bid = "B1"; events.push_back(sb);

    iox::ObjectEvent objA;
    objA.operation = "insert";
    objA.objectId = "TID_A";
    objA.object = iox::IomObject(iox::IomName("M.T.ClassA"));
    // REF with ORDER_POS
    auto& refAttr = objA.object.setAttribute(iox::IomName("OrderedRefs"));
    refAttr.ref = "TID_B1";
    refAttr.orderPos = 1;
    refAttr.values.push_back(iox::IomValue::text("first"));
    events.push_back(objA);

    iox::EndBasketEvent eb; eb.bid = "B1"; events.push_back(eb);
    iox::EndTransferEvent et; events.push_back(et);

    auto xml = writeXtf23(events);
    auto parsed = readXtf(xml);

    bool foundOrderPos = false;
    for (auto& e : parsed) {
        if (auto* obj = std::get_if<iox::ObjectEvent>(&e)) {
            for (std::size_t i = 0; i < obj->object.attributeCount(); ++i) {
                const auto& a = obj->object.attributeAt(i);
                if (a.orderPos && *a.orderPos == 1) foundOrderPos = true;
            }
        }
    }
    IOX_CHECK(foundOrderPos);
}

// ============================================================================
// Association class with own attributes
// ============================================================================

IOX_TEST(association_with_attributes) {
    std::vector<iox::IoxEvent> events;
    iox::StartTransferEvent st; st.version = 23; events.push_back(st);
    iox::StartBasketEvent sb;
    sb.basketType = iox::IomName("M.T.Data"); sb.bid = "B1"; events.push_back(sb);

    // Association object with own attributes + REFs to both ends
    iox::ObjectEvent assoc;
    assoc.operation = "insert";
    assoc.objectId = "TID_ASSOC";
    assoc.object = iox::IomObject(iox::IomName("M.T.AssocClass"));
    assoc.object.setPrimitive("AssocAttr", iox::IomValue::text("link-data"));

    auto& refA = assoc.object.setAttribute(iox::IomName("RoleA"));
    refA.ref = "TID_A";
    refA.values.push_back(iox::IomValue::text("A"));

    auto& refB = assoc.object.setAttribute(iox::IomName("RoleB"));
    refB.ref = "TID_B";
    refB.values.push_back(iox::IomValue::text("B"));

    events.push_back(assoc);
    iox::EndBasketEvent eb; eb.bid = "B1"; events.push_back(eb);
    iox::EndTransferEvent et; events.push_back(et);

    auto xml = writeXtf23(events);
    auto parsed = readXtf(xml);

    IOX_CHECK_EQ(static_cast<std::size_t>(5), parsed.size());
    auto& objEvent = std::get<iox::ObjectEvent>(parsed[2]);
    IOX_CHECK_EQ(std::string("TID_ASSOC"), objEvent.objectId);

    // Should have both REF attributes and the AssocAttr
    int refCount = 0;
    bool hasAssocAttr = false;
    for (std::size_t i = 0; i < objEvent.object.attributeCount(); ++i) {
        const auto& a = objEvent.object.attributeAt(i);
        if (a.ref) ++refCount;
        if (a.name.iliName() == "AssocAttr") hasAssocAttr = true;
    }
    IOX_CHECK_EQ(2, refCount);
    IOX_CHECK(hasAssocAttr);
}

// ============================================================================
// Delete with REF
// ============================================================================

IOX_TEST(association_delete_with_ref) {
    std::vector<iox::IoxEvent> events;
    iox::StartTransferEvent st; st.version = 23; events.push_back(st);
    iox::StartBasketEvent sb;
    sb.basketType = iox::IomName("M.T.Data"); sb.bid = "B1"; events.push_back(sb);

    iox::ObjectEvent obj;
    obj.operation = "delete";
    obj.objectId = "TID_DEL";
    obj.object = iox::IomObject(iox::IomName("M.T.ClassA"));
    auto& refAttr = obj.object.setAttribute(iox::IomName("RefToB"));
    refAttr.ref = "TID_B";
    events.push_back(obj);

    iox::EndBasketEvent eb; eb.bid = "B1"; events.push_back(eb);
    iox::EndTransferEvent et; events.push_back(et);

    auto xml = writeXtf23(events);
    auto parsed = readXtf(xml);

    IOX_CHECK_EQ(static_cast<std::size_t>(5), parsed.size());
    auto& objEvent = std::get<iox::ObjectEvent>(parsed[2]);
    IOX_CHECK_EQ(std::string("delete"), objEvent.operation);
}

// ============================================================================
// Standalone association (separate basket for link objects)
// ============================================================================

IOX_TEST(association_standalone) {
    std::vector<iox::IoxEvent> events;
    iox::StartTransferEvent st; st.version = 23; events.push_back(st);

    // Basket 1: data objects
    iox::StartBasketEvent sb1;
    sb1.basketType = iox::IomName("M.T.Data"); sb1.bid = "B1"; events.push_back(sb1);
    iox::ObjectEvent objA;
    objA.operation = "insert"; objA.objectId = "A1";
    objA.object = iox::IomObject(iox::IomName("M.T.ClassA"));
    objA.object.setPrimitive("Name", iox::IomValue::text("A"));
    events.push_back(objA);
    iox::EndBasketEvent eb1; eb1.bid = "B1"; events.push_back(eb1);

    // Basket 2: link objects (standalone association)
    iox::StartBasketEvent sb2;
    sb2.basketType = iox::IomName("M.T.Links"); sb2.bid = "B2"; events.push_back(sb2);
    iox::ObjectEvent link;
    link.operation = "insert"; link.objectId = "L1";
    link.object = iox::IomObject(iox::IomName("M.T.ABLink"));
    auto& rA = link.object.setAttribute(iox::IomName("RoleA"));
    rA.ref = "A1"; rA.values.push_back(iox::IomValue::text("A"));
    auto& rB = link.object.setAttribute(iox::IomName("RoleB"));
    rB.ref = "B1"; rB.values.push_back(iox::IomValue::text("B"));
    events.push_back(link);
    iox::EndBasketEvent eb2; eb2.bid = "B2"; events.push_back(eb2);

    iox::EndTransferEvent et; events.push_back(et);

    auto xml = writeXtf23(events);
    auto parsed = readXtf(xml);

    // ST + SB1 + ObjectA + EB1 + SB2 + Link + EB2 + ET = 8
    IOX_CHECK_EQ(static_cast<std::size_t>(8), parsed.size());

    int basketCount = 0;
    for (auto& e : parsed) {
        if (std::holds_alternative<iox::StartBasketEvent>(e)) ++basketCount;
    }
    IOX_CHECK_EQ(2, basketCount);
}

// ============================================================================
// Object with BID (Basket ID reference on object)
// ============================================================================

IOX_TEST(association_object_with_bid) {
    std::vector<iox::IoxEvent> events;
    iox::StartTransferEvent st; st.version = 23; events.push_back(st);
    iox::StartBasketEvent sb;
    sb.basketType = iox::IomName("M.T.Data"); sb.bid = "B_MAIN"; events.push_back(sb);

    iox::ObjectEvent obj;
    obj.operation = "insert";
    obj.objectId = "TID_1";
    obj.object = iox::IomObject(iox::IomName("M.T.ClassX"));
    obj.object.setPrimitive("Val", iox::IomValue::text("x"));
    // Object-level BID (basket reference)
    obj.object.setBid("B_MAIN");
    events.push_back(obj);

    iox::EndBasketEvent eb; eb.bid = "B_MAIN"; events.push_back(eb);
    iox::EndTransferEvent et; events.push_back(et);

    auto xml = writeXtf23(events);
    auto parsed = readXtf(xml);

    IOX_CHECK_EQ(static_cast<std::size_t>(5), parsed.size());
    auto& objEvent = std::get<iox::ObjectEvent>(parsed[2]);
    IOX_CHECK(objEvent.object.bid().has_value());
    IOX_CHECK_EQ(std::string("B_MAIN"), *objEvent.object.bid());
}

// ============================================================================
// Same target class (both roles ref same class)
// ============================================================================

IOX_TEST(association_same_target_class) {
    std::vector<iox::IoxEvent> events;
    iox::StartTransferEvent st; st.version = 23; events.push_back(st);
    iox::StartBasketEvent sb;
    sb.basketType = iox::IomName("M.T.Data"); sb.bid = "B1"; events.push_back(sb);

    // Parent object references two children of same class
    iox::ObjectEvent parent;
    parent.operation = "insert";
    parent.objectId = "P1";
    parent.object = iox::IomObject(iox::IomName("M.T.Parent"));
    auto& child1 = parent.object.setAttribute(iox::IomName("Child1"));
    child1.ref = "C1"; child1.values.push_back(iox::IomValue::text("C1"));
    auto& child2 = parent.object.setAttribute(iox::IomName("Child2"));
    child2.ref = "C2"; child2.values.push_back(iox::IomValue::text("C2"));
    events.push_back(parent);

    iox::EndBasketEvent eb; eb.bid = "B1"; events.push_back(eb);
    iox::EndTransferEvent et; events.push_back(et);

    auto xml = writeXtf23(events);
    auto parsed = readXtf(xml);

    auto& objEvent = std::get<iox::ObjectEvent>(parsed[2]);
    auto* c1 = objEvent.object.findAttribute("Child1");
    auto* c2 = objEvent.object.findAttribute("Child2");
    IOX_CHECK(c1 != nullptr);
    IOX_CHECK(c2 != nullptr);
    IOX_CHECK(c1->ref.has_value());
    IOX_CHECK(c2->ref.has_value());
}

#include "iox/test/TestMain.h"
