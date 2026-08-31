# Konformität und Referenzen

Produktionsabhängigkeiten und Referenzen sind auf unveränderliche Versionen
oder Commits festgelegt; bewegliche Branches sind nicht zulässig.

## Toolchain und Implementierungsabhängigkeiten

| Komponente | Festlegung | Rolle |
| --- | --- | --- |
| Expat | `R_2_6_4`, Commit `2d994d11e3a470cbee43f8c29e1b274632bc3b44` | privater XTF-XML-Reader im Source-Build |
| yyjson | `0.12.0`, Commit `8b4a38dc994a110abaec8a400615567bd996105f` | private `iox-json`-Implementierung |
| Emscripten | `.emscripten-version` | WASM-Toolchain |
| ilic | `release/dependencies.lock.json` | optionale Modellintegration und reproduzierbarer CI-Lock |

Die vcpkg-CI darf andere, ebenfalls explizit gelockte Expat-/ilic-Paketstände
verwenden. Die maschinenlesbare Releasequelle ist der Dependency-Lock, nicht
diese Übersicht.

## Normative Dokumente

- [INTERLIS-2.3-Referenzhandbuch](https://www.interlis.ch/download/interlis2/ili2-refman_2006-04-13_d.pdf),
  SHA-256 `8ab1c6b5815aa31f424389d7ffd7af95fe0480d95e24ed926366e8db75afbc52`
- [INTERLIS-2.4-Referenzhandbuch eCH-0031](https://www.interlis.ch/download/interlis2/STAN_d_DEF_2024-04-24_eCH-0031_V2.1.0_INTERLIS_2-Referenzhandbuch.pdf),
  SHA-256 `a8fa2679615ed4bd6939055f3adfd799e0e101afdf7c4cdea9ba85d177e75069`

Bei Widersprüchen sind diese Handbücher für das XTF-Wireformat massgeblich.

## Verhaltensreferenzen

| Quelle | Commit | Verwendung |
| --- | --- | --- |
| [iox-api](https://github.com/claeis/iox-api) | `c9209c8ee78225b73b2460561e326b059c40a4ac` | Event-Semantik |
| [iox-ili](https://github.com/claeis/iox-ili) | `1af01d4bf6b675a490b9f5ad44d41723fdfa3c0f` | Reader-/Writer-Vergleich und Fixtures |
| [suite-interlis](https://github.com/geoadmin/suite-interlis) | `f30711184d2374feacb81c5742382d35597164ca` | Conformance-Fixtures |
| [GDAL IOM](https://github.com/OSGeo/gdal/tree/master/ogr/ogrsf_frmts/ili) | `decb67c35ec249c1bae55f53238d5a69e7eff153` | nur historische Zusatzreferenz |

Die [Portierungsmatrix](iox-ili-test-porting-matrix.md) inventarisiert die
übernommenen Java-Testverträge. Reguläre Tests laden keine dieser Quellen aus
dem Netz.

## Bewusste Unterschiede

- Der generische Reader filtert keine Körbe und bewahrt fachlich relevante
  Daten auch ohne Modell.
- iox-cpp ist kein allgemeiner Constraint-, Kardinalitäts- oder
  Referenzvalidator.
- Lexikalische Primitive werden nicht ungefragt in JSON-Zahlen, Booleans oder
  Datumswerte umgewandelt.
- XTF-2.3-Namespaces, Headerattribute, Referenzen, Geometrie-Clipping und
  Linienattribute folgen dem Referenzhandbuch, auch wenn historische Java-
  Ausgaben abweichen.
- XTF 2.4 vergleicht expandierte XML-Namen statt Präfixe und bewahrt
  Multi-Geometrien in Reihenfolge.
- Die optionale ilic-Schicht löst Übersetzungen, Rollen und Transferreihenfolge
  auf, erfindet aber keine zweite öffentliche Modellhierarchie.

Golden Tests prüfen unabhängig vorgegebene Bytes; Reader→Writer→Reader allein
gilt nicht als hinreichendes Oracle. Chunk-Matrizen vergleichen One-shot,
Ein-Byte- und weitere deterministische Aufteilungen.
