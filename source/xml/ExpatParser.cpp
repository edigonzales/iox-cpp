#include "xml/ExpatParser.h"

#include <expat.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <exception>
#include <limits>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace iox {
namespace xml {
namespace {

constexpr XML_Char namespaceSeparator = '\x1f';

XmlQualifiedName decodeName(std::string_view encoded) {
    const auto first = encoded.find(namespaceSeparator);
    if (first == std::string_view::npos) {
        return {{}, std::string(encoded), {}};
    }
    const auto second = encoded.find(namespaceSeparator, first + 1U);
    return {
        std::string(encoded.substr(0, first)),
        std::string(encoded.substr(first + 1U,
            second == std::string_view::npos
                ? std::string_view::npos
                : second - first - 1U)),
        second == std::string_view::npos
            ? std::string{}
            : std::string(encoded.substr(second + 1U))
    };
}

} // namespace

struct ExpatParser::Impl final {
    XML_Parser parser = nullptr;
    XmlLimits limits;
    std::string sourceName;
    StartHandler startHandler;
    EndHandler endHandler;
    TextHandler textHandler;
    std::exception_ptr callbackError;
    SourceLocation callbackErrorLocation;
    std::vector<std::size_t> textBytes;
    std::vector<XmlNamespaceDeclaration> pendingNamespaces;
    std::size_t totalInputBytes = 0;
    std::size_t depth = 0;
    bool finished_ = false;
    bool failed = false;
    bool suspended_ = false;
    bool inParse = false;

    explicit Impl(XmlLimits value, std::string source)
        : limits(value), sourceName(std::move(source)) {}

    SourceLocation currentLocation() const {
        const auto byteIndex = XML_GetCurrentByteIndex(parser);
        const auto line = XML_GetCurrentLineNumber(parser);
        const auto column = XML_GetCurrentColumnNumber(parser);
        return {
            sourceName,
            byteIndex < 0 ? 0U : static_cast<std::uint64_t>(byteIndex),
            line == 0U ? 0U : static_cast<std::uint32_t>(line),
            column == 0U ? 0U : static_cast<std::uint32_t>(column + 1)
        };
    }

    [[noreturn]] void fail(DiagnosticCode code, std::string message,
                           SourceLocation where = {}) {
        failed = true;
        if (where.empty()) where = currentLocation();
        throw IoxError(code, std::move(message), std::move(where));
    }

    template<typename Function>
    void invokeFromCallback(Function&& function) noexcept {
        if (callbackError || failed) return;
        try {
            function();
        } catch (...) {
            callbackError = std::current_exception();
            callbackErrorLocation = currentLocation();
            XML_StopParser(parser, XML_FALSE);
        }
    }

    void raiseCallbackError() {
        if (!callbackError) return;
        auto error = std::move(callbackError);
        callbackError = nullptr;
        failed = true;
        try {
            std::rethrow_exception(error);
        } catch (const IoxError&) {
            throw;
        } catch (const std::exception& exception) {
            throw IoxError(DiagnosticCode::InternalError,
                           std::string("XML callback failed: ") + exception.what(),
                           callbackErrorLocation);
        } catch (...) {
            throw IoxError(DiagnosticCode::InternalError,
                           "XML callback failed with an unknown exception",
                           callbackErrorLocation);
        }
    }

    static void XMLCALL startElement(void* userData, const XML_Char* name,
                                     const XML_Char** attributes) noexcept {
        auto& self = *static_cast<Impl*>(userData);
        self.invokeFromCallback([&] {
            std::size_t count = 0;
            if (attributes != nullptr) {
                while (attributes[count * 2U] != nullptr) ++count;
            }
            if (count > self.limits.maxAttributesPerElement) {
                self.fail(DiagnosticCode::XmlLimitExceeded,
                          "XML attribute count exceeds maxAttributesPerElement");
            }
            if (self.pendingNamespaces.size() >
                    self.limits.maxAttributesPerElement - count) {
                self.fail(DiagnosticCode::XmlLimitExceeded,
                          "XML attributes and namespace declarations exceed "
                          "maxAttributesPerElement");
            }
            if (self.depth >= self.limits.maxDepth) {
                self.fail(DiagnosticCode::XmlLimitExceeded,
                          "XML depth exceeds maxDepth");
            }

            ++self.depth;
            if (!self.textBytes.empty()) self.textBytes.back() = 0;
            self.textBytes.push_back(0);

            XmlStartElement element;
            element.name = decodeName(name == nullptr ? std::string_view{}
                                                      : std::string_view(name));
            element.location = self.currentLocation();
            element.namespaces = std::move(self.pendingNamespaces);
            self.pendingNamespaces.clear();
            element.attributes.reserve(count);
            for (std::size_t index = 0; index < count; ++index) {
                element.attributes.push_back({
                    decodeName(attributes[index * 2U]),
                    std::string(attributes[index * 2U + 1U])
                });
            }
            if (self.startHandler) self.startHandler(element);
        });
    }

