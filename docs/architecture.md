# iox-cpp Architecture

## Overview

`iox-cpp` is a native C++17 and WebAssembly library for reading and writing
INTERLIS XTF 2.3 and XTF 2.4 transfer files.

## Module Structure

```text
iox-core        model-independent objects, events, and interfaces
iox-xtf         generic XTF 2.3/2.4 reader/writer
iox-json        test/example event format
iox-abi         stable C ABI
iox-factory     built-in format registry and reader/writer facades
iox-ilic        direct ilic-core integration (optional)
```

## Dependency Direction

```
iox-core  ←  iox-json
iox-core  ←  iox-xtf
iox-core  ←  iox-abi
iox-xtf   ←  iox-ilic  (links ilic-core directly)
iox-factory → iox-xtf + iox-json
```

- `iox-core` has NO XML, Expat, XTF, JSON, or ilic dependency.
- `iox-xtf` has NO ilic-core dependency.
- `iox-ilic` links directly to concrete `ilic-core` types.
- There is no abstract model-provider framework.
- `iox-factory` is the convenience layer that may depend on both built-in
  formats; `iox-core` remains format-independent.

When enabled, `iox-ilic` consumes the concrete pinned-fork API in namespace
`metamodel`. Its public entry point accepts `MetaModelStore` directly. The
constructor walks that store once and copies only names, translation links,
QNames, role targets, enumeration paths, transfer order and small semantic
flags into a private index; it retains no metamodel pointers. The adapter is
composed around the generic `XtfReader`/`XtfWriter` and does not make the
generic XTF targets depend on `ilic-core`.

Name lookup is exact: a canonical or translated scoped name and an expanded
XML QName identify one semantic concept or fail with `ilic.model_mismatch` when
ambiguous. The selected transfer-header model chooses the target-language
variant. The ilic layer handles transfer names, inherited properties, roles,
embedded roles, association transferability, transient views/properties and
enumeration translations. It deliberately is not a general validator: it
does not claim constraint, cardinality or file-wide reference validation.
The CMake options `IOX_ILIC_SOURCE_DIR` and `IOX_FETCH_ILIC` select the
immutable fork source; the default build keeps the module disabled.

## Normative Event Stream

The core API is an ordered `std::variant` event stream:

```text
StartTransferEvent → (StartBasketEvent → ObjectEvent* → EndBasketEvent)* → EndTransferEvent
```

Readers produce this stream; writers consume it. All convenience APIs
delegate to this core.

`BasketReader` is the only intentionally basket-buffering facade. It applies
an optional object-count limit and reports a stable fatal diagnostic when the
limit is exceeded.

## IomObject — Copy-on-Write

`IomObject` is a small, copyable handle wrapping `std::shared_ptr<Impl>`.
Mutating methods call `detach()` to copy shared state before modification,
providing value-like semantics without deep copies on every assignment.

## XML Strategy

- **Reader:** Expat (pinned, static, private). Incremental chunk-based parsing.
- **Writer:** Controlled internal UTF-8 XML writer.
- XML parser and writer headers live under `source/xml`; neither Expat nor the
  internal XML event types are part of the installed/public API.
- Parser callbacks copy QName/attribute data and catch every exception before
  returning through Expat's C callback frame.
- `XtfReader` coordinates version detection, event-order validation, input
  chunks, and a bounded event deque. When `maxQueuedEvents` is reached it
  suspends Expat and resumes only after the consumer drains an event.
- No DOM construction for the whole document.
- No Xerces, no external entity resolution, no DTD.

## XTF Dialects

`XtfReader` and `XtfWriter` are the single public coordinators. The complete
XTF 2.3 reader and writer are private dialects under `source/xtf/v23`; the
reader receives expanded XML names and builds only the current header or
attribute subtree, never a document DOM. The writer consumes one event at a
time and writes directly to `OutputSink`. Wire-format decisions stay in the
dialect, while queueing, limits, terminal writer state, and event order stay in
the coordinators. A failed writer is permanently terminal, and `close()` never
invents missing end events. XTF 2.4 has its own private reader and writer under
`source/xtf/v24`; it shares only coordination and XML mechanics with 2.3.
The 2.4 dialect requires stored expanded names or explicit header namespace
metadata and never fabricates a model namespace URI. Common logic is extracted
only when the wire rules are genuinely identical.

## Format Registry

Additional formats register via explicit C++ interfaces and a testable
registry. Content sniffers return bounded confidence scores; content beats an
extension hint and registration order is the deterministic tie-breaker. The
default registry is initialized by a thread-safe function-local static and
contains XTF plus JSON events when `IOX_ENABLE_JSON_FORMAT` is enabled. No
dynamic plugin loading or global static registrar constructors are used.

## Error Model

Readers and writers keep structured diagnostics with stable codes. Nonfatal
issues are returned through `takeDiagnostics()`. Fatal parser, state,
resource, and I/O failures throw `IoxError`; the C ABI converts them into
terminal status/result objects and never lets an exception cross the C
boundary.

## C ABI

Stable C99-compatible header with opaque handles. All exceptions are caught
at the boundary. Strings are UTF-8. Caller-owned buffers are freed after
function return.

## WASM

Emscripten 3.1.64 with `MODULARIZE=1` and `EXPORT_ES6=1`. Environments:
web, worker, node. No DOM dependencies.
