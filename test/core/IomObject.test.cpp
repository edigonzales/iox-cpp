#include "iox/IomObject.h"
#include "iox/IomValue.h"
#include "iox/test/Test.h"

IOX_TEST(iom_object_default_construction) {
    iox::IomObject obj;
    IOX_CHECK(obj.tag().iliName().empty());
    IOX_CHECK_EQ(static_cast<std::size_t>(0), obj.attributeCount());
    IOX_CHECK_EQ(1L, obj.useCount());
}

IOX_TEST(iom_object_tag) {
    iox::IomObject obj(iox::IomName("MyClass"));
    IOX_CHECK_EQ(std::string("MyClass"), obj.tag().iliName());

    obj.setTag(iox::IomName("Renamed"));
    IOX_CHECK_EQ(std::string("Renamed"), obj.tag().iliName());
}

IOX_TEST(iom_object_set_and_get_primitive) {
    iox::IomObject obj(iox::IomName("Test"));
    obj.setPrimitive("Name", iox::IomValue::text("value1"));
    obj.setPrimitive("Count", iox::IomValue::integer(42));

    auto nameVal = obj.getPrimitive("Name");
    IOX_CHECK(nameVal.has_value());
    if (nameVal) {
        IOX_CHECK_EQ(std::string("value1"), nameVal->asText());
    }

    auto countVal = obj.getPrimitive("Count");
    IOX_CHECK(countVal.has_value());
    if (countVal) {
        IOX_CHECK_EQ(42, countVal->asInteger());
    }

    // Missing attribute
    auto missing = obj.getPrimitive("Missing");
    IOX_CHECK(!missing.has_value());
}

IOX_TEST(iom_object_attribute_order_preserved) {
    iox::IomObject obj(iox::IomName("Test"));
    obj.setPrimitive("c", iox::IomValue::integer(3));
    obj.setPrimitive("a", iox::IomValue::integer(1));
    obj.setPrimitive("b", iox::IomValue::integer(2));

    // Order must be: c, a, b (insertion order)
    IOX_CHECK_EQ(static_cast<std::size_t>(3), obj.attributeCount());
    IOX_CHECK_EQ(std::string("c"), obj.attributeAt(0).name.iliName());
    IOX_CHECK_EQ(std::string("a"), obj.attributeAt(1).name.iliName());
    IOX_CHECK_EQ(std::string("b"), obj.attributeAt(2).name.iliName());
}

IOX_TEST(iom_object_find_attribute) {
    iox::IomObject obj(iox::IomName("Test"));
    obj.setPrimitive("alpha", iox::IomValue::integer(1));
    obj.setPrimitive("beta", iox::IomValue::integer(2));

    auto* alpha = obj.findAttribute("alpha");
    IOX_CHECK(alpha != nullptr);
    auto* beta = obj.findAttribute("beta");
    IOX_CHECK(beta != nullptr);
    auto* gamma = obj.findAttribute("gamma");
    IOX_CHECK(gamma == nullptr);
}

IOX_TEST(iom_object_cow_semantics) {
    iox::IomObject a(iox::IomName("Test"));
    a.setPrimitive("x", iox::IomValue::integer(1));

    iox::IomObject b = a;  // shared ownership
    IOX_CHECK_EQ(2L, a.useCount());
    IOX_CHECK_EQ(2L, b.useCount());

    // Mutating b should detach
    b.setPrimitive("x", iox::IomValue::integer(999));

    IOX_CHECK_EQ(1L, a.useCount());
    IOX_CHECK_EQ(1L, b.useCount());

    // a remains unchanged
    auto av = a.getPrimitive("x");
    IOX_CHECK(av.has_value());
    IOX_CHECK_EQ(1, av->asInteger());

    // b has the new value
    auto bv = b.getPrimitive("x");
    IOX_CHECK(bv.has_value());
    IOX_CHECK_EQ(999, bv->asInteger());
}

IOX_TEST(iom_object_repeated_values_order) {
    iox::IomObject obj(iox::IomName("Test"));
    auto& attr = obj.setAttribute(iox::IomName("values"));
    attr.values.push_back(iox::IomValue::integer(1));
    attr.values.push_back(iox::IomValue::integer(2));
    attr.values.push_back(iox::IomValue::integer(3));

    IOX_CHECK_EQ(static_cast<std::size_t>(3), attr.values.size());
    IOX_CHECK_EQ(1, std::get<iox::IomValue>(attr.values[0]).asInteger());
    IOX_CHECK_EQ(2, std::get<iox::IomValue>(attr.values[1]).asInteger());
    IOX_CHECK_EQ(3, std::get<iox::IomValue>(attr.values[2]).asInteger());
}

IOX_TEST(iom_object_deep_copy) {
    iox::IomObject parent(iox::IomName("Parent"));
    iox::IomObject child(iox::IomName("Child"));
    child.setPrimitive("val", iox::IomValue::text("inner"));
    parent.setStructure("child", child);

    auto copy = parent.deepCopy();

    // Modify the copy's child
    auto copiedChild = copy.getStructure("child");
    copiedChild.setPrimitive("val", iox::IomValue::text("modified"));

    // Original child must be unchanged
    auto origChild = parent.getStructure("child");
    auto ov = origChild.getPrimitive("val");
    IOX_CHECK(ov.has_value());
    IOX_CHECK_EQ(std::string("inner"), ov->asText());
}

IOX_TEST(iom_object_remove_attribute) {
    iox::IomObject obj(iox::IomName("Test"));
    obj.setPrimitive("a", iox::IomValue::integer(1));
    obj.setPrimitive("b", iox::IomValue::integer(2));

    IOX_CHECK_EQ(static_cast<std::size_t>(2), obj.attributeCount());

    IOX_CHECK(obj.removeAttribute("a"));
    IOX_CHECK_EQ(static_cast<std::size_t>(1), obj.attributeCount());
    IOX_CHECK(obj.findAttribute("a") == nullptr);

    IOX_CHECK(!obj.removeAttribute("nonexistent"));
}

IOX_TEST(iom_object_reference_metadata) {
    iox::IomObject obj(iox::IomName("Test"));

    IOX_CHECK(!obj.ref().has_value());
    IOX_CHECK(!obj.bid().has_value());
    IOX_CHECK(!obj.orderPos().has_value());

    obj.setRef("REF1");
    obj.setBid("BID42");
    obj.setOrderPos(5);

    IOX_CHECK(obj.ref().has_value());
    IOX_CHECK_EQ(std::string("REF1"), *obj.ref());
    IOX_CHECK_EQ(std::string("BID42"), *obj.bid());
    IOX_CHECK_EQ(5, *obj.orderPos());
}

#include "iox/test/TestMain.h"
