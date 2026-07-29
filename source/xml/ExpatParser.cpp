#include "iox/xml/ExpatParser.h"

#include <expat.h>

#include <cstring>
#include <stdexcept>
#include <vector>
#include <utility>

namespace iox {
namespace xml {

// ============================================================================
// Internal helpers
// ============================================================================

namespace {

// Convert Expat attribute array to a vector of pairs.
// Expat delivers attributes as [name1, value1, name2, value2, ..., nullptr]
std::vector<std::pair<std::string_view, std::string_view>>
makeAttrPairs(const char** atts) {
    std::vector<std::pair<std::string_view, std::string_view>> result;
    if (!atts) return result;
    for (int i = 0; atts[i] != nullptr; i += 2) {
        result.emplace_back(
            std::string_view(atts[i]),
            std::string_view(atts[i + 1] ? atts[i + 1] : ""));
    }
    return result;
}

// Per-parser user data
struct ParserContext {
    ExpatCallbacks callbacks;
    ExpatLimits limits;
    std::vector<Diagnostic> diagnostics;
    XML_Parser parser = nullptr;
    std::uint64_t totalBytes = 0;
    std::size_t depth = 0;
    bool finished = false;
    bool fatal = false;
};

// ---- Expat C callbacks (must not throw) ----

extern "C" {

static void XMLCALL onStartElement(void* userData,
                                    const XML_Char* name,
                                    const XML_Char** atts) {
    auto* ctx = static_cast<ParserContext*>(userData);
    if (ctx->fatal) return;
    try {
        const std::size_t attributeCount = atts == nullptr ? 0 :
            [&]() { std::size_t n = 0; while (atts[n] != nullptr) n += 2; return n / 2; }();
        if (attributeCount > ctx->limits.maxAttributeCount ||
            std::strlen(name) > ctx->limits.maxElementNameLength) {
            ctx->diagnostics.push_back({Diagnostic::Severity::Fatal,
                ErrorCode::XmlLimitExceeded,
                "XML element or attribute limit exceeded"});
            ctx->fatal = true;
            XML_StopParser(ctx->parser, XML_FALSE);
            return;
        }
        for (std::size_t i = 0; i < attributeCount * 2; i += 2) {
            if (std::strlen(atts[i + 1]) > ctx->limits.maxAttributeValueLength) {
                ctx->diagnostics.push_back({Diagnostic::Severity::Fatal,
                    ErrorCode::XmlLimitExceeded,
                    "XML attribute value limit exceeded"});
                ctx->fatal = true;
                XML_StopParser(ctx->parser, XML_FALSE);
                return;
            }
        }
        ++ctx->depth;
        if (ctx->depth > ctx->limits.maxElementDepth) {
            ctx->diagnostics.push_back({Diagnostic::Severity::Fatal,
                ErrorCode::XmlLimitExceeded,
                "XML element depth limit exceeded"});
            ctx->fatal = true;
            XML_StopParser(ctx->parser, XML_FALSE);
            return;
        }
        if (ctx->callbacks.onStartElement) {
            auto pairs = makeAttrPairs(atts);
            ctx->callbacks.onStartElement(std::string_view(name), pairs);
        }
    } catch (const std::exception& e) {
        ctx->diagnostics.push_back({Diagnostic::Severity::Fatal,
            ErrorCode::InternalError,
            std::string("onStartElement: ") + e.what()});
        ctx->fatal = true;
        XML_StopParser(ctx->parser, XML_FALSE);
    } catch (...) {
        ctx->diagnostics.push_back({Diagnostic::Severity::Fatal,
            ErrorCode::InternalError,
            "onStartElement: unknown exception"});
        ctx->fatal = true;
        XML_StopParser(ctx->parser, XML_FALSE);
    }
}

static void XMLCALL onEndElement(void* userData,
                                  const XML_Char* name) {
    auto* ctx = static_cast<ParserContext*>(userData);
    if (ctx->fatal) return;
    try {
        if (ctx->callbacks.onEndElement) {
            ctx->callbacks.onEndElement(std::string_view(name));
        }
        if (ctx->depth > 0) --ctx->depth;
    } catch (...) {
        ctx->fatal = true;
        XML_StopParser(ctx->parser, XML_FALSE);
    }
}

static void XMLCALL onCharacterData(void* userData,
                                     const XML_Char* data,
                                     int len) {
    auto* ctx = static_cast<ParserContext*>(userData);
    if (ctx->fatal) return;
    try {
        if (ctx->callbacks.onCharacterData && len > 0) {
            ctx->callbacks.onCharacterData(std::string_view(data, static_cast<std::size_t>(len)));
        }
    } catch (...) {
        ctx->fatal = true;
        XML_StopParser(ctx->parser, XML_FALSE);
    }
}

static void XMLCALL onProcessingInstruction(void* userData,
                                             const XML_Char* target,
                                             const XML_Char* data) {
    auto* ctx = static_cast<ParserContext*>(userData);
    if (ctx->fatal) return;
    try {
        if (ctx->callbacks.onProcessingInstruction) {
            ctx->callbacks.onProcessingInstruction(
                std::string_view(target ? target : ""),
                std::string_view(data ? data : ""));
        }
    } catch (...) {
        ctx->fatal = true;
        XML_StopParser(ctx->parser, XML_FALSE);
    }
}

static void XMLCALL onComment(void* userData, const XML_Char* data) {
    auto* ctx = static_cast<ParserContext*>(userData);
    if (ctx->fatal) return;
    try {
        if (ctx->callbacks.onComment) {
            ctx->callbacks.onComment(std::string_view(data ? data : ""));
        }
    } catch (...) {
        ctx->fatal = true;
        XML_StopParser(ctx->parser, XML_FALSE);
    }
}

static void XMLCALL onXmlDeclaration(void* userData,
                                      const XML_Char* version,
                                      const XML_Char* encoding,
                                      int standalone) {
    auto* ctx = static_cast<ParserContext*>(userData);
    if (ctx->fatal) return;
    try {
        if (ctx->callbacks.onXmlDeclaration) {
            ctx->callbacks.onXmlDeclaration(
                std::string_view(version ? version : ""),
                std::string_view(encoding ? encoding : ""),
                standalone != 0);
        }
    } catch (...) {
        ctx->fatal = true;
        XML_StopParser(ctx->parser, XML_FALSE);
    }
}

static void XMLCALL onStartDoctypeDecl(void* userData,
                                        const XML_Char* /*doctypeName*/,
                                        const XML_Char* /*sysid*/,
                                        const XML_Char* /*pubid*/,
                                        int /*hasInternalSubset*/) {
    auto* ctx = static_cast<ParserContext*>(userData);
    ctx->diagnostics.push_back({Diagnostic::Severity::Fatal,
        ErrorCode::XmlDtdForbidden,
        "DTD is not supported"});
    ctx->fatal = true;
    XML_StopParser(ctx->parser, XML_FALSE);
}

static int XMLCALL onExternalEntityRef(XML_Parser /*parser*/,
                                        const XML_Char* /*context*/,
                                        const XML_Char* /*base*/,
                                        const XML_Char* /*systemId*/,
                                        const XML_Char* /*publicId*/) {
    // Reject all external entities. Expat never receives a resolver callback
    // that can perform I/O; the normal parse error is still reported below.
    return XML_STATUS_ERROR;
}

} // extern "C"

} // anonymous namespace

// ============================================================================
// ExpatParser::Impl
// ============================================================================

struct ExpatParser::Impl {
    XML_Parser parser = nullptr;
    ParserContext ctx;
};

// ============================================================================
// ExpatParser — public API
// ============================================================================

ExpatParser::ExpatParser(ExpatCallbacks callbacks, ExpatLimits limits)
    : impl_(std::make_unique<Impl>())
{
    impl_->ctx.callbacks = std::move(callbacks);
    impl_->ctx.limits = limits;

    impl_->parser = XML_ParserCreateNS(nullptr, '\xFF'); // namespace separator
    impl_->ctx.parser = impl_->parser;

    XML_SetUserData(impl_->parser, &impl_->ctx);

    // Register callbacks
    XML_SetStartElementHandler(impl_->parser, onStartElement);
    XML_SetEndElementHandler(impl_->parser, onEndElement);
    XML_SetCharacterDataHandler(impl_->parser, onCharacterData);
    XML_SetProcessingInstructionHandler(impl_->parser, onProcessingInstruction);
    XML_SetCommentHandler(impl_->parser, onComment);
    XML_SetXmlDeclHandler(impl_->parser, onXmlDeclaration);
    XML_SetStartDoctypeDeclHandler(impl_->parser, onStartDoctypeDecl);
    XML_SetExternalEntityRefHandler(impl_->parser, onExternalEntityRef);

    // Disable DTD processing completely
    XML_SetParamEntityParsing(impl_->parser, XML_PARAM_ENTITY_PARSING_NEVER);
}

ExpatParser::~ExpatParser() {
    if (impl_->parser) {
        XML_ParserFree(impl_->parser);
    }
}

bool ExpatParser::feed(ByteView data) {
    if (impl_->ctx.fatal || impl_->ctx.finished) return false;

    impl_->ctx.totalBytes += data.size();

    if (XML_Parse(impl_->parser, data.data(), static_cast<int>(data.size()), 0)
        == XML_STATUS_ERROR) {
        auto err = XML_GetErrorCode(impl_->parser);
        impl_->ctx.diagnostics.push_back({
            Diagnostic::Severity::Fatal,
            ErrorCode::XmlMalformed,
            std::string("XML parse error: ") + XML_ErrorString(err),
            Diagnostic::Location{
                impl_->ctx.callbacks.onStartElement ? "" : "",
                impl_->ctx.totalBytes,
                static_cast<int>(XML_GetCurrentLineNumber(impl_->parser)),
                static_cast<int>(XML_GetCurrentColumnNumber(impl_->parser))
            }
        });
        impl_->ctx.fatal = true;
        return false;
    }
    return !impl_->ctx.fatal;
}

bool ExpatParser::finish() {
    if (impl_->ctx.fatal) return false;
    if (impl_->ctx.finished) return true;

    if (XML_Parse(impl_->parser, "", 0, 1) == XML_STATUS_ERROR) {
        auto err = XML_GetErrorCode(impl_->parser);
        impl_->ctx.diagnostics.push_back({
            Diagnostic::Severity::Fatal,
            ErrorCode::XmlMalformed,
            std::string("XML parse error at end: ") + XML_ErrorString(err)
        });
        impl_->ctx.fatal = true;
        return false;
    }
    impl_->ctx.finished = !impl_->ctx.fatal;
    return !impl_->ctx.fatal;
}

std::vector<Diagnostic> ExpatParser::takeDiagnostics() {
    auto diags = std::move(impl_->ctx.diagnostics);
    impl_->ctx.diagnostics.clear();
    return diags;
}

std::uint64_t ExpatParser::byteOffset() const noexcept {
    return impl_->ctx.totalBytes;
}

int ExpatParser::line() const noexcept {
    return static_cast<int>(XML_GetCurrentLineNumber(impl_->parser));
}

int ExpatParser::column() const noexcept {
    return static_cast<int>(XML_GetCurrentColumnNumber(impl_->parser));
}

} // namespace xml
} // namespace iox
