# Architektur

`iox-cpp` liest und schreibt INTERLIS XTF 2.3/2.4 nativ und als
WebAssembly-Modul.

```text
iox-core        modellfreie Objekte, Events und Interfaces
iox-geometry    IOM-zu-WKB-Projektion, optional mit GEOS-Prüfung
iox-xtf         generischer XTF-2.3/2.4-Reader und -Writer
iox-json        Eventformat für Tests und Beispiele
iox-abi         stabile C-ABI
iox-factory     Registry und Convenience-Fassaden
iox-ilic        optionale direkte ilic-core-Integration
```

## Abhängigkeitsrichtung

```text
iox-core  ← iox-json, iox-geometry, iox-xtf, iox-abi
iox-xtf   ← iox-ilic
iox-geometry ← iox-ilic
iox-factory → iox-xtf + iox-json
```

`iox-core` kennt weder XML, Expat, XTF, JSON noch ilic. `iox-xtf` kennt
`ilic-core` nicht. Nur das optionale `iox-ilic` bindet konkrete ilic-Typen ein;
es gibt kein abstraktes Model-Provider- oder dynamisches Plugin-System.

`IlicModelIndex` läuft einmal über `metamodel::MetaModelStore` und kopiert nur
Namen, Übersetzungsbeziehungen, QNames, Rollen, Enumerationen,
Transferreihenfolge und kleine Flags. Es hält keine Metamodellzeiger. Die
kopierten Property- und Geometriedeskriptoren sind unter
[Modelldeskriptoren](model-descriptors.md) beschrieben.

## Event- und Objektmodell

```text
StartTransferEvent → (StartBasketEvent → ObjectEvent* → EndBasketEvent)* → EndTransferEvent
```

Reader erzeugen, Writer konsumieren diesen geordneten `std::variant`-Stream.
Nur `BasketReader` puffert bewusst einen ganzen Korb. `IomObject` ist ein
Copy-on-Write-Handle; schreibende Operationen lösen gemeinsam benutzten Zustand
vor der Änderung. [IOM-Pfade](iom-path.md) greifen auf primitive Werte zu.

## XTF und XML

Der Reader verwendet einen privaten, statisch gebundenen Expat und akzeptiert
beliebige Byte-Chunks. Der Writer erzeugt kontrolliertes UTF-8-XML. Parser- und
XML-Eventtypen sind keine öffentliche API. XTF 2.3 und 2.4 besitzen getrennte
Dialekte; Factory-Sniffing wählt nur den passenden Reader.

Reader besitzen eine begrenzte Event-Queue und liefern `NeedInput`, `Event`
oder `End`. Writer sind terminal: Nach einem Fehler oder `close()` ist kein
weiteres Schreiben erlaubt. Diagnosen haben stabile Codes; Meldungstexte dürfen
sich weiterentwickeln.

## Parallelität und Lebensdauer

Reader, Writer und ilic-Indizes besitzen ihren Zustand selbst und teilen keine
globalen Parser-, Modell- oder GEOS-Kontexte. Ein Objekt wird nicht gleichzeitig
aus mehreren Threads verwendet; unabhängige Instanzen dürfen parallel laufen.
Callbacks lassen keine C++-Exception über C- oder Emscripten-Grenzen entkommen.
