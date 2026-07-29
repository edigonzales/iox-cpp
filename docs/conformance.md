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
- **Commit:** `8582fff47549f8e0ac4d1cd6ec39c66c2bb708b0` (HEAD on 2026-07-29)

### Pin verification

The repository revisions above were obtained with `git ls-remote` and are
used only as immutable references. The normative PDFs were downloaded on
2026-07-29 and hashed with `shasum -a 256`. Regular builds and tests do not
clone or download any of these references.

## Deliberate Differences from iox-ili

### XTF 2.3 Namespace Convention

`iox-ili` uses the default-namespace XTF 2.3 encoding:
```xml
<TRANSFER xmlns="http://www.interlis.ch/INTERLIS2.3">
  <HEADERSECTION SENDER="..." VERSION="2.3">
```

`iox-cpp` uses the `ili:`-prefixed canonical encoding:
```xml
<ili:TRANSFER xmlns:ili="http://www.interlis.ch/INTERLIS2.3">
  <ili:HEADERSECTION>
    <ili:SENDER>...</ili:SENDER>
```

Both are valid XTF 2.3. The `ili:`-prefixed form is the canonical representation
from the INTERLIS 2.3 reference manual. The default-namespace form is a
compatible variant used by older tools.

### Header Field Encoding

- **iox-ili:** Header fields (SENDER, VERSION, COMMENT) are ATTRIBUTES on HEADERSECTION/MODEL
- **iox-cpp:** Header fields are SUB-ELEMENTS of HEADERSECTION

Tests confirm that iox-ili fixtures parse correctly through our reader
(event structure is preserved), though header field values may differ
in extraction method.

### iox-ili Test Fixtures

Selected fixtures from `claeis/iox-ili/src/test/data/` are included under
`test/fixtures/xtf23/` for conformance testing. These are MIT-licensed.
The full iox-ili test suite also covers model-dependent features
(associations, views, translations) that require `ilic-core` integration.

### XTF 2.3 geometry mapping

The checked-in `Surface.xtf`, `PolylineWithArcs.xtf`, `Area.xtf`, and related
fixtures exercise the generic IOM geometry tree. `COORD`, `ARC`, `POLYLINE`,
`SEGMENTS`, `SURFACE`, `BOUNDARY`, and `AREA` remain structured objects; no
numeric conversion or polygonization is performed by the core.

### XTF 2.4 names

The reader retains the expanded namespace URI and local XML name on every
model-qualified class and property it can observe. Prefixes are lexical hints
only; semantic comparisons use URI plus local name. If a model-free transfer
does not carry enough information to derive an INTERLIS scoped name, the
  expanded XML name is retained rather than guessed.

### XTF 2.4 geometry mapping

The reader normalizes direct XTF 2.4 geometry members into the canonical IOM
shape used by the event stream: `POLYLINE.sequence[]` contains `SEGMENTS`
objects whose repeated `segment` values retain `COORD`, `ARC`, or custom line
form objects. `MULTICOORD`, `MULTIPOLYLINE`, `MULTISURFACE`, and `MULTIAREA`
retain ordered member values. `SURFACE` and `AREA` keep distinct `exterior`
and `interior` attributes. The writer flattens this tree to the normative
XTF 2.4 `geom:` element encoding without numeric conversion.
