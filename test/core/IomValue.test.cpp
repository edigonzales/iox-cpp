#include "iox/IomValue.h"
#include "iox/test/Test.h"

IOX_TEST(iomvalue_null) {
    auto v = iox::IomValue::null();
    IOX_CHECK(v.isNull());
    IOX_CHECK_EQ(iox::IomValue::Kind::Null, v.kind());
}

IOX_TEST(iomvalue_text) {
    auto v = iox::IomValue::text("hello");
    IOX_CHECK(!v.isNull());
    IOX_CHECK_EQ(iox::IomValue::Kind::Text, v.kind());
    IOX_CHECK_EQ(std::string("hello"), v.asText());
    IOX_CHECK_EQ(std::string("hello"), v.toTransferString());
}

IOX_TEST(iomvalue_integer) {
    auto v = iox::IomValue::integer(42);
    IOX_CHECK_EQ(iox::IomValue::Kind::Integer, v.kind());
    IOX_CHECK_EQ(42, v.asInteger());
}

IOX_TEST(iomvalue_decimal) {
    auto v = iox::IomValue::decimal(3.14);
    IOX_CHECK_EQ(iox::IomValue::Kind::Decimal, v.kind());
    IOX_CHECK(v.asDecimal() > 3.13 && v.asDecimal() < 3.15);
}

IOX_TEST(iomvalue_boolean) {
    auto v = iox::IomValue::boolean(true);
    IOX_CHECK_EQ(iox::IomValue::Kind::Boolean, v.kind());
    IOX_CHECK(v.asBoolean());
}

IOX_TEST(iomvalue_equality) {
    auto v1 = iox::IomValue::integer(100);
    auto v2 = iox::IomValue::integer(100);
    auto v3 = iox::IomValue::integer(200);

    IOX_CHECK(v1 == v2);
    IOX_CHECK(v1 != v3);
}

#include "iox/test/TestMain.h"
