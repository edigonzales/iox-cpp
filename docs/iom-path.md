# IOM-Pfade

`iox::IomPath` greift gezielt auf primitive Werte eines `IomObject` zu. Ein
Pfad besteht aus durch Punkte getrennten INTERLIS-Attributnamen; jeder Schritt
wählt den ersten Wert, einen einsbasierten Index oder alle Werte:

```text
publicationDate
metadata.publicationDate
documents[1].fileName
documents[*].fileName
```

Filter, Vergleiche, Referenzen, escapte Bezeichner und null-/negative Indizes
sind nicht unterstützt. Syntaxfehler melden `InvalidArgument`, fehlende Namen
`UnknownInterlisName`. Zwischenschritte müssen Strukturen, das Blatt muss ein
Primitivwert sein.

`primitiveMatches()` liefert kopierte Strings und nullbasierte Wertindizes pro
Schritt. `replaceSinglePrimitive()` verbietet Wildcards und verlangt genau
einen Treffer. Verschachtelte Kinder werden rekursiv geändert und wieder in
ihre Eltern geschrieben, sodass Copy-on-Write sichtbar korrekt bleibt. Ein
optionaler erwarteter Altwert schützt vor Konflikten.
