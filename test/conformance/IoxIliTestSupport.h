#pragma once

#include "iox/Events.h"
#include "iox/Reader.h"
#include "iox/Writer.h"
#include "iox/xtf/XtfReader.h"
#include "iox/xtf/XtfWriter.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace iox::conformance {

struct ParsedFixture final {
    std::vector<IoxEvent> events;
    std::vector<Diagnostic> diagnostics;
    bool ended = false;
};

inline std::string readFixture(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) return {};
    const auto end = input.tellg();
    if (end < 0) return {};
    input.seekg(0);
    std::string data(static_cast<std::size_t>(end), '\0');
    if (!data.empty()) input.read(data.data(), static_cast<std::streamsize>(data.size()));
    return data;
}

inline void appendString(std::ostringstream& output, std::string_view value) {
    output << value.size() << ':' << value;
}

inline void appendName(std::ostringstream& output, const IomName& name) {
    appendString(output, name.iliName());
    if (name.xmlName()) {
        output << "<";
        appendString(output, name.xmlName()->namespaceUri);
        appendString(output, name.xmlName()->localName);
        appendString(output, name.xmlName()->prefixHint);
        output << ">";
    } else {
        output << "-";
    }
}

inline void appendValue(std::ostringstream& output, const IomValue& value);
inline void appendValue(std::ostringstream& output, const IomObject& value);
inline void appendValue(std::ostringstream& output,
                        const IomAttribute::AttrValue& value);

inline void appendObject(std::ostringstream& output, const IomObject& object) {
    output << "object{";
    appendName(output, object.tag());
    if (object.ref()) {
        output << "ref=";
        appendString(output, *object.ref());
    } else {
        output << "ref=-";
    }
    if (object.bid()) {
        output << "bid=";
        appendString(output, *object.bid());
    } else {
        output << "bid=-";
    }
    if (object.orderPos()) {
        output << "pos=" << *object.orderPos();
    } else {
        output << "pos=-";
    }
    output << "attrs=" << object.attributeCount() << '[';
    for (std::size_t index = 0; index < object.attributeCount(); ++index) {
        const auto& attribute = object.attributeAt(index);
        appendName(output, attribute.name);
        if (attribute.ref) {
            output << "ref=";
            appendString(output, *attribute.ref);
        } else {
            output << "ref=-";
        }
        if (attribute.bid) {
            output << "bid=";
            appendString(output, *attribute.bid);
        } else {
            output << "bid=-";
        }
        if (attribute.orderPos) {
            output << "pos=" << *attribute.orderPos;
        } else {
            output << "pos=-";
        }
        output << "values=" << attribute.values.size() << '[';
        for (const auto& value : attribute.values) appendValue(output, value);
        output << "]";
    }
    output << "]}";
}

inline void appendValue(std::ostringstream& output, const IomValue& value) {
    switch (value.kind()) {
    case IomValue::Kind::Null:
        output << "null;";
        break;
    case IomValue::Kind::Text:
        output << "text=";
        appendString(output, value.asText());
        output << ';';
        break;
    case IomValue::Kind::Integer:
        output << "integer=" << value.asInteger() << ';';
        break;
    case IomValue::Kind::Decimal:
        output << "decimal=" << std::setprecision(17) << value.asDecimal() << ';';
        break;
    case IomValue::Kind::Boolean:
        output << "boolean=" << (value.asBoolean() ? "true" : "false") << ';';
        break;
    }
}

inline void appendValue(std::ostringstream& output, const IomObject& value) {
    appendObject(output, value);
}

inline void appendValue(std::ostringstream& output,
                        const IomAttribute::AttrValue& value) {
    std::visit([&output](const auto& item) {
        appendValue(output, item);
    }, value);
}

inline std::string eventFingerprint(const IoxEvent& event) {
    std::ostringstream output;
    std::visit([&output](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, StartTransferEvent>) {
            output << "startTransfer{";
            appendString(output, value.sender);
            appendString(output, value.comment);
            appendString(output, value.iliVersion);
            appendString(output, value.software);
            appendString(output, value.date);
            output << "version=";
            if (value.version) output << *value.version;
            else output << '-';
            output << '}';
        } else if constexpr (std::is_same_v<T, StartBasketEvent>) {
            output << "startBasket{";
            appendName(output, value.basketType);
            appendString(output, value.bid);
            appendString(output, value.consistency);
            appendString(output, value.operation);
            output << "domains=" << value.domains.size() << '[';
            for (const auto& domain : value.domains) appendString(output, domain);
            output << ']';
            if (value.oidDomain) output << "oid=" << *value.oidDomain;
            else output << "oid=-";
            if (value.startState) { output << "start="; appendString(output, *value.startState); }
            else output << "start=-";
            if (value.endState) { output << "end="; appendString(output, *value.endState); }
            else output << "end=-";
            if (value.kind) { output << "kind="; appendString(output, *value.kind); }
            else output << "kind=-";
            output << '}';
        } else if constexpr (std::is_same_v<T, ObjectEvent>) {
            output << "objectEvent{";
            appendObject(output, value.object);
            appendString(output, value.operation);
            appendString(output, value.objectId);
            if (value.consistency) { output << "consistency="; appendString(output, *value.consistency); }
            else output << "consistency=-";
            if (value.refBid) { output << "refBid="; appendString(output, *value.refBid); }
            else output << "refBid=-";
            if (value.refOrderPos) { output << "refOrder="; appendString(output, *value.refOrderPos); }
            else output << "refOrder=-";
            output << '}';
        } else if constexpr (std::is_same_v<T, EndBasketEvent>) {
            output << "endBasket{";
            appendString(output, value.bid);
            output << '}';
        } else {
            output << "endTransfer";
        }
    }, event);
    return output.str();
}

