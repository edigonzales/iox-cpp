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
    if (!data.empty()) {
        input.read(data.data(), static_cast<std::streamsize>(data.size()));
    }
    return data;
}

inline void appendString(std::ostringstream& output, std::string_view value) {
    output << value.size() << ':' << value;
}

inline void appendName(std::ostringstream& output, const IomName& name) {
    appendString(output, name.interlisName());
    if (name.hasXmlName()) {
        output << '<';
        appendString(output, name.xmlName().namespaceUri);
        appendString(output, name.xmlName().localName);
        appendString(output, name.xmlName().prefixHint);
        output << '>';
    } else {
        output << '-';
    }
}

inline void appendOptional(std::ostringstream& output,
                           const std::optional<std::string>& value) {
    if (value) appendString(output, *value);
    else output << '-';
}

inline void appendObject(std::ostringstream& output,
                         const IomObject& object) {
    output << "object{";
    appendName(output, object.tag());
    appendOptional(output, object.oid());
    output << "op=" << static_cast<int>(object.operation())
           << "consistency=" << static_cast<int>(object.consistency());
    appendOptional(output, object.reference().targetOid);
    appendOptional(output, object.reference().targetBasketId);
    if (object.reference().orderPosition) {
        output << *object.reference().orderPosition;
    } else {
        output << '-';
    }
    output << "attrs=" << object.attributeCount() << '[';
    for (std::size_t attributeIndex = 0;
         attributeIndex < object.attributeCount(); ++attributeIndex) {
        const auto& name = object.attributeName(attributeIndex);
        appendName(output, name);
        const auto count = object.valueCount(name.interlisName());
        output << "values=" << count << '[';
        for (std::size_t valueIndex = 0; valueIndex < count; ++valueIndex) {
            const auto& value = object.value(name.interlisName(), valueIndex);
            if (value.isPrimitive()) {
                output << "primitive=";
                appendString(output, value.primitive());
            } else {
                appendObject(output, value.object());
            }
        }
        output << ']';
    }
    output << "]}";
}

inline std::string eventFingerprint(const IoxEvent& event) {
    std::ostringstream output;
    std::visit([&output](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, StartTransferEvent>) {
            output << "startTransfer{" << static_cast<int>(value.header.version);
            appendString(output, value.header.sender);
            appendOptional(output, value.header.comment);
            output << "models=" << value.header.models.size();
            for (const auto& model : value.header.models) {
                appendString(output, model.name);
                appendOptional(output, model.version);
                appendOptional(output, model.uri);
                appendString(output, model.xmlNamespace.expanded());
            }
            output << "oids=" << value.header.oidSpaces.size();
            for (const auto& oid : value.header.oidSpaces) {
                appendString(output, oid.name);
                appendString(output, oid.domain);
            }
            output << '}';
        } else if constexpr (std::is_same_v<T, StartBasketEvent>) {
            output << "startBasket{";
            appendName(output, value.basket.topic);
            appendString(output, value.basket.basketId);
            output << static_cast<int>(value.basket.kind)
                   << static_cast<int>(value.basket.consistency);
            appendOptional(output, value.basket.startState);
            appendOptional(output, value.basket.endState);
            output << "domains=" << value.basket.domains.size();
            for (const auto& item : value.basket.domains) appendString(output, item);
            output << "topics=" << value.basket.topics.size();
            for (const auto& item : value.basket.topics) appendString(output, item);
            output << '}';
        } else if constexpr (std::is_same_v<T, ObjectEvent>) {
            output << "objectEvent{";
            appendObject(output, value.object);
            output << '}';
        } else if constexpr (std::is_same_v<T, EndBasketEvent>) {
            output << "endBasket";
        } else {
            output << "endTransfer";
        }
    }, event);
    return output.str();
}

inline std::vector<std::string> eventFingerprints(
    const std::vector<IoxEvent>& events) {
    std::vector<std::string> result;
    result.reserve(events.size());
    for (const auto& event : events) result.push_back(eventFingerprint(event));
    return result;
}

