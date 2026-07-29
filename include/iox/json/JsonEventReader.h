#pragma once

#include "iox/Reader.h"

#include <memory>
#include <string>
#include <vector>

namespace iox {
namespace json {

/// Reads newline-delimited JSON (NDJSON) event streams.
///
/// Each line is a JSON object representing one IoxEvent.
/// See docs/event-json-schema.md for the schema.
class JsonEventReader final : public Reader {
public:
    JsonEventReader();
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