    static void XMLCALL startNamespace(void* userData,
                                       const XML_Char* prefix,
                                       const XML_Char* namespaceUri) noexcept {
        auto& self = *static_cast<Impl*>(userData);
        self.invokeFromCallback([&] {
            if (self.pendingNamespaces.size() >=
                self.limits.maxAttributesPerElement) {
                self.fail(DiagnosticCode::XmlLimitExceeded,
                          "XML namespace declarations exceed "
                          "maxAttributesPerElement");
            }
            self.pendingNamespaces.push_back({
                prefix == nullptr ? std::string{} : std::string(prefix),
                namespaceUri == nullptr ? std::string{}
                                        : std::string(namespaceUri)});
        });
    }

    static void XMLCALL endNamespace(void*, const XML_Char*) noexcept {}

    static void XMLCALL endElement(void* userData, const XML_Char* name) noexcept {
        auto& self = *static_cast<Impl*>(userData);
        self.invokeFromCallback([&] {
            XmlEndElement element;
            element.name = decodeName(name == nullptr ? std::string_view{}
                                                      : std::string_view(name));
            element.location = self.currentLocation();
            if (self.endHandler) self.endHandler(element);
            if (self.depth > 0) --self.depth;
            if (!self.textBytes.empty()) self.textBytes.pop_back();
            if (!self.textBytes.empty()) self.textBytes.back() = 0;
        });
    }

    static void XMLCALL characterData(void* userData, const XML_Char* data,
                                      int length) noexcept {
        auto& self = *static_cast<Impl*>(userData);
        self.invokeFromCallback([&] {
            if (length <= 0) return;
            if (self.textBytes.empty()) {
                const auto onlyWhitespace = std::all_of(
                    data, data + length, [](unsigned char character) {
                        return std::isspace(character) != 0;
                    });
                if (onlyWhitespace) return;
                self.fail(DiagnosticCode::XmlMalformed,
                          "Non-whitespace XML data occurred outside the root element");
            }
            const auto size = static_cast<std::size_t>(length);
            if (size > self.limits.maxTextBytesPerNode -
                           std::min(self.textBytes.back(),
                                    self.limits.maxTextBytesPerNode)) {
                self.fail(DiagnosticCode::XmlLimitExceeded,
                          "XML text exceeds maxTextBytesPerNode");
            }
            self.textBytes.back() += size;
            if (self.textHandler) {
                self.textHandler(std::string_view(data, size),
                                 self.currentLocation());
            }
        });
    }

    static void XMLCALL startDoctype(void* userData, const XML_Char*,
                                     const XML_Char*, const XML_Char*, int) noexcept {
        auto& self = *static_cast<Impl*>(userData);
        self.invokeFromCallback([&] {
            self.fail(DiagnosticCode::XmlDtdForbidden,
                      "DTD declarations are forbidden");
        });
    }

    static void XMLCALL xmlDeclaration(void* userData, const XML_Char*,
                                       const XML_Char* encoding, int) noexcept {
        auto& self = *static_cast<Impl*>(userData);
        self.invokeFromCallback([&] {
            if (encoding == nullptr || *encoding == '\0') return;
            std::string normalized(encoding);
            std::transform(normalized.begin(), normalized.end(),
                           normalized.begin(), [](unsigned char character) {
                               return static_cast<char>(std::tolower(character));
                           });
            if (normalized != "utf-8") {
                self.fail(DiagnosticCode::XmlMalformed,
                          "Only UTF-8 XML input is supported");
            }
        });
    }

    static int XMLCALL externalEntity(XML_Parser parser, const XML_Char*,
                                      const XML_Char*, const XML_Char*,
                                      const XML_Char*) noexcept {
        auto* self = static_cast<Impl*>(XML_GetUserData(parser));
        if (self == nullptr) return XML_STATUS_ERROR;
        self->invokeFromCallback([&] {
            self->fail(DiagnosticCode::XmlExternalEntityForbidden,
                       "External entities are forbidden");
        });
        return XML_STATUS_ERROR;
    }

    void handleStatus(XML_Status status) {
        raiseCallbackError();
        if (status == XML_STATUS_SUSPENDED) {
            suspended_ = true;
            return;
        }
        if (status == XML_STATUS_ERROR) {
            const auto code = XML_GetErrorCode(parser);
            fail(DiagnosticCode::XmlMalformed,
                 std::string("XML parse error: ") + XML_ErrorString(code));
        }
        suspended_ = false;
    }

