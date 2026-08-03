#include "iox/abi/iox.h"

#include "iox/Diagnostic.h"
#include "iox/Reader.h"
#include "iox/Version.h"
#include "iox/Writer.h"
#include "iox/json/JsonEventReader.h"
#include "iox/json/JsonEventWriter.h"
#include "iox/xtf/XtfReader.h"
#include "iox/xtf/XtfReaderOptions.h"
#include "iox/xtf/XtfWriter.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// All objects and allocations crossing this boundary are owned by the ABI.
// No C++ exception is allowed to escape an extern "C" function.

struct iox_result {
    std::string json;
    std::vector<std::uint8_t> bytes;
    iox_status_t status = IOX_STATUS_OK;
};

struct iox_reader {
    std::unique_ptr<iox::Reader> impl;
    std::shared_ptr<iox::StringOutputSink> eventSink;
    std::unique_ptr<iox::json::JsonEventWriter> eventWriter;
    std::vector<iox::Diagnostic> diagnostics;
    bool terminal = false;
};

struct iox_writer {
    std::shared_ptr<iox::StringOutputSink> sink;
    std::unique_ptr<iox::Writer> impl;
    std::unique_ptr<iox::json::JsonEventReader> eventParser;
    std::vector<iox::Diagnostic> diagnostics;
    bool finished = false;
    bool terminal = false;
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

const char* severityName(iox::DiagnosticSeverity severity) noexcept {
    switch (severity) {
    case iox::DiagnosticSeverity::Info: return "info";
    case iox::DiagnosticSeverity::Warning: return "warning";
    case iox::DiagnosticSeverity::Error: return "error";
    case iox::DiagnosticSeverity::Fatal: return "fatal";
    }
    return "error";
}

void appendJsonString(std::string& out, std::string_view value) {
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
            if (c < 0x20U) {
                static constexpr char hex[] = "0123456789abcdef";
                out += "\\u00";
                out.push_back(hex[(c >> 4U) & 0x0fU]);
                out.push_back(hex[c & 0x0fU]);
            } else {
                out.push_back(static_cast<char>(c));
            }
        }
    }
    out.push_back('"');
}

void appendLocation(std::string& out, const iox::SourceLocation& location) {
    out += "{\"sourceName\":";
    appendJsonString(out, location.sourceName);
    out += ",\"byteOffset\":" + std::to_string(location.byteOffset);
    out += ",\"line\":" + std::to_string(location.line);
    out += ",\"column\":" + std::to_string(location.column) + "}";
}

void appendDiagnostic(std::string& out, const iox::Diagnostic& diagnostic) {
    out += "{\"severity\":";
    appendJsonString(out, severityName(diagnostic.severity));
    out += ",\"code\":";
    appendJsonString(out, iox::diagnosticCodeName(diagnostic.code));
    out += ",\"message\":";
    appendJsonString(out, diagnostic.message);
    out += ",\"location\":";
    appendLocation(out, diagnostic.location);
    out += ",\"contextPath\":[";
    for (std::size_t i = 0; i < diagnostic.contextPath.size(); ++i) {
        if (i != 0U) out.push_back(',');
        appendJsonString(out, diagnostic.contextPath[i]);
    }
    out += "]}";
}

bool isError(const iox::Diagnostic& diagnostic) noexcept {
    return diagnostic.severity == iox::DiagnosticSeverity::Error ||
           diagnostic.severity == iox::DiagnosticSeverity::Fatal;
}

bool hasError(const std::vector<iox::Diagnostic>& diagnostics) noexcept {
    return std::any_of(diagnostics.begin(), diagnostics.end(), isError);
}

iox::Diagnostic diagnostic(iox::DiagnosticCode code,
                           std::string message,
                           iox::DiagnosticSeverity severity =
                               iox::DiagnosticSeverity::Fatal,
                           iox::SourceLocation location = {}) {
    return {severity, code, std::move(message), std::move(location), {}};
}

iox::Diagnostic exceptionDiagnostic(const iox::IoxError& error) {
    return diagnostic(error.code(), error.what(),
                      iox::DiagnosticSeverity::Fatal, error.location());
}

iox::Diagnostic internalDiagnostic(const char* message) {
    return diagnostic(iox::DiagnosticCode::InternalError,
                      message == nullptr ? "Unexpected internal error" : message);
}

void appendDiagnostics(std::vector<iox::Diagnostic>& target,
                       std::vector<iox::Diagnostic> source) {
    target.insert(target.end(),
                  std::make_move_iterator(source.begin()),
                  std::make_move_iterator(source.end()));
}

