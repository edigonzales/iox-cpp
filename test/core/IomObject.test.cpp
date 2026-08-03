#include "iox/IomObject.h"
#include "iox/IomValue.h"
#include "iox/test/Test.h"

IOX_TEST(iom_object_default_and_metadata) {
    iox::IomObject object;
    IOX_CHECK(object.empty());
    IOX_CHECK(object.tag().interlisName().empty());

    object.setTag(iox::IomName(
        "Model.Topic.Class", {"urn:model", "Class", "m"}));
    object.setOid("oid-1");
    object.setOperation(iox::ObjectOperation::Update);
    object.setConsistency(iox::Consistency::Incomplete);
    object.setReference({"target", "basket", 7U});
    object.setSourceLocation({"input.xtf", 42U, 3U, 8U});

    IOX_CHECK_EQ(std::string("Model.Topic.Class"),
                 object.tag().interlisName());
    IOX_CHECK(object.tag().hasXmlName());
    IOX_CHECK_EQ(std::string("oid-1"), *object.oid());
    IOX_CHECK_EQ(iox::ObjectOperation::Update, object.operation());
    IOX_CHECK_EQ(iox::Consistency::Incomplete, object.consistency());
    IOX_CHECK(object.isReference());
    IOX_CHECK_EQ(std::string("target"), *object.reference().targetOid);
    IOX_CHECK_EQ(std::uint64_t{7}, *object.reference().orderPosition);
    IOX_CHECK_EQ(std::uint64_t{42}, object.sourceLocation().byteOffset);
}

IOX_TEST(iom_object_preserves_attribute_and_value_order) {
    iox::IomObject object(iox::IomName("Test"));
    object.setPrimitive(iox::IomName("c"), "001.2300");
    object.setPrimitive(iox::IomName("a"), "first");
    object.appendPrimitive(iox::IomName("a"), "second");
    object.setPrimitive(iox::IomName("b"), "last");

    IOX_CHECK_EQ(static_cast<std::size_t>(3), object.attributeCount());
    IOX_CHECK_EQ(std::string("c"), object.attributeName(0).interlisName());
    IOX_CHECK_EQ(std::string("a"), object.attributeName(1).interlisName());
    IOX_CHECK_EQ(std::string("b"), object.attributeName(2).interlisName());
    IOX_CHECK_EQ(static_cast<std::size_t>(2), object.valueCount("a"));
    IOX_CHECK_EQ(std::string_view("first"), *object.primitive("a", 0));
    IOX_CHECK_EQ(std::string_view("second"), *object.primitive("a", 1));
    IOX_CHECK_EQ(std::string_view("001.2300"), *object.primitive("c"));
}

IOX_TEST(iom_object_mutators_do_not_expose_internal_values) {
    iox::IomObject object(iox::IomName("Test"));
    object.appendPrimitive(iox::IomName("values"), "one");
    object.appendPrimitive(iox::IomName("values"), "three");
    object.insertValue(iox::IomName("values"), 1,
                       iox::IomValue::primitive("two"));
    object.replaceValue("values", 2, iox::IomValue::primitive("THREE"));
    IOX_CHECK_EQ(std::string_view("two"), *object.primitive("values", 1));
    IOX_CHECK_EQ(std::string_view("THREE"), *object.primitive("values", 2));

    object.eraseValue("values", 0);
    IOX_CHECK_EQ(static_cast<std::size_t>(2), object.valueCount("values"));
    object.eraseAttribute("values");
    IOX_CHECK(!object.hasAttribute("values"));

    object.setPrimitive(iox::IomName("a"), "a");
    object.setPrimitive(iox::IomName("b"), "b");
    object.clearAttributes();
    IOX_CHECK_EQ(static_cast<std::size_t>(0), object.attributeCount());
}

IOX_TEST(iom_object_cow_separates_nested_mutation) {
    iox::IomObject child(iox::IomName("Child"));
    child.setPrimitive(iox::IomName("value"), "original");
    iox::IomObject first(iox::IomName("Parent"));
    first.setObject(iox::IomName("child"), child);

    auto second = first;
    auto secondChild = *second.object("child");
    secondChild.setPrimitive(iox::IomName("value"), "changed");
    second.setObject(iox::IomName("child"), secondChild);

    IOX_CHECK_EQ(std::string_view("original"),
                 *first.object("child")->primitive("value"));
    IOX_CHECK_EQ(std::string_view("changed"),
                 *second.object("child")->primitive("value"));
}

IOX_TEST(iom_object_deep_copy_is_independent) {
    iox::IomObject parent(iox::IomName("Parent"));
    iox::IomObject child(iox::IomName("Child"));
    child.setPrimitive(iox::IomName("value"), "inner");
    parent.setObject(iox::IomName("child"), child);

    auto copy = parent.deepCopy();
    auto copiedChild = *copy.object("child");
    copiedChild.setPrimitive(iox::IomName("value"), "modified");
    copy.setObject(iox::IomName("child"), copiedChild);

    IOX_CHECK_EQ(std::string_view("inner"),
                 *parent.object("child")->primitive("value"));
    IOX_CHECK(!parent.semanticallyEquals(copy));
    IOX_CHECK(parent.semanticallyEquals(parent.deepCopy()));
}

IOX_TEST(iom_object_reports_invalid_indexes) {
    iox::IomObject object(iox::IomName("Test"));
    object.setPrimitive(iox::IomName("value"), "x");
    bool threw = false;
    try {
        object.eraseValue("value", 1);
    } catch (const iox::IoxError& error) {
        threw = error.code() == iox::DiagnosticCode::InvalidArgument;
    }
    IOX_CHECK(threw);
}

#include "iox/test/TestMain.h"
