#pragma once

#include "iox/Writer.h"
#include "iox/Events.h"

#include <memory>
#include <string>
#include <vector>

namespace iox {
namespace json {

/// Writes IoxEvents as newline-delimited JSON (NDJSON).
///
/// Each event is serialized as a single JSON object line.
class JsonEventWriter final : public Writer {
public:
    explicit JsonEventWriter(std::shared_ptr<OutputSink> output);
    ~JsonEventWriter() override;

    void write(const IoxEvent& event) override;
    void flush() override;
    void close() override;
    bool isClosed() const noexcept override;
    std::vector<Diagnostic> takeDiagnostics() override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace json
} // namespace iox
