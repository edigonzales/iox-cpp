#pragma once

#include <cstddef>
#include <string_view>
#include <string>

namespace iox {

/// Non-owning view of contiguous bytes (UTF-8 or raw).
/// Lightweight, copyable, does not manage memory.
class ByteView final {
public:
    constexpr ByteView() noexcept = default;

    ByteView(const char* data, std::size_t size) noexcept
        : data_(data), size_(size) {}

    ByteView(const unsigned char* data, std::size_t size) noexcept
        : data_(reinterpret_cast<const char*>(data)), size_(size) {}

    ByteView(std::string_view sv) noexcept
        : data_(sv.data()), size_(sv.size()) {}

    ByteView(const std::string& s) noexcept
        : data_(s.data()), size_(s.size()) {}

    constexpr const char* data() const noexcept { return data_; }
    constexpr std::size_t size() const noexcept { return size_; }
    constexpr bool empty() const noexcept { return size_ == 0; }

    constexpr const char* begin() const noexcept { return data_; }
    constexpr const char* end() const noexcept { return data_ + size_; }

    std::string_view sv() const noexcept {
        return std::string_view(data_, size_);
    }

    std::string str() const {
        return std::string(data_, size_);
    }

    constexpr const char& operator[](std::size_t i) const noexcept {
        return data_[i];
    }

    ByteView subspan(std::size_t offset, std::size_t count) const noexcept {
        if (offset > size_) offset = size_;
        if (count > size_ - offset) count = size_ - offset;
        return ByteView(data_ + offset, count);
    }

    friend bool operator==(ByteView a, ByteView b) noexcept {
        return a.sv() == b.sv();
    }

    friend bool operator!=(ByteView a, ByteView b) noexcept {
        return !(a == b);
    }

private:
    const char* data_ = nullptr;
    std::size_t size_ = 0;
};

} // namespace iox
