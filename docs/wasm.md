# WebAssembly

Das Emscripten-Modul stellt XTF-Reader und -Writer für Node, Browser und Module
Worker bereit. Toolchain und Node-Version stehen in den Buildskripten und
Workflows.

```sh
source /path/to/emsdk/emsdk_env.sh
./scripts/build-wasm.sh
./scripts/test-wasm.sh
```

## JavaScript-API

```js
import { createIoxModule, IncrementalXtfReader, XtfWriter } from '@ilic/iox-wasm';

const module = await createIoxModule();
const reader = new IncrementalXtfReader(module, { expectedVersion: '2.3' });
for await (const chunk of inputStream) {
  for (const event of reader.feed(chunk)) console.log(event);
}
for (const event of reader.finish()) console.log(event);
reader.close();

const writer = new XtfWriter(module, { version: '2.3', sender: 'MyApp' });
writer.write(startTransferEvent);
await outputStream.write(writer.takeOutput());
writer.write(endTransferEvent);
await outputStream.write(writer.finish());
writer.close();
```

`XtfReader` bietet einen synchronen Batch-Iterator, `IncrementalXtfReader`
einen Chunk-Stream. Alle Events entsprechen
[`iox-event/2`](event-json-schema.md). `close()` ist verbindlich; Wrapper
verwenden `try/finally`.

`worker.js` bietet Batch- und Streamingoperationen mit Request-ID,
Session-Handle und explizitem Cancel/Close. Unbekannte oder doppelte Handles
werden abgewiesen; Worker-Neustart teilt keine alten Sessions.

Browser- und Node-Tests vergleichen Event-/Diagnose-Payloads mit der nativen
C-ABI. Web Streams werden als Chunks gefüttert; es gibt keine DOM- oder
synchrone Browser-Dateisystemschicht. Das modellfreie Paket enthält kein
ilic-WASM-Bundle.
