#pragma once

#include "iox/ByteView.h"
#include "iox/Diagnostic.h"
#include "iox/Events.h"

#include <optional>
#include <vector>
#include <memory>

namespace iox {

/// Outcome of Reader::next().
struct ReadOutcome final {
    enum class Status {
        Event,       // an event is available
        NeedInput,   // more data needed; call feed() then next() again
        End          // no more events; transfer complete
    };

    Status status = Status::End;
    std::optional<IoxEvent> event;
    std::vector<Diagnostic> diagnostics;
};

/// Abstract synchronous pull-reader.
class Reader {
public:
    virtual ~Reader() = default;

    virtual ReadOutcome next() = 0;
    virtual void feed(ByteView data) = 0;
    virtual void finish() = 0;
    virtual bool isFinished() const noexcept = 0;
    virtual std::vector<Diagnostic> takeDiagnostics() = 0;
};

} // namespace iox
