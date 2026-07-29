# iox-cpp

INTERLIS XTF 2.3/2.4 Reader/Writer Framework — Native C++17 + WebAssembly

## Scope

- **XTF 2.3** — full read/write (objects, references, geometry)
- **XTF 2.4** — full read/write (namespaces, multi-geometry)
- **Native** — macOS ARM64, Linux x86_64, Windows x86_64
- **WebAssembly** — browser, web worker, Node.js ≥ 18
- **Model-free** — works without compiled INTERLIS models
- **Model-aware** — optional direct `ilic-core` integration

## Non-Goals

- ITF, INTERLIS 1, CSV, eCH-0118/GML
- Full data validation (ilivalidator style)
- GEOS/JTS/GDAL geometry conversion
- Dynamic plugins (dlopen/DLL)
- GUI, CI/CD pipelines

## Status

See [docs/roadmap.md](docs/roadmap.md) and [docs/phase-status.md](docs/phase-status.md).

| Phase | Status |
|-------|--------|
| 0 — Baseline | ✅ completed |
| 1 — IOM + Events + JSON | ✅ completed |
| 2 — XML + XTF headers | ✅ completed |
| 3 — XTF 2.3 Objects | ✅ completed |
| 4 — XTF 2.3 Geometry | ✅ completed |
| 5 — XTF 2.4 Objects | ✅ completed |
| 6 — XTF 2.4 Geometry | ✅ completed |
| 7 — C-ABI | ✅ completed |
| 8 — JS/WASM API | ✅ completed |
| 9 — ilic-core | infrastructure ready |
| 10 — Convenience + Tools | ✅ completed |
| 11 — Hardening | partial |

## Quick Start

### Native Build

```sh
./scripts/build-native.sh
./scripts/test-native.sh
```

### WebAssembly

```sh
./scripts/build-wasm.sh
./scripts/test-wasm.sh
```

### Minimal C++ Reader (future)

```cpp
#include <iox/xtf/XtfReader.h>
auto reader = iox::xtf::XtfReader::create("data.xtf");
while (auto event = reader->next()) {
    std::visit([](auto& e) { /* handle */ }, *event);
}
```

### Minimal JavaScript (future)

```js
import { createIoxModule, XtfReader } from '@interlis/iox-wasm';
const mod = await createIoxModule();
const reader = new XtfReader(mod, inputData);
for (const event of reader) {
  console.log(event);
}
```

## License

MIT — see [LICENSE](LICENSE)
