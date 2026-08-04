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
| 11 | completed | 7e98bef | `./scripts/build-native.sh` (0); `./scripts/test-native.sh` (0, 31/31) | `./scripts/build-wasm.sh` (0, Emscripten 3.1.64); `./scripts/test-wasm.sh` (0, 8/8) | Historical aggregate; superseded by Phase 20 module gates | ASan/UBSan 25/25, standalone fuzz 50 runs plus CTest 1/1, 10,000-object streaming, Native/WASM parity, deterministic roundtrip, public-header consumer, direct ilic-core 26/26 |
| post-11 | completed | d4093bf | `./scripts/build-native.sh` (0); `./scripts/test-native.sh` (0, 33/33); `iox.test.iox_ili.porting` (4/4); fixture manifest (0) | `./scripts/build-wasm.sh` (0, Emscripten 3.1.64); `./scripts/test-wasm.sh` (0, 8/8) | Historical aggregate; superseded by Phase 20 module gates | Pinned iox-ili method matrix, 211 XTF fixtures plus 9 model files, fixture manifest, chunk/event/diagnostic parity, semantic writer roundtrip |
| 13 | completed | 22f33fc | Top-level warnings-as-errors build (0); CTest 33/33; direct ilic build CTest 28/28 | Emscripten 3.1.64 build (0); Node/WASM 8/8 | deferred to Phase 20 | Version 0.2.0 API reset, ABI 2, event/result schema 2, lexical IOM values, ordered COW objects, stable diagnostics, private yyjson 0.12.0 |
| 14 | completed | 36679b5 | Top-level warnings-as-errors build (0); CTest 33/33; direct ilic build CTest 28/28 | Emscripten 3.1.64 build (0); Node/WASM 8/8 | deferred to Phase 20 | Private Expat/XML implementation, callback exception containment, UTF-8 and resource limits, source positions, namespace-aware deterministic writer |
| 15 | completed | fdfb681 | Top-level warnings-as-errors build (0); CTest 33/33; direct ilic build CTest 28/28 | Emscripten 3.1.64 build (0); Node/WASM 8/8 | deferred to Phase 20 | Private XTF 2.3 dialect, exact state/header/data rules, bounded event queue, lexical objects/references/geometry, strict/lenient option coverage |
| 16 | completed | 4687479 | Top-level warnings-as-errors build (0); CTest 34/34; direct ilic build CTest 29/29 | Emscripten 3.1.64 build (0); Node/WASM 8/8 | deferred to Phase 20 | Terminal streaming XTF 2.3 writer, complete metadata/reference/geometry output, independent byte goldens and semantic roundtrips |
| 17 | completed | e13dd0d | Top-level warnings-as-errors build (0); CTest 35/35; direct ilic build CTest 30/30 | Emscripten 3.1.64 build (0); Node/WASM 8/8 | deferred to Phase 20 | Independent private XTF 2.4 reader/writer dialect, exact control QNames, namespace preservation, roles and multi-geometries |
| 18 | completed | 04bac0b | Top-level warnings-as-errors build (0); CTest 35/35; direct ilic warnings-as-errors build (0), CTest 30/30 | Emscripten 3.1.64 build (0); Node/WASM 8/8 | deferred to Phase 20 | Concrete MetaModelStore API, pointer-free compact index, exact translations/QNames, roles, views, transient members, enums and transfer order |
| 19 | completed | 07ba57e | Top-level warnings-as-errors build (0); CTest 35/35; direct ilic build CTest 30/30 | Emscripten 3.1.64 build (0); Node/WASM 10/10; real browser + module worker pass | deferred to Phase 20 | ABI/result schema 2 states, bounded JS writer output, streaming Node/browser workers, normalized diagnostic/event and exact 236-byte writer parity |
| 20 | completed | this commit | Debug 38/38; Release 38/38; ASan/UBSan 38/38; 100,000-object reader/writer smoke | Emscripten 3.1.64 build (0); Node/WASM 10/10 | core 98.12/85.81; json 97.77/87.22; xml 94.86/88.26; xtf 93.24/86.16; ABI 90.25/85.52; ilic 92.32/75.00 (line/branch %) | Five fuzz targets 5/5; 214-method matrix; seven-case pinned Java differential; macOS only, Linux/Windows open |

