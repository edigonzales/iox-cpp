#include "iox/ByteView.h"
#include "iox/test/Test.h"

IOX_TEST(byteview_default_empty) {
    iox::ByteView bv;
    IOX_CHECK(bv.empty());
    IOX_CHECK_EQ(static_cast<std::size_t>(0), bv.size());
    IOX_CHECK(bv.data() == nullptr);
}

IOX_TEST(byteview_from_string_view) {
    std::string_view sv = "hello";
    iox::ByteView bv(sv);
    IOX_CHECK(!bv.empty());
    IOX_CHECK_EQ(static_cast<std::size_t>(5), bv.size());
    IOX_CHECK_EQ(sv, bv.sv());
}

IOX_TEST(byteview_equality) {
    iox::ByteView a("abc", 3);
    iox::ByteView b("abc", 3);
    iox::ByteView c("abd", 3);

    IOX_CHECK(a == b);
    IOX_CHECK(a != c);
}

IOX_TEST(byteview_subspan) {
    iox::ByteView bv("0123456789", 10);
    auto sub = bv.subspan(3, 4);
    IOX_CHECK_EQ(static_cast<std::size_t>(4), sub.size());
    IOX_CHECK_EQ(std::string_view("3456"), sub.sv());
}

IOX_TEST(byteview_iteration) {
    iox::ByteView bv("abc", 3);
    std::string result(bv.begin(), bv.end());
    IOX_CHECK_EQ(std::string("abc"), result);
}

#include "iox/test/TestMain.h"
