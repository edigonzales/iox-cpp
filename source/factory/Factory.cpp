#include "iox/Factory.h"

#include "iox/xtf/XtfReader.h"
#include "iox/xtf/XtfWriter.h"

#if IOX_FACTORY_ENABLE_JSON
#include "iox/json/JsonEventReader.h"
#include "iox/json/JsonEventWriter.h"
#endif

#include <algorithm>
#include <cctype>
#include <memory>

namespace iox {

namespace {

std::string lowerAscii(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const auto character : value) {
        result.push_back(static_cast<char>(
            std::tolower(static_cast<unsigned char>(character))));
    }
    return result;
}

std::string extensionOf(std::string_view sourceName) {
    const auto slash = sourceName.find_last_of("/\\");
    const auto dot = sourceName.find_last_of('.');
    if (dot == std::string_view::npos ||
        (slash != std::string_view::npos && dot < slash)) {
        return {};
    }
    return lowerAscii(sourceName.substr(dot));
}

int xtfScore(ByteView prefix) {
    const auto text = lowerAscii(prefix.sv());
    if (text.find("<transfer") != std::string::npos ||
        text.find(":transfer") != std::string::npos) {
        return 100;
    }
    return 0;
}

#if IOX_FACTORY_ENABLE_JSON
int jsonScore(ByteView prefix) {
    const auto text = prefix.sv();
    const auto first = text.find_first_not_of(" \t\r\n");
    return first != std::string_view::npos && text[first] == '{' ? 100 : 0;
}
#endif

void registerBuiltIns(FormatRegistry& registry) {
    FormatEntry xtf;
    xtf.name = "xtf";
    xtf.description = "INTERLIS XTF 2.3/2.4";
    xtf.extensions = {".xtf", ".xml"};
    xtf.mimeTypes = {"application/xml", "application/interlis-xtf"};
    xtf.scoreSniffer = xtfScore;
    xtf.readerFactory = [] { return std::make_unique<xtf::XtfReader>(); };
    xtf.writerFactory = [](std::shared_ptr<OutputSink> output) {
        return std::make_unique<xtf::XtfWriter>(std::move(output));
    };
    registry.addFormat(std::move(xtf));

#if IOX_FACTORY_ENABLE_JSON
    FormatEntry jsonFormat;
    jsonFormat.name = "json-events";
    jsonFormat.description = "iox newline-delimited event JSON";
    jsonFormat.extensions = {".jsonl", ".ndjson"};
    jsonFormat.mimeTypes = {"application/x-ndjson"};
    jsonFormat.scoreSniffer = jsonScore;
    jsonFormat.readerFactory = [] {
        return std::make_unique<json::JsonEventReader>();
    };
    jsonFormat.writerFactory = [](std::shared_ptr<OutputSink> output) {
        return std::make_unique<json::JsonEventWriter>(std::move(output));
    };
    registry.addFormat(std::move(jsonFormat));
#endif
}

} // namespace

FormatRegistry& defaultFormatRegistry() {
    static FormatRegistry registry = [] {
        FormatRegistry value;
        registerBuiltIns(value);
        return value;
    }();
    return registry;
}

std::unique_ptr<Reader> ReaderFactory::create(
    std::string_view sourceName, ByteView prefix) {
    return defaultFormatRegistry().createReaderBySniffing(
        prefix, extensionOf(sourceName));
}

std::unique_ptr<Reader> ReaderFactory::createByName(
    std::string_view formatName) {
    return defaultFormatRegistry().createReader(formatName);
}

std::unique_ptr<Writer> WriterFactory::create(
    std::string_view formatName, std::shared_ptr<OutputSink> output) {
    return defaultFormatRegistry().createWriter(formatName, std::move(output));
}

} // namespace iox