std::string resultJson(iox_status_t status,
                       const std::vector<iox::Diagnostic>& diagnostics,
                       const std::string* eventJson = nullptr) {
    const bool ok = status != IOX_STATUS_ERROR &&
                    status != IOX_STATUS_INVALID_ARGUMENT &&
                    status != IOX_STATUS_INVALID_STATE;
    std::string out = "{\"schema\":\"iox-result/2\",\"ok\":";
    out += ok ? "true" : "false";
    out += ",\"status\":";
    appendJsonString(out, statusName(status));
    out += ",\"event\":";
    if (eventJson != nullptr && status == IOX_STATUS_EVENT) out += *eventJson;
    else out += "null";

    out += ",\"error\":";
    const auto error = std::find_if(diagnostics.begin(), diagnostics.end(), isError);
    if (!ok) {
        const iox::Diagnostic fallback = diagnostic(
            status == IOX_STATUS_INVALID_ARGUMENT
                ? iox::DiagnosticCode::AbiInvalidArgument
                : status == IOX_STATUS_INVALID_STATE
                      ? iox::DiagnosticCode::InvalidState
                      : iox::DiagnosticCode::InternalError,
            statusName(status));
        const auto& selected = error == diagnostics.end() ? fallback : *error;
        out += "{\"code\":";
        appendJsonString(out, iox::diagnosticCodeName(selected.code));
        out += ",\"message\":";
        appendJsonString(out, selected.message);
        out += ",\"location\":";
        appendLocation(out, selected.location);
        out += "}";
    } else {
        out += "null";
    }

    out += ",\"diagnostics\":[";
    for (std::size_t i = 0; i < diagnostics.size(); ++i) {
        if (i != 0U) out.push_back(',');
        appendDiagnostic(out, diagnostics[i]);
    }
    out += "]}";
    return out;
}

iox_result* makeResult(iox_status_t status,
                       const std::vector<iox::Diagnostic>& diagnostics = {},
                       const std::string* eventJson = nullptr) noexcept {
    try {
        auto* result = new (std::nothrow) iox_result;
        if (result == nullptr) return nullptr;
        result->status = status;
        result->json = resultJson(status, diagnostics, eventJson);
        return result;
    } catch (...) {
        return nullptr;
    }
}

void setResult(iox_result_t** output, iox_result* result) noexcept {
    if (output != nullptr) *output = reinterpret_cast<iox_result_t*>(result);
}

iox_status_t invalidArgument(iox_result_t** output, const char* message) noexcept {
    if (output != nullptr) {
        std::vector<iox::Diagnostic> diagnostics{
            diagnostic(iox::DiagnosticCode::AbiInvalidArgument, message)};
        setResult(output, makeResult(IOX_STATUS_INVALID_ARGUMENT, diagnostics));
    }
    return IOX_STATUS_INVALID_ARGUMENT;
}

void takeReaderDiagnostics(iox_reader& reader) {
    if (reader.impl) appendDiagnostics(reader.diagnostics,
                                       reader.impl->takeDiagnostics());
}

void takeWriterDiagnostics(iox_writer& writer) {
    if (writer.eventParser) {
        appendDiagnostics(writer.diagnostics,
                          writer.eventParser->takeDiagnostics());
    }
    if (writer.impl) {
        appendDiagnostics(writer.diagnostics, writer.impl->takeDiagnostics());
    }
}

void remember(iox_reader& reader, const iox::IoxError& error) {
    reader.diagnostics.push_back(exceptionDiagnostic(error));
    reader.terminal = true;
}

void remember(iox_reader& reader, const char* message) {
    reader.diagnostics.push_back(internalDiagnostic(message));
    reader.terminal = true;
}

void remember(iox_writer& writer, const iox::IoxError& error) {
    writer.diagnostics.push_back(exceptionDiagnostic(error));
    writer.terminal = true;
}

void remember(iox_writer& writer, const char* message) {
    writer.diagnostics.push_back(internalDiagnostic(message));
    writer.terminal = true;
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
    if (input.compare(pos, 4U, "true") == 0) return true;
    if (input.compare(pos, 5U, "false") == 0) return false;
    return std::nullopt;
}

} // namespace

