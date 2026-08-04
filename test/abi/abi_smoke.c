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

int iox_abi_smoke(void) {
    assert(iox_abi_version() == 2U);
    assert(strcmp(iox_version(), "0.2.0") == 0);
    void* allocation = iox_alloc(17U);
    assert(allocation != NULL);
    iox_free(allocation);
    iox_free(NULL);

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
    assert(iox_writer_write_event_json(writer, end_event, strlen(end_event),
                                       &result) == IOX_STATUS_INVALID_STATE);
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

    /* Every public reader option is parsed and has both boolean branches. */
    const char* all_reader_options =
        "{\"sourceName\":\"a\\\"b\",\"expectedVersion\":\"23\","
        "\"strict\":true,\"preserveUnknownExtensions\":false,"
        "\"requireAtLeastOneModel\":true,"
        "\"allowVersionAutoDetection\":false,\"maxDepth\": 7 ,"
        "\"maxAttributesPerElement\":8,\"maxTextBytesPerNode\":9,"
        "\"maxTotalInputBytes\":100,\"maxQueuedEvents\":2}";
    reader = iox_reader_create("xtf23", all_reader_options);
    assert(reader != NULL);
    iox_reader_destroy(reader);
    reader = iox_reader_create(
        "xtf24", "{\"expectedVersion\":\"24\",\"strict\":false,"
        "\"preserveUnknownExtensions\":true,"
        "\"requireAtLeastOneModel\":false,"
        "\"allowVersionAutoDetection\":true}");
    assert(reader != NULL);
    iox_reader_destroy(reader);
    reader = iox_reader_create("xtf24", NULL);
    assert(reader != NULL);
    iox_reader_destroy(reader);
    reader = iox_reader_create("json-events", "{\"sourceName\":\"events\"}");
    assert(reader != NULL);
    iox_reader_destroy(reader);

    const char* invalid_reader_options[] = {
        "{\"sourceName\":false}", "{\"expectedVersion\":false}",
        "{\"strict\":\"yes\"}",
        "{\"preserveUnknownExtensions\":1}",
        "{\"requireAtLeastOneModel\":null}",
        "{\"allowVersionAutoDetection\":[]}",
        "{\"maxDepth\":\"bad\"}",
        "{\"maxAttributesPerElement\":-1}",
        "{\"maxTextBytesPerNode\":1x}",
        "{\"maxTotalInputBytes\":false}",
        "{\"maxQueuedEvents\":null}"};
    for (size_t index = 0U;
         index < sizeof invalid_reader_options / sizeof invalid_reader_options[0];
         ++index) {
        assert(iox_reader_create("xtf", invalid_reader_options[index]) == NULL);
    }
    assert(iox_reader_create("xtf", "{\"expectedVersion\":\"unknown\"}") == NULL);
    assert(iox_reader_create("json-events", "{\"sourceName\":\"unterminated}") == NULL);

    /* Every writer option is parsed, including escaped strings. */
    writer = iox_writer_create(
        "xtf", "{\"version\":\"23\",\"sender\":\"s\\\"x\","
        "\"comment\":\"c\",\"software\":\"w\",\"strict\":true,"
        "\"pretty\":true,\"preserveUnknownExtensions\":false,"
        "\"deterministicPrefixes\":false}");
    assert(writer != NULL);
    iox_writer_destroy(writer);
    writer = iox_writer_create(
        "xtf24", "{\"version\":\"24\",\"strict\":false,"
        "\"pretty\":false,\"preserveUnknownExtensions\":true,"
        "\"deterministicPrefixes\":true}");
    assert(writer != NULL);
    iox_writer_destroy(writer);
    const char* invalid_writer_options[] = {
        "{\"version\":false}", "{\"sender\":false}",
        "{\"comment\":false}", "{\"software\":false}",
        "{\"strict\":1}", "{\"pretty\":null}",
        "{\"preserveUnknownExtensions\":[]}",
        "{\"deterministicPrefixes\":\"yes\"}"};
    for (size_t index = 0U;
         index < sizeof invalid_writer_options / sizeof invalid_writer_options[0];
         ++index) {
        assert(iox_writer_create("xtf", invalid_writer_options[index]) == NULL);
    }
    assert(iox_writer_create("unknown", NULL) == NULL);

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

    writer = iox_writer_create("json-events", NULL);
    assert(writer != NULL);
    const char* two_events =
        "{\"schema\":\"iox-event/2\",\"event\":\"endTransfer\"}\n"
        "{\"schema\":\"iox-event/2\",\"event\":\"endTransfer\"}\n";
    assert(iox_writer_write_event_json(writer, two_events, strlen(two_events),
                                       &result) == IOX_STATUS_ERROR);
    iox_result_destroy(result);
    assert(iox_writer_write_event_json(writer, end_event, strlen(end_event),
                                       &result) == IOX_STATUS_ERROR);
    iox_result_destroy(result);
    assert(iox_writer_take_output(writer, &result) == IOX_STATUS_ERROR);
    assert(iox_result_status(result) == IOX_STATUS_ERROR);
    iox_result_destroy(result);
    assert(iox_writer_finish(writer, &result) == IOX_STATUS_ERROR);
    iox_result_destroy(result);
    iox_writer_destroy(writer);

    writer = iox_writer_create("json-events", NULL);
    assert(writer != NULL);
    const char* two_valid_events =
        "{\"schema\":\"iox-event/2\",\"event\":\"startTransfer\","
        "\"header\":{\"version\":\"2.3\",\"sender\":\"x\","
        "\"models\":[],\"oidSpaces\":[],\"extensions\":[]}}\n"
        "{\"schema\":\"iox-event/2\",\"event\":\"endTransfer\"}\n";
    assert(iox_writer_write_event_json(writer, two_valid_events,
                                       strlen(two_valid_events), &result) ==
           IOX_STATUS_ERROR);
    assert(contains(iox_result_json(result), "json.malformed"));
    iox_result_destroy(result);
    iox_writer_destroy(writer);

    writer = iox_writer_create("json-events", NULL);
    assert(writer != NULL);
    assert(iox_writer_write_event_json(writer, start_event, strlen(start_event),
                                       &result) == IOX_STATUS_OK);
    iox_result_destroy(result);
    assert(iox_writer_finish(writer, &result) == IOX_STATUS_ERROR);
    assert(contains(iox_result_json(result), "invalid_event_order"));
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
    assert(iox_reader_feed(NULL, (const uint8_t*)"x", 1U) ==
           IOX_STATUS_INVALID_ARGUMENT);
    assert(iox_reader_finish(NULL) == IOX_STATUS_INVALID_ARGUMENT);
    assert(iox_reader_next(NULL, &result) == IOX_STATUS_INVALID_ARGUMENT);
    assert(result != NULL);
    iox_result_destroy(result);
    assert(iox_writer_take_output(NULL, NULL) == IOX_STATUS_INVALID_ARGUMENT);
    assert(iox_writer_finish(NULL, &result) == IOX_STATUS_INVALID_ARGUMENT);
    assert(result != NULL);
    iox_result_destroy(result);
    assert(iox_writer_write_event_json(NULL, NULL, 0U, &result) ==
           IOX_STATUS_INVALID_ARGUMENT);
    assert(result != NULL);
    assert(contains(iox_result_json(result), "abi.invalid_argument"));
    iox_result_destroy(result);
    assert(iox_result_json(NULL) == NULL);
    assert(iox_result_bytes(NULL) == NULL);
    assert(iox_result_size(NULL) == 0U);
    assert(iox_result_status(NULL) == IOX_STATUS_INVALID_ARGUMENT);

    reader = iox_reader_create("json-events", NULL);
    assert(reader != NULL);
    assert(iox_reader_feed(reader, NULL, 1U) == IOX_STATUS_INVALID_ARGUMENT);
    assert(iox_reader_next(reader, NULL) == IOX_STATUS_INVALID_ARGUMENT);
    assert(iox_reader_feed(reader, (const uint8_t*)"{\n", 2U) ==
           IOX_STATUS_ERROR);
    assert(iox_reader_feed(reader, NULL, 0U) == IOX_STATUS_INVALID_STATE);
    assert(iox_reader_finish(reader) == IOX_STATUS_INVALID_STATE);
    assert(iox_reader_next(reader, &result) == IOX_STATUS_ERROR);
    assert(contains(iox_result_json(result), "json.malformed"));
    iox_result_destroy(result);
    iox_reader_destroy(reader);

    reader = iox_reader_create("json-events", NULL);
    assert(reader != NULL);
    assert(iox_reader_finish(reader) == IOX_STATUS_ERROR);
    assert(iox_reader_next(reader, &result) == IOX_STATUS_ERROR);
    iox_result_destroy(result);
    iox_reader_destroy(reader);

    /* Diagnostic result JSON escapes controls and retains warning severity. */
    const char weird_options[] =
        "{\"sourceName\":\"\\\"\\\\\b\f\n\r\t\x01\"}";
    reader = iox_reader_create("xtf23", weird_options);
    assert(reader != NULL);
    assert(iox_reader_feed(reader, (const uint8_t*)"<", 1U) == IOX_STATUS_OK);
    assert(iox_reader_finish(reader) == IOX_STATUS_ERROR);
    assert(iox_reader_next(reader, &result) == IOX_STATUS_ERROR);
    assert(contains(iox_result_json(result), "\\\""));
    assert(contains(iox_result_json(result), "\\\\"));
    assert(contains(iox_result_json(result), "\\b"));
    assert(contains(iox_result_json(result), "\\f"));
    assert(contains(iox_result_json(result), "\\n"));
    assert(contains(iox_result_json(result), "\\r"));
    assert(contains(iox_result_json(result), "\\t"));
    assert(contains(iox_result_json(result), "\\u0001"));
    iox_result_destroy(result);
    iox_reader_destroy(reader);

    const char* warning_xtf =
        "<TRANSFER xmlns=\"http://www.interlis.ch/INTERLIS2.3\">"
        "<HEADERSECTION VERSION=\"2.3\" SENDER=\"s\"><MODELS>"
        "<MODEL NAME=\"M\"/></MODELS><VENDOR/></HEADERSECTION>"
        "<DATASECTION/></TRANSFER>";
    reader = iox_reader_create("xtf23", NULL);
    assert(reader != NULL);
    assert(iox_reader_feed(reader, (const uint8_t*)warning_xtf,
                           strlen(warning_xtf)) == IOX_STATUS_OK);
    assert(iox_reader_finish(reader) == IOX_STATUS_OK);
    assert(iox_reader_next(reader, &result) == IOX_STATUS_EVENT);
    assert(contains(iox_result_json(result), "\"severity\":\"warning\""));
    iox_result_destroy(result);
    iox_reader_destroy(reader);

    puts("ABI 2 smoke tests passed");
    return 0;
}

#ifndef IOX_ABI_SMOKE_NO_MAIN
int main(void) {
    return iox_abi_smoke();
}
#endif
