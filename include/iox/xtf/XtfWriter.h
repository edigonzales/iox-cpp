#pragma once

#include "iox/Writer.h"
#include "iox/xtf/XtfReaderOptions.h"

#include <memory>

namespace iox {
namespace xtf {

/// XTF writer — writes INTERLIS 2.3 and 2.4 transfer files.
class XtfWriter final : public Writer {
public:
    XtfWriter(std::shared_ptr<OutputSink> output,
              XtfWriterOptions options = {});
    ~XtfWriter() override;

    void write(const IoxEvent& event) override;
    void flush() override;
    void close() override;
    bool isClosed() const noexcept override;
    std::vector<Diagnostic> takeDiagnostics() override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace xtf
} // namespace iox
