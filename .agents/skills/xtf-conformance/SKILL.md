---
name: xtf-conformance
description: Implement and verify XTF 2.3/2.4 transfer semantics against the normative specifications, iox-ili behavior, and checked-in golden fixtures.
---

# XTF conformance skill

## Source priority

1. Current normative INTERLIS reference manual for the relevant version.
2. `iox-api`/`iox-ili` behavior.
3. Official XTF test suite.
4. Historical IOM behavior as supporting evidence only.

## Workflow for every feature

1. Identify the normative rule and record the reference in `docs/conformance.md`.
2. Inspect the corresponding Java reader/writer behavior.
3. Add minimal positive and negative fixtures.
4. Implement the version-specific dialect behavior.
5. Add chunk-boundary tests.
6. Add semantic roundtrip tests.
7. Add Native/WASM parity once WASM is available.
8. Document deliberate differences.

## Rules

- Never use lexical XML comparison as the only correctness check.
- Never infer missing namespace information silently.
- Preserve unknown fachlich relevant elements or diagnose them.
- Treat XML malformed input as fatal.
- Keep XTF 2.3 and 2.4 logic separate where encoding differs.
- Geometry uses canonical IOM structures; no GEOS conversion.
- No regular test may call Java or the network.
