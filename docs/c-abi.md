# C ABI and WASM boundary

The public header is [`include/iox/abi/iox.h`](../include/iox/abi/iox.h). It is
C99-compatible and exposes only opaque reader, writer, and result handles.
The same implementation is compiled into the native `iox-abi` library and the
Emscripten `iox-wasm` ES module.

Inputs are borrowed only for the duration of the call. Results own their JSON
string and output bytes until `iox_result_destroy` is called. `iox_alloc` and
`iox_free` are provided for buffers passed to the WASM heap. No C++ exception
is allowed to cross an exported function.

## Reader state machine

1. Create a reader for `xtf`, `xtf23`, `xtf24`, or `json-events`.
2. Call `iox_reader_feed` with any number of byte chunks.
3. Call `iox_reader_next`; it returns a result with `event`, `need_input`, or
   `end` status. Event results contain the complete ordered event payload.
4. Call `iox_reader_finish` when the input stream ends, then drain remaining
   events and the final `end` result.

`iox_reader_next` returns `IOX_STATUS_ERROR` for fatal diagnostics. The result
JSON contains the first error and the complete diagnostic array.
Feeding or finishing after `finish`, finishing twice, and calling `next` after
the final `end` are invalid states. Destroy functions accept null handles.

`xtf23` and `xtf24` select an exact dialect; a conflicting `expectedVersion`
is rejected at creation. `xtf` auto-detects by default. Reader options expose
strictness, source name, extension preservation, model requirement, version
autodetection, expected version, and every XML/queue limit. Invalid option
types or unsupported versions make creation fail without crossing the C
boundary with an exception.

## Writer output

`iox_writer_write_event_json` accepts one event object using the schema in
[`event-json-schema.md`](event-json-schema.md). `iox_writer_take_output` moves
bytes emitted since the previous take. `iox_writer_finish` closes the writer
and returns the final output chunk. This makes XTF output usable with bounded
memory in both native and WASM clients.

Calling `write_event_json` or `take_output` after `finish` returns
`IOX_STATUS_INVALID_STATE` and a structured error result.

Writer options expose the exact XTF version, strictness, pretty printing,
sender/comment/software metadata, extension preservation, and deterministic
prefix selection. `xtf24` selects the 2.4 writer even when no `version` option
is present.

## Result schema

All results use schema 2. A successful event result is:

```json
{"schema":"iox-result/2","ok":true,"status":"event","event":{"schema":"iox-event/2","event":"startTransfer","header":{"version":"2.3","sender":"iox-cpp","models":[],"oidSpaces":[],"extensions":[]}},"error":null,"diagnostics":[]}
```

Error result:

```json
{"schema":"iox-result/2","ok":false,"status":"error","event":null,"error":{"code":"json.malformed","message":"...","location":{"sourceName":"","byteOffset":0,"line":0,"column":0}},"diagnostics":[]}
```

The native NDJSON reader/writer, C ABI, Node.js, browser, and worker surfaces
all use the same `iox-event/2` payload without normalization layers.
