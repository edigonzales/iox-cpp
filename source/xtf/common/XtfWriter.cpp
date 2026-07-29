#include "iox/xtf/XtfWriter.h"
#include "iox/xml/XmlWriter.h"

#include <string>
#include <vector>
#include <utility>
#include <stdexcept>

namespace iox {
namespace xtf {

// ============================================================================
// Writer event state machine
// ============================================================================

enum class WriterState {
    BeforeTransfer,
    InTransfer,
    InBasket,
    AfterBasket,
    AfterTransfer,
    Closed
};

// ============================================================================
// XtfWriter::Impl
// ============================================================================

struct XtfWriter::Impl {
    XtfWriterOptions options;
    std::shared_ptr<OutputSink> sink;
    std::unique_ptr<xml::XmlWriter> xml;
    WriterState state = WriterState::BeforeTransfer;
    bool closed_ = false;
    std::vector<Diagnostic> diagnostics;

    void validateEvent(const IoxEvent& event);
    void writeStartTransfer(const StartTransferEvent& e);
    void writeStartBasket(const StartBasketEvent& e);
    void writeObject(const ObjectEvent& e);
    void writeEndBasket(const EndBasketEvent& e);
    void writeEndTransfer(const EndTransferEvent& e);
};

void XtfWriter::Impl::validateEvent(const IoxEvent& event) {
    // Check event ordering
    std::visit([this](const auto& e) {
        using T = std::decay_t<decltype(e)>;

        if constexpr (std::is_same_v<T, StartTransferEvent>) {
            if (state != WriterState::BeforeTransfer) {
                diagnostics.push_back({Diagnostic::Severity::Error,
                    ErrorCode::XtfStateViolation,
                    "StartTransferEvent must be first"});
            }
        } else if constexpr (std::is_same_v<T, StartBasketEvent>) {
            if (state != WriterState::InTransfer &&
                state != WriterState::AfterBasket) {
                diagnostics.push_back({Diagnostic::Severity::Error,
                    ErrorCode::XtfStateViolation,
                    "StartBasketEvent must follow StartTransfer or EndBasket"});
            }
        } else if constexpr (std::is_same_v<T, ObjectEvent>) {
            if (state != WriterState::InBasket) {
                diagnostics.push_back({Diagnostic::Severity::Error,
                    ErrorCode::XtfStateViolation,
                    "ObjectEvent must be inside a basket"});
            }
        } else if constexpr (std::is_same_v<T, EndBasketEvent>) {
            if (state != WriterState::InBasket) {
                diagnostics.push_back({Diagnostic::Severity::Error,
                    ErrorCode::XtfStateViolation,
                    "EndBasketEvent must close an open basket"});
            }
        } else if constexpr (std::is_same_v<T, EndTransferEvent>) {
            if (state != WriterState::AfterBasket &&
                state != WriterState::InTransfer) {
                diagnostics.push_back({Diagnostic::Severity::Error,
                    ErrorCode::XtfStateViolation,
                    "EndTransferEvent must be last"});
            }
        }
    }, event);
}

void XtfWriter::Impl::writeStartTransfer(const StartTransferEvent& e) {
    state = WriterState::InTransfer;

    xml->writeDeclaration();

    std::vector<std::pair<std::string, std::string>> attrs;

    if (options.version == XtfVersion::Xtf24) {
        attrs.push_back({"xmlns:ili", "http://www.interlis.ch/INTERLIS2.4"});
        attrs.push_back({"xmlns:geom", "http://www.interlis.ch/GEOMETRY"});
        attrs.push_back({"xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance"});
        xml->writeStartElement("ili:TRANSFER", attrs);
    } else {
        attrs.push_back({"xmlns:ili", "http://www.interlis.ch/INTERLIS2.3"});
        xml->writeStartElement("ili:TRANSFER", attrs);
    }

    // Write header elements
    auto writeHeaderElement = [this](const char* tag, const std::string& value) {
        if (value.empty()) return;
        xml->writeStartElement(tag);
        xml->writeText(value);
        xml->writeEndElement(tag);
    };

    if (options.version == XtfVersion::Xtf24) {
        writeHeaderElement("ili:HEADERSECTION", "");
        // Actually XTF 2.4 uses ili:HeaderSection
        xml->writeStartElement("ili:HeaderSection");
        writeHeaderElement("ili:Sender", e.sender.empty() ? options.sender : e.sender);
        writeHeaderElement("ili:Comment", e.comment.empty() ? options.comment : e.comment);
        writeHeaderElement("ili:Version", e.iliVersion);
        writeHeaderElement("ili:Software", options.software);
        writeHeaderElement("ili:Date", e.date);
        xml->writeEndElement("ili:HeaderSection");
    } else {
        // XTF 2.3 header
        xml->writeStartElement("ili:HEADERSECTION");
        writeHeaderElement("ili:SENDER", e.sender.empty() ? options.sender : e.sender);
        writeHeaderElement("ili:COMMENT", e.comment.empty() ? options.comment : e.comment);
        writeHeaderElement("ili:VERSION", e.iliVersion);
        writeHeaderElement("ili:SOFTWARE", options.software);
        writeHeaderElement("ili:DATE", e.date);
        xml->writeEndElement("ili:HEADERSECTION");
    }
}

void XtfWriter::Impl::writeStartBasket(const StartBasketEvent& e) {
    state = WriterState::InBasket;

    std::vector<std::pair<std::string, std::string>> attrs;
    attrs.push_back({"BID", e.bid});
    if (e.consistency == "incomplete") {
        attrs.push_back({"CONSISTENCY", "incomplete"});
    }
    if (!e.operation.empty() && e.operation != "insert") {
        attrs.push_back({"OPERATION", e.operation});
    }

    xml->writeStartElement("ili:BASKET", attrs);
}

void XtfWriter::Impl::writeObject(const ObjectEvent& e) {
    state = WriterState::InBasket;

    std::vector<std::pair<std::string, std::string>> attrs;
    attrs.push_back({"TID", e.objectId});
    if (!e.operation.empty() && e.operation != "insert") {
        attrs.push_back({"OPERATION", e.operation});
    }
    if (e.refBid) {
        attrs.push_back({"REF", *e.refBid});
    }
    if (e.object.bid()) {
        attrs.push_back({"BID", *e.object.bid()});
    }

    auto tagName = e.object.tag().iliName();
    if (tagName.empty()) tagName = "DataObject";

    xml->writeStartElement(tagName, attrs);

    // Helper: recursively write an IomObject's attributes as XTF elements
    std::function<void(const IomObject&)> writeIomAsXml;
    writeIomAsXml = [this, &writeIomAsXml](const IomObject& obj) {
        for (std::size_t i = 0; i < obj.attributeCount(); ++i) {
            const auto& attr = obj.attributeAt(i);
            auto attrName = attr.name.iliName();
            if (attrName.empty()) continue;

            // REF attribute metadata
            std::vector<std::pair<std::string, std::string>> refAttrs;
            if (attr.ref) refAttrs.push_back({"REF", *attr.ref});
            if (attr.bid) refAttrs.push_back({"BID", *attr.bid});
            if (attr.orderPos) refAttrs.push_back({"ORDER_POS", std::to_string(*attr.orderPos)});

            if (attr.values.empty()) {
                xml->writeStartElement(attrName, refAttrs, true);
            } else if (attr.values.size() == 1) {
                const auto& val = attr.values[0];
                if (auto* prim = std::get_if<IomValue>(&val)) {
                    xml->writeStartElement(attrName, refAttrs);
                    xml->writeText(prim->toTransferString());
                    xml->writeEndElement(attrName);
                } else if (auto* sub = std::get_if<IomObject>(&val)) {
                    // Write the attribute element, then the sub-object
                    xml->writeStartElement(attrName, refAttrs);
                    // Use the sub-object's tag as the wrapper element name
                    auto subTag = sub->tag().iliName();
                    if (!subTag.empty()) {
                        xml->writeStartElement(subTag);
                        writeIomAsXml(*sub);
                        xml->writeEndElement(subTag);
                    } else {
                        writeIomAsXml(*sub);
                    }
                    xml->writeEndElement(attrName);
                }
            } else {
                // Multiple values: write each with the same element name
                for (const auto& val : attr.values) {
                    if (auto* prim = std::get_if<IomValue>(&val)) {
                        xml->writeStartElement(attrName, refAttrs);
                        xml->writeText(prim->toTransferString());
                        xml->writeEndElement(attrName);
                    } else if (auto* sub = std::get_if<IomObject>(&val)) {
                        xml->writeStartElement(attrName, refAttrs);
                        auto subTag = sub->tag().iliName();
                        if (!subTag.empty()) {
                            xml->writeStartElement(subTag);
                            writeIomAsXml(*sub);
                            xml->writeEndElement(subTag);
                        } else {
                            writeIomAsXml(*sub);
                        }
                        xml->writeEndElement(attrName);
                    }
                }
            }
        }
    };

    writeIomAsXml(e.object);
    xml->writeEndElement(tagName);
}

void XtfWriter::Impl::writeEndBasket(const EndBasketEvent& e) {
    (void)e;
    state = WriterState::AfterBasket;
    xml->writeEndElement("ili:BASKET");
}

void XtfWriter::Impl::writeEndTransfer(const EndTransferEvent& e) {
    (void)e;
    state = WriterState::AfterTransfer;
    xml->writeEndElement(options.version == XtfVersion::Xtf24
                         ? "ili:TRANSFER" : "ili:TRANSFER");
}

// ============================================================================
// XtfWriter — public API
// ============================================================================

XtfWriter::XtfWriter(std::shared_ptr<OutputSink> output,
                     XtfWriterOptions options)
    : impl_(std::make_unique<Impl>()) {
    impl_->options = std::move(options);
    impl_->sink = std::move(output);

    auto writeFunc = [this](const void* data, std::size_t size) {
        impl_->sink->write(data, size);
    };

    impl_->xml = std::make_unique<xml::XmlWriter>(
        std::move(writeFunc),
        impl_->options.pretty,
        2);
}

XtfWriter::~XtfWriter() {
    if (!impl_->closed_) {
        close();
    }
}

void XtfWriter::write(const IoxEvent& event) {
    if (impl_->closed_) return;

    impl_->validateEvent(event);

    std::visit([this](const auto& e) {
        using T = std::decay_t<decltype(e)>;
        if constexpr (std::is_same_v<T, StartTransferEvent>) {
            impl_->writeStartTransfer(e);
        } else if constexpr (std::is_same_v<T, StartBasketEvent>) {
            impl_->writeStartBasket(e);
        } else if constexpr (std::is_same_v<T, ObjectEvent>) {
            impl_->writeObject(e);
        } else if constexpr (std::is_same_v<T, EndBasketEvent>) {
            impl_->writeEndBasket(e);
        } else if constexpr (std::is_same_v<T, EndTransferEvent>) {
            impl_->writeEndTransfer(e);
        }
    }, event);
}

void XtfWriter::flush() {
    if (impl_->xml) impl_->xml->flush();
    if (impl_->sink) impl_->sink->flush();
}

void XtfWriter::close() {
    if (!impl_->closed_) {
        impl_->closed_ = true;
        if (impl_->sink) impl_->sink->close();
    }
}

bool XtfWriter::isClosed() const noexcept {
    return impl_->closed_;
}

std::vector<Diagnostic> XtfWriter::takeDiagnostics() {
    auto diags = std::move(impl_->diagnostics);
    impl_->diagnostics.clear();
    return diags;
}

} // namespace xtf
} // namespace iox
