# iox-cpp

`iox-cpp` is a model-free INTERLIS XTF 2.3/2.4 reader and writer for native
C++17 and WebAssembly. Its source of truth is an ordered `std::variant`
event stream containing transfers, baskets, objects, and end events.

## Scope

- XTF 2.3 objects, references, structures, and geometry;
- XTF 2.4 namespaces, references, structures, and multi-geometry;
- ordered, repeated IOM attributes with copy-on-write objects;
- secure incremental XML parsing with private Expat 2.6.4;
- deterministic XML writing;
- C99 ABI and the `@interlis/iox-wasm` Node/browser/worker package;
- optional direct `ilic-core` integration through `iox-ilic`.

ITF, INTERLIS 1, GML/CSV conversion, full constraint validation, GEOS/JTS
conversion, dynamic plugins, GUI code, and CI/CD are intentionally out of
scope.

## Status

Phases 0 through 10 are implemented. Phase 11 contains the final coverage,
sanitizer, fuzz, and clean-build gates. See
[`docs/phase-status.md`](docs/phase-status.md) and
[`docs/roadmap.md`](docs/roadmap.md) for exact results.

## Build and test

```sh
./scripts/build-native.sh
./scripts/test-native.sh

source /path/to/emsdk/emsdk_env.sh
./scripts/build-wasm.sh
./scripts/test-wasm.sh
```

The regular build is offline after its pinned Expat source is available. It
does not require Java. To build the optional model-aware module against an
existing pinned checkout:

```sh
cmake -S . -B build/ilic \
  -DBUILD_TESTING=ON -DIOX_ENABLE_ILIC=ON \
  -DIOX_ILIC_SOURCE_DIR=/path/to/ilic-fork
cmake --build build/ilic --parallel
ctest --test-dir build/ilic --output-on-failure
```

## Minimal C++ reader and writer

```cpp
#include "iox/Factory.h"
#include "iox/Basket.h"
#include <memory>
#include <string>

std::string input; // bytes read from data.xtf
auto reader = iox::ReaderFactory::create("data.xtf", iox::ByteView(input));
reader->feed(iox::ByteView(input));
reader->finish();
for (auto event : iox::readAll(*reader)) {
    std::visit([](const auto& value) {
        // Handle the canonical event variant.
    }, event);
}

auto sink = std::make_shared<iox::StringOutputSink>();
auto writer = iox::WriterFactory::create("xtf", sink);
writer->write(iox::StartTransferEvent{});
writer->write(iox::EndTransferEvent{});
writer->close();
```

`iox::BasketReader` is the deliberately buffering convenience facade for
applications that process one basket at a time. The reader/event API remains
lossless and should be used when object operation or identity metadata is
needed.

## Command-line tool

```text
build/native/iox-dump input.xtf
build/native/iox-dump --events input.xtf
build/native/iox-dump --roundtrip input.xtf output.xtf
```

`iox-dump` is a diagnostic and integration example, not a full product CLI.

## WebAssembly and JavaScript

```js
import { createIoxModule, XtfReader } from '@interlis/iox-wasm';

const module = await createIoxModule();
const reader = new XtfReader(module, inputBytes);
for (const event of reader) console.log(event.event);
reader.close();
```

The package exposes `readAll`, `writeAll`, incremental readers, writers, and a
module-worker protocol. See [`docs/wasm.md`](docs/wasm.md) and the package
[`README`](packages/iox-wasm/README.md).

## Extending formats

Formats register explicit reader and writer creators in a
`FormatRegistry`; there are no global plugin constructors or dynamic loading.
The complete custom-format example is
[`examples/cpp-custom-format.cpp`](examples/cpp-custom-format.cpp), with API
guidance in [`docs/extending-formats.md`](docs/extending-formats.md).

## Architecture and conformance

See [`docs/architecture.md`](docs/architecture.md) for module boundaries and
[`docs/conformance.md`](docs/conformance.md) for immutable dependency and
normative document references.

## License

MIT. See [`LICENSE`](LICENSE).
