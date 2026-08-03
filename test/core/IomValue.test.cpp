#include "iox/IomObject.h"
#include "iox/IomValue.h"
#include "iox/test/Test.h"

IOX_TEST(iomvalue_preserves_lexical_primitive) {
    const auto value = iox::IomValue::primitive("001.2300");
    IOX_CHECK_EQ(iox::IomValue::Kind::Primitive, value.kind());
    IOX_CHECK(value.isPrimitive());
    IOX_CHECK(!value.isObject());
    IOX_CHECK_EQ(std::string("001.2300"), value.primitive());
}

IOX_TEST(iomvalue_holds_nested_object) {
    iox::IomObject child(iox::IomName("Model.Topic.Child"), "oid-1");
    child.setPrimitive(iox::IomName("text"), u8"Grüezi");
    const auto value = iox::IomValue::object(child);
    IOX_CHECK_EQ(iox::IomValue::Kind::Object, value.kind());
    IOX_CHECK(value.isObject());
    IOX_CHECK(value.object().semanticallyEquals(child));
}

IOX_TEST(iomvalue_copy_is_value_semantic) {
    iox::IomValue first = iox::IomValue::object(
        iox::IomObject(iox::IomName("Child")));
    auto second = first;
    second.object().setPrimitive(iox::IomName("value"), "changed");
    IOX_CHECK(!first.object().hasAttribute("value"));
    IOX_CHECK_EQ(std::string_view("changed"),
                 *second.object().primitive("value"));

    IOX_CHECK(iox::IomValue::primitive("true") ==
              iox::IomValue::primitive("true"));
    IOX_CHECK(iox::IomValue::primitive("true") !=
              iox::IomValue::primitive("TRUE"));
}

IOX_TEST(iomvalue_wrong_accessor_throws) {
    bool primitiveThrew = false;
    try {
        (void)iox::IomValue::object(iox::IomObject{}).primitive();
    } catch (const iox::IoxError& error) {
        primitiveThrew = error.code() == iox::DiagnosticCode::InvalidState;
    }
    IOX_CHECK(primitiveThrew);

    bool objectThrew = false;
    try {
        (void)iox::IomValue::primitive("x").object();
    } catch (const iox::IoxError& error) {
        objectThrew = error.code() == iox::DiagnosticCode::InvalidState;
    }
    IOX_CHECK(objectThrew);
}

#include "iox/test/TestMain.h"
