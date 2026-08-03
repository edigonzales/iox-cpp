#pragma once

#include "iox/Events.h"
#include "iox/xtf/XtfReaderOptions.h"
#include "xml/ExpatParser.h"

#include <functional>
#include <memory>

namespace iox {
namespace xtf {

/// Private, model-free XTF 2.3 wire dialect.
class Xtf23Dialect final {
public:
    using EventHandler = std::function<void(IoxEvent)>;
    using DiagnosticHandler = std::function<void(Diagnostic)>;

    Xtf23Dialect(XtfReaderOptions options,
                 EventHandler emitEvent,
                 DiagnosticHandler addDiagnostic);
    ~Xtf23Dialect();

    Xtf23Dialect(const Xtf23Dialect&) = delete;
    Xtf23Dialect& operator=(const Xtf23Dialect&) = delete;

    void onStartElement(const xml::XmlStartElement& element);
    void onEndElement(const xml::XmlEndElement& element);
    void onText(std::string_view data, const SourceLocation& location);
    void onRootClosed(const SourceLocation& location);
    bool finished() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace xtf
} // namespace iox