The older Phase 11 and post-11 percentages were aggregate reports, not the
per-module release gates required for 0.2. Phase 20 supersedes them with one
unified instrumented test executable and exact module reports; LLVM emitted no
profile-mismatch warning.

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

The default build does not configure or build `iox-ilic`. The historical
Phase-9 verification used the adjacent checkout at
`8582fff47549f8e0ac4d1cd6ec39c66c2bb708b0`. The current release contract
supersedes that remote pin with immutable tag `v0.9.10`; local joint
development still uses `IOX_ILIC_SOURCE_DIR`. Both paths disable the fork's
CLI, tests, and native repository through its dedicated CMake options and run
the model-index, strict validation, reference type, and transfer-order tests.

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

The then-current hardening gates passed on 2026-07-29. Its aggregate coverage
number is retained in Git history but is no longer release evidence: it did not
provide the exact per-module accounting required by the 0.2 plan. Phase 20
replaced that report and removed the profile-data mismatch warning.

### Phase 11 verification commands

```text
./scripts/build-native.sh                                      # exit 0
./scripts/test-native.sh                                       # exit 0, 31/31
source /Users/stefan/sources/emsdk/emsdk_env.sh >/dev/null && ./scripts/build-wasm.sh  # exit 0, Emscripten 3.1.64
source /Users/stefan/sources/emsdk/emsdk_env.sh >/dev/null && ./scripts/test-wasm.sh   # exit 0, 8/8
./scripts/coverage.sh                                          # historical aggregate; superseded by Phase 20
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

## Post-11 — iox-ili XTF test-porting matrix

The matrix uses the immutable `iox-ili` revision
`1af01d4bf6b675a490b9f5ad44d41723fdfa3c0f` and maps 207 XTF Java test methods
plus 7 XTF factory/utility methods. The checked-in corpus contains 211 XTF
transfer fixtures and 9 model-support `.ili` files. The regular C++ test path
uses no Java and no network access.

### Post-11 verification commands

```text
./scripts/verify-iox-ili-fixtures.sh                         # exit 0, 211 transfers, 9 model files
./scripts/build-native.sh                                    # exit 0
./scripts/test-native.sh                                     # exit 0, 33/33
cmake -S . -B build/ilic-matrix -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DIOX_BUILD_EXAMPLES=OFF -DIOX_BUILD_TOOLS=OFF -DIOX_ENABLE_ILIC=ON -DIOX_ILIC_SOURCE_DIR=/Users/stefan/sources/ilic-fork  # exit 0
cmake --build build/ilic-matrix --parallel                     # exit 0
ctest --test-dir build/ilic-matrix --output-on-failure         # exit 0, 28/28
./scripts/coverage.sh                                        # historical aggregate; superseded by Phase 20
cmake -S . -B build/asan-matrix -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DIOX_BUILD_EXAMPLES=OFF -DIOX_BUILD_TOOLS=OFF -DIOX_ENABLE_ASAN=ON -DIOX_ENABLE_UBSAN=ON  # exit 0
cmake --build build/asan-matrix --parallel                    # exit 0
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=print_stacktrace=1 ctest --test-dir build/asan-matrix --output-on-failure  # exit 0, 27/27
cmake -S . -B build/fuzz-matrix -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DIOX_BUILD_EXAMPLES=OFF -DIOX_BUILD_TOOLS=OFF -DIOX_ENABLE_FUZZING=ON  # exit 0
cmake --build build/fuzz-matrix --parallel                    # exit 0
build/fuzz-matrix/test/iox-fuzz-xtf --runs 50 test/fuzz/corpus/empty.xtf  # exit 0, 50 standalone runs
ctest --test-dir build/fuzz-matrix -R iox\\.fuzz\\.xtf --output-on-failure  # exit 0, 1/1
source /Users/stefan/sources/emsdk/emsdk_env.sh >/dev/null && ./scripts/build-wasm.sh  # exit 0, Emscripten 3.1.64
source /Users/stefan/sources/emsdk/emsdk_env.sh >/dev/null && ./scripts/test-wasm.sh   # exit 0, 8/8
```

The generic C++ assertions compare the complete ordered event stream,
copy-on-write IOM contents, ordered/repeated values, references and
diagnostics. Writer checks compare deterministic bytes separately from the
semantic Reader → Writer → Reader roundtrip. Phase 20 revisited every
model-dependent row after the direct `iox-ilic` implementation. The current
matrix now permits only `adapted`, `deliberate-difference`, or `out-of-scope`;
no provisional gap status remains.

## Phase 13 — Core, error, and schema reset for 0.2

Phase 13 was verified on macOS on 2026-08-03. It intentionally breaks the
0.1 C++, C ABI, event JSON, and npm contracts together. `IomValue` now retains
only lexical primitives and nested objects; `IomObject` exposes ordered COW
mutators without escaping mutable references. INTERLIS names and XML QNames,
transfer and basket metadata, source locations, references, and extensions are
represented explicitly in `iox-event/2`. The C boundary uses ABI 2 and
`iox-result/2`, catches both expected and unexpected C++ exceptions, and
reports stable diagnostic codes. yyjson 0.12.0 is pinned to commit `8b4a38d`
and linked privately by `iox-json` only.

### Phase 13 verification commands

```text
git fetch --prune origin                                      # exit 0; main == origin/main == d4093bf6
cmake -S . -B build/phase13 -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DIOX_BUILD_EXAMPLES=ON -DIOX_BUILD_TOOLS=ON -DIOX_WARNINGS_AS_ERRORS=ON  # exit 0
cmake --build build/phase13 --parallel                        # exit 0
ctest --test-dir build/phase13 --output-on-failure            # exit 0, 33/33
cmake -S . -B build/phase13-ilic -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DIOX_BUILD_EXAMPLES=OFF -DIOX_BUILD_TOOLS=OFF -DIOX_ENABLE_ILIC=ON -DIOX_ILIC_SOURCE_DIR=/Users/stefan/sources/ilic-fork -DIOX_WARNINGS_AS_ERRORS=ON  # exit 0
cmake --build build/phase13-ilic --parallel                   # exit 0
ctest --test-dir build/phase13-ilic --output-on-failure       # exit 0, 28/28
./scripts/build-wasm.sh                                       # exit 0, Emscripten 3.1.64
./scripts/test-wasm.sh                                        # exit 0, 8/8
cd packages/iox-wasm && npm pack --dry-run                    # exit 0, 9 package files
```

No Linux or Windows run, sanitizer pass, fuzzing, or coverage threshold was
claimed for this phase. Those independent release gates remain assigned to
Phase 20.

## Phase 14 — Hardened incremental XML foundation

Phase 14 was verified on macOS on 2026-08-03. Expat and all XML event types
are now private implementation details. The incremental parser rejects DTDs,
external entities, non-UTF-8 declarations, malformed/truncated input, invalid
UTF-8, and configured depth, attribute, text, and total-input limit breaches.
It reports Expat byte, line, and column positions with the configured source
name. Exceptions from start, end, and text handlers stop Expat inside the C
callback and are rethrown as `IoxError` only after the C frame returns.

The internal writer validates XML 1.0 characters and UTF-8, escapes text and
attributes (including `]]>`), rejects duplicate expanded attributes and
invalid state transitions, maintains namespace scopes, assigns deterministic
prefixes, completes recoverable short writes, and reports zero/throwing sinks
as `io.error`. It never closes missing elements and its destructor performs no
throwing work. The pinned Java implementation was inspected; iox-cpp's
explicit DTD/entity restrictions are intentionally stricter and documented in
`docs/conformance.md`.

### Phase 14 verification commands

```text
cmake -S . -B build/phase14 -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DIOX_BUILD_EXAMPLES=ON -DIOX_BUILD_TOOLS=ON -DIOX_WARNINGS_AS_ERRORS=ON  # exit 0
cmake --build build/phase14 --parallel                        # exit 0
ctest --test-dir build/phase14 --output-on-failure            # exit 0, 33/33
cmake -S . -B build/phase14-ilic -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DIOX_BUILD_EXAMPLES=OFF -DIOX_BUILD_TOOLS=OFF -DIOX_ENABLE_ILIC=ON -DIOX_ILIC_SOURCE_DIR=/Users/stefan/sources/ilic-fork -DIOX_WARNINGS_AS_ERRORS=ON  # exit 0
cmake --build build/phase14-ilic --parallel                   # exit 0
ctest --test-dir build/phase14-ilic --output-on-failure       # exit 0, 28/28
./scripts/build-wasm.sh                                       # exit 0, Emscripten 3.1.64
./scripts/test-wasm.sh                                        # exit 0, 8/8
```

No Linux or Windows run, sanitizer pass, fuzzing, or coverage threshold was
claimed for this phase. Those release gates remain assigned to Phase 20.

## Phase 15 — Conformant XTF 2.3 reader

Phase 15 was verified on macOS on 2026-08-03. A single reader coordinator owns
version detection, event-order checks, chunk buffering, and a bounded event
deque. The private 2.3 dialect implements the exact transfer/header/data state
machine without exposing XML types or building a document DOM. Queue pressure
suspends Expat at `maxQueuedEvents`; draining via `next()` resumes buffered
64-KiB input pieces. XTF 2.2 is rejected explicitly.

The reader preserves model entries, aliases, OID spaces, comments, basket
metadata, operations, consistency, references, ordered/repeated structures,
source positions, extensions, and lexical primitive bytes. Geometry coverage
includes coordinates, arcs, custom line forms, line attributes, clipped
polylines, grouped clipped surfaces, boundaries, surfaces, and areas. Strict
mode enforces normative case, required header metadata, and structural rules;
lenient mode retains recoverable data and emits stable diagnostics. The pinned
Java reader was inspected for the same areas; intentional differences are
recorded in `docs/conformance.md`.

### Phase 15 verification commands

```text
cmake --build build/phase14 --parallel                        # exit 0
ctest --test-dir build/phase14 --output-on-failure            # exit 0, 33/33
cmake --build build/phase14-ilic --parallel                   # exit 0
ctest --test-dir build/phase14-ilic --output-on-failure       # exit 0, 28/28
source /Users/stefan/sources/emsdk/emsdk_env.sh >/dev/null && ./scripts/build-wasm.sh  # exit 0, Emscripten 3.1.64
source /Users/stefan/sources/emsdk/emsdk_env.sh >/dev/null && ./scripts/test-wasm.sh   # exit 0, 8/8
```

The normative PDF and pinned Java source were inspected during implementation;
normal CTest and WASM tests use neither network nor Java. No Linux or Windows
run, sanitizer pass, fuzzing, or coverage threshold is claimed for this phase;
those independent gates remain assigned to Phase 20.

## Phase 16 — Conformant XTF 2.3 writer

Phase 16 was verified on macOS on 2026-08-03. `XtfWriter` now validates the
event sequence before dispatch and delegates XTF 2.3 wire rules to one private
dialect. Fatal state, content, XML, and sink failures make the writer terminal;
`close()` succeeds only after `EndTransfer`, is then idempotent, and never
synthesizes events. Output is written incrementally through `OutputSink`.

The dialect writes normative header order, complete model and OID-space data,
aliases, comments, basket metadata, object operations and consistency,
references including embedded association payloads, structures, lexical
primitives, OID-valued attributes, and the required coordinate, arc, custom
line-form, line-attribute, clipped polyline, boundary, surface and area shapes.
Unknown extensions are preserved or diagnosed according to policy and are
fatal in strict mode. Independently specified byte goldens are checked before
semantic Reader → Writer → Reader comparisons. The pinned Java
`XtfWriterAlt.java` was inspected, while the normal tests use neither Java nor
network access.

### Phase 16 verification commands

```text
cmake --build build/phase14 --parallel                        # exit 0
ctest --test-dir build/phase14 --output-on-failure            # exit 0, 34/34
cmake --build build/phase14-ilic --parallel                   # exit 0
ctest --test-dir build/phase14-ilic --output-on-failure       # exit 0, 29/29
source /Users/stefan/sources/emsdk/emsdk_env.sh >/dev/null && ./scripts/build-wasm.sh  # exit 0, Emscripten 3.1.64
source /Users/stefan/sources/emsdk/emsdk_env.sh >/dev/null && ./scripts/test-wasm.sh   # exit 0, 8/8
```

No Linux or Windows run, sanitizer pass, fuzzing, or coverage threshold is
claimed for this phase; those independent release gates remain assigned to
Phase 20.

## Phase 17 — Conformant XTF 2.4 dialect

Phase 17 was verified on macOS on 2026-08-03. XTF 2.4 now has independent
private reader and writer dialects; the common coordinators retain only event
ordering, queueing, XML mechanics and terminal-state handling. The former
public compatibility dialect and its legacy encoded-name path were removed.
The reader captures root namespace declarations, enforces exact 2.4 expanded
control names, reads the normative text-model header, maps complete basket and
object metadata, preserves QNames and lexical primitives, and handles direct,
embedded and ordered references.

The 2.4 geometry path implements coordinates, arcs, polylines, multi-points,
multi-polylines, surfaces and multi-surfaces without reusing 2.3 clipping or
line-attribute wire rules. Unknown geometry remains visible and diagnosed.
The writer requires an explicit stored QName or header namespace mapping and
never invents a namespace URI. A checked byte golden, complete field
assertions, strict/lenient negative cases, one-byte and irregular chunks, and
the five pinned iox-ili XTF 2.4 writer fixtures provide independent and
roundtrip coverage. The normal tests use neither Java nor network access.

### Phase 17 verification commands

```text
cmake --build build/native -j 8                              # exit 0
ctest --test-dir build/native --output-on-failure            # exit 0, 35/35
cmake --build build/phase14-ilic --parallel                  # exit 0
ctest --test-dir build/phase14-ilic --output-on-failure      # exit 0, 30/30
source /Users/stefan/sources/emsdk/emsdk_env.sh >/dev/null && ./scripts/build-wasm.sh  # exit 0, Emscripten 3.1.64
source /Users/stefan/sources/emsdk/emsdk_env.sh >/dev/null && ./scripts/test-wasm.sh   # exit 0, 8/8
```

No Linux or Windows run, sanitizer pass, fuzzing, or coverage threshold is
claimed for this phase; those independent release gates remain assigned to
Phase 20.

## Phase 18 — Extended direct ilic transfer semantics

Phase 18 was verified on macOS on 2026-08-03. The optional public ilic API now
accepts the concrete `metamodel::MetaModelStore` from the pinned ilic fork.
`IlicModelIndex` copies a compact value index once and retains no metamodel
pointers; a lifetime regression test destroys the source store before lookup.
Canonical, translated and expanded XML names resolve exactly, and ambiguous
concepts fail with `ilic.model_mismatch` rather than selecting the first match.

The index and composed reader/writer cover translated models, topics, classes,
properties and hierarchical enumerations (including `OTHERS`), inherited
transfer order, structures, roles, embedded roles, association targets,
standalone associations, transient views and transient properties. The model
selected in the transfer header controls target-language INTERLIS names; XTF
2.4 retains origin-model wire QNames. Unknown reader values remain visible
with diagnostics, while configured writer rejection is fatal and terminal.
Canonical nested geometry/reference helper objects remain model-independent.
No general constraints, cardinalities or file-wide references are claimed as
validated.

### Phase 18 verification commands

```text
cmake -S . -B build/phase18 -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DIOX_BUILD_EXAMPLES=ON -DIOX_BUILD_TOOLS=ON -DIOX_WARNINGS_AS_ERRORS=ON  # exit 0
cmake --build build/phase18 --parallel 8                      # exit 0
ctest --test-dir build/phase18 --output-on-failure            # exit 0, 35/35
cmake -S . -B build/phase18-ilic -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DIOX_BUILD_EXAMPLES=OFF -DIOX_BUILD_TOOLS=OFF -DIOX_ENABLE_ILIC=ON -DIOX_ILIC_SOURCE_DIR=/Users/stefan/sources/ilic-fork -DIOX_WARNINGS_AS_ERRORS=ON  # exit 0
cmake --build build/phase18-ilic --parallel 8                 # exit 0
ctest --test-dir build/phase18-ilic --output-on-failure       # exit 0, 30/30
source /Users/stefan/sources/emsdk/emsdk_env.sh >/dev/null && ./scripts/build-wasm.sh  # exit 0, Emscripten 3.1.64
source /Users/stefan/sources/emsdk/emsdk_env.sh >/dev/null && ./scripts/test-wasm.sh   # exit 0, 8/8
```

Normal tests use neither Java nor network access. No Linux or Windows run,
sanitizer pass, fuzzing, or coverage threshold is claimed for this phase;
those independent release gates remain assigned to Phase 20.

## Phase 19 — Native, WASM, browser, and worker parity

Phase 19 was verified on macOS on 2026-08-04. The C ABI now maps every public
reader resource option, rejects conflicting/invalid versions and option types,
selects the actual 2.4 writer for `xtf24`, and reports all post-finish/end calls
as stable structured invalid states. Fatal parser diagnostics retain their
original code and source position across the JavaScript boundary. C entry
points continue to catch unexpected exceptions as `internal.error`.

The JavaScript writer exposes `takeOutput()` and forgets drained chunks, so it
does not accumulate a complete transfer. Reader/writer finish is terminal.
Browser and Node module workers now offer keyed incremental reader and writer
sessions in addition to batch calls, transfer exact ArrayBuffers, and release
all live handles on `close`. Type declarations cover the full worker protocol.
A shared fixed 236-byte XTF 2.3 vector is asserted by the
native C ABI, Node/WASM API, browser, and worker; malformed input yields the
same `xml.malformed` code, and direct/worker events compare after source
location normalization.

The real in-app browser loaded the ES module over HTTP and passed direct
reader/writer plus module-worker streaming checks. A separate Release build
measured 662,998 bytes raw and 231,266 bytes with `gzip -9`. No ilic-WASM
bundle was added: it has no exported WASM ABI yet, while the local native
`iox-ilic` and `ilic-core` archives already measure 5,567,104 and 148,154,520
bytes before final linking. The model-free package remains independent.

### Phase 19 verification commands

```text
cmake -S . -B build/phase19 -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DIOX_BUILD_EXAMPLES=ON -DIOX_BUILD_TOOLS=ON -DIOX_WARNINGS_AS_ERRORS=ON  # exit 0
cmake --build build/phase19 --parallel 8                      # exit 0
ctest --test-dir build/phase19 --output-on-failure            # exit 0, 35/35
cmake -S . -B build/phase19-ilic -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DIOX_BUILD_EXAMPLES=OFF -DIOX_BUILD_TOOLS=OFF -DIOX_ENABLE_ILIC=ON -DIOX_ILIC_SOURCE_DIR=/Users/stefan/sources/ilic-fork -DIOX_WARNINGS_AS_ERRORS=ON  # exit 0
cmake --build build/phase19-ilic --parallel 8                 # exit 0
ctest --test-dir build/phase19-ilic --output-on-failure       # exit 0, 30/30
./scripts/build-wasm.sh                                       # exit 0, Emscripten 3.1.64
./scripts/test-wasm.sh                                        # exit 0, 10/10
emcmake cmake -S . -B build/phase19-wasm-release -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DIOX_BUILD_WASM=ON -DIOX_BUILD_EXAMPLES=OFF -DIOX_BUILD_TOOLS=OFF -DIOX_WARNINGS_AS_ERRORS=ON  # exit 0
cmake --build build/phase19-wasm-release --target iox-wasm --parallel 8  # exit 0; 662998 raw, 231266 gzip -9
python3 -m http.server 8765 --bind 127.0.0.1  # browser-smoke.html?phase19=2 passed in real browser and module worker
cd packages/iox-wasm && npm pack --dry-run                    # exit 0, 10 files
```

Normal tests use neither Java nor network access. No Linux or Windows run,
sanitizer pass, fuzzing, or coverage threshold is claimed for this phase;
those independent release gates remain assigned to Phase 20.

## Phase 20 — Independent conformance and release gates

Phase 20 was verified on macOS on 2026-08-04. Positive release fixtures now
assert concrete event sequences, header/basket/object fields, QNames,
references, nested association values, and lexical primitives. Negative
fixtures assert one exact fatal diagnostic code. The method-level verifier
counts 214 relevant pinned iox-ili methods and 15 explicit out-of-scope
methods; every row is `adapted`, `deliberate-difference`, or `out-of-scope`.
The optional offline Java comparison passed all seven representative cases at
the exact pinned commit. Normal tests still require neither Java nor network.

Five separately buildable fuzz targets cover the private XML parser, XTF
reader, event JSON, COW IOM object mutations, and writer event sequences. The
Apple Command Line Tools installation has no libFuzzer runtime, so this run
used the checked deterministic mutation driver for 50 inputs per target. On a
Clang installation with compiler-rt, the same targets use libFuzzer and ASan.
The native suite reads and writes 100,000 objects, drains a queue bounded to
two events, and repeats reader/writer work while bounding post-warmup resident
growth.

Coverage now uses one unified instrumented executable, preventing the previous
profile/binary mismatch. Both the model-free 179-test run and the direct-ilic
193-test run passed. Exact ilic-enabled line/branch results were: core
98.12/85.81, JSON 97.77/87.22, XML 94.86/88.26, XTF 93.24/86.16, C ABI
90.25/85.52, and ilic 92.32/75.00 percent. LLVM reported no mismatched profile
data. The script applies the same per-module thresholds when run with GCC and
gcovr.

Warnings are errors by default only when iox-cpp is the top-level project; an
embedding-project check confirmed the default is off for consumers. Newer
CMake versions use linker-aware archive deduplication. The remaining configure
warning is an upstream CMake deprecation from the pinned Expat source, not a
project compilation warning.

### Phase 20 verification commands

```text
cmake -S . -B build/phase20 -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DIOX_BUILD_EXAMPLES=ON -DIOX_BUILD_TOOLS=ON  # exit 0; IOX_WARNINGS_AS_ERRORS=ON by default
cmake --build build/phase20 --parallel 8                      # exit 0
ctest --test-dir build/phase20 --output-on-failure            # exit 0, 38/38
cmake -S . -B build/phase20-release -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DIOX_BUILD_EXAMPLES=ON -DIOX_BUILD_TOOLS=ON  # exit 0
cmake --build build/phase20-release --parallel 8              # exit 0
ctest --test-dir build/phase20-release --output-on-failure    # exit 0, 38/38
cmake -S . -B build/phase20-sanitize -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DIOX_BUILD_EXAMPLES=ON -DIOX_BUILD_TOOLS=ON -DIOX_ENABLE_ASAN=ON -DIOX_ENABLE_UBSAN=ON  # exit 0
cmake --build build/phase20-sanitize --parallel 8             # exit 0
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=print_stacktrace=1 ctest --test-dir build/phase20-sanitize --output-on-failure  # exit 0, 38/38
./scripts/build-wasm.sh                                       # exit 0, Emscripten 3.1.64
./scripts/test-wasm.sh                                        # exit 0, 10/10
cmake -S . -B build/phase20-fuzz -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DIOX_BUILD_EXAMPLES=OFF -DIOX_BUILD_TOOLS=OFF -DIOX_ENABLE_FUZZING=ON  # exit 0
cmake --build build/phase20-fuzz --parallel 8                 # exit 0
ctest --test-dir build/phase20-fuzz -L fuzz --output-on-failure  # exit 0, 5/5; 50 deterministic runs each
./scripts/coverage.sh -DIOX_ENABLE_ILIC=OFF                   # exit 0, 179/179; all five 90/85 gates
./scripts/coverage.sh -DIOX_ENABLE_ILIC=ON -DIOX_ILIC_SOURCE_DIR=/Users/stefan/sources/ilic-fork  # exit 0, 193/193; ilic 92.32/75.00
./scripts/verify-porting-matrix.sh                            # exit 0, 214 relevant + 15 out-of-scope
IOX_ILI_DIR=/tmp/iox-cpp-java-reference-1af01d4 IOX_ILI_JAR=/tmp/iox-cpp-java-reference-build/iox-ili-1.24.5.jar IOX_ILI_CLASSPATH=/tmp/iox-cpp-java-reference-deps/ehibasics-1.3.0.jar:/tmp/iox-cpp-java-reference-deps/ili2c-core-5.6.5.jar IOX_CPP_DUMP=build/phase20/iox-dump ./scripts/differential-java.sh  # exit 0, 7/7
```

No Linux or Windows verification was run, and neither platform is marked as
passed. The actual release evidence is macOS native Debug/Release,
ASan/UBSan, Emscripten/Node/WASM, the Phase 19 real-browser smoke, and the
optional local Java differential run. No CI/CD pipeline, push, package
publication, or network-dependent normal test was added.
