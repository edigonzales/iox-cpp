#include "iox/abi/iox.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static int contains_bytes(const uint8_t* bytes, size_t size,
                          const char* needle) {
    const size_t needle_size = strlen(needle);
    if (needle_size == 0) return 1;
    if (bytes == NULL || size < needle_size) return 0;
    for (size_t i = 0; i + needle_size <= size; ++i) {
        if (memcmp(bytes + i, needle, needle_size) == 0) return 1;
    }
    return 0;
}

int main(void) {
    /* Version */
    uint32_t abi = iox_abi_version();
    assert(abi == 1);
    const char* ver = iox_version();
    assert(ver != NULL);
    assert(strlen(ver) > 0);
    printf("ABI version: %u, Version: %s\n", abi, ver);

    /* Reader creation / destruction */
    iox_reader_t* reader = iox_reader_create("xtf", NULL);
    assert(reader != NULL);

    /* Feed minimal XTF 2.3 */
    const char* xtf_data =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<ili:TRANSFER xmlns:ili=\"http://www.interlis.ch/INTERLIS2.3\">"
        "<ili:HEADERSECTION>"
        "<ili:SENDER>Test</ili:SENDER>"
        "<ili:SOFTWARE>ABI</ili:SOFTWARE>"
        "</ili:HEADERSECTION>"
        "</ili:TRANSFER>";
    iox_status_t st = iox_reader_feed(reader, (const uint8_t*)xtf_data, strlen(xtf_data));
    assert(st == IOX_STATUS_OK);

    st = iox_reader_finish(reader);
    assert(st == IOX_STATUS_OK);

    /* Read first event */
    iox_result_t* result = NULL;
    st = iox_reader_next(reader, &result);
    assert(st == IOX_STATUS_EVENT);
    assert(result != NULL);
    const char* json = iox_result_json(result);
    assert(json != NULL);
    printf("Event 1: %s\n", json);
    iox_result_destroy(result);

    /* Read second event (EndTransfer auto-emitted on TRANSFER close) */
    st = iox_reader_next(reader, &result);
    assert(st == IOX_STATUS_EVENT);
    iox_result_destroy(result);

    /* Next should be END */
    st = iox_reader_next(reader, &result);
    assert(st == IOX_STATUS_END);
    if (result) iox_result_destroy(result);

    iox_reader_destroy(reader);

    /* Incremental JSON reader: NeedInput, Event, and End are distinct ABI
       states and the event payload must survive chunking. */
    reader = iox_reader_create("json-events", NULL);
    assert(reader != NULL);
    const char* json_evt =
        "{\"event\":\"startTransfer\",\"sender\":\"chunked\","
        "\"version\":24}";
    const size_t json_evt_size = strlen(json_evt);
    st = iox_reader_feed(reader, (const uint8_t*)json_evt, json_evt_size / 2);
    assert(st == IOX_STATUS_OK);
    result = NULL;
    st = iox_reader_next(reader, &result);
    assert(st == IOX_STATUS_NEED_INPUT);
    assert(result != NULL);
    assert(iox_result_status(result) == IOX_STATUS_NEED_INPUT);
    assert(strstr(iox_result_json(result), "\"status\":\"need_input\"") != NULL);
    iox_result_destroy(result);
    st = iox_reader_feed(reader,
                         (const uint8_t*)json_evt + json_evt_size / 2,
                         json_evt_size - json_evt_size / 2);
    assert(st == IOX_STATUS_OK);
    st = iox_reader_feed(reader, (const uint8_t*)"\n", 1);
    assert(st == IOX_STATUS_OK);
    st = iox_reader_next(reader, &result);
    assert(st == IOX_STATUS_EVENT);
    assert(strstr(iox_result_json(result), "\"sender\":\"chunked\"") != NULL);
    iox_result_destroy(result);
    st = iox_reader_finish(reader);
    assert(st == IOX_STATUS_OK);
    st = iox_reader_next(reader, &result);
    assert(st == IOX_STATUS_END);
    assert(result != NULL);
    iox_result_destroy(result);
    iox_reader_destroy(reader);

    /* Writer test */
    iox_writer_t* writer = iox_writer_create("json-events", NULL);
    assert(writer != NULL);

    /* Write a complete event, take its output, then write and take again. */
    const char* startEvt =
        "{\"event\":\"startTransfer\",\"sender\":\"ABI\",\"version\":23}\n";
    st = iox_writer_write_event_json(writer, startEvt, strlen(startEvt), &result);
    assert(st == IOX_STATUS_OK);
    assert(result != NULL);
    assert(strstr(iox_result_json(result), "\"ok\":true") != NULL);
    iox_result_destroy(result);
    result = NULL;
    st = iox_writer_take_output(writer, &result);
    assert(st == IOX_STATUS_OK);
    assert(result != NULL);
    assert(iox_result_size(result) > 0);
    assert(contains_bytes(iox_result_bytes(result), iox_result_size(result),
                          "StartTransfer"));
    iox_result_destroy(result);

    /* Write EndTransfer */
    const char* endEvt = "{\"event\":\"endTransfer\"}\n";
    st = iox_writer_write_event_json(writer, endEvt, strlen(endEvt), &result);
    assert(st == IOX_STATUS_OK);
    iox_result_destroy(result);
    result = NULL;
    st = iox_writer_take_output(writer, &result);
    assert(st == IOX_STATUS_OK);
    assert(result != NULL && iox_result_size(result) > 0);
    iox_result_destroy(result);

    /* Malformed input is a structured error and never an exception. */
    st = iox_writer_write_event_json(writer, "{", 1, &result);
    assert(st == IOX_STATUS_ERROR);
    assert(result != NULL);
    assert(strstr(iox_result_json(result), "\"ok\":false") != NULL);
    assert(strstr(iox_result_json(result), "json.parse_error") != NULL);
    iox_result_destroy(result);

    /* Finish and get output */
    st = iox_writer_finish(writer, &result);
    assert(st == IOX_STATUS_OK);
    assert(result != NULL);
    assert(iox_result_size(result) == 0);
    iox_result_destroy(result);
    st = iox_writer_take_output(writer, &result);
    assert(st == IOX_STATUS_INVALID_STATE);
    assert(result != NULL);
    iox_result_destroy(result);

    iox_writer_destroy(writer);

    /* XTF writer still emits a deterministic transfer document. */
    writer = iox_writer_create("xtf", NULL);
    assert(writer != NULL);
    st = iox_writer_write_event_json(writer, "{\"type\":\"StartTransfer\"}",
                                     strlen("{\"type\":\"StartTransfer\"}"), &result);
    assert(st == IOX_STATUS_OK);
    iox_result_destroy(result);
    st = iox_writer_write_event_json(writer, "{\"type\":\"EndTransfer\"}",
                                     strlen("{\"type\":\"EndTransfer\"}"), &result);
    assert(st == IOX_STATUS_OK);
    iox_result_destroy(result);
    st = iox_writer_finish(writer, &result);
    assert(st == IOX_STATUS_OK);
    assert(iox_result_size(result) > 0);
    printf("XTF writer output size: %zu\n", iox_result_size(result));
    iox_result_destroy(result);
    iox_writer_destroy(writer);

    /* Null-pointer safety */
    iox_reader_destroy(NULL);
    iox_writer_destroy(NULL);
    iox_result_destroy(NULL);
    assert(iox_reader_create(NULL, NULL) == NULL);
    assert(iox_reader_feed(NULL, NULL, 0) == IOX_STATUS_INVALID_ARGUMENT);
    assert(iox_writer_take_output(NULL, NULL) == IOX_STATUS_INVALID_ARGUMENT);

    printf("All ABI tests passed!\n");
    return 0;
}
