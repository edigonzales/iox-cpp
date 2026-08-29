# iox-cpp Conformance References

## Pinned Dependencies

All dependencies are pinned to immutable revisions. No floating branches.

### Expat
- **Version:** 2.6.4 (R_2_6_4)
- **Git tag commit:** `2d994d11e3a470cbee43f8c29e1b274632bc3b44`
- **SHA256:** `a695629dae047055b37d50a0ff4776d1d45d0a4c842cf4ccee158441f55ff7ee`
- **URL:** https://github.com/libexpat/libexpat/releases/download/R_2_6_4/expat-2.6.4.tar.xz
- **License:** MIT

### Emscripten
- **Version:** 3.1.64
- **Pinned in:** `.emscripten-version`

### yyjson
- **Version:** 0.12.0
- **Commit:** `8b4a38dc994a110abaec8a400615567bd996105f`
- **Repository:** https://github.com/ibireme/yyjson
- **License:** MIT
- **Scope:** private implementation dependency of `iox-json` only

## Normative References

### INTERLIS 2.3 Reference Manual
- **URL:** https://www.interlis.ch/download/interlis2/ili2-refman_2006-04-13_d.pdf
- **Retrieved:** 2026-07-29
- **SHA256:** `8ab1c6b5815aa31f424389d7ffd7af95fe0480d95e24ed926366e8db75afbc52`
- **Status:** Normative for XTF 2.3 encoding

### INTERLIS 2.4 Reference Manual (eCH-0031)
- **URL:** https://www.interlis.ch/download/interlis2/STAN_d_DEF_2024-04-24_eCH-0031_V2.1.0_INTERLIS_2-Referenzhandbuch.pdf
- **Retrieved:** 2026-07-29
- **SHA256:** `a8fa2679615ed4bd6939055f3adfd799e0e101afdf7c4cdea9ba85d177e75069`
- **Status:** Normative for XTF 2.4 encoding

## Behavioral References

### iox-api (Java)
- **Repository:** https://github.com/claeis/iox-api
- **Purpose:** Primary behavioral reference for event stream semantics
- **Commit:** `c9209c8ee78225b73b2460561e326b059c40a4ac` (HEAD on 2026-07-29)

### iox-ili (Java)
- **Repository:** https://github.com/claeis/iox-ili
- **Purpose:** Reference XTF reader/writer behavior
- **Commit:** `1af01d4bf6b675a490b9f5ad44d41723fdfa3c0f` (HEAD on 2026-07-29)

### Official INTERLIS Test Suite
- **Repository:** https://github.com/geoadmin/suite-interlis
- **Purpose:** XTF conformance test fixtures
- **Commit:** `f30711184d2374feacb81c5742382d35597164ca` (HEAD on 2026-07-29)

### Historical IOM (GDAL)
- **Repository:** https://github.com/OSGeo/gdal/tree/master/ogr/ogrsf_frmts/ili
- **Purpose:** Supporting reference only — not normative
- **Commit:** `decb67c35ec249c1bae55f53238d5a69e7eff153` (GDAL HEAD on 2026-07-29; historical path `ogr/ogrsf_frmts/ili`)

### ilic-fork
- **Repository:** https://github.com/edigonzales/ilic-fork
- **Purpose:** Compiler/metamodel reference for ilic-core integration
- **Semantic dependency:** `ilic 0.9.10`
- **Release tag:** `v0.9.10`
- **Resolved release commit:** `575e9d43eee460f8e1685d956c498db4425e3fc6`
- **Historical Phase-9 reference:** `8582fff47549f8e0ac4d1cd6ec39c66c2bb708b0` (HEAD on 2026-07-29)

Phase 9 verified the historical checkout through `IOX_ILIC_SOURCE_DIR`. The fork's
public C++ model types are `metamodel::Model`, `SubModel`, `Class`, and
`AttrOrParam`; `iox-ilic` adapts directly to those names. The regular build
does not fetch or configure the optional module, while `IOX_FETCH_ILIC` now
uses the immutable `v0.9.10` tag when explicitly enabled. Source-level
integration remains intentional for the stable `v0.9.10` contract because
that historical tag predates ilic's installable native CMake package. Current
ilic `0.10` snapshots do ship an installable package and are consumed through
vcpkg in the reproducible binary-cache CI. Test `iox.test.ilic.version`
verifies the exact runtime version committed as `runtimeVersion` in the
dependency lock. This is deliberately separate from the vcpkg package version:
the historical package `0.10.0-snapshot.e901af64` reports
`0.10.0-SNAPSHOT` at runtime. The existing model-based test also proves that
the compact index does not retain metamodel pointers beyond construction.

### Pin verification

The repository revisions above were obtained with `git ls-remote` and are
used only as immutable references. The normative PDFs were downloaded on
2026-07-29 and hashed with `shasum -a 256`. Regular builds and tests do not
clone or download any of these references.

## Deliberate Differences from iox-ili

### XTF 2.3 wire rules

