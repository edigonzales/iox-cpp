# @ilic/iox-wasm

WebAssembly-Reader/-Writer für INTERLIS XTF 2.3/2.4. Bis zum ersten stabilen
Release wird der ausdrücklich gewählte Snapshot-Kanal installiert:

```sh
npm install @ilic/iox-wasm@snapshot
```

Snapshots entstehen nur durch den manuellen Release-Workflow. Die Version
folgt `X.Y.Z-snapshot.g<12-stelliger Source-SHA>`; das Paket enthält
`interlis-release.json` und einen vollständigen `gitHead`.

```js
import { createIoxModule, IncrementalXtfReader } from '@ilic/iox-wasm';

const module = await createIoxModule();
const reader = new IncrementalXtfReader(module);
for await (const chunk of inputStream) {
  for (const event of reader.feed(chunk)) console.log(event);
}
for (const event of reader.finish()) console.log(event);
reader.close();
```

`worker.js` stellt dieselben Batch- und Streamingoperationen für Browser- und
Node-Module-Worker bereit, einschliesslich Abbruch nicht abgeschlossener
Sessions. Protokolltypen stehen in `worker.d.ts`; das Eventformat ist unter
[`docs/event-json-schema.md`](../../docs/event-json-schema.md) dokumentiert.
