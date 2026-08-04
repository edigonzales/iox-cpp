#include "iox/abi/iox.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int contains(const char* text, const char* needle) {
    return text != NULL && strstr(text, needle) != NULL;
}

static int bytes_contains(const iox_result_t* result, const char* needle) {
    const uint8_t* bytes = iox_result_bytes(result);
    const size_t size = iox_result_size(result);
    const size_t needle_size = strlen(needle);
    if (bytes == NULL || needle_size > size) return 0;
    for (size_t index = 0; index + needle_size <= size; ++index) {
        if (memcmp(bytes + index, needle, needle_size) == 0) return 1;
    }
    return 0;
}

static size_t append_bytes(uint8_t* output, size_t output_size,
                           const iox_result_t* result) {
    const size_t size = iox_result_size(result);
    assert(output_size + size <= 512U);
    if (size > 0U) memcpy(output + output_size, iox_result_bytes(result), size);
    return output_size + size;
}

int main(void) {
    assert(iox_abi_version() == 2U);
    assert(strcmp(iox_version(), "0.2.0") == 0);

    const char* start_event =
        "{\"schema\":\"iox-event/2\",\"event\":\"startTransfer\","
        "\"header\":{\"version\":\"2.3\",\"sender\":\"ABI\","
        "\"models\":[],\"oidSpaces\":[],\"extensions\":[]}}";
    const char* end_event =
        "{\"schema\":\"iox-event/2\",\"event\":\"endTransfer\"}";

    /* Incremental ABI reader and canonical result schema. */
    iox_reader_t* reader = iox_reader_create("json-events", NULL);
    assert(reader != NULL);
    const size_t split = strlen(start_event) / 2U;
    assert(iox_reader_feed(reader, (const uint8_t*)start_event, split) ==
           IOX_STATUS_OK);
    iox_result_t* result = NULL;
    assert(iox_reader_next(reader, &result) == IOX_STATUS_NEED_INPUT);
    assert(result != NULL);
    assert(contains(iox_result_json(result), "\"schema\":\"iox-result/2\""));
    assert(contains(iox_result_json(result), "\"status\":\"need_input\""));
    iox_result_destroy(result);

    assert(iox_reader_feed(reader, (const uint8_t*)start_event + split,
                           strlen(start_event) - split) == IOX_STATUS_OK);
    assert(iox_reader_feed(reader, (const uint8_t*)"\n", 1U) == IOX_STATUS_OK);
    assert(iox_reader_feed(reader, (const uint8_t*)end_event,
                           strlen(end_event)) == IOX_STATUS_OK);
    assert(iox_reader_feed(reader, (const uint8_t*)"\n", 1U) == IOX_STATUS_OK);
    assert(iox_reader_finish(reader) == IOX_STATUS_OK);

    assert(iox_reader_next(reader, &result) == IOX_STATUS_EVENT);
    assert(contains(iox_result_json(result), "\"schema\":\"iox-event/2\""));
    assert(contains(iox_result_json(result), "\"sender\":\"ABI\""));
    iox_result_destroy(result);
    assert(iox_reader_next(reader, &result) == IOX_STATUS_EVENT);
    iox_result_destroy(result);
    assert(iox_reader_next(reader, &result) == IOX_STATUS_END);
    iox_result_destroy(result);
    assert(iox_reader_next(reader, &result) == IOX_STATUS_INVALID_STATE);
    assert(contains(iox_result_json(result), "api.invalid_state"));
    iox_result_destroy(result);
    assert(iox_reader_finish(reader) == IOX_STATUS_INVALID_STATE);
    assert(iox_reader_feed(reader, NULL, 0U) == IOX_STATUS_INVALID_STATE);
    iox_reader_destroy(reader);

    /* Persistent event parser and chunk-wise output on the writer. */
    iox_writer_t* writer = iox_writer_create("json-events", NULL);
    assert(writer != NULL);
    assert(iox_writer_write_event_json(writer, start_event, strlen(start_event),
                                       &result) == IOX_STATUS_OK);
    assert(contains(iox_result_json(result), "\"ok\":true"));
    iox_result_destroy(result);
    assert(iox_writer_take_output(writer, &result) == IOX_STATUS_OK);
    assert(iox_result_size(result) > 0U);
    iox_result_destroy(result);
    assert(iox_writer_take_output(writer, &result) == IOX_STATUS_OK);
    assert(iox_result_size(result) == 0U);
    iox_result_destroy(result);

    assert(iox_writer_write_event_json(writer, end_event, strlen(end_event),
                                       &result) == IOX_STATUS_OK);
    iox_result_destroy(result);
    assert(iox_writer_finish(writer, &result) == IOX_STATUS_OK);
    assert(iox_result_size(result) > 0U);
    iox_result_destroy(result);
    assert(iox_writer_finish(writer, &result) == IOX_STATUS_INVALID_STATE);
    assert(contains(iox_result_json(result), "\"ok\":false"));
    iox_result_destroy(result);
    assert(iox_writer_take_output(writer, &result) ==
           IOX_STATUS_INVALID_STATE);
    assert(contains(iox_result_json(result), "api.invalid_state"));
    iox_result_destroy(result);
    iox_writer_destroy(writer);

    /* Native output is byte-identical to the Node/browser/worker vector. */
    const char* parity_start =
        "{\"schema\":\"iox-event/2\",\"event\":\"startTransfer\","
        "\"header\":{\"version\":\"2.3\",\"sender\":\"Browser\","
        "\"models\":[{\"name\":\"M\",\"version\":\"1\","
        "\"uri\":\"urn:m\",\"xmlNamespace\":{\"namespaceUri\":\"\","
        "\"localName\":\"\",\"prefixHint\":\"\"}}],"
        "\"oidSpaces\":[],\"extensions\":[]}}";
    const char* parity_xtf =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<TRANSFER xmlns=\"http://www.interlis.ch/INTERLIS2.3\">"
        "<HEADERSECTION VERSION=\"2.3\" SENDER=\"Browser\"><MODELS>"
        "<MODEL NAME=\"M\" VERSION=\"1\" URI=\"urn:m\"/></MODELS>"
        "</HEADERSECTION><DATASECTION/></TRANSFER>";
    uint8_t parity_output[512];
    size_t parity_size = 0U;
    writer = iox_writer_create("xtf23", "{\"pretty\":false}");
    assert(writer != NULL);
    assert(iox_writer_write_event_json(writer, parity_start,
                                       strlen(parity_start), &result) ==
           IOX_STATUS_OK);
    iox_result_destroy(result);
    assert(iox_writer_take_output(writer, &result) == IOX_STATUS_OK);
    parity_size = append_bytes(parity_output, parity_size, result);
    iox_result_destroy(result);
    assert(iox_writer_write_event_json(writer, end_event, strlen(end_event),
                                       &result) == IOX_STATUS_OK);
    iox_result_destroy(result);
    assert(iox_writer_finish(writer, &result) == IOX_STATUS_OK);
    parity_size = append_bytes(parity_output, parity_size, result);
    iox_result_destroy(result);
    assert(parity_size == strlen(parity_xtf));
    assert(memcmp(parity_output, parity_xtf, parity_size) == 0);
    iox_writer_destroy(writer);

    /* Explicit XTF 2.4 selection reaches the independent 2.4 dialect. */
    const char* start24 =
        "{\"schema\":\"iox-event/2\",\"event\":\"startTransfer\","
        "\"header\":{\"version\":\"2.4\",\"sender\":\"ABI\","
        "\"models\":[{\"name\":\"M\",\"xmlNamespace\":{"
        "\"namespaceUri\":\"urn:m\",\"localName\":\"M\","
        "\"prefixHint\":\"m\"}}],\"oidSpaces\":[],"
        "\"extensions\":[]}}";
    writer = iox_writer_create("xtf24", "{\"pretty\":false}");
    assert(writer != NULL);
    assert(iox_writer_write_event_json(writer, start24, strlen(start24),
                                       &result) == IOX_STATUS_OK);
    iox_result_destroy(result);
    assert(iox_writer_take_output(writer, &result) == IOX_STATUS_OK);
    assert(bytes_contains(result, "ili:transfer"));
    assert(bytes_contains(result, "xmlns:m=\"urn:m\""));
    iox_result_destroy(result);
    assert(iox_writer_write_event_json(writer, end_event, strlen(end_event),
                                       &result) == IOX_STATUS_OK);
    iox_result_destroy(result);
    assert(iox_writer_finish(writer, &result) == IOX_STATUS_OK);
    iox_result_destroy(result);
    iox_writer_destroy(writer);

    writer = iox_writer_create("xtf", "{\"version\":\"2.4\","
                               "\"pretty\":false}");
    assert(writer != NULL);
    iox_writer_destroy(writer);
    assert(iox_writer_create("xtf", "{\"version\":\"2.5\"}") == NULL);
    assert(iox_writer_create("xtf23", "{\"version\":\"2.4\"}") == NULL);
    assert(iox_writer_create("xtf", "{\"sender\":false}") == NULL);
    assert(iox_reader_create("xtf", "{\"maxDepth\":\"bad\"}") == NULL);
    assert(iox_reader_create("xtf", "{\"sourceName\":false}") == NULL);
    assert(iox_reader_create("xtf23", "{\"expectedVersion\":\"2.4\"}") ==
           NULL);

    /* Fatal parse failures remain structured and never cross C. */
    writer = iox_writer_create("json-events", NULL);
    assert(writer != NULL);
    assert(iox_writer_write_event_json(writer, "{", 1U, &result) ==
           IOX_STATUS_ERROR);
    assert(result != NULL);
    assert(contains(iox_result_json(result), "json.malformed"));
    assert(contains(iox_result_json(result), "\"ok\":false"));
    iox_result_destroy(result);
    iox_writer_destroy(writer);

    /* XTF reader still streams events through the same result contract. */
    const char* xtf =
        "<?xml version=\"1.0\"?><ili:TRANSFER "
        "xmlns:ili=\"http://www.interlis.ch/INTERLIS2.3\">"
        "<ili:HEADERSECTION SENDER=\"ABI\" VERSION=\"2.3\">"
        "<ili:MODELS><ili:MODEL NAME=\"M\"/></ili:MODELS>"
        "</ili:HEADERSECTION><ili:DATASECTION/></ili:TRANSFER>";
    reader = iox_reader_create("xtf23", NULL);
    assert(reader != NULL);
    assert(iox_reader_feed(reader, (const uint8_t*)xtf, strlen(xtf)) ==
           IOX_STATUS_OK);
    assert(iox_reader_finish(reader) == IOX_STATUS_OK);
    assert(iox_reader_next(reader, &result) == IOX_STATUS_EVENT);
    iox_result_destroy(result);
    assert(iox_reader_next(reader, &result) == IOX_STATUS_EVENT);
    iox_result_destroy(result);
    iox_reader_destroy(reader);

    /* Fatal XML errors retain their stable code and source position. */
    reader = iox_reader_create("xtf23", "{\"sourceName\":\"broken.xtf\"}");
    assert(reader != NULL);
    const char* malformed_xtf = "<ili:TRANSFER>";
    assert(iox_reader_feed(reader, (const uint8_t*)malformed_xtf,
                           strlen(malformed_xtf)) == IOX_STATUS_ERROR);
    assert(iox_reader_next(reader, &result) == IOX_STATUS_ERROR);
    assert(contains(iox_result_json(result), "xml.malformed"));
    assert(contains(iox_result_json(result), "broken.xtf"));
    assert(contains(iox_result_json(result), "\"line\":1"));
    iox_result_destroy(result);
    iox_reader_destroy(reader);

    /* Null argument safety and result accessors. */
    assert(iox_reader_create(NULL, NULL) == NULL);
    assert(iox_writer_create(NULL, NULL) == NULL);
    assert(iox_reader_feed(NULL, NULL, 0U) == IOX_STATUS_INVALID_ARGUMENT);
    assert(iox_reader_next(NULL, &result) == IOX_STATUS_INVALID_ARGUMENT);
    assert(result != NULL);
    iox_result_destroy(result);
    assert(iox_writer_take_output(NULL, NULL) == IOX_STATUS_INVALID_ARGUMENT);
    assert(iox_writer_write_event_json(NULL, NULL, 0U, &result) ==
           IOX_STATUS_INVALID_ARGUMENT);
    assert(result != NULL);
    assert(contains(iox_result_json(result), "abi.invalid_argument"));
    iox_result_destroy(result);
    assert(iox_result_json(NULL) == NULL);
    assert(iox_result_bytes(NULL) == NULL);
    assert(iox_result_size(NULL) == 0U);
    assert(iox_result_status(NULL) == IOX_STATUS_INVALID_ARGUMENT);

    puts("ABI 2 smoke tests passed");
    return 0;
}