    void parse(const char* data, int size, bool final) {
        XML_Status status = XML_STATUS_ERROR;
        inParse = true;
        if (size == 0) {
            status = XML_Parse(parser, "", 0, final ? XML_TRUE : XML_FALSE);
        } else {
            void* buffer = XML_GetBuffer(parser, size);
            if (buffer == nullptr) {
                inParse = false;
                fail(DiagnosticCode::XmlLimitExceeded,
                     "Unable to allocate the Expat input buffer");
            }
            std::memcpy(buffer, data, static_cast<std::size_t>(size));
            status = XML_ParseBuffer(parser, size,
                                     final ? XML_TRUE : XML_FALSE);
        }
        inParse = false;
        handleStatus(status);
    }
};

ExpatParser::ExpatParser(XmlLimits limits, std::string sourceName)
    : impl_(std::make_unique<Impl>(limits, std::move(sourceName))) {
    if (limits.maxDepth == 0 || limits.maxAttributesPerElement == 0 ||
        limits.maxTextBytesPerNode == 0) {
        throw IoxError(DiagnosticCode::InvalidArgument,
                       "XML limits other than maxTotalInputBytes must be non-zero");
    }
    impl_->parser = XML_ParserCreateNS("UTF-8", namespaceSeparator);
    if (impl_->parser == nullptr) {
        throw IoxError(DiagnosticCode::InternalError,
                       "Unable to allocate Expat parser");
    }
    XML_SetUserData(impl_->parser, impl_.get());
    XML_SetReturnNSTriplet(impl_->parser, 1);
    XML_SetElementHandler(impl_->parser, &Impl::startElement, &Impl::endElement);
    XML_SetNamespaceDeclHandler(impl_->parser, &Impl::startNamespace,
                               &Impl::endNamespace);
    XML_SetCharacterDataHandler(impl_->parser, &Impl::characterData);
    XML_SetXmlDeclHandler(impl_->parser, &Impl::xmlDeclaration);
    XML_SetStartDoctypeDeclHandler(impl_->parser, &Impl::startDoctype);
    XML_SetExternalEntityRefHandler(impl_->parser, &Impl::externalEntity);
    XML_SetParamEntityParsing(impl_->parser, XML_PARAM_ENTITY_PARSING_NEVER);
}

ExpatParser::~ExpatParser() {
    if (impl_ && impl_->parser != nullptr) XML_ParserFree(impl_->parser);
}

void ExpatParser::setStartHandler(StartHandler handler) {
    if (impl_->finished_ || impl_->failed) {
        throw IoxError(DiagnosticCode::InvalidState,
                       "Cannot change handlers after XML parsing has ended");
    }
    impl_->startHandler = std::move(handler);
}

void ExpatParser::setEndHandler(EndHandler handler) {
    if (impl_->finished_ || impl_->failed) {
        throw IoxError(DiagnosticCode::InvalidState,
                       "Cannot change handlers after XML parsing has ended");
    }
    impl_->endHandler = std::move(handler);
}

void ExpatParser::setTextHandler(TextHandler handler) {
    if (impl_->finished_ || impl_->failed) {
        throw IoxError(DiagnosticCode::InvalidState,
                       "Cannot change handlers after XML parsing has ended");
    }
    impl_->textHandler = std::move(handler);
}

void ExpatParser::feed(ByteView bytes) {
    if (impl_->finished_ || impl_->failed) {
        throw IoxError(DiagnosticCode::InvalidState,
                       "Cannot feed a finished or failed XML parser");
    }
    if (bytes.size() > std::numeric_limits<std::size_t>::max() -
                           impl_->totalInputBytes ||
        (impl_->limits.maxTotalInputBytes != 0 &&
         bytes.size() > impl_->limits.maxTotalInputBytes -
                            std::min(impl_->totalInputBytes,
                                     impl_->limits.maxTotalInputBytes))) {
        impl_->fail(DiagnosticCode::XmlLimitExceeded,
                    "XML input exceeds maxTotalInputBytes");
    }
    impl_->totalInputBytes += bytes.size();

    std::size_t offset = 0;
    const auto maxChunk = static_cast<std::size_t>(
        std::numeric_limits<int>::max());
    while (offset < bytes.size()) {
        const auto count = std::min(bytes.size() - offset, maxChunk);
        impl_->parse(reinterpret_cast<const char*>(bytes.data() + offset),
                     static_cast<int>(count), false);
        offset += count;
        if (impl_->suspended_) break;
    }
}

void ExpatParser::finish() {
    if (impl_->finished_ || impl_->failed) {
        throw IoxError(DiagnosticCode::InvalidState,
                       "XML parser can only be finished once");
    }
    impl_->parse("", 0, true);
    impl_->finished_ = true;
}

void ExpatParser::suspend() {
    if (impl_->finished_ || impl_->failed || !impl_->inParse) {
        throw IoxError(DiagnosticCode::InvalidState,
                       "XML parser suspension is only valid in a callback");
    }
    if (impl_->suspended_) return;
    if (XML_StopParser(impl_->parser, XML_TRUE) == XML_STATUS_ERROR) {
        impl_->fail(DiagnosticCode::InternalError,
                    "Expat rejected a resumable parser stop");
    }
}

void ExpatParser::resume() {
    if (impl_->finished_ || impl_->failed || !impl_->suspended_) {
        throw IoxError(DiagnosticCode::InvalidState,
                       "Cannot resume an XML parser that is not suspended");
    }
    impl_->inParse = true;
    const auto status = XML_ResumeParser(impl_->parser);
    impl_->inParse = false;
    impl_->handleStatus(status);
}

bool ExpatParser::suspended() const noexcept {
    return impl_->suspended_;
}

bool ExpatParser::finished() const noexcept {
    return impl_->finished_;
}

SourceLocation ExpatParser::location() const {
    return impl_->currentLocation();
}

} // namespace xml
} // namespace iox