Section 3.3.3 (reference-manual pages 79–80) defines the `TRANSFER` expanded
name with namespace `http://www.interlis.ch/INTERLIS2.3`; the XML prefix is not
semantically relevant. The reader therefore accepts both default-namespace and
prefixed documents only when the expanded root name is exact. Section 3.3.4
(pages 80–81) defines `VERSION` and `SENDER` as `HEADERSECTION` attributes and
requires `NAME`, `VERSION`, and `URI` on `MODEL`. Strict mode enforces this;
lenient mode accepts the former 0.1 child-element header encoding with explicit
diagnostics. The 0.2 writer uses the normative attributes.

Sections 3.3.5–3.3.9 (pages 85–88) define data/basket ordering, basket defaults,
object operations, embedded and standalone references, and `ORDER_POS > 0`.
Sections 3.3.11.9–3.3.11.14 (pages 90–92) define structures, lexical coordinate
values, line attributes, clipped polylines/surfaces, boundaries, and reference
attributes. Phase 15 tests these rules in strict and lenient modes and across
whole-input, one-byte, fixed, and deterministic irregular chunk boundaries.

### Behavioral comparison with pinned iox-ili

The pinned `Xtf23Reader.java` remains the behavioral comparison, but the
reference manual wins on conflicts. iox-cpp deliberately rejects XTF 2.2,
which the Java reader partly accepts. It preserves the complete `ALIAS` tree as
an extension until Phase 18 applies model semantics; the Java reader validates
and then discards that tree. iox-cpp also preserves each `OIDSPACE` `NAME` and
`OIDDOMAIN`; the pinned Java implementation assigns synthetic `oidSpaceN`
names. Unlike that implementation, the model-free geometry reader retains
`LINEATTR` and multiple `CLIPPED` groups. No numeric lexical value is converted.

### iox-ili Test Fixtures

The pinned XTF corpus from `claeis/iox-ili/src/test/data/` is included under
`test/fixtures/xtf23/`, `test/fixtures/xtf24/`, and
`test/fixtures/xtf24writer/`. It contains 211 XTF transfer files and 9 `.ili`
model-support files from revision
`1af01d4bf6b675a490b9f5ad44d41723fdfa3c0f`. These fixtures are MIT/X-licensed.
The source-to-local mapping, fixture category, and expected behavior area are
recorded in `test/fixtures/iox-ili-fixtures.tsv`.

The method-level test mapping is recorded in
`docs/iox-ili-test-porting-matrix.md`. Model-dependent features such as
association roles, views, translations, and model declaration validation are
explicitly separated from the model-free event-stream tests. Every inventoried
method has exactly one release status: `adapted`, `deliberate-difference`, or
`out-of-scope`. The offline verifier counts 214 relevant methods and 15
out-of-scope methods and rejects any unclassified row.

### Offline Java differential gate

`scripts/differential-java.sh` compares seven representative XTF 2.3 and 2.4
transfers with the pinned iox-ili revision. It requires an already available
local checkout, jar, dependency classpath, and native `iox-dump`; it verifies
the checkout and jar commit and deliberately never invokes Gradle, downloads a
dependency, or participates in normal CTest.

The comparison covers event order, basket topic/id/kind/consistency, object
tag/id/operation/consistency, references, ordered values within each attribute,
nested objects, and lexical primitive bytes. Attribute names are sorted because
the Java IOM interface does not promise the ordered-attribute contract that
iox-cpp exposes. Only the local tag component is compared because the pinned
Java reader may model-enrich nested association tags. Transfer-header fields,
QNames, source positions, and iox-cpp diagnostics are intentionally outside
this shared subset; they have independent native assertions. These
normalizations are explicit in `scripts/normalize-native-events.py` and
`scripts/java/IoxIliEventDump.java`, rather than allowing a roundtrip to confirm
itself.

### XML security comparison

At the pinned iox-ili revision, `XtfReader.java`, `Xtf23Reader.java`, and
`Xtf24Reader.java` construct a JAXP `XMLInputFactory` without explicitly
setting DTD or external-entity properties. iox-cpp deliberately applies the
stricter rules required by this project's specification: Expat is fixed to
UTF-8, DTD declarations and external entities are rejected, no resolver or
network path exists, and depth, attribute, text-node, and total-input limits
are enforced. This is a robustness difference, not an XTF wire-format
deviation.

### XTF 2.3 geometry mapping

The checked-in `Surface.xtf`, `PolylineWithArcs.xtf`, `Area.xtf`, and related
fixtures exercise the generic IOM geometry tree. `COORD`, `ARC`, `POLYLINE`,
`SEGMENTS`, `SURFACE`, `BOUNDARY`, and `AREA` remain structured objects; no
numeric conversion or polygonization is performed by the core. A line
attribute is stored as `POLYLINE.lineattr`; every direct or clipped segment
sequence is a `SEGMENTS` value under `POLYLINE.sequence`. Clipped polylines and
surfaces carry `Consistency::Incomplete`. Surface clipping groups remain
ordered `SURFACE.clipped` objects with ordered `boundary` values, so a later
writer does not have to guess group boundaries.

