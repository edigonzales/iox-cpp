#pragma once

#include "iox/Events.h"
#include "iox/xtf/XtfReaderOptions.h"
#include "xml/ExpatParser.h"

#include <functional>
#include <memory>
#include <vector>

namespace iox {
namespace xtf {

/// Private, model-free XTF 2.4 wire dialect.
class Xtf24Dialect final {
public:
    using EventHandler = std::function<void(IoxEvent)>;
    using DiagnosticHandler = std::function<void(Diagnostic)>;

    Xtf24Dialect(
        XtfReaderOptions options,
        std::vector<xml::XmlNamespaceDeclaration> rootNamespaces,
        EventHandler emitEvent,
        DiagnosticHandler addDiagnostic);
    ~Xtf24Dialect();

    Xtf24Dialect(const Xtf24Dialect&) = delete;
    Xtf24Dialect& operator=(const Xtf24Dialect&) = delete;

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
