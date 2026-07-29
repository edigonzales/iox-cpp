---
name: wasm-build
description: Build and test the pinned Emscripten WebAssembly module and its browser, worker, and Node.js wrappers without introducing DOM dependencies.
---

# WASM build skill

## Standard flow

```sh
./scripts/build-wasm.sh
./scripts/test-wasm.sh
```

## Invariants

- Emscripten version exactly matches `.emscripten-version`.
- Output is an ES module with `MODULARIZE=1` and `EXPORT_ES6=1`.
- Supported environments are `web,worker,node`.
- JavaScript does not depend on `window`, `document`, or other DOM-only globals.
- C++ exceptions are caught inside the C ABI.
- Input crosses the boundary as bytes or UTF-8, not raw JS object pointers.
- Event and diagnostic JSON schemas are stable and tested.
- Handles are always destroyed, including error paths.
- Large inputs use incremental feed in the final architecture.

## Tests

- Node.js `node --test` is mandatory.
- Browser and worker APIs need smoke tests that can run locally with a minimal harness.
- Compare normalized Native and WASM event JSON.
- Test repeated create/read/destroy cycles for leaks and stale state.

Do not change the ABI only in JavaScript. Update C header, implementation, schema docs, typings, and tests together.
