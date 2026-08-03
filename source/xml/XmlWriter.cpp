#include "xml/XmlWriter.h"

#include "iox/Diagnostic.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace iox {
namespace xml {
namespace {

constexpr std::string_view xmlNamespace =
    "http://www.w3.org/XML/1998/namespace";
constexpr std::string_view xmlnsNamespace =
    "http://www.w3.org/2000/xmlns/";

std::uint32_t nextCodePoint(std::string_view value, std::size_t& offset) {
    const auto first = static_cast<unsigned char>(value[offset++]);
    if (first < 0x80U) return first;

    std::uint32_t codePoint = 0;
    std::size_t trailing = 0;
    if (first >= 0xC2U && first <= 0xDFU) {
        codePoint = first & 0x1FU;
        trailing = 1;
    } else if (first >= 0xE0U && first <= 0xEFU) {
        codePoint = first & 0x0FU;
        trailing = 2;
    } else if (first >= 0xF0U && first <= 0xF4U) {
        codePoint = first & 0x07U;
        trailing = 3;
    } else {
        throw IoxError(DiagnosticCode::InvalidArgument,
                       "XML output contains invalid UTF-8");
    }
    if (offset + trailing > value.size()) {
        throw IoxError(DiagnosticCode::InvalidArgument,
                       "XML output contains truncated UTF-8");
    }
    for (std::size_t index = 0; index < trailing; ++index) {
        const auto next = static_cast<unsigned char>(value[offset++]);
        if ((next & 0xC0U) != 0x80U) {
            throw IoxError(DiagnosticCode::InvalidArgument,
                           "XML output contains invalid UTF-8 continuation bytes");
        }
        codePoint = (codePoint << 6U) | (next & 0x3FU);
    }
    if ((trailing == 2U && codePoint < 0x800U) ||
        (trailing == 3U && codePoint < 0x10000U) ||
        codePoint > 0x10FFFFU ||
        (codePoint >= 0xD800U && codePoint <= 0xDFFFU)) {
        throw IoxError(DiagnosticCode::InvalidArgument,
                       "XML output contains a non-canonical UTF-8 code point");
    }
    return codePoint;
}

bool validXmlCharacter(std::uint32_t codePoint) noexcept {
    return codePoint == 0x09U || codePoint == 0x0AU || codePoint == 0x0DU ||
           (codePoint >= 0x20U && codePoint <= 0xD7FFU) ||
           (codePoint >= 0xE000U && codePoint <= 0xFFFDU) ||
           (codePoint >= 0x10000U && codePoint <= 0x10FFFFU);
}

void validateXmlText(std::string_view value) {
    std::size_t offset = 0;
    while (offset < value.size()) {
        if (!validXmlCharacter(nextCodePoint(value, offset))) {
            throw IoxError(DiagnosticCode::InvalidArgument,
                           "XML output contains a forbidden XML 1.0 character");
        }
    }
}

bool validNameStart(std::uint32_t codePoint) noexcept {
    return codePoint == '_' ||
           (codePoint >= 'A' && codePoint <= 'Z') ||
           (codePoint >= 'a' && codePoint <= 'z') ||
           (codePoint >= 0xC0U && codePoint <= 0xD6U) ||
           (codePoint >= 0xD8U && codePoint <= 0xF6U) ||
           (codePoint >= 0xF8U && codePoint <= 0x2FFU) ||
           (codePoint >= 0x370U && codePoint <= 0x37DU) ||
           (codePoint >= 0x37FU && codePoint <= 0x1FFFU) ||
           (codePoint >= 0x200CU && codePoint <= 0x200DU) ||
           (codePoint >= 0x2070U && codePoint <= 0x218FU) ||
           (codePoint >= 0x2C00U && codePoint <= 0x2FEFU) ||
           (codePoint >= 0x3001U && codePoint <= 0xD7FFU) ||
           (codePoint >= 0xF900U && codePoint <= 0xFDCFU) ||
           (codePoint >= 0xFDF0U && codePoint <= 0xFFFDU) ||
           (codePoint >= 0x10000U && codePoint <= 0xEFFFFU);
}

bool validNameCharacter(std::uint32_t codePoint) noexcept {
    return validNameStart(codePoint) ||
           (codePoint >= '0' && codePoint <= '9') ||
           codePoint == '-' || codePoint == '.' || codePoint == 0xB7U ||
           (codePoint >= 0x300U && codePoint <= 0x36FU) ||
           (codePoint >= 0x203FU && codePoint <= 0x2040U);
}

void validateName(std::string_view value, const char* what) {
    if (value.empty()) {
        throw IoxError(DiagnosticCode::InvalidArgument,
                       std::string("XML ") + what + " must not be empty");
    }
    std::size_t offset = 0;
    if (!validNameStart(nextCodePoint(value, offset))) {
        throw IoxError(DiagnosticCode::InvalidArgument,
                       std::string("Invalid XML ") + what);
    }
    while (offset < value.size()) {
        if (!validNameCharacter(nextCodePoint(value, offset))) {
            throw IoxError(DiagnosticCode::InvalidArgument,
                           std::string("Invalid XML ") + what);
        }
    }
}

std::string escaped(std::string_view value, bool attribute) {
    validateXmlText(value);
    std::string result;
    result.reserve(value.size());
    for (std::size_t index = 0; index < value.size(); ++index) {
        const char character = value[index];
        if (character == '&') result += "&amp;";
        else if (character == '<') result += "&lt;";
        else if (character == '>') result += "&gt;";
        else if (attribute && character == '"') result += "&quot;";
        else if (attribute && character == '\t') result += "&#x9;";
        else if (attribute && character == '\n') result += "&#xA;";
        else if (attribute && character == '\r') result += "&#xD;";
        else result.push_back(character);
    }
    return result;
}

} // namespace

struct XmlWriter::Impl final {
    enum class State { BeforeDocument, InDocument, AfterDocument, Failed };

