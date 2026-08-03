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

class Xtf23Writer final {
public:
    using DiagnosticHandler = std::function<void(Diagnostic)>;

    Xtf23Writer(xml::XmlWriter& xmlWriter,
                XtfWriterOptions options,
                DiagnosticHandler addDiagnostic);
    ~Xtf23Writer();

    Xtf23Writer(const Xtf23Writer&) = delete;
    Xtf23Writer& operator=(const Xtf23Writer&) = delete;

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
