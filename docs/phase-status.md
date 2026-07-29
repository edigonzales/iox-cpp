# iox-cpp Phase Status

| Phase | Status | Commit | Native Tests | WASM Tests | Coverage | Notes |
|-------|--------|--------|-------------|------------|----------|-------|
| 0 | completed | 4347ba1 | `./scripts/build-native.sh` (0); `./scripts/test-native.sh` (0, 18/18) | `./scripts/build-wasm.sh` (0, Emscripten 3.1.64); `./scripts/test-wasm.sh` (0, 2/2) | N/A | Baseline, pinned references, agent controls, native/WASM ABI smoke |
| 1 | completed | e8fec5b | `./scripts/build-native.sh` (0); `./scripts/test-native.sh` (0, 19/19) | `./scripts/build-wasm.sh` (0); `./scripts/test-wasm.sh` (0, 2/2) | N/A | Ordered event helpers, QName metadata aliases, lexical primitive compatibility |
| 2 | completed | aa8f47c | `./scripts/build-native.sh` (0); `./scripts/test-native.sh` (0, 20/20) | `./scripts/build-wasm.sh` (0); `./scripts/test-wasm.sh` (0, 2/2) | N/A | Pinned Expat, DTD/entity rejection, depth/attribute limits, deterministic XML |
| 3 | completed | b5845b1 | `./scripts/build-native.sh` (0); `./scripts/test-native.sh` (0, 21/21) | `./scripts/build-wasm.sh` (0); `./scripts/test-wasm.sh` (0, 2/2) | N/A | XTF 2.3 objects, structures, references, ordered chunk/state tests |
| 4 | completed | 75e2c33 | `./scripts/build-native.sh` (0); `./scripts/test-native.sh` (0, 21/21); `./scripts/coverage.sh` (0, 21/21) | `./scripts/build-wasm.sh` (0); `./scripts/test-wasm.sh` (0, 2/2) | Instrumented gate passed; threshold report deferred to Phase 11 | XTF 2.3 COORD/ARC/POLYLINE/SURFACE/AREA fixtures and recursive geometry assertion |
| 5 | completed | 24b3fff | `./scripts/build-native.sh` (0); `./scripts/test-native.sh` (0, 22/22) | `./scripts/build-wasm.sh` (0); `./scripts/test-wasm.sh` (0, 2/2) | N/A | XTF 2.4 namespace-aware objects, expanded QNames, deterministic prefix bindings |
| 6 | completed | 2e219e7 | `./scripts/build-native.sh` (0); `./scripts/test-native.sh` (0, 22/22) | `./scripts/build-wasm.sh` (0, Emscripten 3.1.64); `./scripts/test-wasm.sh` (0, 2/2) | `./scripts/coverage.sh` (0, 22/22; instrumentation gate) | XTF 2.4 canonical geometry tree, multi-geometries, custom line preservation, fixture chunk matrix |
| 7 | completed | 8b767a8 | `./scripts/build-native.sh` (0); `./scripts/test-native.sh` (0, 22/22) | `./scripts/build-wasm.sh` (0, Emscripten 3.1.64); `./scripts/test-wasm.sh` (0, 4/4) | `./scripts/coverage.sh` (0, 22/22); ASan+UBSan CTest (0, 22/22) | Complete streaming C ABI, chunkwise output, structured results, C-only and Node low-level ABI tests |
| 8 | completed | edf2974 | `./scripts/build-native.sh` (0); `./scripts/test-native.sh` (0, 22/22) | `./scripts/build-wasm.sh` (0, Emscripten 3.1.64); `./scripts/test-wasm.sh` (0, 8/8) | `./scripts/coverage.sh` (0, 22/22 instrumentation) | Idiomatic JS API, TypeScript unions, byte/iterator/incremental tests, module-worker protocol |
| 9 | completed | 017ab90 | `./scripts/build-native.sh` (0); `./scripts/test-native.sh` (0, 21/21, IOX_ENABLE_ILIC=OFF); explicit ilic build (0, 22/22) | `./scripts/build-wasm.sh` (0, Emscripten 3.1.64); `./scripts/test-wasm.sh` (0, 8/8) | `./scripts/coverage.sh` (0, 21/21 instrumentation) | Direct concrete ilic-core integration; no provider abstraction |
| 10 | completed | 05d3a6d | `cmake -S . -B build/phase10 ...` (0); `cmake --build build/phase10 --parallel` (0); `ctest --test-dir build/phase10 --output-on-failure` (0, 28/28) | `./scripts/build-wasm.sh` (0, Emscripten 3.1.64); `./scripts/test-wasm.sh` (0, 8/8) | `./scripts/coverage.sh` (0, 28/28 instrumentation) | BasketReader and limit diagnostics, scored factories, custom format, examples, iox-dump |
| 11 | completed | pending (this commit) | `./scripts/build-native.sh` (0); `./scripts/test-native.sh` (0, 31/31) | `./scripts/build-wasm.sh` (0, Emscripten 3.1.64); `./scripts/test-wasm.sh` (0, 8/8) | `./scripts/coverage.sh` (0, 31/31; 93.69% line, 85.09% branch) | ASan/UBSan 25/25, standalone fuzz 50 runs plus CTest 1/1, 10,000-object streaming, Native/WASM parity, deterministic roundtrip, public-header consumer, direct ilic-core 26/26 |