    struct Attribute final {
        std::string lexicalName;
        std::string value;
    };

    struct Frame final {
        std::string lexicalName;
        std::map<std::string, std::string> bindings;
        std::vector<std::pair<std::string, std::string>> declarations;
        std::set<std::pair<std::string, std::string>> attributeNames;
        std::vector<Attribute> attributes;
        bool startPending = true;
        bool hasText = false;
        bool hasChild = false;
    };

    std::shared_ptr<OutputSink> output;
    XmlWriterOptions options;
    State state = State::BeforeDocument;
    std::vector<Frame> frames;
    std::size_t nextPrefix = 0;
    bool rootStarted = false;
    bool rootEnded = false;

    explicit Impl(std::shared_ptr<OutputSink> sink, XmlWriterOptions value)
        : output(std::move(sink)), options(std::move(value)) {}

    [[noreturn]] void fail(DiagnosticCode code, std::string message) {
        state = State::Failed;
        throw IoxError(code, std::move(message));
    }

    void ensureDocument() {
        if (state != State::InDocument) {
            fail(DiagnosticCode::WriterStateError,
                 "XML writer is not in an open document");
        }
    }

    void emit(std::string_view bytes) {
        std::size_t offset = 0;
        try {
            while (offset < bytes.size()) {
                const auto written = output->write(bytes.data() + offset,
                                                   bytes.size() - offset);
                if (written == 0 || written > bytes.size() - offset) {
                    fail(DiagnosticCode::IoError,
                         "XML output sink returned an invalid short write");
                }
                offset += written;
            }
        } catch (const IoxError&) {
            state = State::Failed;
            throw;
        } catch (const std::exception& exception) {
            fail(DiagnosticCode::IoError,
                 std::string("XML output sink failed: ") + exception.what());
        } catch (...) {
            fail(DiagnosticCode::IoError,
                 "XML output sink failed with an unknown exception");
        }
    }

    void indent(std::size_t depth) {
        if (!options.pretty) return;
        emit(options.newline);
        for (std::size_t index = 0; index < depth; ++index) {
            emit(options.indent);
        }
    }

    void addDeclaration(Frame& frame, const std::string& prefix,
                        const std::string& namespaceUri) {
        if (prefix == "xmlns") {
            fail(DiagnosticCode::InvalidArgument,
                 "The xmlns namespace prefix is reserved");
        }
        if (prefix == "xml" && namespaceUri != xmlNamespace) {
            fail(DiagnosticCode::InvalidArgument,
                 "The xml prefix must use the XML namespace URI");
        }
        if (!prefix.empty() && namespaceUri.empty()) {
            fail(DiagnosticCode::InvalidArgument,
                 "A prefixed namespace declaration requires a namespace URI");
        }
        const auto existing = frame.bindings.find(prefix);
        if (existing != frame.bindings.end() && existing->second == namespaceUri) {
            const auto declared = std::find_if(
                frame.declarations.begin(), frame.declarations.end(),
                [&](const auto& item) { return item.first == prefix; });
            if (declared != frame.declarations.end()) return;
            const bool inherited = frames.size() > 1U &&
                frames[frames.size() - 2U].bindings.count(prefix) != 0U &&
                frames[frames.size() - 2U].bindings.at(prefix) == namespaceUri;
            if (inherited) return;
        }
        for (const auto& declaration : frame.declarations) {
            if (declaration.first == prefix) {
                if (declaration.second == namespaceUri) return;
                fail(DiagnosticCode::UnexpectedAttribute,
                     "Duplicate namespace prefix declaration");
            }
        }
        frame.bindings[prefix] = namespaceUri;
        frame.declarations.push_back({prefix, namespaceUri});
    }

