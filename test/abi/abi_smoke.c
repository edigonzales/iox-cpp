#include "iox/abi/iox.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

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

    /* Writer test */
    iox_writer_t* writer = iox_writer_create("xtf", NULL);
    assert(writer != NULL);

    /* Write a StartTransfer event via JSON */
    const char* startEvt = "{\"type\":\"StartTransfer\"}\n";
    st = iox_writer_write_event_json(writer, startEvt, strlen(startEvt), &result);
    assert(st == IOX_STATUS_OK);
    iox_result_destroy(result);

    /* Write EndTransfer */
    const char* endEvt = "{\"type\":\"EndTransfer\"}\n";
    st = iox_writer_write_event_json(writer, endEvt, strlen(endEvt), &result);
    assert(st == IOX_STATUS_OK);
    iox_result_destroy(result);

    /* Finish and get output */
    st = iox_writer_finish(writer, &result);
    assert(st == IOX_STATUS_OK);
    assert(result != NULL);
    size_t outSize = iox_result_size(result);
    assert(outSize > 0);
    printf("Writer output size: %zu\n", outSize);
    iox_result_destroy(result);

    iox_writer_destroy(writer);

    /* Null-pointer safety */
    iox_reader_destroy(NULL);
    iox_writer_destroy(NULL);
    iox_result_destroy(NULL);
    assert(iox_reader_create(NULL, NULL) == NULL);
    assert(iox_reader_feed(NULL, NULL, 0) == IOX_STATUS_INVALID_ARGUMENT);

    printf("All ABI tests passed!\n");
    return 0;
}
