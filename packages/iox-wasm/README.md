# @interlis/iox-wasm

INTERLIS XTF 2.3/2.4 Reader/Writer WebAssembly module.

## Status

The Phase-0 baseline loads the generated Emscripten ES module and exposes the
ABI/version query. Reader and writer convenience wrappers are added in Phase
8; the C ABI is already available to low-level consumers.

## Usage (future)

```js
import { createIoxModule, XtfReader } from '@interlis/iox-wasm';

const mod = await createIoxModule();
const reader = new XtfReader(mod, inputData);
for (const event of reader) {
  console.log(event);
}
```