inline std::vector<std::string> eventFingerprints(const std::vector<IoxEvent>& events) {
    std::vector<std::string> result;
    result.reserve(events.size());
    for (const auto& event : events) result.push_back(eventFingerprint(event));
    return result;
}

inline std::string semanticEventFingerprint(const IoxEvent& event) {
    if (const auto* transfer = std::get_if<StartTransferEvent>(&event)) {
        std::ostringstream output;
        output << "startTransfer{version=";
        if (transfer->version) output << *transfer->version;
        else output << '-';
        output << '}';
        return output.str();
    }
    if (const auto* basket = std::get_if<StartBasketEvent>(&event)) {
        auto normalized = *basket;
        if (normalized.consistency.empty()) normalized.consistency = "complete";
        if (normalized.operation.empty()) normalized.operation = "insert";
        return eventFingerprint(IoxEvent{std::move(normalized)});
    }
    return eventFingerprint(event);
}

inline std::vector<std::string> semanticEventFingerprints(
    const std::vector<IoxEvent>& events) {
    std::vector<std::string> result;
    result.reserve(events.size());
    for (const auto& event : events) result.push_back(semanticEventFingerprint(event));
    return result;
}

inline std::vector<std::string> diagnosticFingerprints(
    const std::vector<Diagnostic>& diagnostics) {
    std::vector<std::string> result;
    result.reserve(diagnostics.size());
    for (const auto& diagnostic : diagnostics) {
        result.push_back(std::to_string(static_cast<int>(diagnostic.severity)) + ":" +
                         diagnostic.code);
    }
    return result;
}

inline std::vector<std::string> diagnosticContract(
    const std::vector<Diagnostic>& diagnostics) {
    for (const auto& diagnostic : diagnostics) {
        if (diagnostic.severity == Diagnostic::Severity::Fatal) {
            return {"fatal:" + diagnostic.code};
        }
    }
    return diagnosticFingerprints(diagnostics);
}

inline ParsedFixture parseBytes(std::string_view data, std::size_t chunkSize = 0,
                                xtf::XtfReaderOptions options = {}) {
    xtf::XtfReader reader(std::move(options));
    if (chunkSize == 0) {
        reader.feed(ByteView(data));
    } else {
        for (std::size_t offset = 0; offset < data.size(); offset += chunkSize) {
            reader.feed(ByteView(data.data() + offset,
                                  std::min(chunkSize, data.size() - offset)));
        }
    }
    reader.finish();

    ParsedFixture result;
    while (true) {
        auto outcome = reader.next();
        result.diagnostics.insert(result.diagnostics.end(),
                                  outcome.diagnostics.begin(), outcome.diagnostics.end());
        if (outcome.event) result.events.push_back(std::move(*outcome.event));
        if (outcome.status == ReadOutcome::Status::End) {
            result.ended = true;
            break;
        }
        if (outcome.status == ReadOutcome::Status::NeedInput) break;
    }
    const auto remaining = reader.takeDiagnostics();
    result.diagnostics.insert(result.diagnostics.end(), remaining.begin(), remaining.end());
    return result;
}

inline ParsedFixture parseFixture(const std::filesystem::path& path,
                                  std::size_t chunkSize = 0) {
    const auto data = readFixture(path);
    return parseBytes(data, chunkSize,
                      xtf::XtfReaderOptions{false, path.string(), std::nullopt, true});
}

inline std::string writeEvents(const std::vector<IoxEvent>& events,
                               xtf::XtfVersion version) {
    auto sink = std::make_shared<StringOutputSink>();
    xtf::XtfWriterOptions options;
    options.version = version;
    options.pretty = false;
    options.sender = "iox-ili-porting-matrix";
    options.software = "iox-cpp";
    xtf::XtfWriter writer(sink, options);
    for (const auto& event : events) writer.write(event);
    writer.close();
    return sink->str();
}

inline std::vector<std::filesystem::path> transferFixtures(
    const std::filesystem::path& root) {
    std::vector<std::filesystem::path> result;
    if (!std::filesystem::exists(root)) return result;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file()) continue;
        const auto extension = entry.path().extension().string();
        if (extension == ".xtf" || extension == ".xml") result.push_back(entry.path());
    }
    std::sort(result.begin(), result.end());
    return result;
}

} // namespace iox::conformance