inline std::string semanticEventFingerprint(const IoxEvent& event) {
    const auto normalizeName = [](const IomName& name) {
        if (name.hasXmlName() &&
            (name.xmlName().namespaceUri.empty() ||
             name.xmlName().namespaceUri ==
                 "http://www.interlis.ch/INTERLIS2.3")) {
            return IomName(name.interlisName());
        }
        return name;
    };
    const auto normalizeObject = [](const IomObject& source,
                                    const auto& self,
                                    const auto& normalize) -> IomObject {
        IomObject result(normalize(source.tag()), source.oid());
        result.setOperation(source.operation());
        result.setConsistency(
            source.consistency() == Consistency::Unspecified
                ? Consistency::Complete
                : source.consistency());
        result.setReference(source.reference());
        result.setSourceLocation(source.sourceLocation());
        for (std::size_t attributeIndex = 0;
             attributeIndex < source.attributeCount(); ++attributeIndex) {
            const auto& name = source.attributeName(attributeIndex);
            for (std::size_t valueIndex = 0;
                 valueIndex < source.valueCount(name.interlisName());
                 ++valueIndex) {
                const auto& value =
                    source.value(name.interlisName(), valueIndex);
                if (value.isPrimitive()) {
                    result.appendPrimitive(normalize(name), value.primitive());
                } else {
                    auto nested = self(value.object(), self, normalize);
                    if (nested.isReference()) nested.setTag(normalize(name));
                    result.appendObject(normalize(name), std::move(nested));
                }
            }
        }
        return result;
    };
    if (const auto* basket = std::get_if<StartBasketEvent>(&event)) {
        auto normalized = *basket;
        normalized.basket.topic = normalizeName(normalized.basket.topic);
        if (normalized.basket.kind == BasketKind::Unspecified) {
            normalized.basket.kind = BasketKind::Full;
        }
        if (normalized.basket.consistency == Consistency::Unspecified) {
            normalized.basket.consistency = Consistency::Complete;
        }
        return eventFingerprint(IoxEvent{std::move(normalized)});
    }
    if (const auto* object = std::get_if<ObjectEvent>(&event)) {
        return eventFingerprint(
            IoxEvent{ObjectEvent{normalizeObject(object->object,
                                                 normalizeObject,
                                                 normalizeName)}});
    }
    return eventFingerprint(event);
}

inline std::vector<std::string> semanticEventFingerprints(
    const std::vector<IoxEvent>& events) {
    std::vector<std::string> result;
    result.reserve(events.size());
    for (const auto& event : events) {
        result.push_back(semanticEventFingerprint(event));
    }
    return result;
}

inline std::vector<std::string> diagnosticFingerprints(
    const std::vector<Diagnostic>& diagnostics) {
    std::vector<std::string> result;
    result.reserve(diagnostics.size());
    for (const auto& diagnostic : diagnostics) {
        result.push_back(
            std::to_string(static_cast<int>(diagnostic.severity)) + ':' +
            std::string(diagnosticCodeName(diagnostic.code)));
    }
    return result;
}

inline std::vector<std::string> diagnosticContract(
    const std::vector<Diagnostic>& diagnostics) {
    for (const auto& diagnostic : diagnostics) {
        if (diagnostic.severity == DiagnosticSeverity::Fatal) {
            return {"fatal:" + std::string(diagnosticCodeName(diagnostic.code))};
        }
    }
    return diagnosticFingerprints(diagnostics);
}

inline ParsedFixture parseBytes(std::string_view data,
                                std::size_t chunkSize = 0,
                                xtf::XtfReaderOptions options = {}) {
    xtf::XtfReader reader(std::move(options));
    ParsedFixture result;
    try {
        if (chunkSize == 0) {
            reader.feed(ByteView(
                reinterpret_cast<const std::uint8_t*>(data.data()),
                data.size()));
        } else {
            for (std::size_t offset = 0; offset < data.size();
                 offset += chunkSize) {
                reader.feed(ByteView(
                    reinterpret_cast<const std::uint8_t*>(data.data() + offset),
                    std::min(chunkSize, data.size() - offset)));
            }
        }
        reader.finish();
        while (true) {
            auto outcome = reader.next();
            if (outcome.event) result.events.push_back(std::move(*outcome.event));
            if (outcome.progress == ReaderProgress::End) {
                result.ended = true;
                break;
            }
            if (outcome.progress == ReaderProgress::NeedInput) break;
        }
    } catch (const IoxError& error) {
        result.diagnostics.push_back({DiagnosticSeverity::Fatal, error.code(),
                                      error.what(), error.location(), {}});
    }
    auto remaining = reader.takeDiagnostics();
    result.diagnostics.insert(result.diagnostics.end(),
                              std::make_move_iterator(remaining.begin()),
                              std::make_move_iterator(remaining.end()));
    return result;
}

inline ParsedFixture parseFixture(const std::filesystem::path& path,
                                  std::size_t chunkSize = 0) {
    auto options = xtf::XtfReaderOptions{};
    options.sourceName = path.string();
    options.preserveUnknownExtensions = true;
    return parseBytes(readFixture(path), chunkSize, std::move(options));
}

inline std::string writeEvents(const std::vector<IoxEvent>& events,
                               xtf::XtfVersion version) {
    auto sink = std::make_shared<StringOutputSink>();
    xtf::XtfWriterOptions options;
    options.version = version;
    options.pretty = false;
    options.sender = "iox-ili-porting-matrix";
    xtf::XtfWriter writer(sink, options);
    for (const auto& event : events) writer.write(event);
    writer.close();
    return sink->str();
}

inline std::vector<std::filesystem::path> transferFixtures(
    const std::filesystem::path& root) {
    std::vector<std::filesystem::path> result;
    if (!std::filesystem::exists(root)) return result;
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file()) continue;
        const auto extension = entry.path().extension().string();
        if (extension == ".xtf" || extension == ".xml") {
            result.push_back(entry.path());
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

} // namespace iox::conformance
