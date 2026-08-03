#include "iox/xtf/XtfWriter.h"

#include "xtf/v23/Xtf23Writer.h"
#include "xtf/v24/Xtf24Writer.h"
#include "xml/XmlWriter.h"

#include <type_traits>
#include <utility>

namespace iox {
namespace xtf {
namespace {

enum class WriterState {
    BeforeTransfer,
    InTransfer,
    InBasket,
    AfterBasket,
    AfterTransfer,
    Closed,
    Failed
};

} // namespace

struct XtfWriter::Impl final {
    XtfWriterOptions options;
    std::shared_ptr<OutputSink> sink;
    std::unique_ptr<xml::XmlWriter> xml;
    std::unique_ptr<Xtf23Writer> writer23;
    std::unique_ptr<Xtf24Writer> writer24;
    WriterState state = WriterState::BeforeTransfer;
    std::vector<Diagnostic> diagnostics;

    void validate(EventKind kind) {
        const bool valid =
            (state == WriterState::BeforeTransfer &&
             kind == EventKind::StartTransfer) ||
            ((state == WriterState::InTransfer ||
              state == WriterState::AfterBasket) &&
             (kind == EventKind::StartBasket ||
              kind == EventKind::EndTransfer)) ||
            (state == WriterState::InBasket &&
             (kind == EventKind::Object || kind == EventKind::EndBasket));
        if (!valid) {
            state = WriterState::Failed;
            throw IoxError(DiagnosticCode::WriterStateError,
                           "Invalid event order in XTF writer");
        }
    }

    void advance(EventKind kind) noexcept {
        if (kind == EventKind::StartTransfer) {
            state = WriterState::InTransfer;
        } else if (kind == EventKind::StartBasket) {
            state = WriterState::InBasket;
        } else if (kind == EventKind::EndBasket) {
            state = WriterState::AfterBasket;
        } else if (kind == EventKind::EndTransfer) {
            state = WriterState::AfterTransfer;
        }
    }
};

XtfWriter::XtfWriter(std::shared_ptr<OutputSink> output,
                     XtfWriterOptions options)
    : impl_(std::make_unique<Impl>()) {
    if (!output) {
        throw IoxError(DiagnosticCode::InvalidArgument,
                       "XtfWriter requires an output sink");
    }
    impl_->options = std::move(options);
    impl_->sink = std::move(output);
    xml::XmlWriterOptions xmlOptions;
    xmlOptions.pretty = impl_->options.pretty;
    impl_->xml = std::make_unique<xml::XmlWriter>(impl_->sink,
                                                  std::move(xmlOptions));
    const auto addDiagnostic = [this](Diagnostic diagnostic) {
        impl_->diagnostics.push_back(std::move(diagnostic));
    };
    if (impl_->options.version == XtfVersion::V23) {
        impl_->writer23 = std::make_unique<Xtf23Writer>(
            *impl_->xml, impl_->options, addDiagnostic);
    } else {
        impl_->writer24 = std::make_unique<Xtf24Writer>(
            *impl_->xml, impl_->options, addDiagnostic);
    }
}

XtfWriter::~XtfWriter() = default;

void XtfWriter::write(const IoxEvent& event) {
    if (impl_->state == WriterState::Closed ||
        impl_->state == WriterState::Failed) {
        throw IoxError(DiagnosticCode::WriterStateError,
                       "XTF writer is closed or failed");
    }
    const auto kind = eventKind(event);
    impl_->validate(kind);
    try {
        std::visit(
            [this](const auto& value) {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<T, StartTransferEvent>) {
                    if (impl_->writer23) {
                        impl_->writer23->writeStartTransfer(value);
                    } else {
                        impl_->writer24->writeStartTransfer(value);
                    }
                } else if constexpr (std::is_same_v<T, StartBasketEvent>) {
                    if (impl_->writer23) {
                        impl_->writer23->writeStartBasket(value);
                    } else {
                        impl_->writer24->writeStartBasket(value);
                    }
                } else if constexpr (std::is_same_v<T, ObjectEvent>) {
                    if (impl_->writer23) {
                        impl_->writer23->writeObject(value);
                    } else {
                        impl_->writer24->writeObject(value);
                    }
                } else if constexpr (std::is_same_v<T, EndBasketEvent>) {
                    if (impl_->writer23) {
                        impl_->writer23->writeEndBasket(value);
                    } else {
                        impl_->writer24->writeEndBasket(value);
                    }
                } else {
                    if (impl_->writer23) {
                        impl_->writer23->writeEndTransfer(value);
                    } else {
                        impl_->writer24->writeEndTransfer(value);
                    }
                }
            },
            event);
        impl_->advance(kind);
    } catch (...) {
        impl_->state = WriterState::Failed;
        throw;
    }
}

void XtfWriter::flush() {
    if (impl_->state == WriterState::Failed ||
        impl_->state == WriterState::Closed ||
        impl_->state == WriterState::BeforeTransfer) {
        throw IoxError(DiagnosticCode::WriterStateError,
                       "XTF writer cannot be flushed in its current state");
    }
    try {
        impl_->xml->flush();
    } catch (...) {
        impl_->state = WriterState::Failed;
        throw;
    }
}

void XtfWriter::close() {
    if (impl_->state == WriterState::Closed) return;
    if (impl_->state == WriterState::Failed) {
        throw IoxError(DiagnosticCode::WriterStateError,
                       "Cannot close a failed XTF writer");
    }
    if (impl_->state != WriterState::AfterTransfer) {
        impl_->state = WriterState::Failed;
        throw IoxError(DiagnosticCode::WriterStateError,
                       "XTF writer closed before EndTransferEvent");
    }
    try {
        impl_->xml->endDocument();
        impl_->sink->close();
        impl_->state = WriterState::Closed;
    } catch (const IoxError&) {
        impl_->state = WriterState::Failed;
        throw;
    } catch (const std::exception& exception) {
        impl_->state = WriterState::Failed;
        throw IoxError(DiagnosticCode::IoError,
                       std::string("XTF output close failed: ") +
                           exception.what());
    } catch (...) {
        impl_->state = WriterState::Failed;
        throw IoxError(DiagnosticCode::IoError,
                       "XTF output close failed with an unknown exception");
    }
}

bool XtfWriter::isClosed() const noexcept {
    return impl_->state == WriterState::Closed;
}

std::vector<Diagnostic> XtfWriter::takeDiagnostics() {
    auto result = std::move(impl_->diagnostics);
    impl_->diagnostics.clear();
    return result;
}

} // namespace xtf
} // namespace iox
