#pragma once

#include "iox/Events.h"
#include "iox/Diagnostic.h"

#include <cstddef>
#include <string>
#include <vector>
#include <memory>

namespace iox {

/// Abstract output destination for writers.
class OutputSink {
public:
    virtual ~OutputSink() = default;
    virtual std::size_t write(const void* data, std::size_t size) = 0;
    virtual void flush() {}
    virtual void close() {}
};

/// Sink that collects output into a std::string (memory).
class StringOutputSink final : public OutputSink {
public:
    std::size_t write(const void* data, std::size_t size) override {
        buffer_.append(static_cast<const char*>(data), size);
        return size;
    }
    const std::string& str() const noexcept { return buffer_; }
    std::string takeString() { return std::move(buffer_); }
private:
    std::string buffer_;
};

/// Sink that collects output into a std::vector<uint8_t>.
class VectorOutputSink final : public OutputSink {
public:
    std::size_t write(const void* data, std::size_t size) override {
        auto* p = static_cast<const std::uint8_t*>(data);
        buffer_.insert(buffer_.end(), p, p + size);
        return size;
    }
    const std::vector<std::uint8_t>& data() const noexcept { return buffer_; }
    std::vector<std::uint8_t> takeData() { return std::move(buffer_); }
private:
    std::vector<std::uint8_t> buffer_;
};

/// Sink that writes to a callback function.
class CallbackOutputSink final : public OutputSink {
public:
    using WriteFunc = std::size_t (*)(const void* data, std::size_t size, void* userData);
    CallbackOutputSink(WriteFunc func, void* userData)
        : func_(func), userData_(userData) {}
    std::size_t write(const void* data, std::size_t size) override {
        return func_ ? func_(data, size, userData_) : 0;
    }
private:
    WriteFunc func_;
    void* userData_;
};

/// Abstract synchronous push-writer.
class Writer {
public:
    virtual ~Writer() = default;

    virtual void write(const IoxEvent& event) = 0;
    virtual void flush() = 0;
    virtual void close() = 0;
    virtual bool isClosed() const noexcept = 0;
    virtual std::vector<Diagnostic> takeDiagnostics() = 0;
};

} // namespace iox
