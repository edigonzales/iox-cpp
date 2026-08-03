#pragma once

#include "iox/Events.h"
#include "iox/xtf/XtfReaderOptions.h"

#include <functional>
#include <memory>

namespace iox {
namespace xml {
class XmlWriter;
}
namespace xtf {

class Xtf24Writer final {
public:
    using DiagnosticHandler = std::function<void(Diagnostic)>;

    Xtf24Writer(xml::XmlWriter& xmlWriter,
                XtfWriterOptions options,
                DiagnosticHandler addDiagnostic);
    ~Xtf24Writer();

    Xtf24Writer(const Xtf24Writer&) = delete;
    Xtf24Writer& operator=(const Xtf24Writer&) = delete;

    void writeStartTransfer(const StartTransferEvent& event);
    void writeStartBasket(const StartBasketEvent& event);
    void writeObject(const ObjectEvent& event);
    void writeEndBasket(const EndBasketEvent& event);
    void writeEndTransfer(const EndTransferEvent& event);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace xtf
} // namespace iox