## Build Commands

```sh
# Native build and test
./scripts/build-native.sh
./scripts/test-native.sh

# WASM smoke test
node --test packages/iox-wasm/test/*.test.mjs
```

## Phase 7 — C-ABI Results

- Full C99 header with opaque handles (iox_reader_t, iox_writer_t, iox_result_t).
- Status codes: OK, EVENT, NEED_INPUT, END, ERROR, INVALID_ARGUMENT, INVALID_STATE.
- Reader: create, feed, finish, next, destroy; event results preserve the full
  ordered IOM payload and use the normalized lower-camel `event` discriminator.
- Writer: create, write_event_json, incremental `take_output`, finish, destroy.
- Result: structured JSON, bytes, size, status, destroy; all exported entry
  points contain C++ exceptions.
- Native C-only ABI test covers small chunks, NeedInput/Event/End, output
  chunks, malformed JSON, invalid state, null arguments, and XTF output.
- Node.js/WASM low-level ABI test covers the same state and result contract.

### Phase 7 verification commands

```text
./scripts/build-native.sh                         # exit 0
./scripts/test-native.sh                          # exit 0, 22/22
source /Users/stefan/sources/emsdk/emsdk_env.sh >/dev/null && ./scripts/build-wasm.sh  # exit 0, 3.1.64
source /Users/stefan/sources/emsdk/emsdk_env.sh >/dev/null && ./scripts/test-wasm.sh   # exit 0, 4/4
./scripts/coverage.sh                             # exit 0, 22/22 instrumented
cmake -S . -B build/asan -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DIOX_BUILD_EXAMPLES=OFF -DIOX_ENABLE_ASAN=ON -DIOX_ENABLE_UBSAN=ON
cmake --build build/asan --parallel                    # exit 0
ctest --test-dir build/asan --output-on-failure         # exit 0, 22/22
```

On this macOS runner `ASAN_OPTIONS=detect_leaks=1` is unsupported by the
platform runtime; the sanitizer pass was therefore run without that option.

## Phase 0 — Results

- `git rev-parse --show-toplevel` → `/Users/stefan/sources/iox-cpp-codex`
- Initial pre-Git identity commands failed with `fatal: not a git repository`;
  the user explicitly required initialization, so `git init -b main` was run.
- `cmake 4.4.0`, AppleClang 21, Node.js v22.23.1, Emscripten 3.1.64.
- Expat is static and fetched from the pinned 2.6.4 archive hash.
- The generated ES module is copied to `packages/iox-wasm` by the build script;
  the `.wasm` binary remains a build product and is ignored by Git.

## Phase 5 — Completed

- XTF 2.4 detection accepts the normative namespace
  `http://www.interlis.ch/xtf/2.4/INTERLIS` and the historical compatibility
  namespace used by existing fixtures.
- Expat expanded names are retained in `IomName::xmlName()` for baskets,
  objects, attributes, and nested structures; unknown model namespaces receive
  deterministic writer prefixes with collision handling.
- Native and WASM gates pass with 22/22 native tests and 2/2 WASM smoke tests.

## Phase 6 — Completed

- Direct XTF 2.4 geometry members are normalized to ordered canonical IOM
  segments; multi-geometries and exterior/interior rings are preserved.
