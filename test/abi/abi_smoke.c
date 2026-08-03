#include "iox/abi/iox.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int contains(const char* text, const char* needle) {
    return text != NULL && strstr(text, needle) != NULL;
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

    assert(iox_writer_write_event_json(writer, end_event, strlen(end_event),
                                       &result) == IOX_STATUS_OK);
    iox_result_destroy(result);
    assert(iox_writer_finish(writer, &result) == IOX_STATUS_OK);
    assert(iox_result_size(result) > 0U);
    iox_result_destroy(result);
    assert(iox_writer_finish(writer, &result) == IOX_STATUS_INVALID_STATE);
    assert(contains(iox_result_json(result), "\"ok\":false"));
    iox_result_destroy(result);
    iox_writer_destroy(writer);

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
        "<ili:HEADERSECTION><ili:SENDER>ABI</ili:SENDER>"
        "</ili:HEADERSECTION></ili:TRANSFER>";
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

    /* Null argument safety and result accessors. */
    assert(iox_reader_create(NULL, NULL) == NULL);
    assert(iox_writer_create(NULL, NULL) == NULL);
    assert(iox_reader_feed(NULL, NULL, 0U) == IOX_STATUS_INVALID_ARGUMENT);
    assert(iox_reader_next(NULL, &result) == IOX_STATUS_INVALID_ARGUMENT);
    assert(result != NULL);
    iox_result_destroy(result);
    assert(iox_writer_take_output(NULL, NULL) == IOX_STATUS_INVALID_ARGUMENT);
    assert(iox_result_json(NULL) == NULL);
    assert(iox_result_bytes(NULL) == NULL);
    assert(iox_result_size(NULL) == 0U);
    assert(iox_result_status(NULL) == IOX_STATUS_INVALID_ARGUMENT);

    puts("ABI 2 smoke tests passed");
    return 0;
}
