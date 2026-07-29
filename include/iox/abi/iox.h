#ifndef IOX_H
#define IOX_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

/* --- Opaque handles --- */
typedef struct iox_reader iox_reader_t;
typedef struct iox_writer iox_writer_t;
typedef struct iox_result iox_result_t;

/* --- Status codes --- */
typedef enum iox_status {
    IOX_STATUS_OK = 0,
    IOX_STATUS_EVENT = 1,
    IOX_STATUS_NEED_INPUT = 2,
    IOX_STATUS_END = 3,
    IOX_STATUS_ERROR = -1,
    IOX_STATUS_INVALID_ARGUMENT = -2,
    IOX_STATUS_INVALID_STATE = -3
} iox_status_t;

/* --- Version --- */
uint32_t iox_abi_version(void);
const char* iox_version(void);

/* --- Memory (for WASM interop) --- */
void* iox_alloc(size_t size);
void iox_free(void* ptr);

/* --- Reader --- */
iox_reader_t* iox_reader_create(const char* format,
                                const char* options_json);
void iox_reader_destroy(iox_reader_t* reader);

iox_status_t iox_reader_feed(iox_reader_t* reader,
                             const uint8_t* data,
                             size_t size);
iox_status_t iox_reader_finish(iox_reader_t* reader);
iox_status_t iox_reader_next(iox_reader_t* reader,
                             iox_result_t** result);

/* --- Writer --- */
iox_writer_t* iox_writer_create(const char* format,
                                const char* options_json);
void iox_writer_destroy(iox_writer_t* writer);

iox_status_t iox_writer_write_event_json(
    iox_writer_t* writer,
    const char* event_json,
    size_t event_json_size,
    iox_result_t** result);

iox_status_t iox_writer_finish(iox_writer_t* writer,
                               iox_result_t** result);

/* --- Result handle --- */
const char* iox_result_json(const iox_result_t* result);
const uint8_t* iox_result_bytes(const iox_result_t* result);
size_t iox_result_size(const iox_result_t* result);
iox_status_t iox_result_status(const iox_result_t* result);
void iox_result_destroy(iox_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* IOX_H */
