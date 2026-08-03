---
name: testing
description: Design and run unit, component, roundtrip, conformance, Unicode, coverage, sanitizer, fuzz, and Native/WASM parity tests for iox-cpp.
---

# Testing skill

## Mandatory test dimensions

- success and failure paths
- event order
- IOM COW and ordering
- malformed XML
- strict versus lenient behavior
- one-byte and randomized chunk boundaries
- Unicode and invalid UTF-8
- references and structures
- all required geometry families
- semantic roundtrip
- deterministic writer bytes
- C ABI argument/lifetime errors
- Native/WASM parity

## Fixture rules

- Keep fixtures minimal and named after one behavior.
- Record provenance and license.
- Golden files require a documented generation method.
- Never overwrite a golden merely because a test fails; determine whether implementation or golden is wrong.

## Coverage

Use `./scripts/coverage.sh`. Exclude third-party and generated glue only. Do not exclude difficult project code to reach thresholds.

## Fuzzing

Every fuzz crash becomes a deterministic regression test before the fix is considered complete.

Never delete, skip, or weaken a valid test to complete a phase.
