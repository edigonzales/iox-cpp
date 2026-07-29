#include "iox/abi/iox.h"
#include "iox/Version.h"
#include "iox/Reader.h"
#include "iox/Writer.h"
#include "iox/xtf/XtfReader.h"
#include "iox/xtf/XtfWriter.h"
#include "iox/xtf/XtfReaderOptions.h"
#include "iox/json/JsonEventReader.h"
#include "iox/json/JsonEventWriter.h"

#include <algorithm>
#include <cstdlib>
#include <cstddef>
#include <cctype>
#include <cstring>
#include <iterator>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// The C ABI deliberately owns all result memory.  No C++ object, exception,
// or std::string crosses the boundary.

struct iox_result {
    std::string json;
    std::vector<std::uint8_t> bytes;
    iox_status_t status = IOX_STATUS_OK;
};

struct iox_reader {
    std::unique_ptr<iox::Reader> impl;
    std::vector<iox::Diagnostic> diagnostics;
};

struct iox_writer {
    std::shared_ptr<iox::StringOutputSink> sink;
    std::unique_ptr<iox::Writer> impl;
    bool finished = false;
};

namespace {

const char* statusName(iox_status_t status) noexcept {
    switch (status) {
    case IOX_STATUS_OK: return "ok";
    case IOX_STATUS_EVENT: return "event";
    case IOX_STATUS_NEED_INPUT: return "need_input";
    case IOX_STATUS_END: return "end";
    case IOX_STATUS_ERROR: return "error";
    case IOX_STATUS_INVALID_ARGUMENT: return "invalid_argument";
    case IOX_STATUS_INVALID_STATE: return "invalid_state";
    }
    return "error";
}

void appendJsonString(std::string& out, const std::string& value) {
    out.push_back('"');
    for (const char character : value) {
        const auto c = static_cast<unsigned char>(character);
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                static constexpr char hex[] = "0123456789abcdef";
                out += "\\u00";
                out.push_back(hex[(c >> 4) & 0x0f]);
                out.push_back(hex[c & 0x0f]);
            } else {
                out.push_back(static_cast<char>(c));
            }
        }
    }
    out.push_back('"');
}

const char* severityName(iox::Diagnostic::Severity severity) noexcept {
    switch (severity) {
    case iox::Diagnostic::Severity::Warning: return "Warning";
    case iox::Diagnostic::Severity::Error: return "Error";
    case iox::Diagnostic::Severity::Fatal: return "Fatal";
    }
    return "Error";
}

bool isError(const iox::Diagnostic& diagnostic) noexcept {
    return diagnostic.severity == iox::Diagnostic::Severity::Error ||
           diagnostic.severity == iox::Diagnostic::Severity::Fatal;
}

bool hasError(const std::vector<iox::Diagnostic>& diagnostics) noexcept {
    return std::any_of(diagnostics.begin(), diagnostics.end(), isError);
}

void appendDiagnostic(std::string& out, const iox::Diagnostic& diagnostic) {
    out += "{\"severity\":";
    appendJsonString(out, severityName(diagnostic.severity));
    out += ",\"code\":";
    appendJsonString(out, diagnostic.code);
    out += ",\"message\":";
    appendJsonString(out, diagnostic.message);
    if (diagnostic.location) {
        const auto& location = *diagnostic.location;
        out += ",\"location\":{\"sourceName\":";
        appendJsonString(out, location.sourceName);
        out += ",\"byteOffset\":" + std::to_string(location.byteOffset);
        out += ",\"line\":" + std::to_string(location.line);
        out += ",\"column\":" + std::to_string(location.column);
        out += "}";
    }
    out.push_back('}');
}

std::string diagnosticsJson(const std::vector<iox::Diagnostic>& diagnostics) {
    std::string out = "[";
    for (std::size_t i = 0; i < diagnostics.size(); ++i) {
        if (i != 0) out.push_back(',');
        appendDiagnostic(out, diagnostics[i]);
    }
    out.push_back(']');
    return out;
}

