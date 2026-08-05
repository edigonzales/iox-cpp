# Native IOM geometry projection

`IomGeometryConverter` treats an IOM geometry object as the source of truth
and produces a deterministic little-endian WKB projection. The converter does
not mutate or retain the input object. 2D and 3D output use ordinary WKB type
codes and ISO dimensional type codes (`1000 + base type`) respectively; no
SRID or curve-WKB type is emitted.

Supported projections are `COORD`/`MULTICOORD`, `POLYLINE` and its multi
variant, and complete `SURFACE`/`AREA` and multi-surface/multi-area values.
Polygon rings must be closed and line parts must connect in input order. An
explicit `interior` boundary becomes a polygon hole. The converter does not
polygonize arbitrary unordered linework or run `MakeValid`.

`ARC` segments are lossily projected to straight WKB segments. The arc
midpoint (`A1`, `A2`, and optional `A3`) and endpoint (`C1`, `C2`, and
optional `C3`) determine the circle and sweep. The sagitta is taken from
`arcToleranceOverride`, otherwise the descriptor's positive `MaxOverlap`, or
the configured default. The final endpoint is copied exactly from the IOM
value to avoid accumulated floating-point drift. The result reports whether
arcs were approximated and which tolerance was used.

Incomplete/clipped values, custom line forms, line attributes, mixed 2D/3D
ordinates, unsupported dimensions, malformed numbers, broken rings, and
unsupported segment tags raise `IoxError(DiagnosticCode::InvalidGeometry)`.

GEOS is optional and never downloaded by iox-cpp. With `IOX_ENABLE_GEOS=OFF`,
the deterministic native WKB path is used. With GEOS enabled, a private RAII
context and the re-entrant C API validate the projected WKB; there are no
global handlers or public GEOS types. WKB is intentionally the boundary
format: future consumers may attach SRID/model metadata separately, but they
must not infer that the lossy WKB projection can reconstruct original arcs or
line forms.