### XTF 2.3 writer mapping

The Phase 16 writer emits the section 3.3.3--3.3.11 wire order directly to an
`OutputSink`: `HEADERSECTION` attributes, complete `MODEL` entries, aliases,
OID spaces and comments precede `DATASECTION`; basket and object control
attributes retain their event values. Lexical primitives are never parsed or
normalized. A model-free XML attribute with `OID` is represented internally by
an `IomObject` tagged `OID` with its `oid` field set, which distinguishes it
from element text without adding a typed primitive to `IomValue`.

`LINEATTR`, custom line forms, multiple `CLIPPED` segment sequences and grouped
clipped surfaces are serialized from the canonical IOM geometry tree described
above. Unknown extensions are either emitted with
`extension.unknown-preserved`, diagnosed as dropped, or rejected in strict
mode. Unsupported geometry members are likewise never silently discarded.
Golden writer tests assert independently specified normative bytes before a
separate semantic reparse; roundtrip success is not used as the sole oracle.

The pinned `XtfWriterAlt.java` was inspected for header, basket, object,
reference, line-attribute and clipped-geometry behavior. iox-cpp deliberately
keeps a smaller direct streaming implementation: it has no transfer DOM,
model-provider framework or automatic completion of missing end events.

### XTF 2.4 names

The reader retains the expanded namespace URI and local XML name on every
model-qualified class and property it can observe. Prefixes are lexical hints
only; semantic comparisons use URI plus local name. Model namespaces are
associated with header model names only by the normative
`http://www.interlis.ch/xtf/2.4/ModelName` form or by a single unambiguous root
declaration. If a model-free transfer does not carry enough information to
derive an INTERLIS scoped name, the expanded XML name is retained rather than
guessed. The writer likewise requires a stored QName or an explicit
`ModelEntry.xmlNamespace`; it never constructs a namespace URI from an
INTERLIS name.

The exact expanded control names are enforced: `ili:transfer`,
`headersection`, `models`, `model`, `sender`, `comment`, and `datasection` use
the XTF 2.4 INTERLIS namespace and normative case. Header models are text
entries rather than 2.3-style attributes. Basket metadata, object operations,
references, ordered roles, embedded association payloads and lexical values
are mapped directly to the event/IOM contract. The pinned Java
`Xtf24Reader.java` and `Xtf24WriterAlt.java` were inspected; where their legacy
output differs, the current reference manual is authoritative.

### XTF 2.4 geometry mapping

The reader normalizes direct XTF 2.4 geometry members into the canonical IOM
shape used by the event stream: `POLYLINE.sequence[]` contains `SEGMENTS`
objects whose repeated `segment` values retain `COORD`, `ARC`, or custom line
form objects. `MULTICOORD`, `MULTIPOLYLINE`, `MULTISURFACE`, and `MULTIAREA`
retain ordered member values. `SURFACE` and `AREA` keep distinct `exterior`
and `interior` attributes. The writer flattens this tree to the normative
XTF 2.4 `geom:` element encoding without numeric conversion. XTF 2.4 uses
multi-geometries rather than the 2.3 `CLIPPED`/`LINEATTR` wire forms. Area
multi-values use the normative `geom:multisurface` representation; model-free
input cannot infer an area declaration from that XML shape alone, while the
optional ilic layer can apply that semantic distinction.

### Direct ilic transfer semantics

The Phase 18 API uses the actual pinned-fork type
`metamodel::MetaModelStore`; the namespace and type name differ from the
illustrative names in the coding specification. The store is inspected only
during `IlicModelIndex` construction. Runtime lookup uses copied values and
therefore neither depends on store lifetime nor exposes a second public model
hierarchy.

Canonical and translated models, topics, classes, properties, roles and
enumeration nodes are grouped by `_translationOf`. Scoped INTERLIS names and
expanded QNames are resolved exactly; zero matches remain visible as unknown,
while multiple semantic matches are fatal `ilic.model_mismatch` errors. For XTF
2.4, translated events keep the original model's wire namespace and local XML
name while their INTERLIS name follows the model selected in the transfer
header. This matches the name-mapping approach inspected in the pinned
`Xtf24Reader.java`, `TranslateToOrigin.java`, and translation utilities.

The compact index also records inherited attribute/role order, embedded-role
and target-class metadata, standalone association transferability, transient
views/properties, and hierarchical enumeration paths including `OTHERS`.
Writer transformation reorders only known properties and preserves lexical
primitive bytes. Unknown reader content remains present with diagnostics;
configured writer rejection is fatal and terminal. Canonical nested geometry
and reference helper objects remain opaque to the model layer.

This is intentionally transfer semantics, not a general validation engine.
No claim is made for arbitrary constraints, cardinalities, object existence,
or file-wide reference integrity. The normal tests use a concrete in-memory
`MetaModelStore` and require neither Java nor network access.