std::string resultJson(iox_status_t status,
                       const std::vector<iox::Diagnostic>& diagnostics,
                       const std::string* eventJson = nullptr) {
    std::string out = "{\"ok\":";
    out += status == IOX_STATUS_ERROR || status == IOX_STATUS_INVALID_ARGUMENT ||
                   status == IOX_STATUS_INVALID_STATE ? "false" : "true";
    out += ",\"status\":";
    appendJsonString(out, statusName(status));
    if (eventJson != nullptr && status == IOX_STATUS_EVENT) {
        out += ",\"event\":" + *eventJson;
    }
    if (status == IOX_STATUS_ERROR || status == IOX_STATUS_INVALID_ARGUMENT ||
        status == IOX_STATUS_INVALID_STATE) {
        const iox::Diagnostic* error = nullptr;
        for (const auto& diagnostic : diagnostics) {
            if (isError(diagnostic)) {
                error = &diagnostic;
                break;
            }
        }
        iox::Diagnostic fallback{iox::Diagnostic::Severity::Error,
                                 status == IOX_STATUS_INVALID_ARGUMENT
                                     ? iox::ErrorCode::InvalidArgument
                                     : status == IOX_STATUS_INVALID_STATE
                                           ? iox::ErrorCode::InvalidState
                                           : iox::ErrorCode::InternalError,
                                 statusName(status), std::nullopt};
        if (error == nullptr) error = &fallback;
        out += ",\"error\":{";
        out += "\"code\":";
        appendJsonString(out, error->code);
        out += ",\"message\":";
        appendJsonString(out, error->message);
        if (error->location) {
            out += ",\"location\":{";
            const auto& location = *error->location;
            out += "\"sourceName\":";
            appendJsonString(out, location.sourceName);
            out += ",\"byteOffset\":" + std::to_string(location.byteOffset);
            out += ",\"line\":" + std::to_string(location.line);
            out += ",\"column\":" + std::to_string(location.column);
            out += "}";
        }
        out += "}";
    }
    out += ",\"diagnostics\":" + diagnosticsJson(diagnostics) + "}";
    return out;
}

iox_result* makeResult(iox_status_t status,
                       const std::vector<iox::Diagnostic>& diagnostics = {},
                       const std::string* eventJson = nullptr) {
    auto* result = new (std::nothrow) iox_result;
    if (result == nullptr) return nullptr;
    result->status = status;
    result->json = resultJson(status, diagnostics, eventJson);
    return result;
}

void appendDiagnostics(std::vector<iox::Diagnostic>& target,
                       std::vector<iox::Diagnostic> source) {
    target.insert(target.end(),
                  std::make_move_iterator(source.begin()),
                  std::make_move_iterator(source.end()));
}

std::string eventJson(const iox::IoxEvent& event) {
    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::json::JsonEventWriter writer(sink);
    writer.write(event);
    writer.close();
    auto json = sink->str();
    if (!json.empty() && json.back() == '\n') json.pop_back();

    // The native NDJSON spelling is kept for compatibility.  ABI schema v1
    // uses the explicit lower-camel discriminator.
    static constexpr const char* names[][2] = {
        {"\"type\":\"StartTransfer\"", "\"event\":\"startTransfer\""},
        {"\"type\":\"StartBasket\"", "\"event\":\"startBasket\""},
        {"\"type\":\"Object\"", "\"event\":\"object\""},
        {"\"type\":\"EndBasket\"", "\"event\":\"endBasket\""},
        {"\"type\":\"EndTransfer\"", "\"event\":\"endTransfer\""}
    };
    for (const auto& name : names) {
        const std::string from(name[0]);
        const auto position = json.find(from);
        if (position != std::string::npos) {
            json.replace(position, from.size(), name[1]);
            break;
        }
    }
    return json;
}

