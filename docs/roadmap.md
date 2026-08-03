# iox-cpp Roadmap

| Phase | Description | Status |
|-------|-------------|--------|
| 0 | Repository Baseline, Build, and Agent Control | completed |
| 1 | IOM Core, Events, and JSON Event Format | completed |
| 2 | Secure Incremental XML Layer and XTF Detection | completed |
| 3 | XTF 2.3 Basic Objects and References | completed |
| 4 | XTF 2.3 Geometry and Production Hardening | completed |
| 5 | XTF 2.4 Basic Objects, Namespaces, and References | completed |
| 6 | XTF 2.4 Geometry and Production Hardening | completed |
| 7 | Complete C ABI and Incremental WASM | completed |
| 8 | Idiomatic @interlis/iox-wasm | completed |
| 9 | Direct ilic-core Integration | completed |
| 10 | Convenience APIs, Examples, and iox-dump | completed |
| 11 | Final Conformance, Coverage, and Fuzz Hardening | completed |
| post-11 | iox-ili XTF Test Porting Matrix | completed |
| 13 | Core, Error, and Event Schema 0.2 Reset | completed |
| 14 | Hardened Incremental XML Foundation | completed |
| 15 | Conformant XTF 2.3 Reader | pending |
| 16 | Conformant XTF 2.3 Writer | pending |
| 17 | Conformant XTF 2.4 Dialect | pending |
| 18 | Extended Direct ilic Semantics | pending |
| 19 | Native, WASM, Browser, and Worker Parity | pending |
| 20 | Independent Conformance and Release Gates | pending |

## Acceptance Criteria per Phase

See `docs/phase-status.md` for detailed per-phase criteria and test results.

### Phase 0
- CMake 3.20+ build with C++17
- CTest smoke tests pass (native + C ABI)
- WASM stub module loads in Node.js
- AGENTS.md and all six skills present
- Pinned reference versions in `docs/conformance.md`

### Phase 1
- IomObject COW semantics
- Ordered event stream with std::variant
- JSON event reader/writer (NDJSON)
- Format registry without static init order issues
- Native/WASM identical JSON output

### Phase 2
- Pinned Expat integration
- Chunk-based incremental XML parsing
- XTF version detection (2.2 rejected)
- XTF 2.3 and 2.4 header reading/writing
- Deterministic XML writer

### Phase 3
- XTF 2.3 objects, primitives, structures
- References, baskets, delete
- Model-free roundtrip
- Chunk-boundary tests

### Phase 4
- XTF 2.3 geometry (COORD, ARC, POLYLINE, SURFACE, AREA)
- Clipping/INCOMPLETE
- Geometry shape validator
- 90/85 coverage for XTF 2.3

### Phase 5
- XTF 2.4 namespaces, objects, references
- Namespace table with collision handling
- Model-free roundtrip

### Phase 6
- XTF 2.4 geometry including MULTICOORD, MULTIPOLYLINE, MULTISURFACE, MULTIAREA
- 90/85 coverage for XTF 2.4

### Phase 7
- Full streaming C ABI (feed/next/writer)
- Native/WASM ABI parity
- ASan clean

### Phase 8
- Idiomatic JS API with TypeScript
- Iterator, readAll, writeAll
- Worker protocol

### Phase 9
- IlicModelIndex
- IlicXtfReader, IlicXtfWriter
- Build without iox-ilic still works

### Phase 10
- Basket convenience APIs
- iox-dump tool
- Examples (C++, Node, custom format)

### Phase 11
- All coverage targets met
- Sanitizer passes
- Fuzz targets buildable
- Clean build from scratch
- Final report

### Post-11 quality follow-up
- Pinned `iox-ili` XTF method inventory and status matrix
- Complete pinned XTF fixture manifest
- One-shot/chunked event and diagnostic parity over the XTF corpus
- Semantic XTF 2.4 writer roundtrips and ordered-value regression coverage
- Explicit `ilic-required`, `api-gap`, and out-of-scope model/test boundaries
