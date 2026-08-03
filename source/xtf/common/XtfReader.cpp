#include "iox/xtf/XtfReader.h"

#include "xtf/v23/Xtf23Dialect.h"
#include "xtf/v24/Xtf24Dialect.h"
#include "xml/ExpatParser.h"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace iox {
namespace xtf {
namespace {

constexpr std::string_view xtf23Namespace =
    "http://www.interlis.ch/INTERLIS2.3";
constexpr std::string_view xtf24Namespace =
    "http://www.interlis.ch/xtf/2.4/INTERLIS";
constexpr std::size_t queuedInputChunkSize = 64U * 1024U;

} // namespace

enum class CoordinatorState { BeforeRoot, InRoot, AfterRoot, Failed };
enum class EventState { BeforeTransfer, InTransfer, InBasket, AfterTransfer };

struct XtfReader::Impl final {
    XtfReaderOptions options;
    std::vector<Diagnostic> diagnostics;
    std::unique_ptr<xml::ExpatParser> xmlParser;
    std::unique_ptr<Xtf23Dialect> dialect23;
    std::unique_ptr<Xtf24Dialect> dialect24;
    std::deque<IoxEvent> eventQueue;
    std::deque<std::vector<std::uint8_t>> inputQueue;
    CoordinatorState state = CoordinatorState::BeforeRoot;
    EventState eventState = EventState::BeforeTransfer;
    std::optional<XtfVersion> detected;
    XmlQualifiedName rootName;
    std::vector<xml::XmlNamespaceDeclaration> rootNamespaces;
    std::size_t depth = 0;
    std::size_t totalFed = 0;
    bool finishCalled = false;
    bool parseComplete = false;
    bool pumping = false;

    [[noreturn]] void fail(DiagnosticCode code, std::string message,
                           SourceLocation location = {}) {
        state = CoordinatorState::Failed;
        throw IoxError(code, std::move(message), std::move(location));
    }

    void add(Diagnostic diagnostic) {
        if (diagnostic.severity == DiagnosticSeverity::Fatal) {
            fail(diagnostic.code, std::move(diagnostic.message),
                 std::move(diagnostic.location));
        }
        diagnostics.push_back(std::move(diagnostic));
    }

    void validateAndQueue(IoxEvent event) {
        const auto kind = eventKind(event);
        const bool valid =
            (eventState == EventState::BeforeTransfer &&
             kind == EventKind::StartTransfer) ||
            (eventState == EventState::InTransfer &&
             (kind == EventKind::StartBasket ||
              kind == EventKind::EndTransfer)) ||
            (eventState == EventState::InBasket &&
             (kind == EventKind::Object || kind == EventKind::EndBasket));
        if (!valid) {
            fail(DiagnosticCode::InvalidEventOrder,
                 "XTF dialect emitted an invalid event sequence");
        }
        if (kind == EventKind::StartTransfer) eventState = EventState::InTransfer;
        else if (kind == EventKind::StartBasket) eventState = EventState::InBasket;
        else if (kind == EventKind::EndBasket) eventState = EventState::InTransfer;
        else if (kind == EventKind::EndTransfer) eventState = EventState::AfterTransfer;

        eventQueue.push_back(std::move(event));
        if (eventQueue.size() >= options.xmlLimits.maxQueuedEvents &&
            xmlParser && !xmlParser->suspended()) {
            xmlParser->suspend();
        }
    }

    void createDialect() {
        if (*detected == XtfVersion::V23) {
            dialect23 = std::make_unique<Xtf23Dialect>(
                options,
                [this](IoxEvent event) {
                    validateAndQueue(std::move(event));
                },
                [this](Diagnostic diagnostic) {
                    add(std::move(diagnostic));
                });
            return;
        }
        dialect24 = std::make_unique<Xtf24Dialect>(
            options, rootNamespaces,
            [this](IoxEvent event) {
                validateAndQueue(std::move(event));
            },
            [this](Diagnostic diagnostic) {
                add(std::move(diagnostic));
            });
    }

