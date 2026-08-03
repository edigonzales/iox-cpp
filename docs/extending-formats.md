# Extending iox-cpp with Custom Formats

`iox-cpp` supports additional transfer formats through explicit C++ interfaces
and a testable `FormatRegistry`. This document explains how to add a new format.

## Architecture

New formats implement the `Reader` and/or `Writer` interfaces and register
with the `FormatRegistry`. There is no dynamic plugin loading — all formats
are linked statically.

## Step 1: Implement a Reader

```cpp
#include "iox/Reader.h"
#include "iox/FormatRegistry.h"

class MyFormatReader final : public iox::Reader {
public:
    ReadOutcome next() override {
        // Produce events from your format
    }
    void feed(ByteView data) override {
        // Accept input chunks
    }
    void finish() override {
        // Signal end of input
    }
    bool isFinished() const noexcept override {
        return finished_;
    }
    std::vector<Diagnostic> takeDiagnostics() override {
        return std::move(diagnostics_);
    }
private:
    bool finished_ = false;
    std::vector<Diagnostic> diagnostics_;
};
```

## Step 2: Implement a Writer

```cpp
#include "iox/Writer.h"

class MyFormatWriter final : public iox::Writer {
public:
    explicit MyFormatWriter(std::shared_ptr<OutputSink> output)
        : sink_(std::move(output)) {}

    void write(const IoxEvent& event) override {
        // Serialize event to your format
    }
    void flush() override { if (sink_) sink_->flush(); }
    void close() override { closed_ = true; if (sink_) sink_->close(); }
    bool isClosed() const noexcept override { return closed_; }
    std::vector<Diagnostic> takeDiagnostics() override {
        return std::move(diagnostics_);
    }
private:
    std::shared_ptr<OutputSink> sink_;
    bool closed_ = false;
    std::vector<Diagnostic> diagnostics_;
};
```

## Step 3: Register the Format

```cpp
#include "iox/FormatRegistry.h"

void registerMyFormat(iox::FormatRegistry& registry) {
    iox::FormatEntry entry;
    entry.name = "my-format";
    entry.description = "My custom transfer format";
    entry.extensions = {".myf", ".mft"};

    // Optional: a score from 0 (no match) to 100 (certain match).
    entry.scoreSniffer = [](iox::ByteView firstChunk) -> int {
        if (firstChunk.size() >= 4 &&
            firstChunk[0] == 'M' && firstChunk[1] == 'Y' &&
            firstChunk[2] == 'F' && firstChunk[3] == 'M') {
            return 100;
        }
        return 0;
    };

    // Factory functions
    entry.readerFactory = []() -> std::unique_ptr<iox::Reader> {
        return std::make_unique<MyFormatReader>();
    };
    entry.writerFactory = [](std::shared_ptr<iox::OutputSink> sink)
        -> std::unique_ptr<iox::Writer> {
        return std::make_unique<MyFormatWriter>(std::move(sink));
    };

    registry.addFormat(std::move(entry));
}
```

## Step 4: Use the Format

```cpp
iox::FormatRegistry registry;
registerMyFormat(registry);

// Create reader by name
auto reader = registry.createReader("my-format");

// Create reader by sniffing content
auto reader2 = registry.createReaderBySniffing(someBytes, ".myf");

// Create writer
auto writer = registry.createWriter("my-format",
    std::make_shared<iox::StringOutputSink>());
```

## Step 5: Test the Format

Format implementations should be tested like the built-in formats:

```cpp
IOX_TEST(my_format_roundtrip) {
    // Write events
    auto sink = std::make_shared<iox::StringOutputSink>();
    MyFormatWriter writer(sink);
    writer.write(startTransfer);
    writer.write(endTransfer);
    writer.close();

    // Read back
    MyFormatReader reader;
    reader.feed(iox::ByteView(sink->str()));
    reader.finish();

    auto outcome = reader.next();
    IOX_CHECK(outcome.event.has_value());
}
```

## Design Rules

1. **No global state.** Registries are explicit objects, not global singletons.
2. **No dynamic loading.** All formats are linked at compile time.
3. **No model dependency in the core.** Formats in `iox-core` must not depend on
   `ilic-core`, XML, or XTF.
4. **Deterministic output.** Writers must produce identical output for identical
   input events and options.
5. **Error reporting.** Use `Diagnostic` for non-fatal issues and throw
   `IoxError` for fatal parser, state, resource, and I/O failures. C boundaries
   translate exceptions into stable status codes and result JSON.
6. **No silent data loss.** Unknown or unexpected content must be preserved
   or explicitly diagnosed.

Content sniffing has priority over a conflicting extension. If two sniffers
return the same score, registration order is used as the deterministic
tie-breaker. The compatibility `sniffer` field is still accepted for small
adapters; new formats should use `scoreSniffer`.

## Reference: JsonEventReader/JsonEventWriter

See `source/json/JsonEventReader.cpp` and `source/json/JsonEventWriter.cpp`
for a complete example of a custom format implementation.
