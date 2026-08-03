#pragma once

#include "iox/Reader.h"

#include <memory>
#include <string>
#include <vector>

namespace iox {
namespace json {

struct JsonReaderOptions final {
    std::size_t maxLineBytes = 16U * 1024U * 1024U;
    std::string sourceName;
};

/// Reads newline-delimited JSON (NDJSON) event streams.
///
/// Each line is a JSON object representing one IoxEvent.
/// See docs/event-json-schema.md for the schema.
class JsonEventReader final : public Reader {
public:
    explicit JsonEventReader(JsonReaderOptions options = {});
    ~JsonEventReader() override;

    ReadOutcome next() override;
    void feed(ByteView data) override;
    void finish() override;
    bool isFinished() const noexcept override;
    std::vector<Diagnostic> takeDiagnostics() override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace json
} // namespace iox
