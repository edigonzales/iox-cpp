#include "iox/ByteView.h"
#include "iox/test/Test.h"

#include <cstdint>
#include <string>
#include <vector>

IOX_TEST(byteview_default_empty) {
    iox::ByteView view;
    IOX_CHECK(view.empty());
    IOX_CHECK_EQ(static_cast<std::size_t>(0), view.size());
    IOX_CHECK(view.data() == nullptr);

    const auto emptySubview = view.subview(0, 10);
    IOX_CHECK(emptySubview.empty());
    IOX_CHECK(emptySubview.data() == nullptr);
}

IOX_TEST(byteview_borrows_string_storage) {
    std::string text = "hello";
    iox::ByteView view(text);
    IOX_CHECK(!view.empty());
    IOX_CHECK_EQ(text.size(), view.size());
    IOX_CHECK(view.data() ==
              reinterpret_cast<const std::uint8_t*>(text.data()));
    IOX_CHECK_EQ(static_cast<std::uint8_t>('h'), view.data()[0]);
}

IOX_TEST(byteview_borrows_vector_storage) {
    const std::vector<std::uint8_t> bytes{0U, 1U, 255U};
    const iox::ByteView view(bytes);
    IOX_CHECK_EQ(bytes.size(), view.size());
    IOX_CHECK(view.data() == bytes.data());
}

IOX_TEST(byteview_subview_is_bounded) {
    const std::vector<std::uint8_t> bytes{'0', '1', '2', '3', '4'};
    const iox::ByteView view(bytes);
    const auto middle = view.subview(2, 2);
    IOX_CHECK_EQ(static_cast<std::size_t>(2), middle.size());
    IOX_CHECK_EQ(static_cast<std::uint8_t>('2'), middle.data()[0]);
    IOX_CHECK_EQ(static_cast<std::uint8_t>('3'), middle.data()[1]);

    const auto tail = view.subview(3, 100);
    IOX_CHECK_EQ(static_cast<std::size_t>(2), tail.size());

    bool threw = false;
    try {
        (void)view.subview(6, 1);
    } catch (const iox::IoxError& error) {
        threw = error.code() == iox::DiagnosticCode::InvalidArgument;
    }
    IOX_CHECK(threw);
}

#include "iox/test/TestMain.h"
