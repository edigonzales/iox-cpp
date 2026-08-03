#pragma once

#include "iox/Diagnostic.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace iox {

class ByteView final {
public:
    constexpr ByteView() noexcept = default;
    constexpr ByteView(const std::uint8_t* data, std::size_t size) noexcept
        : data_(data), size_(size) {}
    ByteView(const std::string& value) noexcept
        : data_(reinterpret_cast<const std::uint8_t*>(value.data())),
          size_(value.size()) {}
    ByteView(const std::vector<std::uint8_t>& value) noexcept
        : data_(value.data()), size_(value.size()) {}

    constexpr const std::uint8_t* data() const noexcept { return data_; }
    constexpr std::size_t size() const noexcept { return size_; }
    constexpr bool empty() const noexcept { return size_ == 0; }

    ByteView subview(std::size_t offset, std::size_t count) const {
        if (offset > size_) {
            throw IoxError(DiagnosticCode::InvalidArgument,
                           "ByteView offset is out of range");
        }
        const auto resultSize = std::min(count, size_ - offset);
        return ByteView(data_ == nullptr ? nullptr : data_ + offset,
                        resultSize);
    }

private:
    const std::uint8_t* data_ = nullptr;
    std::size_t size_ = 0;
};

} // namespace iox