    void detectRoot(const xml::XmlStartElement& element) {
        if (element.name.namespaceUri == xtf23Namespace &&
            element.name.localName == "TRANSFER") {
            detected = XtfVersion::V23;
        } else if (element.name.namespaceUri == xtf24Namespace &&
                   element.name.localName == "transfer") {
            detected = XtfVersion::V24;
        } else if (element.name.namespaceUri.find("INTERLIS2.2") !=
                   std::string::npos) {
            fail(DiagnosticCode::UnsupportedXtfVersion,
                 "XTF 2.2 is not supported", element.location);
        } else {
            fail(DiagnosticCode::InvalidXtfNamespace,
                 "Expected the exact XTF 2.3 or 2.4 TRANSFER root",
                 element.location);
        }
        if (options.expectedVersion && *options.expectedVersion != *detected) {
            fail(DiagnosticCode::UnsupportedXtfVersion,
                 std::string("Expected XTF ") +
                     toString(*options.expectedVersion) + " but detected " +
                     toString(*detected),
                 element.location);
        }
        rootName = element.name;
        rootNamespaces = element.namespaces;
        depth = 1;
        state = CoordinatorState::InRoot;
        createDialect();
    }

    void onStartElement(const xml::XmlStartElement& element) {
        if (state == CoordinatorState::BeforeRoot) {
            detectRoot(element);
            return;
        }
        if (state != CoordinatorState::InRoot) {
            fail(DiagnosticCode::InvalidEventOrder,
                 "Element encountered outside the TRANSFER root",
                 element.location);
        }
        ++depth;
        if (*detected == XtfVersion::V23) dialect23->onStartElement(element);
        else dialect24->onStartElement(element);
    }

    void onEndElement(const xml::XmlEndElement& element) {
        if (state != CoordinatorState::InRoot || depth == 0U) {
            fail(DiagnosticCode::InvalidEventOrder,
                 "Unexpected XTF end element", element.location);
        }
        if (depth == 1U) {
            if (element.name != rootName) {
                fail(DiagnosticCode::InvalidEventOrder,
                     "Unexpected end of the TRANSFER root", element.location);
            }
            if (*detected == XtfVersion::V23) {
                dialect23->onRootClosed(element.location);
            } else dialect24->onRootClosed(element.location);
            depth = 0;
            state = CoordinatorState::AfterRoot;
            return;
        }
        if (*detected == XtfVersion::V23) dialect23->onEndElement(element);
        else dialect24->onEndElement(element);
        --depth;
    }

    void onCharacterData(std::string_view data,
                         const SourceLocation& location) {
        if (state == CoordinatorState::InRoot && detected) {
            if (*detected == XtfVersion::V23) dialect23->onText(data, location);
            else dialect24->onText(data, location);
        }
    }

