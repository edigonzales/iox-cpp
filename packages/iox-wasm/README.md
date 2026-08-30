# @interlis/iox-wasm

INTERLIS XTF 2.3/2.4 Reader/Writer WebAssembly module.

## Status

`@interlis/iox-wasm` is not yet published on npm. The release workflow and
package checks are ready, but the npm package name and Trusted Publisher still
require the one-time owner/2FA bootstrap described in the release guide.

The package exposes the generated Emscripten ES module through a synchronous
reader/writer API after asynchronous initialization. It supports XTF 2.3 and
2.4, incremental byte feeds, ordered `event`-discriminated Iox events, and a
request-ID based module-worker protocol.

## Usage

```js
import {
  createIoxModule, IncrementalXtfReader, XtfWriter
} from '@interlis/iox-wasm';

const mod = await createIoxModule();
const reader = new IncrementalXtfReader(mod);
for await (const chunk of inputStream) {
  for (const event of reader.feed(chunk)) console.log(event);
}
for (const event of reader.finish()) console.log(event);
reader.close();

const writer = new XtfWriter(mod, { version: '2.3' });
writer.write(startTransferEvent);
await outputStream.write(writer.takeOutput());
writer.write(endTransferEvent);
await outputStream.write(writer.finish());
writer.close();
```

`worker.js` exposes the same batch and streaming operations to browser and
Node module workers, including explicit cancellation of unfinished sessions.
Its protocol types are declared in `worker.d.ts`.