void takeReaderDiagnostics(iox_reader& reader) {
    if (reader.impl) appendDiagnostics(reader.diagnostics, reader.impl->takeDiagnostics());
}

void takeWriterDiagnostics(iox_writer& writer,
                           std::vector<iox::Diagnostic>& diagnostics) {
    if (writer.impl) appendDiagnostics(diagnostics, writer.impl->takeDiagnostics());
}

void setResult(iox_result_t** output, iox_result* result) {
    if (output != nullptr) *output = reinterpret_cast<iox_result_t*>(result);
}

iox_status_t invalidArgument(iox_result_t** output, const char* message) {
    if (output == nullptr) return IOX_STATUS_INVALID_ARGUMENT;
    std::vector<iox::Diagnostic> diagnostics;
    diagnostics.push_back({iox::Diagnostic::Severity::Error,
                           iox::ErrorCode::InvalidArgument, message, std::nullopt});
    auto* result = makeResult(IOX_STATUS_INVALID_ARGUMENT, diagnostics);
    setResult(output, result);
    return IOX_STATUS_INVALID_ARGUMENT;
}

std::optional<std::string> jsonStringOption(const char* json,
                                            std::string_view key) {
    if (json == nullptr) return std::nullopt;
    const std::string input(json);
    const std::string needle = "\"" + std::string(key) + "\"";
    const auto keyPos = input.find(needle);
    if (keyPos == std::string::npos) return std::nullopt;
    auto pos = input.find(':', keyPos + needle.size());
    if (pos == std::string::npos) return std::nullopt;
    do { ++pos; } while (pos < input.size() &&
                         std::isspace(static_cast<unsigned char>(input[pos])));
    if (pos >= input.size() || input[pos] != '"') return std::nullopt;
    ++pos;
    std::string value;
    bool escaped = false;
    for (; pos < input.size(); ++pos) {
        const char character = input[pos];
        if (escaped) {
            value.push_back(character);
            escaped = false;
        } else if (character == '\\') {
            escaped = true;
        } else if (character == '"') {
            return value;
        } else {
            value.push_back(character);
        }
    }
    return std::nullopt;
}

std::optional<bool> jsonBoolOption(const char* json, std::string_view key) {
    if (json == nullptr) return std::nullopt;
    const std::string input(json);
    const std::string needle = "\"" + std::string(key) + "\"";
    const auto keyPos = input.find(needle);
    if (keyPos == std::string::npos) return std::nullopt;
    auto pos = input.find(':', keyPos + needle.size());
    if (pos == std::string::npos) return std::nullopt;
    do { ++pos; } while (pos < input.size() &&
                         std::isspace(static_cast<unsigned char>(input[pos])));
    if (input.compare(pos, 4, "true") == 0) return true;
    if (input.compare(pos, 5, "false") == 0) return false;
    return std::nullopt;
}

} // namespace