- `UnsupportedGeometry.xml` verifies custom line forms remain structured
  objects rather than being dropped or converted to coordinates.
- Fixture parsing passes at one-byte, seven-byte, and 64-byte chunk sizes.
- `./scripts/build-native.sh` (0), `./scripts/test-native.sh` (0, 22/22),
  `./scripts/build-wasm.sh` (0, Emscripten 3.1.64),
  `./scripts/test-wasm.sh` (0, 2/2), and `./scripts/coverage.sh` (0, 22/22).

## Phase 8 — Completed

- `createIoxModule` exposes the pinned Emscripten module and ABI/version.
- `XtfReader` accepts UTF-8 strings, `Uint8Array`, and `ArrayBuffer`, supports
  iterator early-return cleanup, and returns canonical `event`-discriminated
  events.
- `IncrementalXtfReader` feeds arbitrary chunks and drains NeedInput/Event/End
  through the same native C ABI.
- `XtfWriter`, `readAll`, and `writeAll` preserve ordered attributes,
  repeated values, nested structures, and drain writer output incrementally.
- `worker.js` implements request-ID based `init`, `readAll`, `writeAll`, and
  `close`; `handleWorkerMessage` is a Node/browser-independent harness.
- `index.d.ts` contains the complete public discriminated-union declarations;
  no local TypeScript compiler was installed, so the declaration smoke checks
  exported symbols and rejects public `any` types.

### Phase 8 verification commands

```text
./scripts/build-native.sh                         # exit 0
./scripts/test-native.sh                          # exit 0, 22/22
source /Users/stefan/sources/emsdk/emsdk_env.sh >/dev/null && ./scripts/build-wasm.sh  # exit 0, 3.1.64
source /Users/stefan/sources/emsdk/emsdk_env.sh >/dev/null && ./scripts/test-wasm.sh   # exit 0, 8/8
./scripts/coverage.sh                             # exit 0, 22/22 instrumented
node --input-type=module <<'NODE' ... NODE           # exit 0, TypeScript declaration smoke
```

## Phase 9 — Completed

`iox-ilic` is optional and links directly to the pinned `ilic-core` target.
The actual fork API was inspected before implementation: it exposes
`metamodel::Model`, `SubModel`, `Class`, and `AttrOrParam`, rather than the
template names `TransferDescription`, `Topic`, `Viewable`, and `Element`.
The adapter uses those concrete types and adds the fork's `source` directory
to the target include path because the metamodel header is not installed as a
public include. No model-provider interface or invented type aliases were
introduced.

The default build does not configure or build `iox-ilic`. The explicit local
integration build uses the immutable adjacent checkout at
`8582fff47549f8e0ac4d1cd6ec39c66c2bb708b0`, disables the fork's tests and
native repository, and runs the model-index, strict validation, reference
type, and transfer-order tests. `IOX_FETCH_ILIC` is also available with the
same immutable Git tag for an explicitly requested dependency-fetch build.

### Phase 9 verification commands

```text
./scripts/build-native.sh                              # exit 0
./scripts/test-native.sh                               # exit 0, 21/21 (default, ilic OFF)
cmake -S . -B build/ilic -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DIOX_BUILD_EXAMPLES=OFF -DIOX_ENABLE_ILIC=ON -DIOX_ILIC_SOURCE_DIR=/Users/stefan/sources/ilic-fork  # exit 0
cmake --build build/ilic --parallel                    # exit 0
ctest --test-dir build/ilic --output-on-failure         # exit 0, 22/22
source /Users/stefan/sources/emsdk/emsdk_env.sh >/dev/null && ./scripts/build-wasm.sh  # exit 0, Emscripten 3.1.64
source /Users/stefan/sources/emsdk/emsdk_env.sh >/dev/null && ./scripts/test-wasm.sh   # exit 0, 8/8
./scripts/coverage.sh                                   # exit 0, 21/21 instrumented
```

The optional integration is native-only in this phase; the required
model-free WASM package remains unchanged and passes its complete Node test
suite.

## Phase 10 — Completed

- `BasketReader` owns a generic reader, exposes the transfer header, consumes
  complete baskets, and enforces an optional object-count memory limit with
  stable fatal diagnostics.
