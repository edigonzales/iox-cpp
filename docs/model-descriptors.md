# ilic model descriptors

`iox::ilic::IlicModelIndex` copies the small subset of ilic metadata needed by
native consumers. `PropertyDescriptor` and `GeometryDescriptor` contain only
values, strings, optionals, and vectors; they do not retain pointers into the
`metamodel::MetaModelStore`. An index can therefore outlive the store passed to
its constructor.

`propertyDescriptor()` returns one selected property variant. Its `name` is
the local transfer name for the requested target model and XTF version.
`propertyFqn` is the stable fully qualified name of the untranslated semantic
property. `transferPropertyDescriptors()` uses the same inherited transfer
order as `transferProperties()` and excludes transient properties.

Geometry descriptors identify the INTERLIS line or coordinate kind, the
coordinate domain, and the number of coordinate axes. A non-empty
`MaxOverlap` is preserved as a lexical value and parsed as a finite positive
`double`; malformed or non-positive values raise `IoxError` with
`DiagnosticCode::ModelMismatch`.

`STRAIGHTS` and `ARCS` are represented by the corresponding standard flags.
Other line forms are retained in `lineForms` and set `hasCustomLineForms`.
An empty INTERLIS line-form list means the default straight-segment form, so
`hasStraights` is true and `lineForms` remains empty. `LAStructure` is exposed
only through the value flag `hasLineAttributes` and custom line-form structure
names are copied into `structureFqn`.

The descriptor API is intentionally metadata-only. It does not validate
coordinate dimensionality for a downstream storage format; consumers that
support only 2D and 3D should report an explicit projection error for other
dimensions.
