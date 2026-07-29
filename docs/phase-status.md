# iox-cpp Phase Status

| Phase | Status | Commit | Native Tests | WASM Tests | Coverage | Notes |
|-------|--------|--------|-------------|------------|----------|-------|
| 0 | completed | pending | `./scripts/build-native.sh` (0); `./scripts/test-native.sh` (0, 18/18) | `./scripts/build-wasm.sh` (0, Emscripten 3.1.64); `./scripts/test-wasm.sh` (0, 2/2) | N/A | Baseline, pinned references, agent controls, native/WASM ABI smoke |
| 1 | in-progress | — | — | — | — | IOM/events/JSON API verification and ABI wrapper work |
| 2 | not-started | — | — | — | — | Secure XML and XTF header gate |
| 3 | not-started | — | — | — | — | XTF 2.3 objects/references |
| 4 | not-started | — | — | — | — | XTF 2.3 geometry |
| 5 | not-started | — | — | — | — | XTF 2.4 objects/namespaces |
| 6 | not-started | — | — | — | — | XTF 2.4 geometry |
| 7 | not-started | — | — | — | — | Complete streaming C ABI |
| 8 | not-started | — | — | — | — | JavaScript/worker API |
| 9 | not-started | — | — | — | — | Direct ilic-core integration |
| 10 | not-started | — | — | — | — | Convenience APIs, examples, iox-dump |
| 11 | not-started | — | — | — | — | Final conformance, coverage, sanitizer, fuzz gates |

## Build Commands

```sh
# Native build and test
./scripts/build-native.sh
./scripts/test-native.sh

# WASM smoke test
node --test packages/iox-wasm/test/*.test.mjs
```

## Phase 7 — C-ABI Results

- Full C99 header with opaque handles (iox_reader_t, iox_writer_t, iox_result_t)
- Status codes: OK, EVENT, NEED_INPUT, END, ERROR, INVALID_ARGUMENT, INVALID_STATE
- Reader: create, feed, finish, next, destroy
- Writer: create, write_event_json, finish (with byte output), destroy
- Result: json, bytes, size, status, destroy
- C smoke test passes all assertions
- Null-pointer safety tested
- Exceptions caught at ABI boundary
- Native/WASM parity maintained

## Phase 0 — Results

- `git rev-parse --show-toplevel` → `/Users/stefan/sources/iox-cpp-codex`
- Initial pre-Git identity commands failed with `fatal: not a git repository`;
  the user explicitly required initialization, so `git init -b main` was run.
- `cmake 4.4.0`, AppleClang 21, Node.js v22.23.1, Emscripten 3.1.64.
- Expat is static and fetched from the pinned 2.6.4 archive hash.
- The generated ES module is copied to `packages/iox-wasm` by the build script;
  the `.wasm` binary remains a build product and is ignored by Git.

## Phase 9 — Not Started

`iox-ilic` requires fetching `edigonzales/ilic-fork` to inspect the actual
`ilic-core` API types. The CMake infrastructure is prepared
(`IOX_ENABLE_ILIC`, `IOX_FETCH_ILIC`, `IOX_ILIC_SOURCE_DIR`).

## Phase 10 — Not Started

- Final acceptance is deferred until the phase is executed in order.

## Phase 11 — Not Started

- Coverage gates (90% line / 85% branch) not measured
- ASan/UBSan not run
- Fuzz targets not built or run
- Clean build from scratch not verified