    std::string prefixFor(Frame& frame, const XmlQualifiedName& name,
                          bool attribute) {
        validateName(name.localName, "local name");
        if (name.namespaceUri == xmlnsNamespace) {
            fail(DiagnosticCode::InvalidArgument,
                 "The xmlns namespace is reserved for namespace declarations");
        }
        if (name.namespaceUri.empty()) {
            if (!name.prefixHint.empty()) {
                fail(DiagnosticCode::InvalidArgument,
                     "An XML prefix requires a namespace URI");
            }
            return {};
        }
        validateXmlText(name.namespaceUri);
        if (name.namespaceUri == xmlNamespace) return "xml";

        if (!name.prefixHint.empty()) {
            validateName(name.prefixHint, "namespace prefix");
            const auto found = frame.bindings.find(name.prefixHint);
            if (found == frame.bindings.end() || found->second == name.namespaceUri) {
                addDeclaration(frame, name.prefixHint, name.namespaceUri);
                return name.prefixHint;
            }
        }

        if (!attribute) {
            const auto defaultBinding = frame.bindings.find("");
            if (defaultBinding != frame.bindings.end() &&
                defaultBinding->second == name.namespaceUri) {
                return {};
            }
            if (name.prefixHint.empty()) {
                addDeclaration(frame, "", name.namespaceUri);
                return {};
            }
        }
        for (const auto& binding : frame.bindings) {
            if (binding.second == name.namespaceUri &&
                (!attribute || !binding.first.empty())) {
                return binding.first;
            }
        }

        std::string prefix;
        do {
            prefix = "ns" + std::to_string(nextPrefix++);
        } while (frame.bindings.count(prefix) != 0U);
        addDeclaration(frame, prefix, name.namespaceUri);
        return prefix;
    }

    static std::string lexical(std::string_view prefix,
                               std::string_view localName) {
        return prefix.empty() ? std::string(localName)
                              : std::string(prefix) + ":" + std::string(localName);
    }

