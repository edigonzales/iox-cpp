#include "iox/xtf/XtfReader.h"
#include "iox/xtf/Xtf23Dialect.h"
#include "iox/xtf/Xtf24Dialect.h"
#include "xml/ExpatParser.h"

#include <string>
#include <vector>
#include <stack>
#include <utility>
#include <cstring>
#include <cctype>
#include <deque>

namespace iox {
namespace xtf {

namespace {
std::string lowerAscii(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (char c : value) {
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return result;
}

std::string encodedName(const XmlQualifiedName& name) {
    if (name.namespaceUri.empty()) return name.localName;
    return name.namespaceUri + '\xFF' + name.localName;
}
}

// ============================================================================
// XtfReader::Impl
// ============================================================================

enum class ParserPhase {
    BeforeRoot,
    InHeader,
    InContent,
    Fatal
};

struct XtfReader::Impl {
    XtfReaderOptions options;
    std::vector<Diagnostic> diagnostics;

    std::unique_ptr<xml::ExpatParser> xmlParser;
    std::unique_ptr<Xtf23Dialect> dialect23;
    std::unique_ptr<Xtf24Dialect> dialect24;
    bool finished_ = false;

    ParserPhase phase = ParserPhase::BeforeRoot;
    std::optional<XtfVersion> detected;
    bool rootClosed = false;

    // Event queue — filled by header parsing and dialect callbacks
    std::deque<IoxEvent> eventQueue;

    // Header field accumulation
    std::string sender, comment, iliVersion, software, date;
    std::stack<std::string> elementStack;
    std::string currentText;
    bool inSender = false, inComment = false, inIliVersion = false,
         inSoftware = false, inDate = false;

    void reset();
    void onStartElement(const xml::XmlStartElement& element);
    void onEndElement(const xml::XmlEndElement& element);
    void onCharacterData(std::string_view data, const SourceLocation& location);
    void emitStartTransfer();
    void createDialect();

