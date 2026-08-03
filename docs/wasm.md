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
├── package.json          # @interlis/iox-wasm
├── index.js              # ES module entry point
├── index.d.ts            # TypeScript declarations
├── worker.js             # Web Worker protocol
├── test/
│   └── *.test.mjs        # Node.js tests
├── README.md
├── LICENSE
└── THIRD_PARTY_NOTICES.md
```

## JavaScript API

### Module Initialization

```js
import { createIoxModule } from '@interlis/iox-wasm';

const mod = await createIoxModule({
    locateFile: (path) => `/wasm/${path}`
});

console.log(mod.abiVersion()); // 2
console.log(mod.version());    // "0.2.0"
```

### Reading XTF

```js
import { createIoxModule, XtfReader } from '@interlis/iox-wasm';

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
import { IncrementalXtfReader } from '@interlis/iox-wasm';

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
import { XtfWriter } from '@interlis/iox-wasm';

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

const output = writer.finish(); // Uint8Array
```

### Web Worker

```js
// main.js — use a module worker
const worker = new Worker(new URL('./worker.js', import.meta.url), { type: 'module' });
worker.postMessage({ type: 'init', requestId: 1 });
worker.onmessage = (e) => {
    const { requestId, ok, events, bytes, error } = e.data;
    // handle response
};

// The same request-ID protocol supports readAll, writeAll, and close.
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

The baseline WASM module (model-free XTF reader/writer) is expected to be
under 500 KB gzipped. The optional `iox-ilic` integration would add the
`ilic-core` dependency, increasing size significantly.

## Limitations

- No DOM APIs used in the JavaScript layer
- No synchronous file I/O in browser environments
- Web Streams API not yet integrated (incremental reader provides foundation)
- `TextEncoder`/`TextDecoder` used for UTF-8 conversion
