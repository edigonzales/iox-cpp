---
name: architecture
description: Protect the module boundaries, public API invariants, event model, IOM copy-on-write semantics, and direct optional ilic-core integration of iox-cpp.
---

# Architecture skill

Use this skill for public APIs, module boundaries, ownership, event flow, IOM changes, format registration, and ilic integration.

## Required reading

- `docs/architecture.md`
- `docs/conformance.md`
- public headers under `include/iox/`

## Invariants

1. `iox-core` has no XML, Expat, XTF, JSON, or ilic dependency.
2. `iox-xtf` has no `ilic-core` dependency.
3. `iox-ilic` links directly to concrete `ilic-core` types. Never introduce a generic provider hierarchy.
4. The normative API is the ordered `IoxEvent` stream.
5. `IomObject` preserves attribute order and repeated-value order.
6. Public C++ ownership uses RAII. No public retain/release API.
7. Public dependency-heavy classes use PImpl.
8. Unknown fachlich relevant content is preserved or diagnosed; never silently dropped.
9. XTF 2.3 and XTF 2.4 remain separate dialect implementations.
10. Convenience APIs delegate to the normative event API.

## Review checklist

- Does the change add a dependency in the wrong direction?
- Does it expose Expat or ilic internals in unrelated public headers?
- Does a mutation correctly detach shared IOM state?
- Is order preserved?
- Is output deterministic?
- Are fatal and nonfatal failures separated?
- Does the C ABI catch all exceptions?
- Can the generic model-free path still build and run?
- Are Native and WASM semantics identical?

Reject architecture shortcuts that make a single fixture pass while violating these invariants.