    void pump() {
        if (pumping || state == CoordinatorState::Failed) return;
        pumping = true;
        try {
            if (xmlParser->suspended() &&
                eventQueue.size() < options.xmlLimits.maxQueuedEvents) {
                xmlParser->resume();
            }
            while (!xmlParser->suspended() && !inputQueue.empty()) {
                auto input = std::move(inputQueue.front());
                inputQueue.pop_front();
                xmlParser->feed(ByteView(input));
            }
            if (finishCalled && !parseComplete && !xmlParser->suspended() &&
                inputQueue.empty()) {
                xmlParser->finish();
                if (state != CoordinatorState::AfterRoot ||
                    eventState != EventState::AfterTransfer) {
                    fail(DiagnosticCode::InvalidEventOrder,
                         "XTF input ended before the transfer was complete",
                         xmlParser->location());
                }
                parseComplete = true;
            }
            pumping = false;
        } catch (...) {
            pumping = false;
            state = CoordinatorState::Failed;
            throw;
        }
    }
};

XtfReader::XtfReader(XtfReaderOptions options)
    : impl_(std::make_unique<Impl>()) {
    if (options.xmlLimits.maxQueuedEvents == 0U) {
        throw IoxError(DiagnosticCode::InvalidArgument,
                       "maxQueuedEvents must be non-zero");
    }
    if (!options.allowVersionAutoDetection && !options.expectedVersion) {
        throw IoxError(DiagnosticCode::InvalidArgument,
                       "Disabling version autodetection requires expectedVersion");
    }
    impl_->options = std::move(options);
    xml::XmlLimits limits;
    limits.maxDepth = impl_->options.xmlLimits.maxDepth;
    limits.maxAttributesPerElement =
        impl_->options.xmlLimits.maxAttributesPerElement;
    limits.maxTextBytesPerNode =
        impl_->options.xmlLimits.maxTextBytesPerNode;
    // Count all accepted feed chunks before they enter the deferred queue.
    limits.maxTotalInputBytes = 0;
    impl_->xmlParser = std::make_unique<xml::ExpatParser>(
        limits, impl_->options.sourceName);
    impl_->xmlParser->setStartHandler(
        [this](const xml::XmlStartElement& element) {
            impl_->onStartElement(element);
        });
    impl_->xmlParser->setEndHandler(
        [this](const xml::XmlEndElement& element) {
            impl_->onEndElement(element);
        });
    impl_->xmlParser->setTextHandler(
        [this](std::string_view data, const SourceLocation& location) {
            impl_->onCharacterData(data, location);
        });
}

XtfReader::~XtfReader() = default;

ReadOutcome XtfReader::next() {
    if (impl_->state == CoordinatorState::Failed) {
        throw IoxError(DiagnosticCode::InvalidState,
                       "Cannot read from a failed XTF reader");
    }
    if (!impl_->eventQueue.empty()) {
        IoxEvent event = std::move(impl_->eventQueue.front());
        impl_->eventQueue.pop_front();
        impl_->pump();
        return {ReaderProgress::Event, std::move(event)};
    }
    impl_->pump();
    if (!impl_->eventQueue.empty()) {
        IoxEvent event = std::move(impl_->eventQueue.front());
        impl_->eventQueue.pop_front();
        return {ReaderProgress::Event, std::move(event)};
    }
    if (impl_->parseComplete) return {ReaderProgress::End, std::nullopt};
    return {ReaderProgress::NeedInput, std::nullopt};
}

void XtfReader::feed(ByteView data) {
    if (impl_->finishCalled || impl_->state == CoordinatorState::Failed) {
        throw IoxError(DiagnosticCode::InvalidState,
                       "Cannot feed a finished or failed XTF reader");
    }
    if (data.size() > std::numeric_limits<std::size_t>::max() -
                          impl_->totalFed ||
        (impl_->options.xmlLimits.maxTotalInputBytes != 0U &&
         data.size() > impl_->options.xmlLimits.maxTotalInputBytes -
                           std::min(impl_->totalFed,
                                    impl_->options.xmlLimits.maxTotalInputBytes))) {
        impl_->state = CoordinatorState::Failed;
        throw IoxError(DiagnosticCode::XmlLimitExceeded,
                       "XML input exceeds maxTotalInputBytes",
                       impl_->xmlParser->location());
    }
    impl_->totalFed += data.size();
    std::size_t offset = 0;
    do {
        const auto count = std::min(queuedInputChunkSize, data.size() - offset);
        std::vector<std::uint8_t> owned;
        owned.reserve(count);
        if (count != 0U) {
            owned.assign(data.data() + offset, data.data() + offset + count);
        }
        impl_->inputQueue.push_back(std::move(owned));
        offset += count;
    } while (offset < data.size());
    impl_->pump();
}

void XtfReader::finish() {
    if (impl_->finishCalled || impl_->state == CoordinatorState::Failed) {
        throw IoxError(DiagnosticCode::InvalidState,
                       "XTF reader finish() may only be called once");
    }
    impl_->finishCalled = true;
    impl_->pump();
}

bool XtfReader::isFinished() const noexcept {
    return impl_->parseComplete && impl_->eventQueue.empty();
}

std::vector<Diagnostic> XtfReader::takeDiagnostics() {
    auto result = std::move(impl_->diagnostics);
    impl_->diagnostics.clear();
    return result;
}

std::optional<XtfVersion> XtfReader::detectedVersion() const noexcept {
    return impl_->detected;
}

} // namespace xtf
} // namespace iox
