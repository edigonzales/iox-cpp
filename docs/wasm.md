# WebAssembly Guide

`iox-cpp` supports WebAssembly via Emscripten 3.1.64. The WASM module
provides a JavaScript API for reading and writing INTERLIS XTF files
in browsers, Web Workers, and Node.js.

## Prerequisites

- Emscripten 3.1.64 (exact version pinned in `.emscripten-version`)
- Node.js ≥ 18
- CMake 3.20+

## Building

```sh
# Activate Emscripten
source /path/to/emsdk/emsdk_env.sh

# Build WASM
./scripts/build-wasm.sh
```

The build produces an ES module with `MODULARIZE=1` and `EXPORT_ES6=1`.

## Package Structure

```
packages/iox-wasm/
├── package.json          # @ilic/iox-wasm
├── index.js              # ES module entry point
├── index.d.ts            # TypeScript declarations
├── worker.js             # Web Worker protocol
├── worker.d.ts            # Worker request/response declarations
├── test/
│   ├── *.test.mjs        # Node.js tests
│   └── browser-smoke.html # Real browser + module-worker smoke
├── README.md
├── LICENSE
└── THIRD_PARTY_NOTICES.md
```

## JavaScript API

### Module Initialization

```js
import { createIoxModule } from '@ilic/iox-wasm';

const mod = await createIoxModule({
    locateFile: (path) => `/wasm/${path}`
});

console.log(mod.abiVersion()); // 2
console.log(mod.version());    // "0.2.0"
```

### Reading XTF

```js
import { createIoxModule, XtfReader } from '@ilic/iox-wasm';

const mod = await createIoxModule();
const data = new Uint8Array(await readFile('data.xtf'));

// Batch read
const reader = new XtfReader(mod, data);
for (const event of reader) {
    console.log(event.event, event);
}
reader.close();
```

### Incremental Reading

```js
import { IncrementalXtfReader } from '@ilic/iox-wasm';

const reader = new IncrementalXtfReader(mod, { expectedVersion: '2.3' });

// Feed chunks as they arrive
for await (const chunk of stream) {
    const events = reader.feed(chunk);
    for (const event of events) {
        console.log(event);
    }
}

// Finalize
const remaining = reader.finish();
```

### Writing XTF

```js
import { XtfWriter } from '@ilic/iox-wasm';

const writer = new XtfWriter(mod, { version: '2.3', sender: 'MyApp' });

writer.write({ schema: 'iox-event/2', event: 'startTransfer', header: {
    version: '2.3', sender: 'MyApp', models: [], oidSpaces: [], extensions: []
}});
writer.write({ schema: 'iox-event/2', event: 'startBasket', basket: {
    topic: { interlisName: 'Model.Topic', xml: null }, basketId: 'B1',
    kind: 'full', consistency: 'complete', domains: [], topics: [], extensions: [],
    location: { sourceName: '', byteOffset: 0, line: 0, column: 0 }
}});
writer.write({ schema: 'iox-event/2', event: 'object', object: {
    tag: { interlisName: 'Model.Topic.MyClass', xml: null }, oid: 'T1',
    operation: 'insert', consistency: 'complete', reference: null,
    location: { sourceName: '', byteOffset: 0, line: 0, column: 0 },
    attributes: [{ name: { interlisName: 'Name', xml: null },
                   values: [{ kind: 'primitive', value: 'test' }] }]
}});
writer.write({ schema: 'iox-event/2', event: 'endBasket' });
writer.write({ schema: 'iox-event/2', event: 'endTransfer' });

// Drain bounded chunks while writing; each call forgets returned bytes.
const available = writer.takeOutput();
const final = writer.finish();
```

`finish()` is terminal. A second finish, a write after finish, or a
`takeOutput()` after finish throws `IoxError` with `api.invalid_state`.

### Web Worker

```js
// main.js — use a module worker
const worker = new Worker(new URL('./worker.js', import.meta.url), { type: 'module' });
worker.postMessage({ type: 'init', requestId: 1 });
worker.onmessage = (e) => {
    const { requestId, ok, events, bytes, error } = e.data;
    // handle response
};

// Batch operations: readAll, writeAll.
// Streaming operations:
worker.postMessage({ type: 'readerCreate', requestId: 2, streamId: 'r1' });
worker.postMessage({
    type: 'readerFeed', requestId: 3, streamId: 'r1', input: chunk
});
worker.postMessage({ type: 'readerFinish', requestId: 4, streamId: 'r1' });

// Writers analogously use writerCreate, writerWrite, and writerFinish.
// readerClose/writerClose cancel one unfinished session.
// close releases all streams owned by the worker.
```

## Supported Environments

| Environment | Status | Notes |
|-------------|--------|-------|
| Node.js ≥ 18 | ✅ | Primary test target |
| Browser | ✅ | No DOM dependencies |
| Web Worker | ✅ | Message-based protocol |
| Deno | ⚠️ | Not tested, may work with ES module shims |

## Memory and Handles

- All C++ objects are managed through opaque handles
- Handles must be explicitly released (`close()`, `destroy()`)
- JavaScript wrappers use `try/finally` patterns for safety
- Input data crosses the boundary as `Uint8Array` or `ArrayBuffer`
- Output data is returned as `Uint8Array`

## C ABI Consistency

The JavaScript API delegates to the same C ABI used by native code.
Event and diagnostic JSON schemas are identical between native and WASM:

```sh
# Native
./build/native/iox-dump --events data.xtf > native.ndjson

# WASM
node -e "..." > wasm.ndjson

# Compare (should be identical after source location normalization)
diff native.ndjson wasm.ndjson
```

## Size Considerations

The Phase 19 model-free Release build is 662,998 bytes raw and 231,266 bytes
with `gzip -9` (Emscripten 3.1.64). The ordinary test artifact is a Debug build
and is intentionally much larger.

No ilic-WASM bundle is shipped in 0.2. The current Java-derived ilic-core has
no WASM C-ABI contract, and the local native archives measure 5,567,104 bytes
for `iox-ilic` plus 148,154,520 bytes for `ilic-core` before final linking.
Adding that dependency to the model-free bundle without a measured,
tree-shaken exported surface would be misleading. It remains an optional,
separate future artifact and does not block the model-free package.

## Limitations

- No DOM APIs used in the JavaScript layer
- No synchronous file I/O in browser environments
- Web Streams are consumed by feeding their chunks to `IncrementalXtfReader`;
  no DOM or full-input buffering layer is involved
- `TextEncoder`/`TextDecoder` used for UTF-8 conversion
