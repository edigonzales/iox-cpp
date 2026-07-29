# @interlis/iox-wasm

INTERLIS XTF 2.3/2.4 Reader/Writer WebAssembly module.

## Status

The package exposes the generated Emscripten ES module through a synchronous
reader/writer API after asynchronous initialization. It supports XTF 2.3 and
2.4, incremental byte feeds, ordered `event`-discriminated Iox events, and a
request-ID based module-worker protocol.

## Usage

```js
import { createIoxModule, XtfReader } from '@interlis/iox-wasm';

const mod = await createIoxModule();
const reader = new XtfReader(mod, inputData);
for (const event of reader) {
  console.log(event);
}
reader.close();
```
