# ilic-Modelldeskriptoren

`iox::ilic::IlicModelIndex` kopiert die für native Consumer benötigten
Metadaten. `PropertyDescriptor` und `GeometryDescriptor` enthalten nur Werte,
Strings, Optionals und Vektoren und dürfen den `MetaModelStore` überleben.

`propertyDescriptor()` liefert die ausgewählte Übersetzungsvariante. `name` ist
der lokale Transfername, `propertyFqn` der stabile vollqualifizierte Name des
kanonischen semantischen Elements. `transferPropertyDescriptors()` folgt der
geerbten Transferreihenfolge und lässt transiente Properties aus.

Geometriedeskriptoren enthalten INTERLIS-Typ, Koordinatendomäne, Achsenzahl,
lexikalisches und geprüftes `MaxOverlap`, Standard-/eigene Linienformen sowie
das Linienattribut-Flag. Eine leere Linienformenliste bedeutet standardmässig
gerade Segmente. Ungültiges oder nichtpositives `MaxOverlap` ergibt
`DiagnosticCode::ModelMismatch`.

Die API beschreibt nur Metadaten. Sie garantiert nicht, dass jede Dimension
in ein bestimmtes Storage-Format projizierbar ist; Consumer müssen dafür eine
eigene explizite Diagnose liefern.
