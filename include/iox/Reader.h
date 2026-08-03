#pragma once

#include "iox/ByteView.h"
#include "iox/Diagnostic.h"
#include "iox/Events.h"

#include <optional>
#include <vector>

namespace iox {

enum class ReaderProgress { Event, NeedInput, End };

struct ReadOutcome final {
    ReaderProgress progress = ReaderProgress::NeedInput;
    std::optional<IoxEvent> event;
};

class Reader {
public:
    virtual ~Reader() = default;
    virtual ReadOutcome next() = 0;
    virtual void feed(ByteView chunk) = 0;
    virtual void finish() = 0;
    virtual bool isFinished() const noexcept = 0;
    virtual std::vector<Diagnostic> takeDiagnostics() = 0;
};

} // namespace iox