extern "C" {

uint32_t iox_abi_version(void) { return iox::abiVersion(); }
const char* iox_version(void) { return iox::version(); }

void* iox_alloc(size_t size) { return std::malloc(size); }
void iox_free(void* ptr) { std::free(ptr); }

iox_reader_t* iox_reader_create(const char* format, const char* optionsJson) {
    if (format == nullptr) return nullptr;
    try {
        auto reader = std::make_unique<iox_reader>();
        const std::string value(format);
        if (value == "xtf" || value == "xtf23" || value == "xtf24") {
            iox::xtf::XtfReaderOptions options;
            if (const auto strict = jsonBoolOption(optionsJson, "strict")) {
                options.strictness = *strict ? iox::xtf::Strictness::Strict
                                             : iox::xtf::Strictness::Lenient;
            }
            if (const auto source = jsonStringOption(optionsJson, "sourceName")) {
                options.sourceName = *source;
            }
            if (const auto preserve =
                    jsonBoolOption(optionsJson, "preserveUnknownExtensions")) {
                options.preserveUnknownExtensions = *preserve;
            }
            if (const auto expected =
                    jsonStringOption(optionsJson, "expectedVersion")) {
                if (*expected == "2.3" || *expected == "23") {
                    options.expectedVersion = iox::xtf::XtfVersion::V23;
                } else if (*expected == "2.4" || *expected == "24") {
                    options.expectedVersion = iox::xtf::XtfVersion::V24;
                }
            } else if (value == "xtf23") {
                options.expectedVersion = iox::xtf::XtfVersion::V23;
            } else if (value == "xtf24") {
                options.expectedVersion = iox::xtf::XtfVersion::V24;
            }
            reader->impl = std::make_unique<iox::xtf::XtfReader>(options);
        } else if (value == "json-events") {
            iox::json::JsonReaderOptions options;
            if (const auto source = jsonStringOption(optionsJson, "sourceName")) {
                options.sourceName = *source;
            }
            reader->impl = std::make_unique<iox::json::JsonEventReader>(options);
        } else {
            return nullptr;
        }
        reader->eventSink = std::make_shared<iox::StringOutputSink>();
        reader->eventWriter =
            std::make_unique<iox::json::JsonEventWriter>(reader->eventSink);
        return reinterpret_cast<iox_reader_t*>(reader.release());
    } catch (...) {
        return nullptr;
    }
}

void iox_reader_destroy(iox_reader_t* handle) {
    try { delete reinterpret_cast<iox_reader*>(handle); } catch (...) {}
}

iox_status_t iox_reader_feed(iox_reader_t* handle,
                             const uint8_t* data,
                             size_t size) {
    if (handle == nullptr || (data == nullptr && size != 0U)) {
        return IOX_STATUS_INVALID_ARGUMENT;
    }
    auto& reader = *reinterpret_cast<iox_reader*>(handle);
    if (reader.terminal) return IOX_STATUS_INVALID_STATE;
    try {
        reader.impl->feed(iox::ByteView(data, size));
        takeReaderDiagnostics(reader);
        if (hasError(reader.diagnostics)) reader.terminal = true;
        return reader.terminal ? IOX_STATUS_ERROR : IOX_STATUS_OK;
    } catch (const iox::IoxError& error) {
        remember(reader, error);
    } catch (const std::exception& error) {
        remember(reader, error.what());
    } catch (...) {
        remember(reader, "Unexpected exception while feeding reader");
    }
    return IOX_STATUS_ERROR;
}

iox_status_t iox_reader_finish(iox_reader_t* handle) {
    if (handle == nullptr) return IOX_STATUS_INVALID_ARGUMENT;
    auto& reader = *reinterpret_cast<iox_reader*>(handle);
    if (reader.terminal) return IOX_STATUS_INVALID_STATE;
    try {
        reader.impl->finish();
        takeReaderDiagnostics(reader);
        if (hasError(reader.diagnostics)) reader.terminal = true;
        return reader.terminal ? IOX_STATUS_ERROR : IOX_STATUS_OK;
    } catch (const iox::IoxError& error) {
        remember(reader, error);
    } catch (const std::exception& error) {
        remember(reader, error.what());
    } catch (...) {
        remember(reader, "Unexpected exception while finishing reader");
    }
    return IOX_STATUS_ERROR;
}

iox_status_t iox_reader_next(iox_reader_t* handle, iox_result_t** output) {
    if (output != nullptr) *output = nullptr;
    if (handle == nullptr || output == nullptr) {
        return invalidArgument(output, "reader and result are required");
    }
    auto& reader = *reinterpret_cast<iox_reader*>(handle);
    try {
        if (reader.terminal || hasError(reader.diagnostics)) {
            auto* result = makeResult(IOX_STATUS_ERROR, reader.diagnostics);
            setResult(output, result);
            return IOX_STATUS_ERROR;
        }

        auto outcome = reader.impl->next();
        takeReaderDiagnostics(reader);
        if (hasError(reader.diagnostics)) {
            reader.terminal = true;
            auto* result = makeResult(IOX_STATUS_ERROR, reader.diagnostics);
            setResult(output, result);
            return IOX_STATUS_ERROR;
        }

        iox_status_t status = IOX_STATUS_NEED_INPUT;
        std::string serialized;
        const std::string* event = nullptr;
        switch (outcome.progress) {
        case iox::ReaderProgress::Event:
            if (!outcome.event) {
                throw iox::IoxError(iox::DiagnosticCode::InternalError,
                                    "Reader returned Event without an event value");
            }
            reader.eventWriter->write(*outcome.event);
            serialized = reader.eventSink->takeString();
            if (!serialized.empty() && serialized.back() == '\n') {
                serialized.pop_back();
            }
            event = &serialized;
            status = IOX_STATUS_EVENT;
            break;
        case iox::ReaderProgress::NeedInput:
            status = IOX_STATUS_NEED_INPUT;
            break;
        case iox::ReaderProgress::End:
            reader.eventWriter->close();
            status = IOX_STATUS_END;
            break;
        }
        auto* result = makeResult(status, reader.diagnostics, event);
        if (result == nullptr) return IOX_STATUS_ERROR;
        reader.diagnostics.clear();
        setResult(output, result);
        return status;
    } catch (const iox::IoxError& error) {
        remember(reader, error);
    } catch (const std::exception& error) {
        remember(reader, error.what());
    } catch (...) {
        remember(reader, "Unexpected exception while reading next event");
    }
    setResult(output, makeResult(IOX_STATUS_ERROR, reader.diagnostics));
    return IOX_STATUS_ERROR;
}

iox_writer_t* iox_writer_create(const char* format, const char* optionsJson) {
    if (format == nullptr) return nullptr;
    try {
        auto writer = std::make_unique<iox_writer>();
        const std::string value(format);
        writer->sink = std::make_shared<iox::StringOutputSink>();
        if (value == "xtf" || value == "xtf23" || value == "xtf24") {
            iox::xtf::XtfWriterOptions options;
            if (value != "xtf24") options.version = iox::xtf::XtfVersion::V23;
            if (const auto strict = jsonBoolOption(optionsJson, "strict")) {
                options.strict = *strict;
            }
            if (const auto pretty = jsonBoolOption(optionsJson, "pretty")) {
                options.pretty = *pretty;
            }
            if (const auto sender = jsonStringOption(optionsJson, "sender")) {
                options.sender = *sender;
            }
            if (const auto comment = jsonStringOption(optionsJson, "comment")) {
                options.comment = *comment;
            }
            if (const auto software = jsonStringOption(optionsJson, "software")) {
                options.software = *software;
            }
            writer->impl =
                std::make_unique<iox::xtf::XtfWriter>(writer->sink, options);
        } else if (value == "json-events") {
            writer->impl =
                std::make_unique<iox::json::JsonEventWriter>(writer->sink);
        } else {
            return nullptr;
        }
        writer->eventParser =
            std::make_unique<iox::json::JsonEventReader>();
        return reinterpret_cast<iox_writer_t*>(writer.release());
    } catch (...) {
        return nullptr;
    }
}

void iox_writer_destroy(iox_writer_t* handle) {
    try { delete reinterpret_cast<iox_writer*>(handle); } catch (...) {}
}

iox_status_t iox_writer_write_event_json(iox_writer_t* handle,
                                         const char* eventJson,
                                         size_t eventJsonSize,
                                         iox_result_t** output) {
    if (output != nullptr) *output = nullptr;
    if (handle == nullptr || eventJson == nullptr || output == nullptr) {
        return invalidArgument(output,
                               "writer, event_json, and result are required");
    }
    auto& writer = *reinterpret_cast<iox_writer*>(handle);
    if (writer.finished || writer.terminal) {
        const auto status = writer.terminal ? IOX_STATUS_ERROR
                                            : IOX_STATUS_INVALID_STATE;
        if (!writer.terminal) {
            writer.diagnostics.push_back(diagnostic(
                iox::DiagnosticCode::InvalidState,
                "Cannot write to a finished writer"));
        }
        setResult(output, makeResult(status, writer.diagnostics));
        return status;
    }
    try {
        writer.eventParser->feed(iox::ByteView(
            reinterpret_cast<const std::uint8_t*>(eventJson), eventJsonSize));
        if (eventJsonSize == 0U || eventJson[eventJsonSize - 1U] != '\n') {
            static constexpr std::uint8_t newline = '\n';
            writer.eventParser->feed(iox::ByteView(&newline, 1U));
        }
        auto outcome = writer.eventParser->next();
        if (outcome.progress != iox::ReaderProgress::Event || !outcome.event) {
            throw iox::IoxError(iox::DiagnosticCode::JsonMalformed,
                                "Expected exactly one complete JSON event");
        }
        if (writer.eventParser->next().progress !=
            iox::ReaderProgress::NeedInput) {
            throw iox::IoxError(iox::DiagnosticCode::JsonMalformed,
                                "Expected exactly one JSON event");
        }
        writer.impl->write(*outcome.event);
        takeWriterDiagnostics(writer);
        if (hasError(writer.diagnostics)) writer.terminal = true;
        const auto status = writer.terminal ? IOX_STATUS_ERROR : IOX_STATUS_OK;
        setResult(output, makeResult(status, writer.diagnostics));
        writer.diagnostics.clear();
        return status;
    } catch (const iox::IoxError& error) {
        remember(writer, error);
    } catch (const std::exception& error) {
        remember(writer, error.what());
    } catch (...) {
        remember(writer, "Unexpected exception while writing event");
    }
    setResult(output, makeResult(IOX_STATUS_ERROR, writer.diagnostics));
    return IOX_STATUS_ERROR;
}

iox_status_t iox_writer_take_output(iox_writer_t* handle,
                                    iox_result_t** output) {
    if (output != nullptr) *output = nullptr;
    if (handle == nullptr || output == nullptr) {
        return invalidArgument(output, "writer and result are required");
    }
    auto& writer = *reinterpret_cast<iox_writer*>(handle);
    try {
        const auto status = writer.terminal ? IOX_STATUS_ERROR : IOX_STATUS_OK;
        auto* result = makeResult(status, writer.diagnostics);
        if (result == nullptr) return IOX_STATUS_ERROR;
        const auto bytes = writer.sink->takeString();
        result->bytes.assign(bytes.begin(), bytes.end());
        setResult(output, result);
        return status;
    } catch (const std::exception& error) {
        remember(writer, error.what());
    } catch (...) {
        remember(writer, "Unexpected exception while taking writer output");
    }
    setResult(output, makeResult(IOX_STATUS_ERROR, writer.diagnostics));
    return IOX_STATUS_ERROR;
}

iox_status_t iox_writer_finish(iox_writer_t* handle, iox_result_t** output) {
    if (output != nullptr) *output = nullptr;
    if (handle == nullptr || output == nullptr) {
        return invalidArgument(output, "writer and result are required");
    }
    auto& writer = *reinterpret_cast<iox_writer*>(handle);
    if (writer.finished || writer.terminal) {
        const auto status = writer.terminal ? IOX_STATUS_ERROR
                                            : IOX_STATUS_INVALID_STATE;
        if (!writer.terminal) {
            writer.diagnostics.push_back(diagnostic(
                iox::DiagnosticCode::InvalidState,
                "Writer has already finished"));
        }
        setResult(output, makeResult(status, writer.diagnostics));
        return status;
    }
    try {
        writer.eventParser->finish();
        const auto trailing = writer.eventParser->next();
        if (trailing.progress != iox::ReaderProgress::End) {
            throw iox::IoxError(iox::DiagnosticCode::JsonMalformed,
                                "Incomplete event JSON at writer finish");
        }
        writer.impl->close();
        writer.finished = true;
        takeWriterDiagnostics(writer);
        if (hasError(writer.diagnostics)) writer.terminal = true;
        const auto status = writer.terminal ? IOX_STATUS_ERROR : IOX_STATUS_OK;
        auto* result = makeResult(status, writer.diagnostics);
        if (result == nullptr) return IOX_STATUS_ERROR;
        const auto bytes = writer.sink->takeString();
        result->bytes.assign(bytes.begin(), bytes.end());
        setResult(output, result);
        return status;
    } catch (const iox::IoxError& error) {
        remember(writer, error);
    } catch (const std::exception& error) {
        remember(writer, error.what());
    } catch (...) {
        remember(writer, "Unexpected exception while finishing writer");
    }
    setResult(output, makeResult(IOX_STATUS_ERROR, writer.diagnostics));
    return IOX_STATUS_ERROR;
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
    return handle == nullptr
               ? 0U
               : reinterpret_cast<const iox_result*>(handle)->bytes.size();
}

iox_status_t iox_result_status(const iox_result_t* handle) {
    return handle == nullptr
               ? IOX_STATUS_INVALID_ARGUMENT
               : reinterpret_cast<const iox_result*>(handle)->status;
}

void iox_result_destroy(iox_result_t* handle) {
    try { delete reinterpret_cast<iox_result*>(handle); } catch (...) {}
}

} // extern "C"