    void flushStart(Frame& frame, bool selfClosing) {
        if (!frame.startPending) return;
        emit("<");
        emit(frame.lexicalName);
        for (const auto& declaration : frame.declarations) {
            emit(declaration.first.empty() ? " xmlns=\"" : " xmlns:");
            if (!declaration.first.empty()) {
                emit(declaration.first);
                emit("=\"");
            }
            emit(escaped(declaration.second, true));
            emit("\"");
        }
        for (const auto& attribute : frame.attributes) {
            emit(" ");
            emit(attribute.lexicalName);
            emit("=\"");
            emit(escaped(attribute.value, true));
            emit("\"");
        }
        emit(selfClosing ? "/>" : ">");
        frame.startPending = false;
    }
};

XmlWriter::XmlWriter(std::shared_ptr<OutputSink> output,
                     XmlWriterOptions options)
    : impl_(std::make_unique<Impl>(std::move(output), std::move(options))) {
    if (!impl_->output) {
        throw IoxError(DiagnosticCode::InvalidArgument,
                       "XmlWriter requires an output sink");
    }
    validateXmlText(impl_->options.indent);
    validateXmlText(impl_->options.newline);
}

XmlWriter::~XmlWriter() noexcept = default;

void XmlWriter::startDocument() {
    if (impl_->state != Impl::State::BeforeDocument) {
        impl_->fail(DiagnosticCode::WriterStateError,
                    "XML document can only be started once");
    }
    impl_->state = Impl::State::InDocument;
    if (impl_->options.writeDeclaration) {
        impl_->emit("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    }
}

void XmlWriter::startElement(const XmlQualifiedName& name) {
    impl_->ensureDocument();
    if (!impl_->frames.empty()) {
        auto& parent = impl_->frames.back();
        impl_->flushStart(parent, false);
        if (!parent.hasText) impl_->indent(impl_->frames.size());
        parent.hasChild = true;
    } else if (impl_->options.writeDeclaration) {
        impl_->indent(0);
    }

    if (impl_->frames.empty()) {
        if (impl_->rootStarted) {
            impl_->fail(DiagnosticCode::WriterStateError,
                        "An XML document may contain only one root element");
        }
        impl_->rootStarted = true;
    }

    Impl::Frame frame;
    if (!impl_->frames.empty()) frame.bindings = impl_->frames.back().bindings;
    frame.bindings["xml"] = std::string(xmlNamespace);
    impl_->frames.push_back(std::move(frame));
    auto& current = impl_->frames.back();
    const auto prefix = impl_->prefixFor(current, name, false);
    current.lexicalName = Impl::lexical(prefix, name.localName);
}

void XmlWriter::writeNamespace(std::string_view prefix,
                               std::string_view namespaceUri) {
    impl_->ensureDocument();
    if (impl_->frames.empty() || !impl_->frames.back().startPending) {
        impl_->fail(DiagnosticCode::WriterStateError,
                    "Namespaces must be written directly after startElement");
    }
    if (!prefix.empty()) validateName(prefix, "namespace prefix");
    validateXmlText(namespaceUri);
    impl_->addDeclaration(impl_->frames.back(), std::string(prefix),
                          std::string(namespaceUri));
}

void XmlWriter::writeAttribute(const XmlQualifiedName& name,
                               std::string_view value) {
    impl_->ensureDocument();
    if (impl_->frames.empty() || !impl_->frames.back().startPending) {
        impl_->fail(DiagnosticCode::WriterStateError,
                    "Attributes must be written directly after startElement");
    }
    validateXmlText(value);
    auto& frame = impl_->frames.back();
    const auto expanded = std::make_pair(name.namespaceUri, name.localName);
    if (!frame.attributeNames.insert(expanded).second) {
        impl_->fail(DiagnosticCode::UnexpectedAttribute,
                    "Duplicate XML attribute");
    }
    const auto prefix = impl_->prefixFor(frame, name, true);
    frame.attributes.push_back({Impl::lexical(prefix, name.localName),
                                std::string(value)});
}

void XmlWriter::text(std::string_view value) {
    impl_->ensureDocument();
    if (impl_->frames.empty()) {
        impl_->fail(DiagnosticCode::WriterStateError,
                    "XML text requires an open element");
    }
    auto& frame = impl_->frames.back();
    impl_->flushStart(frame, false);
    impl_->emit(escaped(value, false));
    frame.hasText = true;
}

void XmlWriter::endElement() {
    impl_->ensureDocument();
    if (impl_->frames.empty()) {
        impl_->fail(DiagnosticCode::WriterStateError,
                    "No XML element is open");
    }
    auto& frame = impl_->frames.back();
    if (frame.startPending) {
        impl_->flushStart(frame, true);
    } else {
        if (frame.hasChild && !frame.hasText) {
            impl_->indent(impl_->frames.size() - 1U);
        }
        impl_->emit("</");
        impl_->emit(frame.lexicalName);
        impl_->emit(">");
    }
    impl_->frames.pop_back();
    if (impl_->frames.empty()) impl_->rootEnded = true;
}

void XmlWriter::endDocument() {
    impl_->ensureDocument();
    if (!impl_->frames.empty() || !impl_->rootStarted || !impl_->rootEnded) {
        impl_->fail(DiagnosticCode::WriterStateError,
                    "XML document must contain exactly one closed root element");
    }
    if (impl_->options.pretty) impl_->emit(impl_->options.newline);
    impl_->state = Impl::State::AfterDocument;
}

void XmlWriter::flush() {
    if (impl_->state != Impl::State::InDocument &&
        impl_->state != Impl::State::AfterDocument) {
        impl_->fail(DiagnosticCode::WriterStateError,
                    "Cannot flush an XML writer outside a document");
    }
    if (impl_->state == Impl::State::InDocument && !impl_->frames.empty()) {
        impl_->flushStart(impl_->frames.back(), false);
    }
    try {
        impl_->output->flush();
    } catch (const IoxError&) {
        impl_->state = Impl::State::Failed;
        throw;
    } catch (const std::exception& exception) {
        impl_->fail(DiagnosticCode::IoError,
                    std::string("XML output flush failed: ") + exception.what());
    } catch (...) {
        impl_->fail(DiagnosticCode::IoError,
                    "XML output flush failed with an unknown exception");
    }
}

} // namespace xml
} // namespace iox
