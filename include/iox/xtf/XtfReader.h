#pragma once

#include "iox/Reader.h"
#include "iox/xtf/XtfReaderOptions.h"
#include "iox/xtf/XtfVersion.h"

#include <memory>
#include <optional>

namespace iox {
namespace xtf {

/// XTF reader — reads INTERLIS 2.3 and 2.4 transfer files.
///
/// This is the main entry point for reading XTF. It detects the version
/// automatically from the document root element and dispatches to the
/// appropriate version-specific dialect internally.
class XtfReader final : public Reader {
public:
    explicit XtfReader(XtfReaderOptions options = {});
    ~XtfReader() override;

    ReadOutcome next() override;
    void feed(ByteView data) override;
    void finish() override;
    bool isFinished() const noexcept override;
    std::vector<Diagnostic> takeDiagnostics() override;

    /// The detected XTF version, or Unknown if not yet determined.
    XtfVersion detectedVersion() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace xtf
} // namespace iox