- `ReaderFactory`/`WriterFactory` and `defaultFormatRegistry()` provide
  deterministic built-in XTF and JSON-event selection. Content sniff scores
  outrank conflicting extensions.
- `iox-dump` has print, NDJSON, and roundtrip smoke coverage. C++ reader,
  roundtrip, and custom-format examples are compiled and executed; the Node
  example runs against the generated WASM package.

### Phase 10 verification commands

```text
cmake -S . -B build/phase10 -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DIOX_BUILD_EXAMPLES=ON -DIOX_BUILD_TOOLS=ON  # exit 0
cmake --build build/phase10 --parallel                    # exit 0
ctest --test-dir build/phase10 --output-on-failure         # exit 0, 28/28
node examples/node-read-events.mjs test/fixtures/xtf23/dataSection/EmptyBasket.xtf  # exit 0, 4 events
source /Users/stefan/sources/emsdk/emsdk_env.sh >/dev/null && ./scripts/build-wasm.sh  # exit 0, Emscripten 3.1.64
source /Users/stefan/sources/emsdk/emsdk_env.sh >/dev/null && ./scripts/test-wasm.sh   # exit 0, 8/8
./scripts/coverage.sh                                      # exit 0, 28/28 instrumented
```

The Phase 10 scope has no separate sanitizer or fuzz gate; those final gates
remain explicitly scheduled for Phase 11.

## Phase 11 — Completed

The final hardening gates passed on 2026-07-29. The coverage report includes
the public inline API headers and excludes third-party, tests, the optional
factory convenience module, examples/tools, and generated WASM glue. The
result is 93.69% line and 85.09% branch coverage for the core-library report.

### Phase 11 verification commands

```text
./scripts/build-native.sh                                      # exit 0
./scripts/test-native.sh                                       # exit 0, 31/31
source /Users/stefan/sources/emsdk/emsdk_env.sh >/dev/null && ./scripts/build-wasm.sh  # exit 0, Emscripten 3.1.64
source /Users/stefan/sources/emsdk/emsdk_env.sh >/dev/null && ./scripts/test-wasm.sh   # exit 0, 8/8
./scripts/coverage.sh                                          # exit 0, 31/31; 93.69% line, 85.09% branch
cmake -S . -B build/asan11 -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DIOX_BUILD_EXAMPLES=OFF -DIOX_BUILD_TOOLS=OFF -DIOX_ENABLE_ASAN=ON -DIOX_ENABLE_UBSAN=ON  # exit 0
cmake --build build/asan11 --parallel                             # exit 0
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=print_stacktrace=1 ctest --test-dir build/asan11 --output-on-failure  # exit 0, 25/25
cmake -S . -B build/fuzz -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DIOX_BUILD_EXAMPLES=OFF -DIOX_BUILD_TOOLS=OFF -DIOX_ENABLE_FUZZING=ON  # exit 0
cmake --build build/fuzz --parallel                              # exit 0
build/fuzz/test/iox-fuzz-xtf --runs 50 test/fuzz/corpus/empty.xtf  # exit 0, 50 standalone runs
ctest --test-dir build/fuzz -R iox\\.fuzz\\.xtf --output-on-failure  # exit 0, 1/1
cmake -S . -B build/ilic11 -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DIOX_BUILD_EXAMPLES=OFF -DIOX_BUILD_TOOLS=OFF -DIOX_ENABLE_ILIC=ON -DIOX_ILIC_SOURCE_DIR=/Users/stefan/sources/ilic-fork  # exit 0
cmake --build build/ilic11 --parallel                           # exit 0
ctest --test-dir build/ilic11 --output-on-failure               # exit 0, 26/26
./scripts/conformance.sh                                      # exit 0 (optional differential stub; regular conformance is CTest)
```

The native suite includes the 10,000-object, 4096-byte chunk streaming test,
the public-header consumer, and the coverage edge-path suite. A manual
Native/WASM event-kind comparison over `EmptyBasket.xtf` produced the same
`startTransfer,startBasket,endBasket,endTransfer` sequence. Two native
roundtrips of the same fixture were byte-identical at 277 bytes. The regular
CTest suite performs no network access; Java remains outside the regular test
path. Remaining TODO/FIXME markers are confined to generated Emscripten glue,
not product or test sources.
