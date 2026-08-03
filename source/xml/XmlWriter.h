#pragma once

#include "iox/IomName.h"
#include "iox/Writer.h"

#include <memory>
#include <string>
#include <string_view>

namespace iox {
namespace xml {

struct XmlWriterOptions final {
    bool pretty = true;
    std::string indent = "  ";
    std::string newline = "\n";
    bool writeDeclaration = true;
};

class XmlWriter final {
public:
    XmlWriter(std::shared_ptr<OutputSink> output,
              XmlWriterOptions options = {});
    ~XmlWriter() noexcept;

    XmlWriter(const XmlWriter&) = delete;
    XmlWriter& operator=(const XmlWriter&) = delete;

    void startDocument();
    void startElement(const XmlQualifiedName& name);
    void writeNamespace(std::string_view prefix,
                        std::string_view namespaceUri);
    void writeAttribute(const XmlQualifiedName& name,
                        std::string_view value);
    void text(std::string_view value);
    void endElement();
    void endDocument();
    void flush();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace xml
} // namespace iox
