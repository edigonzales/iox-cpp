# IOM paths

`iox::IomPath` is a deliberately small accessor for primitive values in an
`IomObject`. A path is a dot-separated sequence of INTERLIS attribute names.
Each step can select the first value, a 1-based value index, or all values:

```text
publicationDate
metadata.publicationDate
documents[1].fileName
documents[*].fileName
```

The parser does not implement filters, comparisons, references, escaped
identifiers, or zero-/negative-based indexes. Syntax errors use
`DiagnosticCode::InvalidArgument`. Missing attributes are reported as
`UnknownInterlisName`; a structure is required for intermediate steps and a
primitive is required at the leaf.

`primitiveMatches()` returns copied strings. `valueIndexes` contains the
selected zero-based `IomObject` value index for every path step, including
intermediate steps; this makes the result directly usable with
`IomObject::replaceValue()`.

`replaceSinglePrimitive()` rejects wildcards and requires exactly one match.
For nested values it reads, recursively updates, and writes the child back to
the parent, preserving `IomObject` copy-on-write behavior. The optional
expected value is checked before mutation and raises `ModelMismatch` when it
does not match.
