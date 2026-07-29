#include "iox/abi/iox.h"
#include "iox/Version.h"
#include "iox/Reader.h"
#include "iox/Writer.h"
#include "iox/xtf/XtfReader.h"
#include "iox/xtf/XtfWriter.h"
#include "iox/xtf/XtfReaderOptions.h"
#include "iox/json/JsonEventReader.h"
#include "iox/json/JsonEventWriter.h"

#include <string>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <memory>
#include <new>

// ============================================================================
// Internal structs
// ============================================================================

struct iox_result {
    std::string json;
    std::vector<uint8_t> bytes;
    iox_status_t status = IOX_STATUS_OK;
};

struct iox_reader {
    std::unique_ptr<iox::Reader> impl;
    std::string format;
    std::vector<iox::Diagnostic> diagnostics;
};

struct iox_writer {
    std::shared_ptr<iox::StringOutputSink> sink;
    std::unique_ptr<iox::Writer> impl;
    std::string format;
    std::vector<iox::Diagnostic> diagnostics;
};

// ============================================================================
// Version
// ============================================================================

extern "C" {

uint32_t iox_abi_version(void) {
    return iox::abiVersion();
}

const char* iox_version(void) {
    return iox::version();
}

void* iox_alloc(size_t size) {
    return std::malloc(size);
}

void iox_free(void* ptr) {
    std::free(ptr);
}

// ============================================================================
// Reader
// ============================================================================

iox_reader_t* iox_reader_create(const char* format, const char* /*options_json*/) {
    if (!format) return nullptr;

    try {
        auto reader = new (std::nothrow) iox_reader;
        if (!reader) return nullptr;
        reader->format = format;

        if (reader->format == "xtf" || reader->format == "xtf23" || reader->format == "xtf24") {
            iox::xtf::XtfReaderOptions opts;
            reader->impl = std::make_unique<iox::xtf::XtfReader>(opts);
        } else if (reader->format == "json-events") {
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

void iox_reader_destroy(iox_reader_t* reader) {
    delete reinterpret_cast<iox_reader*>(reader);
}

iox_status_t iox_reader_feed(iox_reader_t* handle,
                             const uint8_t* data, size_t size) {
    if (!handle || !data) return IOX_STATUS_INVALID_ARGUMENT;
    auto* reader = reinterpret_cast<iox_reader*>(handle);
    try {
        reader->impl->feed(iox::ByteView(reinterpret_cast<const char*>(data), size));
        return IOX_STATUS_OK;
    } catch (...) { return IOX_STATUS_ERROR; }
}

iox_status_t iox_reader_finish(iox_reader_t* handle) {
    if (!handle) return IOX_STATUS_INVALID_ARGUMENT;
    auto* reader = reinterpret_cast<iox_reader*>(handle);
    try {
        reader->impl->finish();
        return IOX_STATUS_OK;
    } catch (...) { return IOX_STATUS_ERROR; }
}

iox_status_t iox_reader_next(iox_reader_t* handle, iox_result_t** result) {
    if (!handle || !result) return IOX_STATUS_INVALID_ARGUMENT;
    auto* reader = reinterpret_cast<iox_reader*>(handle);
    try {
        auto outcome = reader->impl->next();

        auto* res = new (std::nothrow) iox_result;
        if (!res) return IOX_STATUS_ERROR;
        *result = reinterpret_cast<iox_result_t*>(res);

        switch (outcome.status) {
        case iox::ReadOutcome::Status::Event:
            res->status = IOX_STATUS_EVENT;
            // Serialize event to JSON (for ABI simplicity)
            {
                auto sink = std::make_shared<iox::StringOutputSink>();
                iox::json::JsonEventWriter writer(sink);
                if (outcome.event) writer.write(*outcome.event);
                writer.close();
                res->json = sink->str();
            }
            break;
        case iox::ReadOutcome::Status::NeedInput:
            res->status = IOX_STATUS_NEED_INPUT;
            break;
        case iox::ReadOutcome::Status::End:
            res->status = IOX_STATUS_END;
            break;
        }
        return res->status;
    } catch (...) { return IOX_STATUS_ERROR; }
}

// ============================================================================
// Writer
// ============================================================================

iox_writer_t* iox_writer_create(const char* format, const char* /*options_json*/) {
    if (!format) return nullptr;

    try {
        auto writer = new (std::nothrow) iox_writer;
        if (!writer) return nullptr;
        writer->format = format;
        writer->sink = std::make_shared<iox::StringOutputSink>();

        if (writer->format == "xtf" || writer->format == "xtf23") {
            iox::xtf::XtfWriterOptions opts;
            opts.version = iox::xtf::XtfVersion::Xtf23;
            writer->impl = std::make_unique<iox::xtf::XtfWriter>(writer->sink, opts);
        } else if (writer->format == "xtf24") {
            iox::xtf::XtfWriterOptions opts;
            opts.version = iox::xtf::XtfVersion::Xtf24;
            writer->impl = std::make_unique<iox::xtf::XtfWriter>(writer->sink, opts);
        } else if (writer->format == "json-events") {
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
    delete reinterpret_cast<iox_writer*>(handle);
}

iox_status_t iox_writer_write_event_json(
    iox_writer_t* handle,
    const char* event_json, size_t event_json_size,
    iox_result_t** result)
{
    if (!handle || !event_json || !result) return IOX_STATUS_INVALID_ARGUMENT;
    auto* writer = reinterpret_cast<iox_writer*>(handle);
    try {
        // Parse the event JSON and feed to writer
        iox::json::JsonEventReader jsonReader;
        jsonReader.feed(iox::ByteView(event_json, event_json_size));
        jsonReader.finish();

        auto outcome = jsonReader.next();
        if (outcome.event) {
            writer->impl->write(*outcome.event);
        }

        auto* res = new (std::nothrow) iox_result;
        if (!res) return IOX_STATUS_ERROR;
        res->status = IOX_STATUS_OK;
        *result = reinterpret_cast<iox_result_t*>(res);
        return IOX_STATUS_OK;
    } catch (...) { return IOX_STATUS_ERROR; }
}

iox_status_t iox_writer_finish(iox_writer_t* handle, iox_result_t** result) {
    if (!handle || !result) return IOX_STATUS_INVALID_ARGUMENT;
    auto* writer = reinterpret_cast<iox_writer*>(handle);
    try {
        writer->impl->close();
        auto* res = new (std::nothrow) iox_result;
        if (!res) return IOX_STATUS_ERROR;
        std::string outStr = writer->sink->str();
        res->bytes.assign(
            reinterpret_cast<const uint8_t*>(outStr.data()),
            reinterpret_cast<const uint8_t*>(outStr.data()) + outStr.size());
        res->status = IOX_STATUS_OK;
        *result = reinterpret_cast<iox_result_t*>(res);
        return IOX_STATUS_OK;
    } catch (...) { return IOX_STATUS_ERROR; }
}

// ============================================================================
// Result
// ============================================================================

const char* iox_result_json(const iox_result_t* handle) {
    if (!handle) return nullptr;
    auto* res = reinterpret_cast<const iox_result*>(handle);
    return res->json.c_str();
}

const uint8_t* iox_result_bytes(const iox_result_t* handle) {
    if (!handle) return nullptr;
    auto* res = reinterpret_cast<const iox_result*>(handle);
    return res->bytes.empty() ? nullptr : res->bytes.data();
}

size_t iox_result_size(const iox_result_t* handle) {
    if (!handle) return 0;
    auto* res = reinterpret_cast<const iox_result*>(handle);
    return res->bytes.size();
}

iox_status_t iox_result_status(const iox_result_t* handle) {
    if (!handle) return IOX_STATUS_INVALID_ARGUMENT;
    auto* res = reinterpret_cast<const iox_result*>(handle);
    return res->status;
}

void iox_result_destroy(iox_result_t* handle) {
    delete reinterpret_cast<iox_result*>(handle);
}

} // extern "C"
