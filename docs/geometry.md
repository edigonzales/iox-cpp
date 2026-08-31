# Native IOM-Geometrieprojektion

`IomGeometryConverter` erzeugt deterministisches Little-Endian-WKB, ohne das
IOM-Eingabeobjekt zu verändern oder zu behalten. 2D verwendet normale, 3D die
ISO-Typcodes `1000 + Basistyp`; SRID und Curve-WKB werden nicht ausgegeben.

Unterstützt sind `COORD`/`MULTICOORD`, `POLYLINE`/MultiPolyline sowie
vollständige `SURFACE`/`AREA`- und Multi-Varianten. Ringe müssen geschlossen
und Linienteile in Eingabereihenfolge verbunden sein. `interior` wird zum Loch;
ungeordnetes Linienwerk wird weder polygonisiert noch mit `MakeValid` repariert.

`ARC` wird verlustbehaftet in gerade Segmente projiziert. Toleranzpriorität:
expliziter Override, positives `MaxOverlap`, dann der konfigurierte Default.
Der Endpunkt wird exakt aus dem IOM kopiert, um Drift zu vermeiden. Das
Resultat meldet Approximation und verwendete Toleranz.

Clipping, eigene Linienformen, Linienattribute, gemischte Dimensionen,
ungültige Zahlen/Ringe und unbekannte Segmente ergeben
`DiagnosticCode::InvalidGeometry`.

GEOS ist optional. Ohne GEOS bleibt der deterministische WKB-Pfad aktiv; mit
GEOS validiert ein privater re-entrant RAII-Kontext das Ergebnis. WKB ist die
bewusste Grenze und kann ursprüngliche Bögen oder Linienformen nicht
rekonstruieren.