extern "C" {

uint32_t iox_abi_version(void) { return iox::abiVersion(); }
const char* iox_version(void) { return iox::version(); }

void* iox_alloc(size_t size) { return std::malloc(size); }
void iox_free(void* ptr) { std::free(ptr); }

iox_reader_t* iox_reader_create(const char* format, const char* options_json) {
    if (format == nullptr) return nullptr;
    try {
        auto reader = new (std::nothrow) iox_reader;
        if (reader == nullptr) return nullptr;
        const std::string value(format);
        if (value == "xtf" || value == "xtf23" || value == "xtf24") {
            iox::xtf::XtfReaderOptions options;
            if (const auto strict = jsonBoolOption(options_json, "strict")) options.strict = *strict;
            if (const auto sourceName = jsonStringOption(options_json, "sourceName")) {
                options.sourceName = *sourceName;
            }
            if (const auto preserve = jsonBoolOption(options_json, "preserveUnknownExtensions")) {
                options.preserveUnknownExtensions = *preserve;
            }
            if (const auto expected = jsonStringOption(options_json, "expectedVersion")) {
                if (*expected == "2.3" || *expected == "23") options.expectedVersion = iox::xtf::XtfVersion::Xtf23;
                if (*expected == "2.4" || *expected == "24") options.expectedVersion = iox::xtf::XtfVersion::Xtf24;
            } else if (value == "xtf23") {
                options.expectedVersion = iox::xtf::XtfVersion::Xtf23;
            } else if (value == "xtf24") {
                options.expectedVersion = iox::xtf::XtfVersion::Xtf24;
            }
            reader->impl = std::make_unique<iox::xtf::XtfReader>(std::move(options));
        } else if (value == "json-events") {
            reader->impl = std::make_unique<iox::json::JsonEventReader>();
        } else {
            delete reader;
            return nullptr;
        }
        return reinterpret_cast<iox_reader_t*>(reader);
    } catch (...) {
        return nullptr;
    }
}

void iox_reader_destroy(iox_reader_t* handle) {
    try { delete reinterpret_cast<iox_reader*>(handle); } catch (...) {}
}

iox_status_t iox_reader_feed(iox_reader_t* handle,
                             const uint8_t* data, size_t size) {
    if (handle == nullptr || (data == nullptr && size != 0)) return IOX_STATUS_INVALID_ARGUMENT;
    try {
        auto& reader = *reinterpret_cast<iox_reader*>(handle);
        if (size != 0) reader.impl->feed(iox::ByteView(reinterpret_cast<const char*>(data), size));
        takeReaderDiagnostics(reader);
        return hasError(reader.diagnostics) ? IOX_STATUS_ERROR : IOX_STATUS_OK;
    } catch (...) {
        return IOX_STATUS_ERROR;
    }
}

iox_status_t iox_reader_finish(iox_reader_t* handle) {
    if (handle == nullptr) return IOX_STATUS_INVALID_ARGUMENT;
    try {
        auto& reader = *reinterpret_cast<iox_reader*>(handle);
        reader.impl->finish();
        takeReaderDiagnostics(reader);
        return hasError(reader.diagnostics) ? IOX_STATUS_ERROR : IOX_STATUS_OK;
    } catch (...) {
        return IOX_STATUS_ERROR;
    }
}

iox_status_t iox_reader_next(iox_reader_t* handle, iox_result_t** output) {
    if (output != nullptr) *output = nullptr;
    if (handle == nullptr || output == nullptr) return IOX_STATUS_INVALID_ARGUMENT;
    try {
        auto& reader = *reinterpret_cast<iox_reader*>(handle);
        auto outcome = reader.impl->next();
        takeReaderDiagnostics(reader);
        appendDiagnostics(reader.diagnostics, std::move(outcome.diagnostics));

        if (hasError(reader.diagnostics)) {
            auto* result = makeResult(IOX_STATUS_ERROR, reader.diagnostics);
            if (result == nullptr) return IOX_STATUS_ERROR;
            reader.diagnostics.clear();
            setResult(output, result);
            return IOX_STATUS_ERROR;
        }

        iox_status_t status = IOX_STATUS_END;
        std::string serialized;
        const std::string* serializedPtr = nullptr;
        switch (outcome.status) {
        case iox::ReadOutcome::Status::Event:
            status = IOX_STATUS_EVENT;
            if (outcome.event) {
                serialized = eventJson(*outcome.event);
                serializedPtr = &serialized;
            }
            break;
        case iox::ReadOutcome::Status::NeedInput: status = IOX_STATUS_NEED_INPUT; break;
        case iox::ReadOutcome::Status::End: status = IOX_STATUS_END; break;
        }
        auto* result = makeResult(status, reader.diagnostics, serializedPtr);
        if (result == nullptr) return IOX_STATUS_ERROR;
        reader.diagnostics.clear();
        setResult(output, result);
        return status;
    } catch (...) {
        return IOX_STATUS_ERROR;
    }
}

iox_writer_t* iox_writer_create(const char* format, const char* options_json) {
    if (format == nullptr) return nullptr;
    try {
        auto writer = new (std::nothrow) iox_writer;
        if (writer == nullptr) return nullptr;
        const std::string value(format);
        writer->sink = std::make_shared<iox::StringOutputSink>();
        if (value == "xtf" || value == "xtf23") {
            iox::xtf::XtfWriterOptions options;
            options.version = iox::xtf::XtfVersion::Xtf23;
            if (const auto strict = jsonBoolOption(options_json, "strict")) options.strict = *strict;
            if (const auto pretty = jsonBoolOption(options_json, "pretty")) options.pretty = *pretty;
            if (const auto sender = jsonStringOption(options_json, "sender")) options.sender = *sender;
            if (const auto comment = jsonStringOption(options_json, "comment")) options.comment = *comment;
            if (const auto software = jsonStringOption(options_json, "software")) options.software = *software;
            writer->impl = std::make_unique<iox::xtf::XtfWriter>(writer->sink, options);
        } else if (value == "xtf24") {
            iox::xtf::XtfWriterOptions options;
            options.version = iox::xtf::XtfVersion::Xtf24;
            if (const auto strict = jsonBoolOption(options_json, "strict")) options.strict = *strict;
            if (const auto pretty = jsonBoolOption(options_json, "pretty")) options.pretty = *pretty;
            if (const auto sender = jsonStringOption(options_json, "sender")) options.sender = *sender;
            if (const auto comment = jsonStringOption(options_json, "comment")) options.comment = *comment;
            if (const auto software = jsonStringOption(options_json, "software")) options.software = *software;
            writer->impl = std::make_unique<iox::xtf::XtfWriter>(writer->sink, options);
        } else if (value == "json-events") {
            writer->impl = std::make_unique<iox::json::JsonEventWriter>(writer->sink);
        } else {
            delete writer;
            return nullptr;
        }
        return reinterpret_cast<iox_writer_t*>(writer);
    } catch (...) {
        return nullptr;
    }
}

void iox_writer_destroy(iox_writer_t* handle) {
    try { delete reinterpret_cast<iox_writer*>(handle); } catch (...) {}
}

iox_status_t iox_writer_write_event_json(iox_writer_t* handle,
                                         const char* event_json,
                                         size_t event_json_size,
                                         iox_result_t** output) {
    if (output != nullptr) *output = nullptr;
    if (handle == nullptr || event_json == nullptr || output == nullptr) {
        return invalidArgument(output, "writer, event_json, and result are required");
    }
    try {
        auto& writer = *reinterpret_cast<iox_writer*>(handle);
        if (writer.finished) {
            std::vector<iox::Diagnostic> diagnostics{{iox::Diagnostic::Severity::Error,
                iox::ErrorCode::InvalidState, "Cannot write to a finished writer", std::nullopt}};
            auto* result = makeResult(IOX_STATUS_INVALID_STATE, diagnostics);
            if (result == nullptr) return IOX_STATUS_ERROR;
            setResult(output, result);
            return IOX_STATUS_INVALID_STATE;
        }

        iox::json::JsonEventReader parser;
        parser.feed(iox::ByteView(event_json, event_json_size));
        parser.finish();
        auto outcome = parser.next();
        auto diagnostics = parser.takeDiagnostics();
        appendDiagnostics(diagnostics, std::move(outcome.diagnostics));
        if (outcome.status != iox::ReadOutcome::Status::Event || !outcome.event) {
            if (diagnostics.empty()) {
                diagnostics.push_back({iox::Diagnostic::Severity::Error,
                    iox::ErrorCode::JsonParseError, "Expected one JSON event", std::nullopt});
            }
        } else {
            writer.impl->write(*outcome.event);
        }
        takeWriterDiagnostics(writer, diagnostics);
        const auto status = hasError(diagnostics) ? IOX_STATUS_ERROR : IOX_STATUS_OK;
        auto* result = makeResult(status, diagnostics);
        if (result == nullptr) return IOX_STATUS_ERROR;
        setResult(output, result);
        return status;
    } catch (...) {
        return IOX_STATUS_ERROR;
    }
}

iox_status_t iox_writer_take_output(iox_writer_t* handle, iox_result_t** output) {
    if (output != nullptr) *output = nullptr;
    if (handle == nullptr || output == nullptr) {
        return invalidArgument(output, "writer and result are required");
    }
    try {
        auto& writer = *reinterpret_cast<iox_writer*>(handle);
        if (writer.finished) {
            std::vector<iox::Diagnostic> diagnostics{{iox::Diagnostic::Severity::Error,
                iox::ErrorCode::InvalidState, "Cannot take output after finish", std::nullopt}};
            auto* result = makeResult(IOX_STATUS_INVALID_STATE, diagnostics);
            if (result == nullptr) return IOX_STATUS_ERROR;
            setResult(output, result);
            return IOX_STATUS_INVALID_STATE;
        }
        auto* result = makeResult(IOX_STATUS_OK);
        if (result == nullptr) return IOX_STATUS_ERROR;
        const auto bytes = writer.sink->takeString();
        result->bytes.assign(bytes.begin(), bytes.end());
        setResult(output, result);
        return IOX_STATUS_OK;
    } catch (...) {
        return IOX_STATUS_ERROR;
    }
}

iox_status_t iox_writer_finish(iox_writer_t* handle, iox_result_t** output) {
    if (output != nullptr) *output = nullptr;
    if (handle == nullptr || output == nullptr) {
        return invalidArgument(output, "writer and result are required");
    }
    try {
        auto& writer = *reinterpret_cast<iox_writer*>(handle);
        if (writer.finished) {
            std::vector<iox::Diagnostic> diagnostics{{iox::Diagnostic::Severity::Error,
                iox::ErrorCode::InvalidState, "Writer has already finished", std::nullopt}};
            auto* result = makeResult(IOX_STATUS_INVALID_STATE, diagnostics);
            if (result == nullptr) return IOX_STATUS_ERROR;
            setResult(output, result);
            return IOX_STATUS_INVALID_STATE;
        }
        writer.impl->close();
        writer.finished = true;
        std::vector<iox::Diagnostic> diagnostics;
        takeWriterDiagnostics(writer, diagnostics);
        const auto status = hasError(diagnostics) ? IOX_STATUS_ERROR : IOX_STATUS_OK;
        auto* result = makeResult(status, diagnostics);
        if (result == nullptr) return IOX_STATUS_ERROR;
        const auto bytes = writer.sink->takeString();
        result->bytes.assign(bytes.begin(), bytes.end());
        setResult(output, result);
        return status;
    } catch (...) {
        return IOX_STATUS_ERROR;
    }
}

const char* iox_result_json(const iox_result_t* handle) {
    if (handle == nullptr) return nullptr;
    return reinterpret_cast<const iox_result*>(handle)->json.c_str();
}

const uint8_t* iox_result_bytes(const iox_result_t* handle) {
    if (handle == nullptr) return nullptr;
    const auto& bytes = reinterpret_cast<const iox_result*>(handle)->bytes;
    return bytes.empty() ? nullptr : bytes.data();
}

size_t iox_result_size(const iox_result_t* handle) {
    return handle == nullptr ? 0 : reinterpret_cast<const iox_result*>(handle)->bytes.size();
}

iox_status_t iox_result_status(const iox_result_t* handle) {
    return handle == nullptr ? IOX_STATUS_INVALID_ARGUMENT
                             : reinterpret_cast<const iox_result*>(handle)->status;
}

void iox_result_destroy(iox_result_t* handle) {
    try { delete reinterpret_cast<iox_result*>(handle); } catch (...) {}
}

} // extern "C"
