#include "iox/xml/XmlWriter.h"

#include <string>
#include <vector>
#include <utility>
#include <cstdio>

namespace iox {
namespace xml {

// ============================================================================
// XmlWriter::Impl
// ============================================================================

struct XmlWriter::Impl {
    WriteFunc writeFunc;
    bool pretty = true;
    int indentSpaces = 2;
    int currentDepth = 0;
    bool hasWritten_ = false;
    bool elementOpen = false;    // true if a start tag has been written but no content/end yet
    std::string openElementName; // name of the currently open element
    std::vector<bool> elementHasText;
    std::vector<bool> elementHasChild;

    void write(const void* data, std::size_t size) {
        if (writeFunc) writeFunc(data, size);
        hasWritten_ = true;
    }

    void writeStr(const std::string& s) {
        write(s.data(), s.size());
    }

    void writeIndent() {
        if (!pretty) return;
        writeStr("\n");
        for (int i = 0; i < currentDepth * indentSpaces; ++i) {
            writeStr(" ");
        }
    }

    void closeOpenElement() {
        if (elementOpen) {
            writeStr(">");
            elementOpen = false;
        }
    }

    void writeEscaped(std::string_view text) {
        for (char c : text) {
            switch (c) {
            case '&':  writeStr("&amp;"); break;
            case '<':  writeStr("&lt;"); break;
            case '>':  writeStr("&gt;"); break;
            case '"':  writeStr("&quot;"); break;
            case '\'': writeStr("&apos;"); break;
            default:
                write(&c, 1);
            }
        }
    }
};

// ============================================================================
// XmlWriter — public API
// ============================================================================

XmlWriter::XmlWriter(WriteFunc writeFunc)
    : impl_(std::make_unique<Impl>()) {
    impl_->writeFunc = std::move(writeFunc);
}

XmlWriter::XmlWriter(WriteFunc writeFunc, bool pretty, int indentSpaces)
    : impl_(std::make_unique<Impl>()) {
    impl_->writeFunc = std::move(writeFunc);
    impl_->pretty = pretty;
    impl_->indentSpaces = indentSpaces;
}

XmlWriter::~XmlWriter() = default;

void XmlWriter::writeDeclaration() {
    impl_->writeStr("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    if (impl_->pretty) impl_->writeStr("\n");
}

void XmlWriter::writeStartElement(std::string_view name,
                                   const std::vector<std::pair<std::string, std::string>>& attributes,
                                   bool selfClosing) {
    impl_->closeOpenElement();
    if (!impl_->elementHasChild.empty()) impl_->elementHasChild.back() = true;
    impl_->writeIndent();
    impl_->writeStr("<");
    impl_->writeStr(std::string(name));

    for (const auto& attr : attributes) {
        impl_->writeStr(" ");
        impl_->writeStr(attr.first);
        impl_->writeStr("=\"");
        impl_->writeEscaped(attr.second);
        impl_->writeStr("\"");
    }

    if (selfClosing) {
        impl_->writeStr("/>");
    } else {
        impl_->elementOpen = true;
        impl_->openElementName = std::string(name);
        impl_->elementHasText.push_back(false);
        impl_->elementHasChild.push_back(false);
        ++impl_->currentDepth;
    }
}

void XmlWriter::writeEndElement(std::string_view name) {
    --impl_->currentDepth;

    const bool hasText = !impl_->elementHasText.empty() && impl_->elementHasText.back();

    if (impl_->elementOpen &&
        impl_->openElementName == std::string(name)) {
        // No content was written between start and end — use self-closing
        impl_->elementOpen = false;
        impl_->writeStr("/>");
        if (!impl_->elementHasText.empty()) impl_->elementHasText.pop_back();
        if (!impl_->elementHasChild.empty()) impl_->elementHasChild.pop_back();
        return;
    }

    impl_->closeOpenElement();
    if (!hasText) impl_->writeIndent();
    impl_->writeStr("</");
    impl_->writeStr(std::string(name));
    impl_->writeStr(">");
    if (!impl_->elementHasText.empty()) impl_->elementHasText.pop_back();
    if (!impl_->elementHasChild.empty()) impl_->elementHasChild.pop_back();
}

void XmlWriter::writeText(std::string_view text) {
    impl_->closeOpenElement();
    if (!impl_->elementHasText.empty()) impl_->elementHasText.back() = true;
    impl_->writeEscaped(text);
}

void XmlWriter::writeComment(std::string_view text) {
    impl_->closeOpenElement();
    impl_->writeIndent();
    impl_->writeStr("<!-- ");
    // Comments must not contain "--" — replace with "- -" for safety
    std::string safe(text);
    for (std::size_t i = 0; i + 1 < safe.size(); ++i) {
        if (safe[i] == '-' && safe[i + 1] == '-') {
            safe.insert(i + 1, " ");
        }
    }
    impl_->writeStr(safe);
    impl_->writeStr(" -->");
}

void XmlWriter::flush() {
    // Nothing to flush — writes are immediate
}

bool XmlWriter::hasWritten() const noexcept {
    return impl_->hasWritten_;
}

int XmlWriter::depth() const noexcept {
    return impl_->currentDepth;
}

} // namespace xml
} // namespace iox