    // Helper to get the active dialect
    void* activeDialect() {
        if (dialect23) return dialect23.get();
        if (dialect24) return dialect24.get();
        return nullptr;
    }
};

void XtfReader::Impl::reset() {
    sender.clear(); comment.clear(); iliVersion.clear();
    software.clear(); date.clear();
    inSender = inComment = inIliVersion = inSoftware = inDate = false;
    currentText.clear();
    while (!elementStack.empty()) elementStack.pop();
}

void XtfReader::Impl::createDialect() {
    if (!detected || *detected == XtfVersion::V23) {
        Xtf23Callbacks cb;
        cb.emitEvent = [this](IoxEvent event) {
            eventQueue.push_back(std::move(event));
        };
        cb.addDiagnostic = [this](Diagnostic d) {
            if (d.severity == DiagnosticSeverity::Fatal) {
                throw IoxError(d.code, std::move(d.message),
                               std::move(d.location));
            }
            diagnostics.push_back(std::move(d));
        };
        dialect23 = std::make_unique<Xtf23Dialect>(std::move(cb));
    } else {
        Xtf24Callbacks cb;
        cb.emitEvent = [this](IoxEvent event) {
            eventQueue.push_back(std::move(event));
        };
        cb.addDiagnostic = [this](Diagnostic d) {
            if (d.severity == DiagnosticSeverity::Fatal) {
                throw IoxError(d.code, std::move(d.message),
                               std::move(d.location));
            }
            diagnostics.push_back(std::move(d));
        };
        dialect24 = std::make_unique<Xtf24Dialect>(std::move(cb));
    }
}

void XtfReader::Impl::onStartElement(const xml::XmlStartElement& element)
{
    if (phase == ParserPhase::Fatal) return;
    const auto sname = encodedName(element.name);
    std::vector<std::pair<std::string, std::string>> attributeStorage;
    attributeStorage.reserve(element.attributes.size());
    for (const auto& attribute : element.attributes) {
        attributeStorage.push_back({encodedName(attribute.name), attribute.value});
    }
    std::vector<std::pair<std::string_view, std::string_view>> attrs;
    attrs.reserve(attributeStorage.size());
    for (const auto& attribute : attributeStorage) {
        attrs.push_back({attribute.first, attribute.second});
    }

    if (rootClosed) {
        phase = ParserPhase::Fatal;
        throw IoxError(DiagnosticCode::InvalidEventOrder,
                       "Element encountered after the TRANSFER root was closed",
                       element.location);
    }

    // Extract local name (after namespace separator)
    auto _sep = sname.find('\xFF');
    std::string local = sname;
    if (_sep != std::string::npos) local = sname.substr(_sep + 1);

    // --- Content phase: delegate to dialect ---
    if (phase == ParserPhase::InContent) {
        elementStack.push(sname);
        if (dialect23) dialect23->onStartElement(sname, attrs); else if (dialect24) dialect24->onStartElement(sname, attrs);
        return;
    }

    elementStack.push(sname);

    // --- Root detection ---
    if (phase == ParserPhase::BeforeRoot) {
        if (local == "TRANSFER" || local == "transfer") {
            const auto sepPos = sname.find('\xFF');
            const auto ns = sepPos == std::string::npos
                                ? std::string{}
                                : sname.substr(0, sepPos);
            if (ns == "http://www.interlis.ch/INTERLIS2.3") {
                detected = XtfVersion::V23;
            } else if (ns ==
                       "http://www.interlis.ch/xtf/2.4/INTERLIS") {
                detected = XtfVersion::V24;
            } else {
                phase = ParserPhase::Fatal;
                const auto code = ns.find("2.2") != std::string::npos
                    ? DiagnosticCode::UnsupportedXtfVersion
                    : DiagnosticCode::InvalidXtfNamespace;
                throw IoxError(code,
                    ns.find("2.2") != std::string::npos
                        ? "XTF 2.2 is not supported"
                        : "Invalid XTF namespace: " + ns,
                    element.location);
            }

            if (options.expectedVersion && *options.expectedVersion != *detected) {
                phase = ParserPhase::Fatal;
                throw IoxError(DiagnosticCode::UnsupportedXtfVersion,
                    std::string("Expected XTF ") +
                        toString(*options.expectedVersion) +
                        " but detected " + toString(*detected),
                    element.location);
            }

            phase = ParserPhase::InHeader;
            return;
        }

        phase = ParserPhase::Fatal;
        throw IoxError(DiagnosticCode::InvalidXtfNamespace,
                       "Unknown root element: " + sname,
                       element.location);
    }

    // --- Header phase ---
    if (phase == ParserPhase::InHeader) {
        const auto lowerLocal = lowerAscii(local);
        if (lowerLocal == "sender") {
            inSender = true; currentText.clear();
        } else if (lowerLocal == "comment") {
            inComment = true; currentText.clear();
        } else if (lowerLocal == "version") {
            inIliVersion = true; currentText.clear();
        } else if (lowerLocal == "software") {
            inSoftware = true; currentText.clear();
        } else if (lowerLocal == "date") {
            inDate = true; currentText.clear();
        }

        // Check for end of header section — first basket
        bool hasBasketId = false;
        bool hasObjectId = false;
        for (const auto& a : attrs) {
            std::string attrLocal(a.first);
            const auto attrSep = attrLocal.find('\xFF');
            if (attrSep != std::string::npos) attrLocal = attrLocal.substr(attrSep + 1);
            if (lowerAscii(attrLocal) == "bid") hasBasketId = true;
            if (lowerAscii(attrLocal) == "tid") hasObjectId = true;
        }
        if (lowerLocal == "basket" || (hasBasketId && !hasObjectId)) {
            // Emit StartTransfer, then switch to content
            emitStartTransfer();
            phase = ParserPhase::InContent;
            createDialect();
            // Re-dispatch this element to the dialect
            if (dialect23) dialect23->onStartElement(sname, attrs); else if (dialect24) dialect24->onStartElement(sname, attrs);
            return;
        }

        // Check for end of header section — first data object (no HEADERSECTION end marker)
        std::string tid = "";
        for (const auto& a : attrs) {
            std::string lk(a.first);
            auto sep = lk.find('\xFF');
            if (sep != std::string::npos) lk = lk.substr(sep + 1);
            if (lowerAscii(lk) == "tid") { tid = std::string(a.second); break; }
        }
        if (!tid.empty()) {
            emitStartTransfer();
            phase = ParserPhase::InContent;
            createDialect();
            if (dialect23) dialect23->onStartElement(sname, attrs); else if (dialect24) dialect24->onStartElement(sname, attrs);
            return;
        }
    }
}

void XtfReader::Impl::onEndElement(const xml::XmlEndElement& element) {
    if (phase == ParserPhase::Fatal) return;
    // Check for root TRANSFER close
    const auto sname = encodedName(element.name);
    auto sepPos = sname.find('\xFF');
    std::string localName = sname;
    if (sepPos != std::string::npos) {
        localName = sname.substr(sepPos + 1);
    }
    if (localName == "TRANSFER" || localName == "transfer") {
        // Is this the root close?
        if (elementStack.empty() || elementStack.size() == 1) {
            // If still in header, emit StartTransfer first
            if (phase == ParserPhase::InHeader) {
                emitStartTransfer();
            }
            eventQueue.push_back(EndTransferEvent{});
            if (!elementStack.empty()) elementStack.pop();
            rootClosed = true;
            // The root is successfully closed. Keep a non-fatal phase so
            // finish() can finalize the parser and enforce its one-shot API.
            phase = ParserPhase::InContent;
            return;
        }
    }

    // Content phase: delegate to dialect
    if (phase == ParserPhase::InContent) {
        if (!elementStack.empty()) elementStack.pop();
        if (dialect23) dialect23->onEndElement(sname); else if (dialect24) dialect24->onEndElement(sname);
        return;
    }

    if (elementStack.empty()) return;
    elementStack.pop();

    if (phase == ParserPhase::InHeader) {
        if (inSender)      { sender = currentText; inSender = false; }
        else if (inComment) { comment = currentText; inComment = false; }
        else if (inIliVersion) { iliVersion = currentText; inIliVersion = false; }
        else if (inSoftware) { software = currentText; inSoftware = false; }
        else if (inDate)    { date = currentText; inDate = false; }
        currentText.clear();
    }
}

void XtfReader::Impl::onCharacterData(std::string_view data,
                                      const SourceLocation&) {
    if (phase == ParserPhase::Fatal) return;
    // Content phase: delegate to dialect
    if (phase == ParserPhase::InContent) {
        if (dialect23) dialect23->onCharacterData(data); else if (dialect24) dialect24->onCharacterData(data);
        return;
    }

    if (inSender || inComment || inIliVersion || inSoftware || inDate) {
        currentText.append(data.data(), data.size());
    }
}

void XtfReader::Impl::emitStartTransfer() {
    StartTransferEvent st;
    st.header.version = detected.value_or(XtfVersion::V23);
    st.header.sender = std::move(sender);
    if (!comment.empty()) st.header.comment = std::move(comment);
    eventQueue.push_back(std::move(st));
    reset();
}

// ============================================================================
// XtfReader — public API
// ============================================================================

XtfReader::XtfReader(XtfReaderOptions options)
    : impl_(std::make_unique<Impl>()) {
    impl_->options = std::move(options);

    xml::XmlLimits limits;
    limits.maxDepth = impl_->options.xmlLimits.maxDepth;
    limits.maxAttributesPerElement =
        impl_->options.xmlLimits.maxAttributesPerElement;
    limits.maxTextBytesPerNode =
        impl_->options.xmlLimits.maxTextBytesPerNode;
    limits.maxTotalInputBytes =
        impl_->options.xmlLimits.maxTotalInputBytes;
    impl_->xmlParser = std::make_unique<xml::ExpatParser>(
        limits, impl_->options.sourceName);
    impl_->xmlParser->setStartHandler([this](const xml::XmlStartElement& element) {
        impl_->onStartElement(element);
    });
    impl_->xmlParser->setEndHandler([this](const xml::XmlEndElement& element) {
        impl_->onEndElement(element);
    });
    impl_->xmlParser->setTextHandler(
        [this](std::string_view data, const SourceLocation& location) {
            impl_->onCharacterData(data, location);
        });
}

XtfReader::~XtfReader() = default;

ReadOutcome XtfReader::next() {
    ReadOutcome outcome;

    if (!impl_->eventQueue.empty()) {
        outcome.progress = ReaderProgress::Event;
        outcome.event = std::move(impl_->eventQueue.front());
        impl_->eventQueue.pop_front();
        return outcome;
    }

    if (impl_->phase == ParserPhase::Fatal) {
        outcome.progress = ReaderProgress::End;
        return outcome;
    }

    if (impl_->finished_ && impl_->eventQueue.empty()) {
        outcome.progress = ReaderProgress::End;
        return outcome;
    }

    outcome.progress = ReaderProgress::NeedInput;
    return outcome;
}

void XtfReader::feed(ByteView data) {
    if (impl_->finished_) {
        impl_->phase = ParserPhase::Fatal;
        throw IoxError(DiagnosticCode::InvalidState,
                       "XTF reader received input after finish()");
    }
    if (impl_->phase == ParserPhase::Fatal) {
        throw IoxError(DiagnosticCode::InvalidState,
                       "Cannot feed a failed XTF reader");
    }
    try {
        impl_->xmlParser->feed(data);
    } catch (...) {
        impl_->phase = ParserPhase::Fatal;
        throw;
    }
}

void XtfReader::finish() {
    if (impl_->finished_) {
        impl_->phase = ParserPhase::Fatal;
        throw IoxError(DiagnosticCode::InvalidState,
                       "XTF reader finish() called more than once");
    }
    if (impl_->phase == ParserPhase::Fatal) {
        throw IoxError(DiagnosticCode::InvalidState,
                       "Cannot finish a failed XTF reader");
    }
    try {
        impl_->xmlParser->finish();
        if (impl_->phase == ParserPhase::InHeader) {
            impl_->emitStartTransfer();
        }
    } catch (...) {
        impl_->phase = ParserPhase::Fatal;
        throw;
    }

    impl_->finished_ = true;
}

bool XtfReader::isFinished() const noexcept {
    return impl_->finished_ && impl_->eventQueue.empty();
}

std::vector<Diagnostic> XtfReader::takeDiagnostics() {
    auto diags = std::move(impl_->diagnostics);
    impl_->diagnostics.clear();
    return diags;
}

std::optional<XtfVersion> XtfReader::detectedVersion() const noexcept {
    return impl_->detected;
}

} // namespace xtf
} // namespace iox
